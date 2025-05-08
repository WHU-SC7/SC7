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
#include "inode.h"
#include "fs.h"
#include "file.h"
#include "vfs_ext4.h"
#include "buf.h"
#include "vma.h"

#if defined RISCV
#include "riscv.h"
#include "riscv_memlayout.h"
#else
#include "loongarch.h"
#endif

extern struct proc *current_proc;
extern void uservec();
// extern void *userret(uint64); // trampoline 进入用户态
extern int init_main(void); // 用户程序

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

extern void virtio_probe();
extern void la_virtio_disk_init();

struct buf buf; // 临时用来测试磁盘读写
int sc7_start_kernel()
{
    // if ( hsai::get_cpu()->get_cpu_id() == 0 )
    consoleinit();
    printfinit();
    for (int i = 65; i < 65 + 26; i++)
    {
        put_char_sync(i);
        put_char_sync('t');
    }
    put_char_sync('\n');
    LOG("sc7_start_kernel at :%p\n", &sc7_start_kernel);

    proc_init();
    printf("proc初始化完成\n");
    // 初始化物理内存
    pmem_init();
    vmem_init();
    hsai_trap_init();
    
    printf("开始查找设备\n");
#if defined RISCV
    virtio_writeAndRead_test();
    //while(1);
#else //< loongarch识别磁盘。不是-M ls2k
    virtio_probe();//发现virtio-blk-pci设备
    la_virtio_disk_init();
    printf("la virtio初始化完成\n");
    virtio_writeAndRead_test();
    printf("-------------------------------\nla读写磁盘成功!\n");
#endif
    init_fs_table();
    binit();
    fileinit();
    inodeinit();
    vfs_ext4_init();
    // vmem_test();
    //  test_print();
    //  test_assert();
    //  test_spinlock ();
    // 初始化init线程
    init_process();

    scheduler();
    while (1)
        return 0;
}

#if defined RISCV

#else
extern void la_virtio_disk_rw(struct buf *b, int write); //< loongarch磁盘读写
#endif

void virtio_writeAndRead_test()
{
#if defined RISCV
    /*在最近的修改之后，要手动开启外部中断了.最终版不能这么做*/
    // intr_on();
    printf("virtio测试,开启外部中断");
    // 发送写请求后，进程（现在只有一个）等待磁盘读写完成后的中断信号。
    // 中断会在virtio_disk_intr()标识读写完成并且打印读写数据，中断结束后让进程被唤醒
    printf("识别硬盘\n");
    plicinit();
    plicinithart();
    //w_stvec((uint64)kernelvec & ~0x3); 
    virtio_disk_init();
    // w_stvec((uint64)kernelvec); // 设置好内核中断处理函数的地址，中断而不是异常！

    // buf.blockno = 0;
    // buf.dev = 1;
    // memset((void *)buf.data, 7, 1024);
    // virtio_rw(&buf, 1); // 每一字节都写7
    // virtio_rw(&buf, 0);
    // memset((void *)buf.data, 9, 1024);
    // virtio_rw(&buf, 1); // 每一字节都写9
    // virtio_rw(&buf, 0);
    printf("----------------------------\n读写磁盘测试完成!\n"); ///< 测试完之后要进入用户程序一定要设置sepc和stval！使用例子如下
// 只用于riscv
//  virtio_writeAndRead_test();
//  hsai_set_usertrap();//!!!!!!!!!!!注意，这里还要重新设置sepc，因为之前磁盘触发的内核中断保存sepc并且返回了virtio_disk_rw.
//  hsai_set_csr_sepc((uint64)(void *)init_main);
//  userret((uint64)p->trapframe);
#else // loongarch没有！
        //< 现在loongarch有了!
    
    // buf.blockno = 6;
    // buf.dev = 0;
    // //< 一次读写测试
    // memset((void *)buf.data, 8, 1024);
    // la_virtio_disk_rw(&buf, 1); // 每一字节都写7
    // la_virtio_disk_rw(&buf, 0);
    // memset((void *)buf.data, 9, 1024);
    // la_virtio_disk_rw(&buf, 1); // 每一字节都写9
    // la_virtio_disk_rw(&buf, 0);

    // for(int i=0;i<4;i++) //< 多次读写测试
    // {
    //     memset((void *)(buf.data), i, 1024);
    //     buf.blockno=i;
    //     la_virtio_disk_rw(&buf, 1);
    // }

    // for(int i=0;i<4;i++) //< 多次读写测试
    // {
    //     buf.blockno=i;
    //     la_virtio_disk_rw(&buf, 0);
    // }

#endif
}

#if defined RISCV
#include "rv_init_code.h" //< unsigned char initcode[]
#else
#include "la_init_code.h" //< unsigned char initcode[]

#endif
/**
 * @brief 初始化init线程，进入scheduler后调度执行
 */
extern proc_t *initproc;
void init_process()
{
    struct proc *p = allocproc();
    initproc = p;
    p->state = RUNNABLE;
    uint32 len = sizeof(init_code);
    printf("user len:%p\n", len);
    LOG("user init_code: %p\n", init_code);
    uvminit(p, init_code, len);
    p->virt_addr = 0;
    p->sz = len ;
    p->sz = PGROUNDUP(p->sz);
    p->cwd.fs = get_fs_by_type(EXT4);
    uint64 sp =  get_proc_sp(p);
    strcpy(p->cwd.path, "/");
    hsai_set_trapframe_epc(p->trapframe, 0);
    hsai_set_trapframe_user_sp(p->trapframe, sp);
    release(&p->lock); ///< 释放在allocproc()中加的锁
}
