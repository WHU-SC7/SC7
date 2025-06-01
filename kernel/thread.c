#include "thread.h"
#include "pmem.h"
#include "vmem.h"
#include "printf.h"
#include "syscall.h"
#include "list.h"

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
 * 
 * @return thread_t* 指向新分配的线程的指针
 */
thread_t *
alloc_thread(void) 
{
    if (list_empty(&free_thread)) 
        panic("No free thread available");
    
    if ((list_entry(list_begin(&free_thread), thread_t, elem)->trapframe 
        = (struct trapframe *)kalloc()) == NULL)
        panic("Can not allocate trapframe for thread");
    
    return list_entry(list_pop_front(&free_thread), thread_t, elem);
}