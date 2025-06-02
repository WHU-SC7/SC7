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

#define NPROC (16)
#define CLONE_VM 0x00000100

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
    int uid;                     // Process User ID
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
uint64 clone_thread(uint64 stack_va, uint64 ptid, uint64 tls, uint64 ctid);
#endif // PROC_H