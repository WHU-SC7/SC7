// 处理两个架构的异常。为kernel提供架构无关的接口
//
//
#include "types.h"
#include "trap.h"
#include "print.h"
#include "virt.h"
#include "plic.h"
#include "process.h"
#include "cpu.h"
#include "timer.h"
#if defined RISCV
#include <riscv.h>
#include "riscv_memlayout.h"
#else
#include "loongarch.h"
#endif


/* 两个架构的trampoline函数名称一致 */
extern char uservec[];    ///< trampoline 用户态异常，陷入。hsai_set_usertrap使用
extern char userret[];    ///< trampoline 进入用户态。hsai_usertrapret使用
extern void kernelvec();  ///< 外部中断/异常入口
extern char trampoline[]; ///< trampoline 代码段的起始地址
extern void handle_tlbr();
extern void handle_merr();

int devintr(void); ///< 中断判断函数

/* usertrap()需要这两个 */
#define SSTATUS_SPP (1L << 8) ///< Previous mode, 1=Supervisor, 0=User
extern void syscall();        ///< 系统调用中断处理函数

/* hsai_set_trapframe_kernel_sp需要这个 */
extern struct proc *curr_proc();

/* 把idle和p交换.再swtch.S中，目前没有使用 */
extern void swtch(struct context *idle, struct context *p);

/**
 * @brief 对loongarch设置ecfg,应该只在初始化时设置一次。RISCV设置中断入口。开启外部中断和时钟中断
 *
 * Riscv对应的是设置sie，但是已经在start.c中设置了，同时设置中断入口。而loongarch没有M态的初始化。
 * 这里只对loongarch执行操作，riscv什么都不做
 */
void hsai_trap_init(void)
{
#if defined RISCV
    // w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
    w_stvec((uint64)kernelvec); ///< 设置内核trap入口
#else
    uint32 ecfg = (0U << CSR_ECFG_VS_SHIFT) | HWI_VEC | TI_VEC; ///< 例外配置
    w_csr_ecfg(ecfg);                                           ///< 设置例外配置
    w_csr_eentry((uint64)kernelvec);                            ///< 设置内核trap入口
    w_csr_tlbrentry((uint64)handle_tlbr);                       ///< TLB重填exception
    w_csr_merrentry((uint64)handle_merr);                       ///< 机器exception
    timer_init();                                               ///< 启动时钟中断
#endif
}

void machine_trap()
{
    panic("machine error");
}

/**
 * @brief 设置异常处理函数到uservec,对于U态的异常
 */
void hsai_set_usertrap(void)
{
#if defined RISCV // trap_init
    w_stvec(TRAMPOLINE + (uservec - trampoline));
#else
    w_csr_eentry((uint64)uservec & ~0x3); //
#endif
}

/**
 * @brief 设置好sstatus或prmd,准备进入U态
 */
void hsai_set_csr_to_usermode(void) // 设置好csr寄存器，准备进入U态
{
#if defined RISCV
    // set S Previous Privilege mode to User.
    uint64 x = r_sstatus();
    x &= ~SSTATUS_SPP; ///< clear SPP to 0 for user mode
    x |= SSTATUS_SPIE; ///< enable interrupts in user mode
    // x |= SSTATUS_SIE; 	///< 委托给S态处理中断，要使能
    w_sstatus(x);
#else
    // 类似设置sstatus
    uint32 x = r_csr_prmd();
    x |= PRMD_PPLV; ///< set PPLV to 3 for user mode
    x |= PRMD_PIE;  ///< enable interrupts in user mode
    w_csr_prmd(x);
#endif
}

/**
 * @brief 设置sepc或era,返回用户态时跳转到用户程序
 */
void hsai_set_csr_sepc(uint64 addr) ///< 设置sepc, sret时跳转
{
#if defined RISCV
    w_sepc(addr);
#else
    /* 设置era,指令ertn使用。S态进入U态 */
    w_csr_era((uint64)(void *)addr);
#endif
}

/**
 * @brief 得到对应trapframe的参数
 *
 * @param trapframe
 * @param register_num
 * @return uint64
 */
