#include "process.h"
#include "string.h"
#include "hsai_trap.h"
#include "print.h"
#include "cpu.h"
#include "spinlock.h"
#include "pmem.h"
#include "vmem.h"
#include "vma.h"
#include "thread.h"
#include "futex.h"
#ifdef RISCV
#include "riscv.h"
#include "riscv_memlayout.h"
#else
#include "loongarch.h"
#endif

#define PAGE_SIZE (0x1000)

struct proc pool[NPROC];
char kstack[NPROC][PAGE_SIZE];
//__attribute__((aligned(4096))) char ustack[NPROC][PAGE_SIZE];
//__attribute__((aligned(4096))) char trapframe[NPROC][PAGE_SIZE];
__attribute__((aligned(4096))) char entry_stack[PAGE_SIZE];
// extern char boot_stack_top[];
proc_t *initproc; // 第一个用户态进程,永不退出
spinlock_t pid_lock;
static spinlock_t parent_lock; // 在涉及进程父子关系时使用
extern thread_t threads[];

pgtbl_t proc_pagetable(struct proc *p);

void reg_info(void)
{
#if defined RISCV
#else
    printf("register info: {\n");
    printf("prmd: %p\n", r_csr_prmd());
    printf("ecfg: %p\n", r_csr_ecfg());
    printf("era: %p\n", r_csr_era());
    printf("eentry: %p\n", r_csr_eentry());
    printf("pgdl: %p\n", r_csr_pgdl());
    printf("estatus: %p\n", r_csr_estat());
    printf("sp: %p\n", r_sp());
    printf("tp: %p\n", r_tp());
    printf("}\n");
#endif
}

// initialize the proc table at boot time.
void proc_init(void)
{
    struct proc *p;
    initlock(&pid_lock, "nextpid");
    for (p = pool; p < &pool[NPROC]; p++)
    {
        initlock(&p->lock, "proc");
        p->state = UNUSED;
        p->exit_state = 0;
        p->kstack = KSTACK((int)(p - pool));
        // p->trapframe = (struct trapframe *)trapframe[p - pool];
        p->trapframe = 0;
        p->parent = 0;
        p->ktime = 0;
        p->utime = 0;
    }
}

int allocpid(void)
{
    static int PID = 1;
    acquire(&pid_lock);
    int pid = PID++;
    release(&pid_lock);
    return pid;
}

struct proc *getproc(int pid)
{
    return &pool[pid - 1];
}

