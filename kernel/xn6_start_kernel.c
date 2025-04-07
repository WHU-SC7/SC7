// complete all data type macro is in type.h

// this program only need these
// wait to be unified in style
#include "types.h"
#include "test.h"
#include "uart.h"
#include "print.h"
#include "process.h"
#include "pmem.h"
#include "string.h"
#include "virt.h"
#include "hsai_trap.h"
#include "plic.h"
#include "vmem.h"
#if defined RISCV
#include "riscv.h"
#include "riscv_memlayout.h"
#else
#include "loongarch.h"
#endif

extern struct proc *current_proc;
extern void uservec();
extern void *userret(uint64); // trampoline 进入用户态
extern int init_main(void);   // 用户程序

__attribute__((aligned(4096))) char user_stack[4096]; // 用户栈

int main()
{
    while (1)
        ;
} // 为了通过编译, never use this

void test_pmem();
void virtio_writeAndRead_test();
void loongarch_user_program_run();
void riscv_user_program_run();
void user_program_run();
void init_process();
extern void kernelvec();

struct buf buf; // 临时用来测试磁盘读写
int xn6_start_kernel()
{
    // if ( hsai::get_cpu()->get_cpu_id() == 0 )
    uart_init();
    for (int i = 65; i < 65 + 26; i++)
    {
        put_char_sync(i);
        put_char_sync('t');
    }
    put_char_sync('\n');
    LOG("xn6_start_kernel at :%p\n", &xn6_start_kernel);

    proc_init();
    printf("proc初始化完成\n");
    hsai_trap_init();

    // 初始化init线程
    init_process();
    // scheduler();
    // while(1);

    // 初始化物理内存
    pmem_init();

    vmem_init();
    // test_pmem();
    vmem_test();
    // test_print();
    // test_assert();
    // test_spinlock ();

    scheduler();
    while (1)
        ;
    return 0;
}

void virtio_writeAndRead_test()
{
#if defined RISCV
    // 发送写请求后，进程（现在只有一个）等待磁盘读写完成后的中断信号。
    // 中断会在virtio_disk_intr()标识读写完成并且打印读写数据，中断结束后让进程被唤醒
    printf("识别硬盘\n");
    virtio_disk_init();
    w_stvec((uint64)kernelvec & ~0x3); // 设置好内核中断处理函数的地址，中断而不是异常！
    plicinit();
    plicinithart();
    buf.blockno = 6;
    buf.dev = 1;
    memset((void *)buf.data, 7, 1024);
    virtio_rw(&buf, 1); // 每一字节都写7
    virtio_rw(&buf, 0);
    memset((void *)buf.data, 9, 1024);
    virtio_rw(&buf, 1); // 每一字节都写9
    virtio_rw(&buf, 0);
    printf("----------------------------\n读写磁盘测试完成!\n"); ///< 测试完之后要进入用户程序一定要设置sepc和stval！使用例子如下
// 只用于riscv
//  virtio_writeAndRead_test();
//  hsai_set_usertrap();//!!!!!!!!!!!注意，这里还要重新设置sepc，因为之前磁盘触发的内核中断保存sepc并且返回了virtio_disk_rw.
//  hsai_set_csr_sepc((uint64)(void *)init_main);
//  userret((uint64)p->trapframe);
#else // loongarch没有！

#endif
}

/**
 * @brief 初始化init线程，进入scheduler后调度执行
 */
void init_process()
{
    struct proc *p = allocproc();
    current_proc = p;
    p->state = RUNNABLE;
    hsai_set_trapframe_epc(p->trapframe, (uint64)init_main);
    hsai_set_trapframe_user_sp(p->trapframe, (uint64)user_stack + 4096);
}

// #define PTE_V (1L << 0) // valid
// #define PTE_R (1L << 1)
// #define PTE_W (1L << 2)
// #define PTE_X (1L << 3)
// #define PTE_U (1L << 4) // 1 -> user can access
#define PTE_G (1 << 5)
#define PTE_A (1 << 6)
// #define PTE_D (1 << 7)
#define BASE_ADDRESS 0        ///< 使用第一个页。目前的用户程序用一个页应该足够了
#define USTACK_ADDRESS 0x2000 ///< 使用第二个页
/**
 * @brief 虚拟内存下的用户程序
 */
