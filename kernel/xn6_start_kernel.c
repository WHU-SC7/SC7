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
    PRINT_COLOR(BLUE_COLOR_PRINT, "xn6_start_kernel at :%p\n",
                &xn6_start_kernel);

    proc_init();
    printf("proc初始化完成\n");

#if defined RISCV // 进入用户态
    // 下面设置只涉及csr
    // 设置ecall的跳转地址到stvec，固定为uservec
    hsai_set_usertrap();
    // 设置sstatus
    hsai_set_csr_to_usermode(); //
    // 设置sepc. 指令sret使用。S态进入U态
    hsai_set_csr_sepc((uint64)(void *)init_main); // *******************************************

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

#else // loongarch
    struct proc *p = allocproc();
    current_proc = p;

    // trap_init.!!!!!!!!!有了这个才能跑
    uint32 ecfg = (0U << CSR_ECFG_VS_SHIFT) | HWI_VEC | TI_VEC; // 例外配置
    // uint64 tcfg = 0x1000000UL | CSR_TCFG_EN | CSR_TCFG_PER;
    w_csr_ecfg(ecfg);
    // w_csr_tcfg(tcfg);//计时器

    // 设置syscall的跳转地址到CSR.EENTRY
    printf("uservec address: %p\n", &uservec);
    w_csr_eentry((uint64)uservec); // send syscalls, interrupts, and exceptions
                                   // to trampoline.S
    // 类似设置sstatus
    uint32 x = r_csr_prmd();
    x |= PRMD_PPLV; // set PPLV to 3 for user mode
    x |= PRMD_PIE;  // enable interrupts in user mode
    w_csr_prmd(x);
    // 设置era,指令ertn使用。S态进入U态
    w_csr_era((uint64)(void *)init_main);
    printf("era init_main address: %p\n", &init_main);

    // 下面涉及到trapframe
    // 设置内核栈，用户栈，用户页表(现在是0)
    hsai_set_trapframe_kernel_sp(p->trapframe, p->kstack);
    hsai_set_trapframe_user_sp(p->trapframe, (uint64)user_stack + 4096);
    hsai_set_trapframe_pagetable(p->trapframe, 0);
    // 设置内核异常处理函数的地址，固定为usertrap
    hsai_set_trapframe_kernel_trap(p->trapframe);

    printf("hsai设置完成,准备进入用户态\n");
    pmem_init();
    vmem_init();
    vmem_test();
    // test_print();
    // test_assert();
    // test_spinlock ();
    userret((uint64)p->trapframe);

    while (1)
        ;

    printf("never reach");
#endif
    // 初始化物理内存
    pmem_init();

    vmem_init();
    // test_pmem();
    vmem_test();
    // test_print();
    // test_assert();
    // test_spinlock ();

    // 只用于riscv
    //  virtio_writeAndRead_test();
    //  hsai_set_usertrap();//!!!!!!!!!!!注意，这里还要重新设置sepc，因为之前磁盘触发的内核中断保存sepc并且返回了virtio_disk_rw.
    //  hsai_set_csr_sepc((uint64)(void *)init_main);
    //  //{while(1);}
    //  userret((uint64)p->trapframe);

    p->state = RUNNABLE;
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
    w_stvec((uint64)kernelvec &
            ~0x3); // 设置好内核中断处理函数的地址，中断而不是异常！
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
    printf("----------------------------\n读写磁盘测试完成!\n");
#else // loongarch没有！

#endif
}