static void
copycontext(context_t *t1, context_t *t2)
{
#ifdef RISCV
    t1->ra = t2->ra;
    t1->sp = t2->sp;
    t1->s0 = t2->s0;
    t1->s1 = t2->s1;
    t1->s2 = t2->s2;
    t1->s3 = t2->s3;
    t1->s4 = t2->s4;
    t1->s5 = t2->s5;
    t1->s6 = t2->s6;
    t1->s7 = t2->s7;
    t1->s8 = t2->s8;
    t1->s9 = t2->s9;
    t1->s10 = t2->s10;
    t1->s11 = t2->s11;
#else
    t1->ra = t2->ra;
    t1->sp = t2->sp;
    t1->s0 = t2->s0;
    t1->s1 = t2->s1;
    t1->s2 = t2->s2;
    t1->s3 = t2->s3;
    t1->s4 = t2->s4;
    t1->s5 = t2->s5;
    t1->s6 = t2->s6;
    t1->s7 = t2->s7;
    t1->s8 = t2->s8;
    t1->fp = t2->fp;
#endif
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel.
// If there are no free procs, or a memory allocation fails, return 0.
struct proc *allocproc(void)
{
    struct proc *p;
    for (p = pool; p < &pool[NPROC]; p++)
    {
        acquire(&p->lock);
        if (p->state == UNUSED)
        {
            goto found;
        }
        else
            release(&p->lock);
    }
    return 0;

found:
    p->ktime = 1;
    p->utime = 1;
    p->pid = allocpid();
    p->uid = 0;
    p->gid = 0;
    p->thread_num = 0;
    p->state = USED;
    p->exit_state = 0;
    p->vma = NULL;
    p->killed = 0;
    p->clear_child_tid = 0;
    p->ofn = (struct rlimit){NOFILE, NOFILE};
    memset(&p->context, 0, sizeof(p->context));
    p->trapframe = (struct trapframe *)pmem_alloc_pages(1);
    p->pagetable = proc_pagetable(p);
    memset(p->sig_set.__val, 0, sizeof(p->sig_set));
    memset(p->sig_pending.__val, 0, sizeof(p->sig_pending));
    // memset((void *)p->kstack, 0, PAGE_SIZE);
    p->context.ra = (uint64)forkret;
    p->context.sp = p->kstack + KSTACKSIZE;
    p->main_thread = alloc_thread();
    copycontext(&p->main_thread->context, &p->context);
    p->thread_num++;
    p->main_thread->p = p;
    p->main_thread->sz = p->sz;
    p->main_thread->clear_child_tid = p->clear_child_tid;
    p->main_thread->kstack = p->kstack;
    list_init(&p->thread_queue);
    list_push_front(&p->thread_queue, &p->main_thread->elem);
    // if (mappages(kernel_pagetable, p->kstack - PAGE_SIZE, (uint64)p->main_thread->trapframe, PAGE_SIZE, PTE_R | PTE_W) != 1)
    // {
    //     panic("allocproc: mappages failed");
    // }
    // p->main_thread->vtf = p->kstack - PAGE_SIZE;
    return p;
}

/**
 * @brief 释放进程资源并将其标记为未使用状态,调用者必须持有该进程的锁
 *        释放allocproc时分配的资源
 * @param p
 */
extern struct list free_thread; ///< 全局空闲线程链表
static void freeproc(proc_t *p)
{
    assert(holding(&p->lock), "caller must hold p->lock");
    
    if (debug_buddy)
    {
        printf("freeproc: freeing process %d (pid %d)\n", (int)(p - pool), p->pid);
        printf("freeproc: process trapframe: %p\n", p->trapframe);
        printf("freeproc: main thread trapframe: %p\n", p->main_thread ? p->main_thread->trapframe : NULL);
    }
    
    // 先释放进程的trapframe（主线程也使用这个）
    if (p->trapframe)
    {
        if (debug_buddy)
            printf("freeproc: freeing process trapframe %p\n", p->trapframe);
        pmem_free_pages(p->trapframe, 1);
        p->trapframe = NULL;
    }
    
    /* 清空thread_queue */
    struct list_elem *e = list_begin(&p->thread_queue);
    while (e != list_end(&p->thread_queue))
    {
        struct list_elem *tmp = list_next(e);
        thread_t *t = list_entry(e, thread_t, elem);
        t->state = t_UNUSED; ///< 将线程状态设置为未使用
        
        if (debug_buddy)
            printf("freeproc: processing thread %d, trapframe: %p\n", t->tid, t->trapframe);
        
        // 关键修复：避免重复释放trapframe
        // 主线程的trapframe已经通过进程trapframe释放了
        // 其他线程的trapframe需要单独释放
        if (t->trapframe && t->trapframe != p->trapframe)
        {
            if (debug_buddy)
                printf("freeproc: freeing thread trapframe %p\n", t->trapframe);
            kfree((void *)t->trapframe); ///< 释放线程的trapframe
        }
        else if (t->trapframe)
        {
            if (debug_buddy)
                printf("freeproc: skipping thread trapframe %p (same as process)\n", t->trapframe);
        }
        t->trapframe = NULL; // 清空指针防止重复释放
        
        // vmunmap(kernel_pagetable, t->vtf, 1, 0); ///< 忘了为什么写这个了
        // vmunmap(kernel_pagetable, t->kstack - PGSIZE, 1, 0); ///< 忘了为什么写这个了
        if (t->kstack != p->kstack)
        {
            vmunmap(kernel_pagetable, t->kstack, 1, 0); ///< 释放线程的内核栈
            kfree((void *)t->kstack_pa);
        }
        list_push_front(&free_thread, e);
        e = tmp;
    }

    if (p->pagetable)
    {
        free_vma_list(p);
        proc_freepagetable(p, p->sz);
        p->pagetable = NULL;
    }

    /* 释放文件资源，清空文件描述符 */
    for (int i = 0; i < NOFILE; i++)
    {
        if (p->ofile[i])
        {
            get_file_ops()->close(p->ofile[i]);
            p->ofile[i] = NULL;
        }
    }

    p->pid = 0;
    p->state = UNUSED;
    p->main_thread->state = t_UNUSED;
    p->main_thread = NULL;
    p->ktime = 0;
    p->utime = 0;
    p->parent = NULL;
    p->virt_addr = 0;
    p->exit_state = 0;
    p->killed = 0;
    
    if (debug_buddy)
        printf("freeproc: process %d freed successfully\n", (int)(p - pool));
}

/**
 * @brief  释放进程的页表及其映射的物理内存
 *
 * @param p
 * @param sz
 * 注意:
 * 1. trapframe所占物理页的释放应该在外部
 * 2. trampoline属于代码区域根本不应释放
 */
void proc_freepagetable(struct proc *p, uint64 sz)
{
    vmunmap(p->pagetable, TRAMPOLINE, 1, 0);
    vmunmap(p->pagetable, TRAPFRAME, 1, 0);
    uvmfree(p->pagetable, p->virt_addr, sz - p->virt_addr); ///< 释放进程的页表和用户内存空间
}
void debug_print_all_kstack_extpage()
{
    struct proc *p;

    PRINT_COLOR(YELLOW_COLOR_PRINT, "Kernel stack and  extension page for all processes:\n");
    PRINT_COLOR(YELLOW_COLOR_PRINT, "----------------------------------------------------------------------------------------\n");

    for (p = pool; p < &pool[NPROC]; p++)
    {
        acquire(&p->lock);
        // 计算内核栈底部地址 (KSATCK)
        uint64 kstack_bottom = KSTACK((int)(p - pool));
        uint64 kstack_top = KSTACK((int)(p - pool)) + KSTACKSIZE;

        PRINT_COLOR(YELLOW_COLOR_PRINT, "%d(top)  0x%lx\n",
                    (int)(p - pool),
                    kstack_top);
        PRINT_COLOR(YELLOW_COLOR_PRINT, "%d(btm)  0x%lx\n",
                    (int)(p - pool),
                    kstack_bottom);
        release(&p->lock);
    }

    PRINT_COLOR(YELLOW_COLOR_PRINT, "----------------------------------------------------------------------------------------\n");
}

/**
 * @brief 给每个进程分配内核栈空间
 *
 * @param pagetable 内核页表
 */
void proc_mapstacks(pgtbl_t pagetable)
{
    int ret;
    uint64 va, pa;

    for (proc_t *p = pool; p < pool + NPROC; p++)
    {

        /**
         * riscv 虚拟内存布局  Maxva : 1L<<38
         * / TRAMPOLINE / TRAPFRAME / KSTACK / EXTSIZE  / KSTACK
         * / PGSIZE     /  PGSIZE   / PGSIZE / PGSIZE   /
         *
         * loongarch 虚拟内存布局  Maxva : 1L<<31
         * / TRAMPOLINE / TRAPFRAME /  KSTACK  / EXTSIZE / KSTACK
         * /   PGSIZE   /  PGSIZE   / 6*PGSIZE /2*PGSIZE /
         */
        va = KSTACK((int)(p - pool));
        for (int i = 0; i < KSTACKSIZE / PGSIZE; i++)
        {
            pa = (uint64)pmem_alloc_pages(1);
            assert(pa != 0, "Error alloc pmem\n");
            /**
             * riscv kstack 权限位 ： PTE_R | PTE_W
             * loongarch    权限位 ： PTE_NX(禁止执行) | PTE_P（存在于物理页中） | PTE_W（可写） | PTE_MAT(内存属性) | PTE_D(脏位) | PTE_PLV3（用户态）
             */
            ret = mappages(pagetable, va + i * PGSIZE, pa, PAGE_SIZE, PTE_MAPSTACK);
            assert(ret == 1, "Error Map Proc Stack\n");
        }
    }
    debug_print_all_kstack_extpage();
}

extern char trampoline;
pgtbl_t proc_pagetable(struct proc *p)
{
    pgtbl_t pagetable;
    pagetable = uvmcreate();
    if (pagetable == NULL)
        return NULL;
    if (vma_init(p) == NULL)
        return NULL;
    /*映射trampoline区,代码区*/
    mappages(pagetable, TRAMPOLINE, (uint64)&trampoline, PAGE_SIZE, PTE_TRAMPOLINE);
    /*映射trapframe区,数据区*/
    mappages(pagetable, TRAPFRAME, (uint64)p->trapframe, PAGE_SIZE, PTE_TRAPFRAME);

    return pagetable;
}

void scheduler(void)
{
    struct proc *p;
    cpu_t *cpu = mycpu();
    cpu->proc = NULL;
    for (;;)
    {
        intr_on();
        for (p = pool; p < &pool[NPROC]; p++)
        {
            acquire(&p->lock);
            if (p->state == RUNNABLE)
            {
                thread_t *t = NULL;
                // 寻找可运行的线程
                for (struct list_elem *e = list_begin(&p->thread_queue);
                     e != list_end(&p->thread_queue); e = list_next(e))
                {
                    thread_t *candidate = list_entry(e, thread_t, elem);
                    if (candidate->state == t_RUNNABLE ||
                        (candidate->state == t_TIMING && candidate->awakeTime < r_time() + (1LL << 35))) ///< 57min+time，防止awaketime < 0
                    {
                        t = candidate;
                        break;
                    }
                }

                if (t == NULL)
                {
                    release(&p->lock);
                    continue;
                }
/*
 * LAB1: you may need to init proc start time here
 */
#if DEBUG
                printf("线程切换, pid = %d, tid = %d\n", p->pid, t->tid);
#endif
                p->main_thread = t;                                     ///< 切换到当前线程
                copycontext(&p->context, &p->main_thread->context);     ///< 切换到线程的上下文
                copytrapframe(p->trapframe, p->main_thread->trapframe); ///< 切换到线程的trapframe
                p->main_thread->state = t_RUNNING;
                p->main_thread->awakeTime = 0;
                p->state = RUNNING;
                futex_clear(p->main_thread);
                cpu->proc = p;
#ifdef RISCV
                DEBUG_LOG_LEVEL(LOG_DEBUG, "epc=%p, ra=%p, sp=%p\n", p->trapframe->epc, p->context.ra, p->context.sp);
#else
                DEBUG_LOG_LEVEL(LOG_DEBUG, "era=%p, ra=%p, sp=%p\n", p->trapframe->era, p->context.ra, p->context.sp);
#endif
                DEBUG_LOG_LEVEL(LOG_DEBUG, "pid=%d, tid=%d\n", p->pid, p->main_thread->tid);
                hsai_swtch(&cpu->context, &p->context);

                // 线程执行完毕后，保存其状态
                copycontext(&p->main_thread->context, &p->context);
                // copytrapframe(p->main_thread->trapframe, p->trapframe); ///< 切换回线程的上下文和trapframe
#ifdef RISCV
                DEBUG_LOG_LEVEL(LOG_DEBUG, "epc=%p, ra=%p, sp=%p, trapframe=%p\n", p->trapframe->epc, p->context.ra, p->context.sp, p->trapframe);
#else
                DEBUG_LOG_LEVEL(LOG_DEBUG, "era=%p, ra=%p, sp=%p, trapframe=%p\n", p->trapframe->era, p->context.ra, p->context.sp, p->trapframe);
#endif
                DEBUG_LOG_LEVEL(LOG_DEBUG, "pid=%d, tid=%d\n", p->pid, p->main_thread->tid);
                list_remove(&t->elem);
                list_push_back(&p->thread_queue, &t->elem);

                /* 返回这里时没有用户进程在CPU上执行 */
                cpu->proc = NULL;
            }
            release(&p->lock);
        }
#if DEBUG
        printf("scheduler没有线程可运行\n");
#endif
    }
}

/** Switch to scheduler.  Must hold only p->lock
 * and have changed proc->state. Saves and restores
 * intena because intena is a property of this
 * kernel thread, not this CPU. It should
 * be proc->intena and proc->noff, but that would
 * break in the few places where a lock is held but
 * there's no process.
 */
void sched(void)
{
    int intena;
    struct proc *p = myproc();
    if (!holding(&p->lock))
        panic("sched p->lock");
    if (mycpu()->noff != 1)
    {
        panic("sched locks");
    }
    if (p->state == RUNNING || p->main_thread->state == t_RUNNING)
        panic("sched running");
    if (intr_get())
        panic("sched interruptible");

    /* 切换线程上下文 */
    // 保存当前线程的trapframe和context到线程结构中
    copytrapframe(p->main_thread->trapframe, p->trapframe);
    // copycontext(&p->main_thread->context, &p->context);

    intena = mycpu()->intena;
    hsai_swtch(&p->context, &mycpu()->context);
    mycpu()->intena = intena;
}

/**
 * Atomically release lock and sleep on chan.
 * Reacquires lock when awakened.
 */
void sleep_on_chan(void *chan, struct spinlock *lk)
{
    struct proc *p = myproc();

    /*
     * Must acquire p->lock in order to
     * change p->state and then call sched.
     * Once we hold p->lock, we can be
     * guaranteed that we won't miss any wakeup
     * (wakeup locks p->lock),
     * so it's okay to release lk.
     */
    if (lk != &p->lock) ///< DOC: sleeplock0
    {
        acquire(&p->lock); ///< DOC: sleeplock1
        release(lk);
    }

    /* Go to sleep. */
    p->chan = chan;
    p->state = SLEEPING;
    p->main_thread->state = t_RUNNABLE;

    sched();

    /* Tidy up. */
    p->chan = 0;

    /* Reacquire original lock.  */
    if (lk != &p->lock)
    {
        release(&p->lock);
        acquire(lk);
    }
}

void wakeup(void *chan)
{
    struct proc *p;
    for (p = pool; p < &pool[NPROC]; p++)
    {
        if (p != myproc())
        {
            acquire(&p->lock);
            if (p->state == SLEEPING && p->chan == chan)
            {
                p->state = RUNNABLE;
            }
            release(&p->lock);
        }
    }
}

/**
 * @brief 放弃CPU，一次调度轮转
 * 时钟轮转算法，在时钟中断中调用，因为时间片可能到期了
 */
void yield(void)
{
    proc_t *p = myproc();
    acquire(&p->lock);
    p->state = RUNNABLE;
    p->main_thread->state = t_RUNNABLE;
    sched();
    release(&p->lock);
}

void reparent(proc_t *p)
{
    for (proc_t *child = pool; child < pool + NPROC; child++)
    {
        if (child->parent == p)
        {
            child->parent = initproc;
            wakeup(initproc);
        }
    }
}

static void copycontext_from_trapframe(context_t *t, struct trapframe *f)
{
#ifdef RISCV
    t->ra = f->ra;
    t->sp = f->kernel_sp;
    t->s0 = f->s0;
    t->s1 = f->s1;
    t->s2 = f->s2;
    t->s3 = f->s3;
    t->s4 = f->s4;
    t->s5 = f->s5;
    t->s6 = f->s6;
    t->s7 = f->s7;
    t->s8 = f->s8;
    t->s9 = f->s9;
    t->s10 = f->s10;
    t->s11 = f->s11;
#else
    t->ra = f->ra;
    t->sp = f->kernel_sp;
    t->s0 = f->s0;
    t->s1 = f->s1;
    t->s2 = f->s2;
    t->s3 = f->s3;
    t->s4 = f->s4;
    t->s5 = f->s5;
    t->s6 = f->s6;
    t->s7 = f->s7;
    t->s8 = f->s8;
    t->fp = f->tp; // loongarch uses tp as frame pointer
#endif
}

uint64
clone_thread(uint64 stack_va, uint64 ptid, uint64 tls, uint64 ctid, uint64 flags)
{
    struct proc *p = myproc();
    thread_t *t = alloc_thread();

    acquire(&t->lock);
    t->p = p;
    // /* 1. trapframe映射 */
    // DEBUG_LOG_LEVEL(LOG_DEBUG, "[map]thread trapframe: %p\n", p->kstack - PGSIZE * p->thread_num * 2);
    // if (mappages(kernel_pagetable, p->kstack - PGSIZE * p->thread_num * 2,
    //              (uint64)(t->trapframe), PGSIZE, PTE_R | PTE_W) < 0)
    //     panic("toread_clone: mappages");
    // t->vtf = p->kstack - PGSIZE * p->thread_num * 2;

    /* 2. 映射栈 */
    void *kstack_pa = kalloc();
    if (NULL == kstack_pa)
        panic("thread_clone: kalloc kstack failed");
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[map]thread kstack: %p\n", p->kstack - PGSIZE * (p->thread_num));
    if (mappages(kernel_pagetable, p->kstack - PGSIZE * p->thread_num,
                 (uint64)kstack_pa, PGSIZE, PTE_R | PTE_W) < 0)
        panic("thread_clone: mappages");

    t->kstack_pa = (uint64)kstack_pa;
    t->kstack = p->kstack - PGSIZE * p->thread_num;

    /* 3. 设置新线程的函数入口 */
    args_t tmp;
    if (copyin(p->pagetable, (char *)(&tmp), stack_va,
               sizeof(args_t)) < 0)
    {
        panic("copy in thread_stack_param failed");
    }

    /* 4. 设置trapframe和上下文，做好调度准备 */
    list_push_front(&p->thread_queue, &t->elem);

    copytrapframe(t->trapframe, p->trapframe);

    /* 对于 pthread_create，栈指针指向新线程的栈顶 */
    t->trapframe->a0 = tmp.arg;  ///< 设置新线程的参数
    t->trapframe->sp = stack_va; ///< 设置新线程的栈指针
    t->trapframe->kernel_sp = p->kstack - PGSIZE * p->thread_num + PGSIZE;

    /* 处理CLONE_SETTLS */
    if (flags & CLONE_SETTLS)
        t->trapframe->tp = tls;
    else
        t->trapframe->tp = p->trapframe->tp;

#ifdef RISCV
    t->trapframe->epc = tmp.start_func;
#else
    t->trapframe->era = tmp.start_func;
#endif

    copycontext_from_trapframe(&t->context, t->trapframe);
    t->context.ra = (uint64)forkret;
    t->context.sp = t->trapframe->kernel_sp;

    if (flags & CLONE_PARENT_SETTID)
    {
        if (ptid != 0 && either_copyout(1, ptid, (void *)&t->tid, sizeof(int)) < 0)
            panic("thread_clone: either_copyout");
    }
    if (flags & CLONE_CHILD_SETTID)
    {
        t->clear_child_tid = ctid;
        if (ctid != 0 && either_copyout(1, ctid, (void *)&t->tid, sizeof(int)) < 0)
            panic("thread_clone: child_settid");
    }
    if (flags & CLONE_CHILD_CLEARTID)
    {
        t->clear_child_tid = ctid;
        /* @todo 线程退出时要清零ctid指向的用户空间，这部分需在exit/线程回收时实现 */
    }
    t->sz = p->sz;         ///< 继承父进程的内存顶
    t->state = t_RUNNABLE; ///< 设置线程状态为可运行
    p->thread_num++;
    release(&t->lock);
    DEBUG_LOG_LEVEL(LOG_DEBUG, "clone_thread new thread pid is %d, tid is %d\n", t->p->pid, t->tid);
    return t->tid;
}

/**
 * @brief  创建当前进程的副本（子进程）
 *
 * @return uint64  子进程的PID（父进程返回成功时的子进程PID，子进程返回0）
 */
uint64 fork(void)
{
    struct proc *np;
    struct proc *p = myproc();
    int i, pid;
    
    // 在fork开始时诊断伙伴系统状态
    if (debug_buddy)
    {
        printf("=== Fork started for pid %d ===\n", p->pid);
        buddy_safe_check(); // 使用安全的检查函数
        
        // 如果检测到问题，尝试清理和重建
        // 这里可以根据实际情况决定是否自动清理
        // buddy_cleanup_and_rebuild();
    }
    
    if ((np = allocproc()) == 0)
    {
        panic("fork:allocproc fail");
    }
    if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0) ///< 复制父进程页表到子进程（包含代码段、数据段等）
        panic("fork:uvmcopy fail");
    struct vma *nvma = vma_copy(np, p->vma);
    if (nvma != NULL)
    {
        nvma = nvma->next;
        while (nvma != np->vma)
        {
            if (nvma->type != MMAP || (nvma->addr == nvma->end))
            if (vma_map(p->pagetable, np->pagetable, nvma) < 0)
            {
                panic("clone: vma deep mapping failed\n");
                return -1;
            }
            nvma = nvma->next;
        }
    }
    np->sz = p->sz; ///< 继承父进程内存大小
    np->virt_addr = p->virt_addr;
    np->parent = p;
    // 复制trapframe, np的返回值设为0, 堆栈指针设为目标堆栈
    *(np->trapframe) = *(p->trapframe); ///< 复制陷阱帧（Trapframe）并修改返回值
    np->trapframe->a0 = 0;
    copytrapframe(np->main_thread->trapframe, np->trapframe);
    // @todo 未复制栈    if(stack != 0) np->tf->sp = stack;

    // 复制打开文件
    // increment reference counts on open file descriptors.
    for (i = 0; i < NOFILE; i++)
        if (p->ofile[i])
            np->ofile[i] = get_file_ops()->dup(p->ofile[i]);

    np->cwd.fs = p->cwd.fs;
    strcpy(np->cwd.path, p->cwd.path);

    pid = np->pid;
    np->state = RUNNABLE;
    np->main_thread->state = t_RUNNABLE; ///< 设置主线程状态为可运行

    release(&np->lock); ///< 释放 allocproc中加的锁
    
    // 在fork结束时再次诊断伙伴系统状态
    if (debug_buddy)
    {
        printf("=== Fork completed for pid %d, new pid %d ===\n", p->pid, np->pid);
        buddy_safe_check(); // 使用安全的检查函数
    }
    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "fork new proc pid is %d, tid is %d\n", np->pid, np->main_thread->tid);
    return pid;
}
int clone(uint64 flags, uint64 stack, uint64 ptid, uint64 ctid)
{
    struct proc *np;
    struct proc *p = myproc();
    int i, pid;
    if ((np = allocproc()) == 0)
    {
        panic("fork:allocproc fail");
    }
    if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0) ///< 复制父进程页表到子进程（包含代码段、数据段等）
        panic("fork:uvmcopy fail");
    struct vma *nvma = vma_copy(np, p->vma);
    if (nvma != NULL)
    {
        nvma = nvma->next;
        while (nvma != np->vma)
        {
            if (nvma->type != MMAP || (nvma->addr == nvma->end))
                if (vma_map(p->pagetable, np->pagetable, nvma) < 0)
                {
                    panic("clone: vma deep mapping failed\n");
                    return -1;
                }
            nvma = nvma->next;
        }
    }
    np->sz = p->sz; ///< 继承父进程内存大小
    np->virt_addr = p->virt_addr;
    np->parent = p;
    // @todo 未拷贝用户栈
    // 复制trapframe, np的返回值设为0, 堆栈指针设为目标堆栈
    *(np->trapframe) = *(p->trapframe); ///< 复制陷阱帧（Trapframe）并修改返回值
    np->trapframe->a0 = 0;

    // @todo 未复制栈    if(stack != 0) np->tf->sp = stack;

    // 复制打开文件
    // increment reference counts on open file descriptors.
    for (i = 0; i < NOFILE; i++)
        if (p->ofile[i])
            np->ofile[i] = get_file_ops()->dup(p->ofile[i]);

    np->cwd.fs = p->cwd.fs;
    strcpy(np->cwd.path, p->cwd.path);
    args_t tmp;
    if (copyin(p->pagetable, (char *)(&tmp), stack,
               sizeof(args_t)) < 0)
    {
        panic("copy in thread_stack_param failed");
    }
    pid = np->pid;
    np->state = RUNNABLE;
    np->main_thread->state = t_RUNNABLE;
