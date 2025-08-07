// 处理两个架构的异常。为kernel提供架构无关的接口
//
//
#include "types.h"
#include "trap.h"
#include "print.h"
#include "virt.h"
#include "procfs.h"
#include "plic.h"
#include "process.h"
#include "cpu.h"
#include "timer.h"
#include "vmem.h"
#include "vma.h"
#include "vfs_ext4.h"
#include "ext4_oflags.h"
#if defined RISCV
#include <riscv.h>
#include "riscv_memlayout.h"
// UART寄存器定义
#define LSR 5
#define LSR_RX_READY (1 << 0)
#define RHR 0
// SBI调用定义
#define SBI_CONSOLE_GETCHAR 2
// SBI函数实现
static int inline sbi_call(uint64 which, uint64 arg0, uint64 arg1, uint64 arg2)
{
    register uint64 a0 asm("a0") = arg0;
    register uint64 a1 asm("a1") = arg1;
    register uint64 a2 asm("a2") = arg2;
    register uint64 a7 asm("a7") = which;
    asm volatile("ecall"
                 : "=r"(a0)
                 : "r"(a0), "r"(a1), "r"(a2), "r"(a7)
                 : "memory");
    return a0;
}
#else
#include "loongarch.h"
#endif
#include "test.h"
#include "string.h"
#include "futex.h"

/* 两个架构的trampoline函数名称一致 */
extern char uservec[];    ///< trampoline 用户态异常，陷入。hsai_set_usertrap使用
extern char userret[];    ///< trampoline 进入用户态。hsai_usertrapret使用
extern void kernelvec();  ///< 外部中断/异常入口
extern char trampoline[]; ///< trampoline 代码段的起始地址
extern void handle_tlbr();
extern void handle_merr();

int devintr(void); ///< 中断判断函数

/* usertrap()需要这两个 */
#define SSTATUS_SPP (1L << 8)                     ///< Previous mode, 1=Supervisor, 0=User
extern void syscall(struct trapframe *trapframe); ///< 系统调用中断处理函数

/* hsai_set_trapframe_kernel_sp需要这个 */
extern struct proc *myproc();

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

    /*busybox需要开启浮点扩展*/
    w_csr_euen(FPE_ENABLE);
#endif
}

void machine_trap(void)
{
    panic("machine error");
}

