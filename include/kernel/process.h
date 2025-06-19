#ifndef __PROCESS_H__
#define __PROCESS_H__

#include "types.h"
#include "spinlock.h"
#include "trap.h"
#include "fs_defs.h"
#include "vma.h"
#include "file.h"
#include "timer.h"
#include "signal.h"
#include "context.h"
#include "thread.h"
#include "list.h"
#include "resource.h"

#define NPROC (16)

/* Cloning flags.  */
# define CSIGNAL       0x000000ff /* Signal mask to be sent at exit.  */
# define CLONE_VM      0x00000100 /* Set if VM shared between processes.  */
# define CLONE_FS      0x00000200 /* Set if fs info shared between processes.  */
# define CLONE_FILES   0x00000400 /* Set if open files shared between processes.  */
# define CLONE_SIGHAND 0x00000800 /* Set if signal handlers shared.  */
# define CLONE_PIDFD   0x00001000 /* Set if a pidfd should be placed
				     in parent.  */
# define CLONE_PTRACE  0x00002000 /* Set if tracing continues on the child.  */
# define CLONE_VFORK   0x00004000 /* Set if the parent wants the child to
				     wake it up on mm_release.  */
# define CLONE_PARENT  0x00008000 /* Set if we want to have the same
				     parent as the cloner.  */
# define CLONE_THREAD  0x00010000 /* Set to add to same thread group.  */
# define CLONE_NEWNS   0x00020000 /* Set to create new namespace.  */
# define CLONE_SYSVSEM 0x00040000 /* Set to shared SVID SEM_UNDO semantics.  */
# define CLONE_SETTLS  0x00080000 /* Set TLS info.  */
# define CLONE_PARENT_SETTID 0x00100000 /* Store TID in userlevel buffer
					   before MM copy.  */
# define CLONE_CHILD_CLEARTID 0x00200000 /* Register exit futex and memory
					    location to clear.  */
# define CLONE_DETACHED 0x00400000 /* Create clone detached.  */
# define CLONE_UNTRACED 0x00800000 /* Set if the tracing process can't
				      force CLONE_PTRACE on this clone.  */
# define CLONE_CHILD_SETTID 0x01000000 /* Store TID in userlevel buffer in
					  the child.  */
# define CLONE_NEWCGROUP    0x02000000	/* New cgroup namespace.  */
# define CLONE_NEWUTS	0x04000000	/* New utsname group.  */
# define CLONE_NEWIPC	0x08000000	/* New ipcs.  */
# define CLONE_NEWUSER	0x10000000	/* New user namespace.  */
# define CLONE_NEWPID	0x20000000	/* New pid namespace.  */
# define CLONE_NEWNET	0x40000000	/* New network namespace.  */
# define CLONE_IO	0x80000000	/* Clone I/O context.  */

enum procstate
{
    UNUSED,
    USED,
    SLEEPING,
    RUNNABLE,
    RUNNING,
    ZOMBIE
};
typedef struct thread thread_t; // 前向声明，保证thread_t已知

// Per-process state
typedef struct proc
{
    spinlock_t lock;     ///< 自旋锁限制修改
    void *chan;          ///< 如果 non-zero，sleeping on chan
    struct proc *parent; ///< Parent process

    thread_t *main_thread;       ///< 主线程
    struct list thread_queue;    ///< 线程链表

    enum procstate state;        ///< Process state
    int exit_state;              ///< 进程退出状态
    int killed;                  ///< 如果不为0，则进程被杀死
    int pid;                     ///< Process ID
    int uid;                     ///< Process User ID
    int gid;                     ///< Group ID
    uint64 virt_addr;            ///< Virtual address of proc
    uint64 sz;                   ///< Size of process memory (bytes)
    uint64 kstack;               ///< Virtual address of kernel stack
    struct trapframe *trapframe; ///< data page for trampoline.S
    struct context context;      ///< swtch() here to run process
    pgtbl_t pagetable;           ///< User page table

    int utime;                   ///< 用户态运行时间
    int ktime;                   ///< 内核态运行时间
    int thread_num;              ///< 线程数量
    uint64 clear_child_tid;      ///< 子线程ID清除标志
    struct vma *vma;
    // /* 定时器设置 */
    // struct itimerval itimer;  // 定时器设置
    // uint64 alarm_ticks;       // 下一次警报的tick值
    // int timer_active;         // 定时器是否激活

    /* 和文件有关数据结构 */
    struct file *ofile[NOFILE]; ///< Open files
    struct file_vnode cwd;      ///< Current directory 因为暂时用file结构来代表目录，所以这里这样实现
    struct rlimit    ofn;       ///< 打开文件数量限制

    /* 信号相关 */
    __sigset_t sig_set;
    sigaction sigaction[SIGRTMAX + 1]; // signal action
    __sigset_t sig_pending;            // pending signal
} proc_t;


typedef struct thread_stack_param 
{
    uint64 func_point;
    uint64 arg_point;
} thread_stack_param;

void copytrapframe(struct trapframe *dest, struct trapframe *src);
void proc_init();
struct proc *getproc(int pid);
void scheduler() __attribute__((noreturn));
struct proc *allocproc();
pgtbl_t proc_pagetable(struct proc *p);
void proc_freepagetable(struct proc *p, uint64 sz);
void proc_mapstacks(pgtbl_t pagetable);
void sleep_on_chan(void *, struct spinlock *);
void wakeup(void *);
void yield(void);
uint64 fork(void);
int clone(uint64 stack, uint64 ptid, uint64 ctid);
int wait(int pid, uint64 addr);
void exit(int exit_state);
void proc_yield(void);
void reg_info(void);
int growproc(int n);
int exec(char *path, char **argv, char **env);
int killed(struct proc *p);
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int either_copyin(void *dst, int user_src, uint64 src, uint64 len);
void procdump(void);
uint64 procnum(void);
int kill(int pid, int sig);
int tgkill(int tgid, int tid, int sig);
void sched(void);
uint64 clone_thread(uint64 stack_va, uint64 ptid, uint64 tls, uint64 ctid, uint64 flags);
#endif // PROC_H