#ifdef RISCV
    np->trapframe->epc = tmp.start_func;
#else
    np->trapframe->era = tmp.start_func;
#endif
    np->trapframe->sp = stack;
    np->trapframe->a0 = tmp.arg;
    copytrapframe(np->main_thread->trapframe, np->trapframe);
    if (ptid != 0)
    {
        if (copyout(np->pagetable, ptid, (char *)&p->pid, sizeof(p->pid)) < 0)
        {
            panic("clone: copyout failed\n");
            return -1;
        }
    }
    if (ctid != 0)
    {
        if (copyout(np->pagetable, ctid, (char *)&np->pid, sizeof(np->pid)) < 0)
        {
            panic("clone: copyout failed\n");
            return -1;
        }
    }

    release(&np->lock); ///< 释放 allocproc中加的锁
    DEBUG_LOG_LEVEL(LOG_DEBUG, "clone new thread pid is %d, tid is %d\n", np->pid, np->main_thread->tid);
    return pid;
}
/**
 * @brief 等待任意一个子进程退出并回收其资源
 *
 * @param pid  等待的子进程PID，若为-1则等待任意一个子进程退出
 * @param addr 用户空间地址，用于存储子进程的退出状态（若不为0）
 * @return int 成功返回子进程PID，失败返回-1
 */