int pagefault_handler(uint64 addr)
{
    struct proc *p = myproc();
    struct vma *find_vma = NULL;
    int flag = 0;
    uint64 perm;
    int npages = 1;

    // +++ 关键修复：确保地址页面对齐 +++
    uint64 aligned_addr = PGROUNDDOWN(addr);

    // +++ 检查访问权限 +++
    if (addr <= p->sz)
    {
        flag = 1;
        perm = PTE_R | PTE_W | PTE_X | PTE_D | PTE_U;
        // npages = (addr + (16)*PGSIZE >PGROUNDUP(p->sz) )? (PGROUNDUP(p->sz) - PGROUNDDOWN(addr)) / PGSIZE : 16;
    }
    else
    {
        find_vma =  p->vma->next;
        while (find_vma != p->vma)
        {
            if (addr >= find_vma->end)
                find_vma = find_vma->next;
            else if (addr >= find_vma->addr && addr <= find_vma->end)
            {
                flag = 1;
                perm = find_vma->perm | PTE_U;
                // npages = (addr + 16 * PGSIZE > PGROUNDUP(find_vma->end) )?  (PGROUNDUP(find_vma->end) -  PGROUNDDOWN(addr)) / PGSIZE : (16);
                break;
            }
            else
            {
                DEBUG_LOG_LEVEL(LOG_ERROR,"don't find addr:%p in vma\n", addr);
                kill(myproc()->pid,SIGSEGV);
                return -1;
            }
        }
    }

    // 找到缺页对应的vma
    if (!flag)
    {
        // 地址不在任何VMA范围内，发送SIGSEGV信号
        DEBUG_LOG_LEVEL(LOG_ERROR, "Page fault: address %p not in any VMA\n", addr);
        kill(p->pid, SIGSEGV);
        return -1;
    }
    // 检查VMA权限是否允许访问
    if (find_vma && find_vma->orig_prot == PROT_NONE)
    {
        // PROT_NONE禁止所有访问，但我们可以尝试权限转换
        DEBUG_LOG_LEVEL(LOG_DEBUG, "PROT_NONE access detected at %p, attempting permission conversion\n", addr);
        kill(p->pid, SIGSEGV);
        return -1;
    }

    // // +++ 检查是否是PROT_NONE的权限转换 +++
    // if (find_vma && find_vma->orig_prot == PROT_NONE) {
    //     // 对于PROT_NONE，我们需要根据实际访问类型来决定权限
    //     // 这里我们假设用户想要读取，所以给予读权限
    //     int real_prot = PROT_READ;

    //     // 为文件映射按需加载内容
    //     if (find_vma->fd != -1) {
    //         // 文件映射：加载文件内容
    //         struct file *f = p->ofile[find_vma->fd];
    //         if (f) {
    //             // 分配物理页面
    //             void *pa = pmem_alloc_pages(1);
    //             if (!pa) {
    //                 return -1;
    //             }
    //             memset(pa, 0, PGSIZE);

    //             // 计算文件偏移
    //             uint64 file_offset = find_vma->f_off + (aligned_addr - find_vma->addr);

    //             // 设置文件位置并读取内容
    //             vfs_ext4_lseek(f, file_offset, SEEK_SET);
    //             get_file_ops()->read(f, aligned_addr, PGSIZE);

    //             // 建立映射
    //             int real_perm = get_mmapperms(real_prot);
    //             if (mappages(p->pagetable, aligned_addr, (uint64)pa, PGSIZE, real_perm) < 0) {
    //                 pmem_free_pages(pa, 1);
    //                 return -1;
    //             }

    //             DEBUG_LOG_LEVEL(LOG_DEBUG, "PROT_NONE file page loaded: va=%p, offset=%p\n",
    //                            aligned_addr, file_offset);
    //             return 0;
    //         }
    //     } else {
    //         // 匿名映射：分配零填充页
    //         void *pa = pmem_alloc_pages(1);
    //         if (!pa) {
    //             return -1;
    //         }
    //         memset(pa, 0, PGSIZE);

    //         // 建立映射
    //         int real_perm = get_mmapperms(real_prot);
    //         if (mappages(p->pagetable, aligned_addr, (uint64)pa, PGSIZE, real_perm) < 0) {
    //             pmem_free_pages(pa, 1);
    //             return -1;
    //         }

    //         DEBUG_LOG_LEVEL(LOG_DEBUG, "PROT_NONE anonymous page allocated: va=%p\n", aligned_addr);
    //         return 0;
    //     }
    // }

    // +++ 检查是否是MAP_PRIVATE的写时复制 +++
    if (find_vma && (find_vma->flags & MAP_PRIVATE))
    {
        pte_t *pte = walk(p->pagetable, aligned_addr, 0);
        if (pte && (*pte & PTE_V) && !(*pte & PTE_W))
        {
            // 页面已存在但无写权限，这是写时复制的情况
            if (handle_cow_write(p, aligned_addr) == 0)
            {
                return 0; // 写时复制成功
            }
        }
    }

    // +++ 共享内存缺页处理 +++
    if (find_vma && find_vma->type == SHARE && find_vma->shm_kernel)
    {
        if((addr - find_vma->addr) > find_vma->fsize){
            DEBUG_LOG_LEVEL(LOG_ERROR,"access file out of range\n");
            printf("kill proc SIGBUS\n");
            kill(myproc()->pid , SIGBUS);
            return -1;
        }
        struct shmid_kernel *shp = find_vma->shm_kernel;
        uint64 offset = aligned_addr - find_vma->addr;
        int idx = offset / PGSIZE;

        // 检查索引是否有效
        if (idx >= (shp->size + PGSIZE - 1) / PGSIZE)
        {
            // panic("shm page index out of range: %d\n", idx);
            return -1;
        }

        // 如果共享内存页还没有分配物理页
        if (shp->shm_pages[idx] == 0)
        {
            void *pa = pmem_alloc_pages(1);
            if (!pa)
            {
                panic("shm pmem_alloc_pages failed\n");
                return -1;
            }

            // 清零物理页
            memset(pa, 0, PGSIZE);

            // 保存物理页地址到共享内存段
            shp->shm_pages[idx] = (pte_t)pa;

            DEBUG_LOG_LEVEL(LOG_INFO, "shm alloc page: addr=%p, idx=%d, pa=%p\n",
                            aligned_addr, idx, pa);
        }

        pte_t *pte = walk(p->pagetable, aligned_addr, 0);
        if (pte && (*pte & PTE_V))
        {
            DEBUG_LOG_LEVEL(LOG_ERROR, "address:aligned_addr:%p is already mapped!,not enough permission\n", aligned_addr);
            kill(myproc()->pid,SIGSEGV);
            // *pte |= PTE_R | PTE_W;
        }else{
            if (mappages(p->pagetable, aligned_addr, (uint64)shp->shm_pages[idx], PGSIZE, perm) < 0)
            {
                panic("shm mappages failed\n");
                return -1;
            }
        }
        // 建立当前进程页表的映射


        return 0;
    }

    char *pa;
    pa = pmem_alloc_pages(npages);

    // +++ 关键修复：验证分配的内存页面对齐 +++
    if (pa == NULL)
    {
        panic("pmem_alloc_pages failed for %d pages\n", npages);
        return -1;
    }

    if ((uint64)pa % PGSIZE != 0)
    {
        printf("WARNING: pmem_alloc_pages returned unaligned address %p\n", pa);
        pmem_free_pages(pa, npages);
        return -1;
    }

    // 确保分配的内存完全清零
    memset(pa, 0, npages * PGSIZE);
    perm = PTE_R | PTE_W | PTE_X | PTE_D | PTE_U | PTE_V;
    pte_t *pte = walk(p->pagetable, aligned_addr, 0);
    if (pte && (*pte & PTE_V))
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "address:aligned_addr:%p is already mapped!\n", aligned_addr);
        *pte |= perm;
    }
    else
    {
        if (mappages(p->pagetable, aligned_addr, (uint64)pa, npages * PGSIZE, perm) < 0)
        {
            panic("mappages failed\n");
            pmem_free_pages(pa, npages);
            return -1;
        }
    }

    return 0;
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
 * @brief 交换old和new线程上下文
 *
 * @param old 旧线程
 * @param new 新线程
 */