uint64
hsai_get_arg(struct trapframe *trapframe, uint64 register_num) ///< 从trapframe获取参数a0-a7
{
    switch (register_num) ///< 从0开始编号，0返回a0
    {
    case 0:
        return trapframe->a0;
        break;
    case 1:
        return trapframe->a1;
        break;
    case 2:
        return trapframe->a2;
        break;
    case 3:
        return trapframe->a3;
        break;
    case 4:
        return trapframe->a4;
        break;
    case 5:
        return trapframe->a5;
        break;
    case 6:
        return trapframe->a6;
        break;
    case 7:
        return trapframe->a7;
        break;
    default:
        printf("无效的regisrer_num: %d", register_num);
        return -1;
        break;
    }
    return 0;
}

/**
 * @brief 交换idle和p上下文
 *
 * @param idle
 * @param p
 */
void hsai_swtch(struct context *idle, struct context *p)
{
#if defined RISCV
    swtch(idle, p);
#else
    swtch(idle, p);
#endif
}

/**
 * @brief 设置线程内核栈
 *
 * @param trapframe
 * @param value
 */
void hsai_set_trapframe_kernel_sp(struct trapframe *trapframe, uint64 value) // 修改线程内核栈
{
#if defined RISCV
    trapframe->kernel_sp = value;
#else
    trapframe->kernel_sp = value;
#endif
}

/**
 * @brief 设置内核的用户态陷入处理函数
 *
 * @param trapframe
 */
// 为给定的trapframe设置usertrap,在trampoline保存状态后usertrap处理陷入或异常
// 这个usertrap地址是固定的
void hsai_set_trapframe_kernel_trap(struct trapframe *trapframe)
{
#if defined RISCV
    trapframe->kernel_trap = (uint64)usertrap;
#else
    trapframe->kernel_trap = (uint64)usertrap;
#endif
}

/**
 * @brief 设置trapframe的返回地址
 *
 * @param trapframe
 * @param value
 */
void hsai_set_trapframe_epc(struct trapframe *trapframe, uint64 value) // 修改返回地址，loongarch的Trapframe为era,意义相同
{
#if defined RISCV
    trapframe->epc = value;
#else
    trapframe->era = value;
#endif
}

/**
 * @brief 设置trapframe的用户栈
 *
 * @param trapframe
 * @param value
 */
void hsai_set_trapframe_user_sp(struct trapframe *trapframe, uint64 value) // 修改用户态的栈
{
#if defined RISCV
    trapframe->sp = value;
#else
    trapframe->sp = value;
#endif
}

/**
 * @brief 设置trapframe的内核页表
 *
 * @param trapframe
 */
void hsai_set_trapframe_pagetable(struct trapframe *trapframe) // 修改页表
{
#if defined RISCV
    trapframe->kernel_satp = r_satp();
#else
    trapframe->kernel_pgdl = r_csr_pgdl();
#endif
}

// extern void userret(uint64 trapframe_addr, uint64 pgdl);
// 如果是第一次进入用户程序，调用usertrapret之前，还要初始化trapframe->sp
void hsai_usertrapret()
{
    struct trapframe *trapframe = curr_proc()->trapframe;
    hsai_set_usertrap();

    hsai_set_trapframe_kernel_sp(trapframe, curr_proc()->kstack + PGSIZE);
    hsai_set_trapframe_pagetable(curr_proc()->trapframe); ///< 待修改
    hsai_set_trapframe_kernel_trap(curr_proc()->trapframe);
    hsai_set_csr_to_usermode();
#if defined RISCV ///< 后续系统调用，只需要下面的代码
    intr_off();
    hsai_set_csr_sepc(trapframe->epc);
    printf("epc: %x  ", trapframe->epc);
    uint64 satp = MAKE_SATP(curr_proc()->pagetable);
    uint64 fn = TRAMPOLINE + (userret - trampoline);
    printf("即将跳转: %p\n", fn);
    ((void (*)(uint64, uint64))fn)(TRAPFRAME, satp);

#else
    // 设置ertn的返回地址
    hsai_set_csr_sepc(trapframe->era);
    printf("epc: %x  ", trapframe->era);
    uint64 fn = TRAMPOLINE + (userret - trampoline);
    printf("即将跳转: %p\n", fn);
    volatile uint64 pgdl = (uint64)(curr_proc()->pagetable);
    ((void (*)(uint64, uint64))fn)(TRAPFRAME, pgdl); // 可以传参
#endif
}