int wait(int pid, uint64 addr)
{
    struct proc *np;
    int havekids;
    int childpid = -1;
    struct proc *p = myproc();
    acquire(&p->lock); ///< 获取父进程锁，防止并发修改进程状态
    for (;;)
    {
        havekids = 0;
        for (np = pool; np < &pool[NPROC]; np++)
        {
            if (np->parent == p)
            {
                intr_off();
                acquire(&np->lock); ///<  获取子进程锁
                havekids = 1;
                if ((pid == -1 || np->pid == pid) && np->state == ZOMBIE)
                {
                    childpid = np->pid;
                    /*
                     * //TODO 完整规则如下，这里先只进行左移8位
                     * 组合退出码和信号为完整状态码（高8位为退出码，低8位为信号)
                     * (np->exit_state << 8) | np->signal;
                     *
                     */
                    uint16_t status = np->exit_state << 8;
                    if (addr != 0 && copyout(p->pagetable, addr, (char *)&status, sizeof(status)) < 0) ///< 若用户指定了状态存储地址
                    {
                        release(&np->lock);
                        release(&p->lock);
                        return -1;
                    }
                    freeproc(np);
                    release(&np->lock);
                    release(&p->lock);
                    intr_on();
                    return childpid;
                }
                release(&np->lock);
            }
        }
        /*若没有子进程 或 当前进程已被杀死*/
        if (!havekids || p->killed)
        {
            release(&p->lock);
            return -1;
        }
        /*子进程未退出，父进程进入睡眠等待*/
        sleep_on_chan(p, &p->lock);
    }
}

