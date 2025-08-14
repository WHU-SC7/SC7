#ifndef __PROCESS_H__
#define __PROCESS_H__

#include "types.h"
#include "spinlock.h"
#include "trap.h"

// 前向声明
typedef struct file_vnode file_vnode_t;
typedef struct thread thread_t;

// 类型定义
typedef uint32_t mode_t;
#include "fs_defs.h"
#include "vma.h"
#include "file.h"
#include "timer.h"
#include "signal.h"
#include "context.h"
#include "thread.h"
#include "list.h"
#include "resource.h"
#include "stat.h"

// 类型定义
typedef uint32_t gid_t;

#define NPROC (128)
#define NGROUPS_MAX 32 ///< Maximum number of supplementary groups
#define PID_MAX 10000

/* Cloning flags.  */
#define CSIGNAL 0x000000ff              /* Signal mask to be sent at exit.  */
#define CLONE_VM 0x00000100             /* Set if VM shared between processes.  */
#define CLONE_FS 0x00000200             /* Set if fs info shared between processes.  */
#define CLONE_FILES 0x00000400          /* Set if open files shared between processes.  */
#define CLONE_SIGHAND 0x00000800        /* Set if signal handlers shared.  */
#define CLONE_PIDFD 0x00001000          /* Set if a pidfd should be placed \
                                           in parent.  */
#define CLONE_PTRACE 0x00002000         /* Set if tracing continues on the child.  */
#define CLONE_VFORK 0x00004000          /* Set if the parent wants the child to \
                                           wake it up on mm_release.  */
#define CLONE_PARENT 0x00008000         /* Set if we want to have the same \
                                           parent as the cloner.  */
#define CLONE_THREAD 0x00010000         /* Set to add to same thread group.  */
#define CLONE_NEWNS 0x00020000          /* Set to create new namespace.  */
#define CLONE_SYSVSEM 0x00040000        /* Set to shared SVID SEM_UNDO semantics.  */
#define CLONE_SETTLS 0x00080000         /* Set TLS info.  */
#define CLONE_PARENT_SETTID 0x00100000  /* Store TID in userlevel buffer \
                                           before MM copy.  */
#define CLONE_CHILD_CLEARTID 0x00200000 /* Register exit futex and memory \
                                           location to clear.  */
#define CLONE_DETACHED 0x00400000       /* Create clone detached.  */
#define CLONE_UNTRACED 0x00800000       /* Set if the tracing process can't \
                                           force CLONE_PTRACE on this clone.  */
#define CLONE_CHILD_SETTID 0x01000000   /* Store TID in userlevel buffer in \
                                           the child.  */
#define CLONE_NEWCGROUP 0x02000000      /* New cgroup namespace.  */
#define CLONE_NEWUTS 0x04000000         /* New utsname group.  */
#define CLONE_NEWIPC 0x08000000         /* New ipcs.  */
#define CLONE_NEWUSER 0x10000000        /* New user namespace.  */
#define CLONE_NEWPID 0x20000000         /* New pid namespace.  */
#define CLONE_NEWNET 0x40000000         /* New network namespace.  */
#define CLONE_IO 0x80000000             /* Clone I/O context.  */

/* waitpid options */
#define WNOHANG 0x00000001 /* Don't hang if no status is available */

/*
 * Scheduling policies
 */
#define SCHED_OTHER 0
#define SCHED_FIFO 1
#define SCHED_RR 2
#define SCHED_BATCH 3
#define SCHED_ISO 4
#define SCHED_IDLE 5
#define SCHED_DEADLINE 6

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