void hsai_swtch(struct context *old, struct context *new)
{
#if defined RISCV
    swtch(old, new);
#else
    swtch(old, new);
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

/**
 * @brief 设置trapframe的hartid为当前cpu的hartid
 *
 * @param trapframe
 */
void hsai_set_trapframe_hartid(struct trapframe *trapframe) // 修改页表
{
#if defined RISCV
    trapframe->kernel_hartid = r_tp();
#else
    trapframe->kernel_hartid = r_tp();
#endif
}

/**
 * @brief 开启时钟中断使能
 */
void hsai_clock_intr_on()
{
#if RISCV
    w_sie(r_sie() | SIE_STIE);
#else
    w_csr_ecfg(r_csr_ecfg() | TI_VEC);
#endif
}

/**
 * @brief 关闭时钟中断使能
 */
void hsai_clock_intr_off()
{
#if RISCV
    w_sie(r_sie() & ~SIE_STIE);
#else
    w_csr_ecfg(r_csr_ecfg() & ~TI_VEC);
#endif
}

// extern void userret(uint64 trapframe_addr, uint64 pgdl);
// 如果是第一次进入用户程序，调用usertrapret之前，还要初始化trapframe->sp
void hsai_usertrapret()
{
    proc_t *p = myproc();
    struct trapframe *trapframe = p->trapframe;
    intr_off();
    hsai_set_usertrap();

    /* 使用当前线程的内核栈而不是进程的主栈 */
    if (p->main_thread->kstack != p->kstack)
        hsai_set_trapframe_kernel_sp(trapframe, p->main_thread->kstack + PGSIZE);
    else
        hsai_set_trapframe_kernel_sp(trapframe, p->kstack + KSTACKSIZE);

    hsai_set_trapframe_pagetable(trapframe);
    hsai_set_trapframe_kernel_trap(trapframe);
    hsai_set_trapframe_hartid(trapframe);
    hsai_set_csr_to_usermode();
#if defined RISCV ///< 后续系统调用，只需要下面的代码
    // intr_off();
    hsai_set_csr_sepc(trapframe->epc);

    uint64 satp = MAKE_SATP(myproc()->pagetable);
    uint64 fn = TRAMPOLINE + (userret - trampoline);
#if DEBUG
    // printf("epc: 0x%p  ", trapframe->epc);
    // printf("即将跳转: %p\n", fn);
#endif

    ((void (*)(uint64, uint64))fn)(TRAPFRAME, satp);

#else ///< loongarch
    // intr_off();
    //  设置ertn的返回地址
    hsai_set_csr_sepc(trapframe->era);
    uint64 fn = TRAMPOLINE + (userret - trampoline);
#if DEBUG
    // printf("epc: 0x%p  ", trapframe->era);
    // printf("即将跳转: %p\n", fn);
#endif
    volatile uint64 pgdl = (uint64)(myproc()->pagetable);
    ((void (*)(uint64, uint64))fn)(TRAPFRAME, pgdl); // 可以传参
#endif
}

extern void list_file(const char *path);
void forkret(void)
{
    static int first = 1;
    release(&myproc()->lock);
    if (first)
    {
        // File system initialization must be run in the context of a
        // regular process (e.g., because it calls sleep), and thus cannot
        // be run from main().
        first = 0;
        fs_mount(ROOTDEV, EXT4, "/", 0, NULL); // 挂载文件系统
        dir_init();
        futex_init();

        /* init线程cwd设置 */
        struct file_vnode *cwd = &(myproc()->cwd);
        strcpy(cwd->path, "/");
        cwd->fs = get_fs_by_type(EXT4);

        /* 列目录 */
        // #if DEBUG
        //         list_file("/");
        //         list_file("/musl");
        // #endif
        /*
         * NOTE: DEBUG用
         * forkret好像是内核态的，我在forkret中测试，所以
         * 用了isnotforkret，后面可能要删掉
         */
        __sync_synchronize();
        extern bool isnotforkret;
        isnotforkret = true;
    }
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

    w_stvec((uint64)kernelvec);

    trapframe->epc = r_sepc();
    if ((r_sstatus() & SSTATUS_SPP) != 0)
    {
        panic("usertrap: not from user mode");
        while (1)
            ;
    }

    uint64 cause = r_scause();
    if ((which_dev = devintr()) != 0)
    {
        // printf("usertrap: which_dev=%d, p=%p\n", which_dev, p);
        if (which_dev == 2 && p != 0)
        { /* 时钟中断 */
            p->utime++;
            yield();
            // hsai_usertrapret();
        }
        // TODO 其他中断
    }
    else
    { /* 异常 */
        if (cause == UserEnvCall)
        {
            if (p->killed)
            {
                exit(0);
            }
            // printf(BLUE_COLOR_PRINT"epc: %x",trapframe->epc);
            trapframe->epc += 4;
            intr_on();
            syscall(trapframe);
            check_and_handle_signals(p, trapframe);
            hsai_usertrapret();
        }
        switch (cause)
        {
        case StoreMisaligned:
        case LoadMisaligned:
        case InstructionMisaligned:
        case InstructionPageFault:
            printf("%d in application, bad addr = %p, bad instruction = %p, core "
                   "dumped.\n",
                   cause, r_stval(), trapframe->epc);
            printf("a0=%p\na1=%p\na2=%p\na3=%p\na4=%p\na5=%p\na6=%p\na7=%p\nsp=%p\n", trapframe->a0, trapframe->a1, trapframe->a2, trapframe->a3, trapframe->a4, trapframe->a5, trapframe->a6, trapframe->a7, trapframe->sp);
            printf("p->pid=%d, p->sz=%d\n", p->pid, p->sz);

            // For instruction page fault, check the page table entry at the faulting instruction address
            uint64 fault_addr = (cause == InstructionPageFault) ? trapframe->epc : r_stval();
            pte_t *pte = walk(p->pagetable, fault_addr, 0);
            if (pte != NULL && (*pte & PTE_V))
            {
                printf("PTE for addr 0x%p: valid=%d, read=%d, write=%d, exec=%d, user=%d, full_pte=0x%p\n",
                       fault_addr,
                       !!(*pte & PTE_V),
                       !!(*pte & PTE_R),
                       !!(*pte & PTE_W),
                       !!(*pte & PTE_X),
                       !!(*pte & PTE_U),
                       *pte);
            }
            else
            {
                printf("PTE for addr 0x%p: not found or invalid (pte=%p)\n", fault_addr, pte);
            }

            // Also check the stval address if different from epc
            if (cause == InstructionPageFault && r_stval() != trapframe->epc)
            {
                pte_t *pte_stval = walk(p->pagetable, r_stval(), 0);
                if (pte_stval != NULL && (*pte_stval & PTE_V))
                {
                    printf("PTE for stval 0x%p: valid=%d, read=%d, write=%d, exec=%d, user=%d, full_pte=0x%p\n",
                           r_stval(),
                           !!(*pte_stval & PTE_V),
                           !!(*pte_stval & PTE_R),
                           !!(*pte_stval & PTE_W),
                           !!(*pte_stval & PTE_X),
                           !!(*pte_stval & PTE_U),
                           *pte_stval);
                }
                else
                {
                    printf("PTE for stval 0x%p: not found or invalid (pte=%p)\n", r_stval(), pte_stval);
                }
            }

            p->killed = 1;
            break;
        case LoadPageFault:
        case StorePageFault:
            pagefault_handler(r_stval());
            // 移除这里的 hsai_usertrapret();
            break;
        default:
            printf("unknown trap: %p, stval = %p sepc = %p\n", r_scause(),
                   r_stval(), r_sepc());
            p->killed = 1;
            break;
        }
    }

    // 在返回用户态前检查信号
    if (check_and_handle_signals(p, trapframe))
    {
        // 如果需要处理信号，直接返回用户态
        hsai_usertrapret();
        return;
    }

    // 检查信号处理函数是否正常返回（没有调用sigreturn）
    // 只有当信号处理函数没有调用sigreturn就返回时，才设置killed标志
    // 注意：current_signal在check_and_handle_signals中设置，在sigreturn中清除
    // 如果当前trapframe的返回地址是SIGTRAMPOLINE，说明信号处理函数正在执行
    // 此时不应该检查current_signal，因为信号处理函数可能还没有调用sigreturn

    // // 如果current_signal不为0，说明信号处理函数没有调用sigreturn就返回了
    // if (p->current_signal != 0) {
    //     // 信号处理函数没有调用sigreturn，设置killed标志
    //     if (p->killed == 0 || p->killed > p->current_signal) {
    //         p->killed = p->current_signal;
    //     }
    //     p->current_signal = 0;  // 清除当前信号标志
    // }

    // 在信号处理后再检查killed标志
    if (p->killed)
        exit(0);

    hsai_usertrapret();
#else
    /*
     * 我真的服了，xv6-loongarch的trampoline不写入era，要在usertrap保存。
     * riscv都是在trampoline保存的。就这样，在usertrap保存era,不在trampoline保存了
     */
    w_csr_eentry((uint64)kernelvec);

    // #if DEBUG
    //     printf("usertrap():handling exception\n");
    //     uint32 info = r_csr_crmd();
    //     printf("usertrap(): crmd=0x%x\n", info);
    //     info = r_csr_prmd();
    //     printf("usertrap(): prmd=0x%x\n", info);
    //     info = r_csr_estat();
    //     printf("usertrap(): estat=0x%x\n", info);
    //     info = r_csr_era();
    //     printf("usertrap(): era=0x%x\n", info);
    //     info = r_csr_ecfg();
    //     printf("usertrap(): ecfg=0x%x\n", info);
    //     info = r_csr_badi();
    //     printf("usertrap(): badi=0x%x\n", info);
    //     info = r_csr_badv();
    //     printf("usertrap(): badv=0x%x\n\n", info);
    // #endif
    trapframe->era = r_csr_era(); ///< 记录trap发生地址
    if ((r_csr_prmd() & PRMD_PPLV) == 0)
    {
        printf("#### OS COMP TEST GROUP END libcbench-musl ####\n");
        panic("usertrap: not from user mode");
    }
    /*如果是用户程序的断点，简单的跳过断点指令*/
    if (((r_csr_estat() & CSR_ESTAT_ECODE) >> 16) == 0xc)
    {
#if DEBUG_BREAK //< 想看断点就改这个宏吧
        LOG_LEVEL(LOG_DEBUG, "用户程序断点\n");
#endif
        exit(0);
        trapframe->era += 4;
        goto end;
    }
    if (((r_csr_estat() & CSR_ESTAT_ECODE) >> 16) == 0xb)
    {
        if (p->killed)
        {
            exit(0);
        }
        /* 系统调用 */
        trapframe->era += 4;
        intr_on();
        syscall(trapframe);
    }
    else if (((r_csr_estat() & CSR_ESTAT_ECODE) >> 16 == 0x1 ||
              (r_csr_estat() & CSR_ESTAT_ECODE) >> 16 == 0x2))
    {
        /*
         * load page fault or store page fault
         * check if the page fault is caused by stack growth
         */
        pagefault_handler(r_csr_badv());
        // printf("usertrap():handling exception\n");
        // uint64 info = r_csr_crmd();
        // printf("usertrap(): crmd=0x%p\n", info);
        // info = r_csr_prmd();
        // printf("usertrap(): prmd=0x%p\n", info);
        // info = r_csr_estat();
        // printf("usertrap(): estat=0x%p\n", info);
        // info = r_csr_era();
        // printf("usertrap(): era=0x%p\n", info);
        // info = r_csr_ecfg();
        // printf("usertrap(): ecfg=0x%p\n", info);
        // info = r_csr_badi();
        // printf("usertrap(): badi=0x%p\n", info);
        // info = r_csr_badv();
        // printf("usertrap(): badv=0x%p\n\n", info);
        // printf("a0=%p\na1=%p\na2=%p\na3=%p\na4=%p\na5=%p\na6=%p\na7=%p\nsp=%p\n", trapframe->a0, trapframe->a1, trapframe->a2, trapframe->a3, trapframe->a4, trapframe->a5, trapframe->a6, trapframe->a7, trapframe->sp);
        // printf("p->pid=%d, p->sz=0x%p\n", p->pid, p->sz);
        // pte_t *pte = walk(p->pagetable, r_csr_badv(), 0);
        // printf("pte=%p (valid=%d, *pte=0x%p)\n", pte, *pte & PTE_V, *pte);
        // uint64 estat = r_csr_estat();
        // uint64 ecode = (estat & 0x3F0000) >> 16;
        // uint64 esubcode = (estat & 0x7FC00000) >> 22;
        // handle_exception(ecode, esubcode);
        // LOG_LEVEL(3, "\n       era=%p\n       badi=%p\n       badv=%p\n       crmd=%x\n", r_csr_era(), r_csr_badi(), r_csr_badv(), r_csr_crmd());
        // panic("usertrap\n");
    }
    else if ((which_dev = devintr()) != 0)
    {
#if DEBUG
        printf("中断类型: %d\n", which_dev);
#endif
    }
    else
    {
        printf("usertrap():handling exception\n");
        uint64 info = r_csr_crmd();
        printf("usertrap(): crmd=0x%p\n", info);
        info = r_csr_prmd();
        printf("usertrap(): prmd=0x%p\n", info);
        info = r_csr_estat();
        printf("usertrap(): estat=0x%p\n", info);
        info = r_csr_era();
        printf("usertrap(): era=0x%p\n", info);
        info = r_csr_ecfg();
        printf("usertrap(): ecfg=0x%p\n", info);
        info = r_csr_badi();
        printf("usertrap(): badi=0x%p\n", info);
        info = r_csr_badv();
        printf("usertrap(): badv=0x%p\n\n", info);

        // 添加更详细的调试信息
        printf("trapframe->era=0x%p\n", trapframe->era);
        printf("trapframe values:\n");
        printf("a0=%p\na1=%p\na2=%p\na3=%p\na4=%p\na5=%p\na6=%p\na7=%p\nsp=%p\n", trapframe->a0, trapframe->a1, trapframe->a2, trapframe->a3, trapframe->a4, trapframe->a5, trapframe->a6, trapframe->a7, trapframe->sp);
        printf("p->pid=%d, p->sz=0x%p\n", p->pid, p->sz);

        // 检查era是否为0，这是一个关键的异常情况
        if (r_csr_era() == 0 || trapframe->era == 0)
        {
            printf("CRITICAL: era is 0! This indicates a jump to NULL pointer.\n");
            printf("Process information:\n");
            printf("  pid=%d, tid=%d\n", p->pid, p->main_thread ? p->main_thread->tid : -1);
            printf("  kstack=0x%p, pagetable=0x%p\n", p->kstack, p->pagetable);
            // 打印VMA信息以帮助调试
            if (p->vma)
            {
                struct vma *vma = p->vma->next;
                int vma_count = 0;
                printf("  VMA list:\n");
                while (vma != p->vma && vma_count < 10)
                { // 限制打印数量防止无限循环
                    printf("    VMA[%d]: addr=0x%p-0x%p, type=%d, perm=0x%x\n",
                           vma_count, vma->addr, vma->end, vma->type, vma->perm);
                    vma = vma->next;
                    vma_count++;
                }
            }
        }

        pte_t *pte = walk(p->pagetable, r_csr_badv(), 0);
        printf("pte=%p (valid=%d, *pte=0x%p)\n", pte, *pte & PTE_V, *pte);
        printf("p->pid=%d, p->sz=0x%p\n", p->pid, p->sz);
        uint64 estat = r_csr_estat();
        uint64 ecode = (estat & 0x3F0000) >> 16;
        uint64 esubcode = (estat & 0x7FC00000) >> 22;
        handle_exception(ecode, esubcode);
        LOG_LEVEL(3, "\n       era=%p\n       badi=%p\n       badv=%p\n       crmd=%x\n", r_csr_era(), r_csr_badi(), r_csr_badv(), r_csr_crmd());
        panic("usertrap\n");
    }
    if (which_dev == 2)
    {
        yield();
        p->utime++;
    }

    // 在返回用户态前检查信号
    check_and_handle_signals(p, trapframe);

end:
    hsai_usertrapret();
#endif
}

#define VIRTIO0_IRQ 1
#define UART0_IRQ 10
/**
 * @brief 判断中断类型
 *
 * @return int 1是外部中断(读磁盘)，2是时钟中断，0是错误
 */
int devintr(void)
{
#if defined RISCV
    uint64 scause = r_scause();
#if DEBUG
// printf("devintr: scause=0x%lx\n", scause);
#endif
    if ((scause & 0x8000000000000000L) &&
        (scause & 0xff) == 9)
    {
        // this is a supervisor external interrupt, via PLIC.

        // irq indicates which device interrupted.
        int irq = plic_claim();

        if (irq == VIRTIO0_IRQ)
        {
            increment_interrupt_count(irq);
            virtio_disk_intr();
        }
        else if (irq == UART0_IRQ)
        {
// UART中断处理
// 从UART读取字符并调用consoleintr
#if defined SBI
            // 使用SBI方式读取字符
            int c = sbi_call(SBI_CONSOLE_GETCHAR, 0, 0, 0);
            if (c >= 0)
            {
                consoleintr(c);
            }
#else
            // 直接从UART寄存器读取字符
            if ((*(volatile unsigned char *)(UART0 + LSR) & LSR_RX_READY) != 0)
            {
                int c = *(volatile unsigned char *)(UART0 + RHR);
                consoleintr(c);
            }
#endif
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
        // printf("devintr: 时钟中断触发, scause=0x%lx\n", scause);
        increment_interrupt_count(5);  // 时钟中断号为5
        timer_tick();
        return 2;
    }
    else
    {
        /* 不知道的中断类型 */
        if (!(scause & 0x8UL))
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
        // 对于LoongArch，硬中断号从0开始
        increment_interrupt_count(0);  // 假设virtio中断为0
        return 1;
    }
    else if (estat & ecfg & TI_VEC) ///< 定时器中断
    {
        increment_interrupt_count(11);  // LoongArch时钟中断号为11
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
#if DEBUG
// printf("kerneltrap! \n");
#endif
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
        struct proc *p = myproc();
        struct trapframe *trapframe = p->trapframe;
        printf("trapframe a0=%p\na1=%p\na2=%p\na3=%p\na4=%p\na5=%p\na6=%p\na7=%p\nsp=%p\nepc=%p\n",
               trapframe->a0, trapframe->a1, trapframe->a2, trapframe->a3, trapframe->a4,
               trapframe->a5, trapframe->a6, trapframe->a7, trapframe->sp, trapframe->epc);
        printf("thread tid=%d pid=%d, p->sz=0x%p\n", p->main_thread->tid, p->pid, p->sz);
        printf("context ra=%p sp=%p\n", p->context.ra, p->context.sp);
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
#else           ///< Loongarch
    int which_dev = 0;
    // ERA寄存器：异常程序计数器，记录异常发生时的指令地址。
    uint64 era = r_csr_era();
    // PRMD寄存器：记录异常发生时的特权级别、中断使能、写使能。
    uint64 prmd = r_csr_prmd();

    assert((prmd & PRMD_PPLV) == 0,
           "kerneltrap: not from privilege0");
    assert(intr_get() == 0,
           "kerneltrap: interrupts enabled");
    /*如果是内核的断点，简单的跳过断点指令*/
    if (((r_csr_estat() & CSR_ESTAT_ECODE) >> 16) == 0xc)
    {
#if DEBUG_BREAK //< 想看内核断点就改这个宏吧
        LOG_LEVEL(LOG_DEBUG, "内核断点\n");
#endif
        era += 4;
        goto end;
    }

    if ((which_dev = devintr()) == 0)
    {
        printf("usertrap():handling exception\n");
        uint64 info = r_csr_crmd();
        printf("usertrap(): crmd=0x%p\n", info);
        info = r_csr_prmd();
        printf("usertrap(): prmd=0x%p\n", info);
        info = r_csr_estat();
        printf("usertrap(): estat=0x%p\n", info);
        info = r_csr_era();
        printf("usertrap(): era=0x%p\n", info);
        info = r_csr_ecfg();
        printf("usertrap(): ecfg=0x%p\n", info);
        info = r_csr_badi();
        printf("usertrap(): badi=0x%p\n", info);
        info = r_csr_badv();
        printf("usertrap(): badv=0x%p\n\n", info);
        uint64 estat = r_csr_estat();
        uint64 ecode = (estat & 0x3F0000) >> 16;
        uint64 esubcode = (estat & 0x7FC00000) >> 22;
        handle_exception(ecode, esubcode);
        LOG_LEVEL(3, "\n       era=%p\n       badi=%p\n       badv=%p\n       crmd=%x\n", r_csr_era(), r_csr_badi(), r_csr_badv(), r_csr_crmd());
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
    /*如果是内核的断点，简单的跳过断点指令*/
end:
    w_csr_era(era);
    w_csr_prmd(prmd);
#endif
}