/**
 * @brief 终止当前进程并将其资源回收工作委托给父进程
 *
 * @param exit_state 进程退出状态码（通常0表示成功，非0表示错误）
 */
void exit(int exit_state)
{
    struct proc *p = myproc();
    /* 禁止init进程退出 */
    if (p == initproc)
        panic("init exiting");

    /* 关掉所有打开的文件 */
    for (int fd = 0; fd < NOFILE; fd++)
    {
        if (p->ofile[fd])
        {
            struct file *f = p->ofile[fd];
            get_file_ops()->close(f);
            p->ofile[fd] = 0;
        }
    }

    acquire(&parent_lock); ///< 获取全局父进程锁
    reparent(p);           ///<  将所有子进程的父进程改为initproc
    wakeup(p->parent);     ///< 唤醒父进程进行回收

    // 获取p的锁以改变一些属性
    acquire(&p->lock);
    p->exit_state = exit_state;
    p->state = ZOMBIE;
    p->main_thread->state = t_ZOMBIE; ///< 将主线程状态设置为僵尸状态

    // /* 托孤，遍历进程池，如果进程池的进程parent是它，就托付给1号进程 */
    // for (struct proc *child = pool; child < &pool[NPROC]; child++)
    // {
    //     if (child->parent == p)
    //     {
    //         child->parent = initproc;
    //     }
    // }

    release(&parent_lock);
    sched();
}
/**
 * @brief  调整进程的内存大小
 *
 * @param n 要增加或减少的字节数（正数为增加，负数为减少）
 * @return int 成功返回0，失败返回-1
 */
