#ifndef __THREAD_H__
#define __THREAD_H__

#include "types.h"
#include "queue.h"
#include "spinlock.h"
#include "cpu.h"
#include "list.h"
#include "context.h"
#include "process.h"
#include "signal.h"

// 前向声明
typedef struct proc proc_t;
typedef struct trapframe trapframe_t;
typedef struct vma vma_t;

#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

#define THREAD_NUM 2048

enum thread_state
{
    t_UNUSED,
    t_USED,
    t_SLEEPING,
    t_RUNNABLE,
    t_RUNNING,
    t_ZOMBIE,
    t_TIMING,
};

typedef struct thread
{
    struct spinlock lock;

    /* 当使用下面这些变量的时候，thread的锁必须持有 */
    enum thread_state state; //< 线程的状态
    proc_t *p;               //< 这个线程属于哪一个进程
    void *chan;              //< 如果不为NULL,则在chan地址上睡眠
    int tid;                 //< 线程ID
    uint64 awakeTime;        //< futex 睡眠时间
    int timeout_occurred;    //< 标记是否因为超时而唤醒
    pid_t ppid;              //< 父进程ID,用于线程退出时的父进程回收资源

    /* 使用下面这些变量的时候，thread的锁不需要持有 */
    uint64 kstack;  //< 线程内核栈的地址,一个进程的不同线程所用的内核栈的地址应该不同
    uint64 vtf;     //< 线程的trapframe的虚拟地址
    uint64 sz;      //< 复制自进程的sz
    int thread_idx; //< 线程列表中的第几个
    trapframe_t *trapframe;
    context_t context;     //< 每个进程应该有自己的context
    uint64 kstack_pa;      //< 当线程的栈和进程的栈不是一个的时候，用它保存物理地址
    struct list_elem elem; //< 用于进程的线程链表

    uint64 clear_child_tid; //< 子线程ID清除标志
    vma_t *stack_vma;       //< 线程栈VMA的引用，用于退出时减少引用计数

    __sigset_t sig_set;     // 信号掩码
    __sigset_t sig_pending; // pending signal
    
    /* 信号处理函数数组 - 每个线程独立的信号处理 */
    sigaction sigaction[SIGRTMAX + 1]; // signal action 信号处理函数
    
    /* 线程取消相关 */
    int cancel_state;             // 线程取消状态
    int cancel_type;              // 线程取消类型
    int cancel_requested;         // 线程取消请求标志
    int exit_status;              // 线程退出状态
    uint64 join_futex_addr;       // 用于pthread_join的futex地址
} thread_t;

// 添加线程取消相关的常量定义
#define PTHREAD_CANCEL_ENABLE     0
#define PTHREAD_CANCEL_DISABLE    1
#define PTHREAD_CANCEL_DEFERRED   0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1
#define PTHREAD_CANCELED          (-1)

extern thread_t thread_pools[THREAD_NUM];
extern struct list free_thread; //< 线程链表

void thread_init();
thread_t *alloc_thread();

// 线程取消相关函数
int thread_set_cancel_state(thread_t *t, int state);
int thread_set_cancel_type(thread_t *t, int type);
int thread_should_cancel(thread_t *t);
int thread_check_cancellation(thread_t *t);

#endif /* __THREAD_H__ */