void forkret(void)
{
    release(&myproc()->lock);
    hsai_usertrapret();
}
///< 如果已经进入了U态，每次系统调用完成后返回时只需要如下就可以（不考虑虚拟内存
// 如果是第一次进入用户程序，调用usertrapret之前，还要初始化trapframe->sp
//  void minium_usertrap(struct trapframe *trapframe)
//  {
//  	#if defined RISCV
//  		hsai_set_csr_sepc(trapframe->epc);
//  		userret((uint64)trapframe);
//      #else
//  		hsai_set_csr_sepc(trapframe->era);
//  		userret((uint64)trapframe);
//      #endif
//  }

/**
 * @brief 用户态中断和异常处理函数
 *
 */
// 其实xv6-loongarch从uservec进入usertrap时，a0也是trapframe.只不过xv6-loongarch声明为usertrap(void)。我们是可以用a0当trapframe的
void usertrap(void)
{
    struct proc *p = myproc();
    struct trapframe *trapframe = p->trapframe;
    int which_dev = 0;
#if defined RISCV
    trapframe->epc = r_sepc();
    if ((r_sstatus() & SSTATUS_SPP) != 0)
    {
        printf("usertrap: not from user mode");
        while (1)
            ;
    }

    uint64 cause = r_scause();
    if ((which_dev = devintr()) != 0)
    {
        if (which_dev == 2 && p != 0)
        { /* 时钟中断 */
            yield();
            p->utime++;
            hsai_usertrapret();
        }
        // TODO 其他中断
    }
    else
    { /* 异常 */
        if (cause == UserEnvCall)
        {
            trapframe->epc += 4;
            syscall(trapframe);
            hsai_usertrapret();
        }
        switch (cause)
        {
        case StoreMisaligned:
        case StorePageFault:
        case LoadMisaligned:
        case LoadPageFault:
        case InstructionMisaligned:
        case InstructionPageFault:
            printf("%d in application, bad addr = %p, bad instruction = %p, core "
                   "dumped.",
                   cause, r_stval(), trapframe->epc);
            break;
        case IllegalInstruction:
            printf("IllegalInstruction in application, epc = %p, core dumped.",
                   trapframe->epc);
            break;
        default:
            printf("unknown trap: %p, stval = %p sepc = %p", r_scause(),
                   r_stval(), r_sepc());
            break;
        }
    }
#else
    /*
     * 我真的服了，xv6-loongarch的trampoline不写入era，要在usertrap保存。
     * riscv都是在trampoline保存的。就这样，在usertrap保存era,不在trampoline保存了
     */
    w_csr_eentry((uint64)kernelvec);

#if DEBUG
    printf("usertrap():handling exception\n");
    uint32 info = r_csr_crmd();
    printf("usertrap(): crmd=0x%x\n", info);
    info = r_csr_prmd();
    printf("usertrap(): prmd=0x%x\n", info);
    info = r_csr_estat();
    printf("usertrap(): estat=0x%x\n", info);
    info = r_csr_era();
    printf("usertrap(): era=0x%x\n", info);
    info = r_csr_ecfg();
    printf("usertrap(): ecfg=0x%x\n", info);
    info = r_csr_badi();
    printf("usertrap(): badi=0x%x\n", info);
    info = r_csr_badv();
    printf("usertrap(): badv=0x%x\n\n", info);
#endif
    trapframe->era = r_csr_era(); ///< 记录trap发生地址
    if ((r_csr_prmd() & PRMD_PPLV) == 0)
    {
        panic("usertrap: not from user mode");
    }
    if (((r_csr_estat() & CSR_ESTAT_ECODE) >> 16) == 0xb)
    {
        /* 系统调用 */
        trapframe->era += 4;
        syscall(trapframe);
    }
    else if (((r_csr_estat() & CSR_ESTAT_ECODE) >> 16 == 0x1 ||
              (r_csr_estat() & CSR_ESTAT_ECODE) >> 16 == 0x2))
    {
        /*
         * load page fault or store page fault
         * check if the page fault is caused by stack growth
         */
        panic("usertrap():handle stack page fault\n");
    }
    else if ((which_dev = devintr()) != 0)
    {
        printf("中断类型: %d\n", which_dev);
    }
    else
    {
        printf("usertrap(): unexpected trapcause %x\n", r_csr_estat());
        printf("            era=%p badi=%x\n", r_csr_era(), r_csr_badi());
        while (1)
            ;
    }
    if (which_dev == 2)
    {
        yield();
        p->utime++;
    }
    hsai_usertrapret();
#endif
}