int growproc(int n)
{
    uint64 sz;
    proc_t *p = myproc();

    sz = p->sz;
    if (n > 0)
    {
        if (sz + n >= MAXVA - PGSIZE)
            return -1;
        if (n > 0x10000)
        {
            if ((sz = uvmalloc(p->pagetable, sz, sz + 0x10000,
                               PTE_RW)) == 0)
            {
                return -1;
            }
        }
        else
        {
            if ((sz = uvmalloc(p->pagetable, sz, sz + n,
                               PTE_RW)) == 0)
            {
                return -1;
            }
        }
    }
    if (n < 0)
    {
        sz = uvmdealloc(p->pagetable, sz, sz + n);
    }
    p->sz += n;

    return 0;
}

int killed(struct proc *p)
{
    int k;

    acquire(&p->lock);
    k = p->killed;
    release(&p->lock);
    return k;
}

/*
 * //TODO
 * forkret好像是内核态的，我在forkret中测试，所以
 * 用了isnotforkret，后面可能要删掉
 */
bool isnotforkret = false;
// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
    struct proc *p = myproc();
    if (user_dst && isnotforkret)
    {
        return copyout(p->pagetable, dst, src, len);
    }
    else
    {
        memmove((char *)dst, src, len);
        return 0;
    }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
    struct proc *p = myproc();

    if (user_src && isnotforkret)
    {
        return copyin(p->pagetable, dst, src, len);
    }
    else
    {
        memmove(dst, (char *)src, len);
        return 0;
    }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
    static char *states[] = {
        [UNUSED] "unused",
        [USED] "used",
        [SLEEPING] "sleep ",
        [RUNNABLE] "runble",
        [RUNNING] "run   ",
        [ZOMBIE] "zombie"};
    struct proc *p;
    char *state;

    printf("\n");
    for (p = pool; p < &pool[NPROC]; p++)
    {
        if (p->state == UNUSED)
            continue;
        if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
            state = states[p->state];
        else
            state = "???";
        printf("%d %s", p->pid, state);
        printf("\n");
    }
}

