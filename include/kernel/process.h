#ifndef __PROCESS_H__
#define __PROCESS_H__

#include "types.h"
#include "spinlock.h"
#include "trap.h"
#include "fs_defs.h"
#include "file.h"

#define NPROC (16)

enum procstate
{
    UNUSED,
    USED,
    SLEEPING,
    RUNNABLE,
    RUNNING,
    ZOMBIE
};

#if defined RISCV
typedef struct context
{ // riscv 14个
    uint64 ra;
    uint64 sp;

    // callee-saved
    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
} context_t;
#else
typedef struct context // loongarch 12个
{
    uint64 ra;
    uint64 sp;

    // callee-saved
    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 fp;
} context_t;
#endif

// Per-process state
typedef struct proc
{
    spinlock_t lock;     ///< 自旋锁限制修改
    void *chan;          ///< 如果 non-zero，sleeping on chan
    struct proc *parent; ///< Parent process

    enum procstate state;        ///< Process state
    int exit_state;              ///< 进程退出状态
    int killed;                  ///< 如果不为0，则进程被杀死
    int pid;                     ///< Process ID
    uint64 virt_addr;            ///< Virtual address of proc
    uint64 sz;                   ///< Size of process memory (bytes)
    uint64 kstack;               ///< Virtual address of kernel stack
    struct trapframe *trapframe; ///< data page for trampoline.S
    struct context context;      ///< swtch() here to run process
    pgtbl_t pagetable;           ///< User page table
    int utime;                   ///< 用户态运行时间
    int ktime;                   ///< 内核态运行时间
    
    /* 和文件有关数据结构 */
    struct file *ofile[NOFILE];  ///< Open files
    struct file_vnode cwd;       ///< Current directory 因为暂时用file结构来代表目录，所以这里这样实现
} proc_t;

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
int wait(int pid , uint64 addr);
void exit(int exit_state);
void proc_yield(void);
void reg_info(void);
int growproc(int n);
int exec(char *path, char **argv, char **env);
int killed(struct proc *p);
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int either_copyin(void *dst, int user_src, uint64 src, uint64 len);
void procdump(void);
#endif // PROC_H