#include "thread.h"
#include "pmem.h"
#include "vmem.h"
#include "printf.h"
#include "syscall.h"
#include "list.h"
#include "string.h"

int nexttid = 1;

thread_t thread_pools[THREAD_NUM];
struct list free_thread;  //< 线程链表

/**
 * @brief 初始化线程池
 * 
 * 主要是把所有线程的状态设置为未使用，并将它们添加到free_thread链表中。
 */
void 
thread_init(void) 
{
    list_init(&free_thread);
    for (int i = 0; i < THREAD_NUM; i++) 
    {
        initlock(&thread_pools[i].lock, "threadlock"); ///< 初始化线程锁
        thread_pools[i].state = t_UNUSED;
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
    if (t->trapframe == NULL) {
        release(&t->lock);
        panic("alloc_thread: kalloc failed for trapframe");
    }
    t->tid = nexttid++;
    t->state = t_USED;
    t->chan = NULL;
    t->awakeTime = 0;
    t->sz = 0;
    t->clear_child_tid = 0;
    release(&t->lock); ///< 释放线程锁
    return t;
}