uint64 procnum(void)
{
    int num = 0;
    struct proc *p;

    for (p = pool; p < &pool[NPROC]; p++)
    {
        if (p->state != UNUSED)
        {
            num++;
        }
    }

    return num;
}

int kill(int pid, int sig)
{
    proc_t *p;
    for (p = pool; p < &pool[NPROC]; p++)
    {
        acquire(&p->lock);
        if (p->pid == pid)
        {
            p->sig_pending.__val[0] |= (1 << sig);
            if (p->killed == 0 || p->killed > sig)
            {
                p->killed = sig;
            }
            if (p->state == SLEEPING)
            {
                p->state = RUNNABLE;
            }
            release(&p->lock);
            return 0;
        }
        release(&p->lock);
    }
    return 0;
}

int tgkill(int tgid, int tid, int sig)
{
    proc_t *p;
    for (p = pool; p < &pool[NPROC]; p++)
    {
        acquire(&p->lock);
        if (p->pid == tgid)
        {
            thread_t *t;
            for (struct list_elem *e = list_begin(&p->thread_queue); e != list_end(&p->thread_queue); e = list_next(e))
            {
                t = list_entry(e, thread_t, elem);
                if (t->tid == tid)
                {
                    p->sig_pending.__val[0] |= (1 << sig);
                    if (p->killed == 0 || p->killed > sig)
                    {
                        p->killed = sig;
                    }
                    if (p->state == SLEEPING)
                    {
                        p->state = RUNNABLE;
                    }
                    release(&p->lock);
                    return 0;
                }
            }
        }
        release(&p->lock);
    }
    return -1;
}

