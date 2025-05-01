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
proc_t *initproc; // 第一个用户态进程,永不退出
spinlock_t pid_lock;
static spinlock_t parent_lock; // 在涉及进程父子关系时使用

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
        /*
         * LAB1: you may need to initialize your new fields of proc here
         */
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
    p->exit_state = 0;
    p->killed = 0;
    memset(&p->context, 0, sizeof(p->context));
    p->trapframe = (struct trapframe *)pmem_alloc_pages(1);
    p->pagetable = proc_pagetable(p);
    // memset((void *)p->kstack, 0, PAGE_SIZE);
    p->context.ra = (uint64)forkret;
    p->context.sp = p->kstack + KSTACKSIZE;
    return p;
}

/**
 * @brief 释放进程资源并将其标记为未使用状态,调用者必须持有该进程的锁
 *        释放allocproc时分配的资源
 * @param p
 */
static void freeproc(proc_t *p)
{
    assert(holding(&p->lock), "caller must hold p->lock");
    if (p->trapframe)
    {
        pmem_free_pages(p->trapframe, 1);
        p->trapframe = NULL;
    }
    if (p->pagetable)
    {
        proc_freepagetable(p, p->sz);
        p->pagetable = NULL;
    }
    p->pid = 0;
    p->state = UNUSED;
    p->ktime = 0;
    p->utime = 0;
    p->parent = NULL;
    p->virt_addr = 0;
    p->exit_state = 0;
    p->killed = 0;
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
    uvmfree(p->pagetable, p->virt_addr, sz); ///< 释放进程的页表和用户内存空间
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
                #if  DEBUG
                printf("线程切换\n");
                #endif
                p->state = RUNNING;
                cpu->proc = p;
                hsai_swtch(&cpu->context, &p->context);

                /* 返回这里时没有用户进程在CPU上执行 */
                cpu->proc = NULL;
            }
            release(&p->lock);
        }
        #if  DEBUG
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
    if ((np = allocproc()) == 0)
    {
        panic("fork:allocproc fail");
    }
    if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0) ///< 复制父进程页表到子进程（包含代码段、数据段等）
        panic("fork:uvmcopy fail");
    np->sz = p->sz; ///< 继承父进程内存大小
    np->parent = p;
    // @todo 未拷贝用户栈
    // 复制trapframe, np的返回值设为0, 堆栈指针设为目标堆栈
    *(np->trapframe) = *(p->trapframe); ///< 复制陷阱帧（Trapframe）并修改返回值
    np->trapframe->a0 = 0;
    // @todo 未复制栈    if(stack != 0) np->tf->sp = stack;
    
    // 复制打开文件
    // increment reference counts on open file descriptors.
    for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
        np->ofile[i] = get_fops()->dup(p->ofile[i]);

    pid = np->pid;
    np->state = RUNNABLE;

    release(&np->lock); ///< 释放 allocproc中加的锁
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
                acquire(&np->lock); ///<  获取子进程锁
                havekids = 1;
                if ((pid == -1 || np->pid == pid) && np->state == ZOMBIE)
                {
                    childpid = np->pid;
                    if (addr != 0 && copyout(p->pagetable, addr, (char *)&np->exit_state, sizeof(np->exit_state)) < 0) ///< 若用户指定了状态存储地址
                    {
                        release(&np->lock);
                        release(&p->lock);
                        return -1;
                    }
                    freeproc(np);
                    release(&np->lock);
                    release(&p->lock);
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
    for(int fd = 0; fd < NOFILE; fd++){
        if(p->ofile[fd]){
        struct file *f = p->ofile[fd];
        get_fops()->close(f);
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
        if ((sz = uvmalloc(p->pagetable, sz, sz + n,
                           PTE_RW)) == 0)
        {
            return -1;
        }
    }
    else if (n < 0)
    {
        sz = uvmdealloc(p->pagetable, sz, sz + n);
    }
    p->sz = sz;
    return 0;
}

int
killed(struct proc *p)
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
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst && isnotforkret){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();

 if(user_src && isnotforkret){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = pool; p < &pool[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s", p->pid, state);
    printf("\n");
  }
}