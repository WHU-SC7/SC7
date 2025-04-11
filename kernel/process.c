#include "process.h"
#include "string.h"
#include "hsai_trap.h"
#include "print.h"
#include "cpu.h"
#include "spinlock.h"
#include "pmem.h"
#include "vmem.h"
#ifdef RISCV
#include "riscv.h"
#include "riscv_memlayout.h"
#else
#include "loongarch.h"
#endif

#define PAGE_SIZE (0x1000)

struct proc pool[NPROC];
char kstack[NPROC][PAGE_SIZE];
__attribute__((aligned(4096))) char ustack[NPROC][PAGE_SIZE];
__attribute__((aligned(4096))) char trapframe[NPROC][PAGE_SIZE];
__attribute__((aligned(4096))) char entry_stack[PAGE_SIZE];
// extern char boot_stack_top[];
struct proc *current_proc;
struct proc idle;
spinlock_t pid_lock;

int threadid()
{
    return curr_proc()->pid;
}
pgtbl_t proc_pagetable(struct proc *p);

struct proc *curr_proc()
{
    return current_proc;
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
        p->kstack = KSTACK((int)(p - pool));
        // p->kstack = (uint64)kstack[p - pool];
        // p->ustack = (uint64)ustack[p - pool];
        // p->trapframe = (struct trapframe *)trapframe[p - pool];
        p->trapframe = 0;
        p->parent = 0;
        p->ktime = 0;
        p->utime = 0;
        /*
         * LAB1: you may need to initialize your new fields of proc here
         */
    }
    // idle.kstack = (uint64)entry_stack;
    // idle.pid = 0;
    // current_proc = &idle;
}

int allocpid(void)
{
    static int PID = 1;
    acquire(&pid_lock);
    int pid = PID++;
    release(&pid_lock);
    return pid;
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
    p->state = USED;
    memset(&p->context, 0, sizeof(p->context));
    p->trapframe = (struct trapframe *)pmem_alloc_pages(1);
    p->pagetable = proc_pagetable(p);
    // memset(p->trapframe, 0, PAGE_SIZE);
    // memset((void *)p->kstack, 0, PAGE_SIZE);
    p->context.ra = (uint64)hsai_usertrapret;
    p->context.sp = p->kstack + KSTACKSIZE;
    release(&p->lock);
    return p;
}

/**
 * @brief 给每个进程分配栈空间
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
}

extern char trampoline;
pgtbl_t proc_pagetable(struct proc *p)
{
    pgtbl_t pagetable;
    pagetable = uvmcreate();
    if (pagetable == NULL)
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
                /*
                 * LAB1: you may need to init proc start time here
                 */
                printf("线程切换\n");
                p->state = RUNNING;
                cpu->proc = p;
                current_proc = p;

                hsai_swtch(&idle.context, &p->context);

                /* 返回这里时没有用户进程在CPU上执行 */
                cpu->proc = NULL;
            }
            release(&p->lock);
            printf("scheduler没有线程可运行");
        }
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
    if (p->state == RUNNING)
        panic("sched running");
    if (intr_get())
        panic("sched interruptible");

    /* 切换线程上下文 */
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
        acquire(&p->lock);
        if (p->state == SLEEPING && p->chan == chan)
        {
            p->state = RUNNABLE;
        }
        release(&p->lock);
    }
}

/**
 * @brief 放弃CPU，一次调度轮转
 * 时钟轮转算法，在时钟中断中调用，因为时间片可能到期了
 */
void 
yield(void) 
{
    proc_t *p = myproc();
    acquire(&p->lock);
    p->state = RUNNABLE;
    sched();
    release(&p->lock);
}