void copytrapframe(struct trapframe *f1, struct trapframe *f2)
{
#ifdef RISCV
    f1->kernel_satp = f2->kernel_satp;
    f1->kernel_sp = f2->kernel_sp;
    f1->kernel_trap = f2->kernel_trap;
    f1->epc = f2->epc;
    f1->kernel_hartid = f2->kernel_hartid;
    f1->ra = f2->ra;
    f1->sp = f2->sp;
    f1->gp = f2->gp;
    f1->tp = f2->tp;
    f1->t0 = f2->t0;
    f1->t1 = f2->t1;
    f1->t2 = f2->t2;
    f1->s0 = f2->s0;
    f1->s1 = f2->s1;
    f1->a0 = f2->a0;
    f1->a1 = f2->a1;
    f1->a2 = f2->a2;
    f1->a3 = f2->a3;
    f1->a4 = f2->a4;
    f1->a5 = f2->a5;
    f1->a6 = f2->a6;
    f1->a7 = f2->a7;
    f1->s2 = f2->s2;
    f1->s3 = f2->s3;
    f1->s4 = f2->s4;
    f1->s5 = f2->s5;
    f1->s6 = f2->s6;
    f1->s7 = f2->s7;
    f1->s8 = f2->s8;
    f1->s9 = f2->s9;
    f1->s10 = f2->s10;
    f1->s11 = f2->s11;
    f1->t3 = f2->t3;
    f1->t4 = f2->t4;
    f1->t5 = f2->t5;
    f1->t6 = f2->t6;
#else
    f1->ra = f2->ra;
    f1->tp = f2->tp;
    f1->sp = f2->sp;
    f1->a0 = f2->a0;
    f1->a1 = f2->a1;
    f1->a2 = f2->a2;
    f1->a3 = f2->a3;
    f1->a4 = f2->a4;
    f1->a5 = f2->a5;
    f1->a6 = f2->a6;
    f1->a7 = f2->a7;
    f1->t0 = f2->t0;
    f1->t1 = f2->t1;
    f1->t2 = f2->t2;
    f1->t3 = f2->t3;
    f1->t4 = f2->t4;
    f1->t5 = f2->t5;
    f1->t6 = f2->t6;
    f1->t7 = f2->t7;
    f1->t8 = f2->t8;
    f1->r21 = f2->r21;
    f1->fp = f2->fp;
    f1->s0 = f2->s0;
    f1->s1 = f2->s1;
    f1->s2 = f2->s2;
    f1->s3 = f2->s3;
    f1->s4 = f2->s4;
    f1->s5 = f2->s5;
    f1->s6 = f2->s6;
    f1->s7 = f2->s7;
    f1->s8 = f2->s8;
    f1->kernel_sp = f2->kernel_sp;
    f1->kernel_trap = f2->kernel_trap;
    f1->era = f2->era;                     ///< 记录trap发生地址
    f1->kernel_hartid = f2->kernel_hartid; ///< 内核hartid
    f1->kernel_pgdl = f2->kernel_pgdl;     ///< 内核页表地址
#endif
}