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
#include "figlet.h"
#include "thread.h"
#include "slab.h"

#if defined RISCV
#include "riscv.h"
#include "riscv_memlayout.h"
#else
#include "loongarch.h"
#endif

#include "hsai.h"

int main()
{
    while (1)
        ;
} // 为了通过编译, never use this

void init_process();

#if defined RISCV
extern void virtio_disk_init();
#else
extern void virtio_probe();
extern void la_virtio_disk_init(); 
#endif

volatile static int started = 0;
volatile static int hart0_is_starting = 0;
volatile static int first_hart = 0; //以防启动不完全
extern int sbi_hart_start(uint64_t hartid, uint64_t start_addr, uint64_t opaque);
void sbi_hart_stop(void);

int sc7_start_kernel()
{
    /*非常高级的opensbi支持boot hart不等于0的情况*/
    if((hsai_get_cpuid() != 0)&&(hart0_is_starting == 0)) //别的hart在hart0之前启动
    { 
        hart0_is_starting = 1;
        first_hart = hsai_get_cpuid(); //记录最先启动的hart的id
        sbi_hart_start(0, 0x80200000, 0); //甚至这个时候不能打印 "唤醒hart 0"
    }

    if(hsai_get_cpuid() == 0)
    {
        hart0_is_starting = 1;
        // 初始化输出串口
        chardev_init();
        printfinit();
        for (int i = 65; i < 65 + 26; i++)
        {
            put_char_sync(i);
            put_char_sync('t');
        }
        put_char_sync('\n');
        printf_figlet_color("SC7 Is Booting!"); //< 艺术字打印
        LOG("sc7_start_kernel at :%p\n", &sc7_start_kernel);
        extern uint64 boot_time;
        LOG("System boot timestamp is: %lld\n", boot_time);
        // 初始化线程和进程
        thread_init();
        proc_init();
        // 初始化物理内存
        pmem_init();
        vmem_init();
        slab_init();
        // 初始化中断和异常
        hsai_trap_init();
        // 初始化磁盘设备。本来想把这个提取到hsai的，但是发现那么做会使la运行速度变慢，先这样
    #if defined RISCV
        plicinit();
        plicinithart();
        virtio_disk_init();
    #else 
        virtio_probe();//发现virtio-blk-pci设备
        la_virtio_disk_init();
    #endif
        // 初始化文件系统
        init_fs();
        binit();
        fileinit();
        inodeinit();
        vfs_ext4_init();
        // 初始化init线程
        init_process();
        __sync_synchronize();
        // printf("main hart starting\n");
        printf("hart %d starting\n", hsai_get_cpuid());
        printf("第一个启动的是hart %d\n",first_hart);
        started = 1;

        int ret;
        // 依次启动hart 1和其他hart.最多NCPU个
        for(int i=1;i<NCPU;i++)
        {
        //不同版本的错误码不一样，比赛qemu自带OpenSBI v1.5.1，如果重复启动，错误码是3。最新的OpenSBI v1.7，错误码是7。难以通过错误码判断
            if((ret = sbi_hart_start(i, 0x80200000, 0))) // 不存在对应hart或者重复启动都会返回错误码
            {
                if(i == first_hart) //first_hart后面可能还有hart要启动
                {
                    continue;
                }
                else
                    break;
            }
        }
    }
    else //其它核心初始化自己
    {
        while(started == 0)
            ;
        __sync_synchronize();
        printf("hart %d starting\n", hsai_get_cpuid());
    }
    while(1)
    ;
        // 进入调度器
        scheduler();
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
    p->sz = len;
    p->sz = PGROUNDUP(p->sz);
    p->cwd.fs = get_fs_by_type(EXT4);
    uint64 sp =  get_proc_sp(p);
    strcpy(p->cwd.path, "/");
    hsai_set_trapframe_epc(p->trapframe, 0);
    hsai_set_trapframe_user_sp(p->trapframe, sp);
    copytrapframe(p->main_thread->trapframe, p->trapframe);
    p->main_thread->state = t_RUNNABLE; ///< 设置主线程状态为可运行
    release(&p->lock); ///< 释放在allocproc()中加的锁
}