void vm_user_program_run()
{
    // 下面设置只涉及csr
    // 设置ecall的跳转地址到stvec，固定为uservec
    hsai_set_usertrap();
    // 设置sstatus
    hsai_set_csr_to_usermode(); //
    // 设置sepc. 指令sret使用。S态进入U态
    hsai_set_csr_sepc((uint64)(void *)init_main);

    // 下面设置涉及到trapframe
    // 分配线程，现在只在用户态进行一次系统调用
    struct proc *p = allocproc();
    current_proc = p;

    // 初始化用户页表
    uint64 *user_pagetable = pmem_alloc_pages(1); // 页面已经清零
    mappages(user_pagetable, BASE_ADDRESS, (uint64)init_main, PGSIZE, PTE_U | PTE_R | PTE_W | PTE_X);
    mappages(user_pagetable, TRAPFRAME, (uint64)p->trapframe, PGSIZE, PTE_R | PTE_W);
    mappages(user_pagetable, USTACK_ADDRESS, (uint64)pmem_alloc_pages(1), PGSIZE, PTE_R | PTE_W | PTE_X); ///< 栈需要执行权限吗？

    // 设置内核栈，用户栈,用户页表
    hsai_set_trapframe_kernel_sp(p->trapframe, p->kstack + 4096);
    hsai_set_trapframe_user_sp(p->trapframe, USTACK_ADDRESS);
    hsai_set_trapframe_pagetable(p->trapframe, (uint64)user_pagetable);
    // 设置内核异常处理函数的地址，固定为usertrap
    hsai_set_trapframe_kernel_trap(p->trapframe);
    LOG("hsai设置完成\n");
    // 运行线程
    userret((uint64)p->trapframe);
}

void user_program_run() ///< 两种架构都可以
{
    // 下面设置只涉及csr
    // 设置ecall的跳转地址到stvec，固定为uservec
    hsai_set_usertrap();
    // 设置sstatus
    hsai_set_csr_to_usermode(); //
    // 设置sepc. 指令sret使用。S态进入U态
    hsai_set_csr_sepc((uint64)(void *)init_main);

    // 下面设置涉及到trapframe
    // 分配线程，现在只在用户态进行一次系统调用
    struct proc *p = allocproc();
    current_proc = p;
    // 设置内核栈，用户栈,用户页表
    hsai_set_trapframe_kernel_sp(p->trapframe, p->kstack);
    hsai_set_trapframe_user_sp(p->trapframe, (uint64)user_stack + 4096);
    hsai_set_trapframe_pagetable(p->trapframe, 0);
    // 设置内核异常处理函数的地址，固定为usertrap
    hsai_set_trapframe_kernel_trap(p->trapframe);
    LOG("hsai设置完成\n");
    // 运行线程
    userret((uint64)p->trapframe);
}

/**
 * 老版本的用户程序测试函数
 */
#if defined RISCV
void riscv_user_program_run()
{
    // 下面设置只涉及csr
    // 设置ecall的跳转地址到stvec，固定为uservec
    hsai_set_usertrap();
    // 设置sstatus
    hsai_set_csr_to_usermode(); //
    // 设置sepc. 指令sret使用。S态进入U态
    hsai_set_csr_sepc((uint64)(void *)init_main);

    // 下面设置涉及到trapframe
    // 分配线程，现在只在用户态进行一次系统调用
    struct proc *p = allocproc();
    current_proc = p;
    // 设置内核栈，用户栈,用户页表
    hsai_set_trapframe_kernel_sp(p->trapframe, p->kstack);
    hsai_set_trapframe_user_sp(p->trapframe, (uint64)user_stack + 4096);
    hsai_set_trapframe_pagetable(p->trapframe, 0);
    // 设置内核异常处理函数的地址，固定为usertrap
    hsai_set_trapframe_kernel_trap(p->trapframe);
    LOG("hsai设置完成\n");
    // 运行线程
    userret((uint64)p->trapframe);
}
#else
void loongarch_user_program_run()
{
    struct proc *p = allocproc();
    current_proc = p;

    // trap_init.!!!!!!!!!有了这个才能跑
    uint32 ecfg = (0U << CSR_ECFG_VS_SHIFT) | HWI_VEC | TI_VEC; // 例外配置
    // uint64 tcfg = 0x1000000UL | CSR_TCFG_EN | CSR_TCFG_PER;
    w_csr_ecfg(ecfg);
    // w_csr_tcfg(tcfg);//计时器                          ///<类似trap_init，只设置一次即可，应该与usertrapret无关

    // 设置syscall的跳转地址到CSR.EENTRY
    printf("uservec address: %p\n", &uservec);
    w_csr_eentry((uint64)uservec); //                    ///< hsai_set_usertrap

    // 类似设置sstatus
    uint32 x = r_csr_prmd();
    x |= PRMD_PPLV; // set PPLV to 3 for user mode
    x |= PRMD_PIE;  // enable interrupts in user mode
    w_csr_prmd(x);  ///< hsai_set_csr_to_usermode

    // 设置era,指令ertn使用。S态进入U态
    w_csr_era((uint64)(void *)init_main);
    printf("era init_main address: %p\n", &init_main); ///< hsai_set_csr_sepc

    // 下面涉及到trapframe
    // 设置内核栈，用户栈，用户页表(现在是0)
    hsai_set_trapframe_kernel_sp(p->trapframe, p->kstack);
    hsai_set_trapframe_user_sp(p->trapframe, (uint64)user_stack + 4096);
    hsai_set_trapframe_pagetable(p->trapframe, 0);
    // 设置内核异常处理函数的地址，固定为usertrap
    hsai_set_trapframe_kernel_trap(p->trapframe);

    printf("hsai设置完成,准备进入用户态\n");
    userret((uint64)p->trapframe);
}
#endif