#define MAX_SHAREMEMORY_REGION_NUM 20 // 一个进程最多的共享内存段数量
// Per-process state
typedef struct proc
{
    spinlock_t lock;     ///< 自旋锁限制修改
    void *chan;          ///< 如果 non-zero，sleeping on chan
    struct proc *parent; ///< Parent process

    thread_t *current_thread; ///< 当前运行的线程
    thread_t *main_thread;    ///< 主线程
    struct list thread_queue; ///< 线程链表

    enum procstate state;                    ///< Process state
    int exit_state;                          ///< 进程退出状态
    int killed;                              ///< 如果不为0，则进程被杀死，值为信号号
    int pid;                                 ///< Process ID
    uid_t ruid;                              ///< Real User ID
    uid_t euid;                              ///< Effective User ID
    uid_t suid;                              ///< Saved User ID
    gid_t rgid;                              ///< Real Group ID
    gid_t egid;                              ///< Effective Group ID
    gid_t sgid;                              ///< Saved Group ID
    mode_t umask;                            ///< File creation mask
    int pgid;                                ///< Process Group ID
    int sid;                                 ///< Session ID
    gid_t supplementary_groups[NGROUPS_MAX]; ///< Supplementary group IDs
    int ngroups;                             ///< Number of supplementary groups
    uint64 virt_addr;                        ///< Virtual address of proc
    uint64 sz;                               ///< Size of process memory (bytes)
    uint64 kstack;                           ///< Virtual address of kernel stack
    struct trapframe *trapframe;             ///< data page for trampoline.S
    struct context context;                  ///< swtch() here to run process
    pgtbl_t pagetable;                       ///< User page table

    int utime;              ///< 用户态运行时间
    int ktime;              ///< 内核态运行时间
    int thread_num;         ///< 线程数量
    uint64 clear_child_tid; ///< 子线程ID清除标志
    struct vma *vma;
    struct sharememory *sharememory[MAX_SHAREMEMORY_REGION_NUM]; ///< 共享内存段
    int shm_num;                                                 ///< 记录有几个共享内存段
    uint64 shm_size;                                             // 已经映射的共享内存大小
    struct shm_attach *shm_attaches;                             // 共享内存附加链表
    /* 定时器设置 */
    struct itimerval itimer; // 定时器设置
    uint64 alarm_ticks;      // 下一次警报的tick值
    int timer_active;        // 定时器是否激活
    int timer_type;          // 定时器类型：TIMER_ONESHOT 或 TIMER_PERIODIC

    /* 和文件有关数据结构 */
    struct file *ofile[NOFILE]; ///< Open files
    struct file_vnode cwd;      ///< Current directory 因为暂时用file结构来代表目录，所以这里这样实现
    struct file_vnode root;     ///< Root directory for chroot
    struct rlimit ofn;          ///< 打开文件数量限制

    /* 资源限制 */
    struct rlimit rlimits[RLIMIT_NLIMITS]; ///< 各种资源限制

    /* 信号相关 */
    __sigset_t sig_set;
    sigaction sigaction[SIGRTMAX + 1]; // signal action 信号处理函数
    __sigset_t sig_pending;            // pending signal
    struct trapframe sig_trapframe;    // 信号处理上下文
    int current_signal;                // 当前正在处理的信号
    int signal_interrupted;            // 是否被信号中断
    int continued;                     // 是否被SIGCONT继续

    /* prctl 相关字段 */
    char comm[16];       // 进程名称 (PR_SET_NAME/PR_GET_NAME)
    int pdeathsig;       // 父进程死亡信号 (PR_SET_PDEATHSIG/PR_GET_PDEATHSIG)
    int dumpable;        // 是否可dump (PR_SET_DUMPABLE/PR_GET_DUMPABLE)
    int no_new_privs;    // 不获取新权限 (PR_SET_NO_NEW_PRIVS/PR_GET_NO_NEW_PRIVS)
    int thp_disable;     // 禁用透明大页 (PR_SET_THP_DISABLE/PR_GET_THP_DISABLE)
    int child_subreaper; // 子进程回收器 (PR_SET_CHILD_SUBREAPER/PR_GET_CHILD_SUBREAPER)

    /* CPU亲和性相关 */
    uint64 cpu_affinity; // CPU亲和性掩码，每个位表示一个CPU
} proc_t;

#define _NSIG 65
typedef struct start_args
{
    void *(*start_func)(void *);
    void *start_arg;
    volatile int control;
    unsigned long sig_mask[_NSIG / 8 / sizeof(long)];
} args_t;

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
int clone(uint64 flags, uint64 stack, uint64 ptid, uint64 ctid);
int wait(int pid, uint64 addr);
int waitpid(int pid, uint64 addr, int options);
int waitid(int idtype, int id, uint64 infop, int options);
void exit(int exit_state);
void thread_exit(int exit_code);
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
int has_file_permission(struct kstat *st, int perm);
int check_file_access(struct kstat *st, int mode);
int check_root_access(struct kstat *st, int mode);
#endif // PROC_H