#include "thread.h"
#include "pmem.h"
#include "vmem.h"
#include "printf.h"
#include "syscall.h"
#include "list.h"
#include "string.h"
#include "futex.h"
#ifdef RISCV
#include "riscv_memlayout.h"
#else
#include "loongarch.h"
#endif

thread_t thread_pools[THREAD_NUM];
struct list free_thread; //< 线程链表

/**
 * @brief 初始化线程池
 *
 * 主要是把所有线程的状态设置为未使用，并将它们添加到free_thread链表中。
 */
void thread_init(void)
{
    list_init(&free_thread);
    for (int i = 0; i < THREAD_NUM; i++)
    {
        initlock(&thread_pools[i].lock, "threadlock"); ///< 初始化线程锁
        thread_pools[i].state = t_UNUSED;
        thread_pools[i].thread_idx = i;
        list_push_back(&free_thread, &thread_pools[i].elem);
    }
}

/**
 * @brief 分配一个新的线程
 *
 * 分配一个新的线程，返回一个指向新线程的指针。
 * 如果没有可用的线程，程序将会panic。
 * 主要是分配了一个trapframe，并将其地址赋值给新线程的trapframe指针。
 * 同时对变量进行必要的初始化
 * @return thread_t* 指向新分配的线程的指针
 */
thread_t *
alloc_thread(void)
{
    if (list_empty(&free_thread))
        panic("No free thread available");

    thread_t *t = list_entry(list_pop_front(&free_thread), thread_t, elem);

    acquire(&t->lock); ///< 获取线程锁，确保线程的状态安全
    if ((t->trapframe = (struct trapframe *)kalloc()) == NULL)
        panic("Can not allocate trapframe for thread");
    if (t->trapframe == NULL)
    {
        release(&t->lock);
        panic("alloc_thread: kalloc failed for trapframe");
    }
    t->state = t_USED;
    t->chan = NULL;
    t->awakeTime = 0;
    t->timeout_occurred = 0; // 初始化超时标志
    t->sz = 0;
    t->clear_child_tid = 0;
    t->stack_vma = NULL; // 初始化栈VMA引用为NULL
    
    // 初始化线程取消相关字段
    t->cancel_state = PTHREAD_CANCEL_ENABLE;
    t->cancel_type = PTHREAD_CANCEL_DEFERRED;
    t->cancel_requested = 0;      // 初始化取消请求标志
    t->exit_status = 0;
    t->join_futex_addr = 0; // 初始化join futex地址
    
    // 初始化线程信号字段
    memset(&t->sig_pending, 0, sizeof(__sigset_t));
    memset(&t->sig_set, 0, sizeof(__sigset_t));
    
    // 初始化信号处理函数数组
    for (int i = 0; i <= SIGRTMAX; i++) {
        t->sigaction[i].__sigaction_handler.sa_handler = NULL; // 默认处理
        t->sigaction[i].sa_flags = 0;
        memset(&t->sigaction[i].sa_mask, 0, sizeof(__sigset_t));
    }
    
    // 设置一些默认的信号处理
    // SIGKILL 和 SIGSTOP 不能被修改，所以不设置
    // 其他信号使用默认处理
    
    release(&t->lock);   ///< 释放线程锁
    return t;
}

/**
 * @brief 设置线程取消状态
 * @param t 目标线程
 * @param state 新的取消状态
 * @return 之前的取消状态
 */
int thread_set_cancel_state(thread_t *t, int state)
{
    if (!t) return -1;
    
    acquire(&t->lock);
    int old_state = t->cancel_state;
    t->cancel_state = state;
    release(&t->lock);
    
    return old_state;
}

/**
 * @brief 设置线程取消类型
 * @param t 目标线程
 * @param type 新的取消类型
 * @return 之前的取消类型
 */
int thread_set_cancel_type(thread_t *t, int type)
{
    if (!t) return -1;
    
    acquire(&t->lock);
    int old_type = t->cancel_type;
    t->cancel_type = type;
    release(&t->lock);
    
    return old_type;
}

/**
 * @brief 检查线程是否应该被取消
 * @param t 目标线程
 * @return 1表示应该取消，0表示不应该取消
 */
int thread_should_cancel(thread_t *t)
{
    if (!t) return 0;
    
    acquire(&t->lock);
    int should_cancel = (t->cancel_state == PTHREAD_CANCEL_ENABLE && 
                        t->cancel_requested);
    release(&t->lock);
    
    return should_cancel;
}

/**
 * @brief 在关键点检查线程取消状态
 * 这个函数应该在系统调用、信号处理等关键点被调用
 * @param t 目标线程
 * @return 1表示线程应该退出，0表示继续执行
 */
int thread_check_cancellation(thread_t *t)
{
    if (!t) return 0;
    
    // 检查是否有取消请求
    if (thread_should_cancel(t))
    {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "thread_check_cancellation: 线程 %d 被取消\n", t->tid);
        
        // 设置线程退出状态为取消
        acquire(&t->lock);
        t->exit_status = PTHREAD_CANCELED;
        t->state = t_ZOMBIE;
        release(&t->lock);
        
        // 唤醒等待此线程的pthread_join
        if (t->join_futex_addr != 0) {
            futex_wake(t->join_futex_addr, INT_MAX);
        }
        
        return 1; // 表示线程应该退出
    }
    
    return 0; // 表示继续执行
}