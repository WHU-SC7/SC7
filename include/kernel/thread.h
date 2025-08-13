#ifndef __THREAD_H__
#define __THREAD_H__

#include "types.h"
#include "queue.h"
#include "spinlock.h"
#include "cpu.h"
#include "list.h"
#include "context.h"
#include "process.h"

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

    /* 使用下面这些变量的时候，thread的锁不需要持有 */
    uint64 kstack; //< 线程内核栈的地址,一个进程的不同线程所用的内核栈的地址应该不同
    uint64 vtf;    //< 线程的trapframe的虚拟地址
    uint64 sz;     //< 复制自进程的sz
    trapframe_t *trapframe;
    context_t context;     //< 每个进程应该有自己的context
    uint64 kstack_pa;      //< 当线程的栈和进程的栈不是一个的时候，用它保存物理地址
    struct list_elem elem; //< 用于进程的线程链表

    uint64 clear_child_tid; //< 子线程ID清除标志
    vma_t *stack_vma;       //< 线程栈VMA的引用，用于退出时减少引用计数

    // TODO: signal
} thread_t;

extern thread_t thread_pools[THREAD_NUM];
extern struct list free_thread; //< 线程链表

void thread_init();
thread_t *alloc_thread();

#endif /* __THREAD_H__ */