#define VIRTIO0_IRQ 1
/**
 * @brief 判断中断类型
 *
 * @return int 1是外部中断(读磁盘)，2是时钟中断，0是错误
 */
int devintr()
{
#if defined RISCV
    uint64 scause = r_scause();
    printf("devintr: scause=0x%lx\n", scause);

    if ((scause & 0x8000000000000000L) &&
        (scause & 0xff) == 9)
    {
        // this is a supervisor external interrupt, via PLIC.

        // irq indicates which device interrupted.
        int irq = plic_claim();

        if (irq == VIRTIO0_IRQ)
        {
            virtio_disk_intr();
        }
        else if (irq)
        {
            printf("unexpected interrupt irq=%d\n", irq);
        }

        // the PLIC allows each device to raise at most one
        // interrupt at a time; tell the PLIC the device is
        // now allowed to interrupt again.
        if (irq)
            plic_complete(irq);

        return 1;
    }
    /* 时钟中断 */
    else if (scause == 0x8000000000000005L)
    {
        timer_tick();
        return 2;
    }
    else
    {
        /* 不知道的中断类型 */
        printf("unexpected interrupt scause=0x%lx\n", scause);
        return 0;
    }
#else ///< Loongarch
    uint32 estat = r_csr_estat();
    uint32 ecfg = r_csr_ecfg();

    /* CSR.ESTAT.IS & CSR.ECFG.LIE -> int_vec(13bits stand for irq type) */
    if (estat & ecfg & HWI_VEC) ///< 8个硬中断
    {
        // TODO
        printf("kerneltrap: hardware interrupt cause %x\n", estat);
        return 1;
    }
    else if (estat & ecfg & TI_VEC) ///< 定时器中断
    {
        timer_tick();

        /* 标明已经处理中断信号 */
        w_csr_ticlr(r_csr_ticlr() | CSR_TICLR_CLR);
        return 2;
    }
    else ///< 其他未处理的中断
    {
        printf("kerneltrap: unexpected trap cause %x\n", estat);
        return 0;
    }
#endif
}

/**
 * @brief 内核态中断和异常处理函数
 *
 */
void kerneltrap(void)
{
#if defined RISCV
    printf("kerneltrap! \n");
    // while(1) ;
    int which_dev = 0;
    uint64 sepc = r_sepc();
    uint64 sstatus = r_sstatus();
    uint64 scause = r_scause();

    if ((sstatus & SSTATUS_SPP) == 0)
        panic("kerneltrap: not from supervisor mode");
    if (intr_get() != 0)
        panic("kerneltrap: interrupts enabled");

    if ((which_dev = devintr()) == 0)
    {
        printf("scause %p\n", scause);
        printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
        panic("kerneltrap");
    }
    // 这里删去了时钟中断的代码，时钟中断使用yield

    /* 内核对应time++ */
    if (which_dev == 2 && myproc() != 0)
    {
        // printf("内核态时钟中断\n");
        myproc()->ktime++;
    }
    /* 正在运行的进程需要重新调度 */
    if (which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
        yield();

    // give up the CPU if this is a timer interrupt.

    // the yield() may have caused some traps to occur,
    // so restore trap registers for use by kernelvec.S's sepc instruction.
    w_sepc(sepc);
    w_sstatus(sstatus);
#else ///< Loongarch
    int which_dev = 0;
    // ERA寄存器：异常程序计数器，记录异常发生时的指令地址。
    uint64 era = r_csr_era();
    // PRMD寄存器：记录异常发生时的特权级别、中断使能、写使能。
    uint64 prmd = r_csr_prmd();

    assert((prmd & PRMD_PPLV) == 0,
           "kerneltrap: not from privilege0");
    assert(intr_get() == 0,
           "kerneltrap: interrupts enabled");

    if ((which_dev = devintr()) == 0)
    {
        printf("estat %x\n", r_csr_estat());
        printf("era=%p eentry=%p\n", r_csr_era(), r_csr_eentry());
        panic("kerneltrap");
    }

    if (which_dev == 2 && myproc() != 0)
        myproc()->ktime++;

    /* give up the CPU if this is a timer interrupt. */
    if (which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
        yield();

    /*
     * the yield() may have caused some traps to occur,
     * so restore trap registers for use by kernelvec.S's instruction.
     */
    w_csr_era(era);
    w_csr_prmd(prmd);
#endif
}