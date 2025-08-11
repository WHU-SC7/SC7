#include "futex.h"
#include "types.h"
#include "timer.h"
#include "trap.h"
#include "thread.h"
#include "process.h"
#include "printf.h"

typedef struct futex_queue
{
    uint64 addr;
    thread_t *thread;
    uint8 valid;
} futex_queue_t;

futex_queue_t futex_queue[FUTEX_COUNT];

/**
 * @brief 等待一个futex
 *
 * @param addr futex的地址
 * @param th 线程指针，表示进行等待的线程
 * @param ts 超时时间，如果为NULL则表示无限等待
 */
void futex_wait(uint64 addr, thread_t *th, timespec_t *ts)
{
    timeval_t current_time = timer_get_time();
    for (int i = 0; i < FUTEX_COUNT; i++)
    {
        if (!futex_queue[i].valid)
        {
            futex_queue[i].valid = 1;
            futex_queue[i].addr = addr;
            futex_queue[i].thread = th;

            /* 设置线程状态为睡眠或定时等待 */
            if (ts)
            {
                th->awakeTime = ts->tv_sec * 1000000 + ts->tv_nsec / 1000 + current_time.sec * 1000000 + current_time.usec;
                th->state = t_TIMING;
            }
            else
            {
                th->awakeTime = 0; // 清除唤醒时间
                th->state = t_SLEEPING;
            }

            /* 获取进程锁 */
            acquire(&th->p->lock);
#ifdef RISCV
            DEBUG_LOG_LEVEL(LOG_INFO, "futex_wait: 保存上下文前pid=%d, tid=%d, ra=%p, sp=%p, epc=%p, trapframe=%p\n",
                            th->p->pid, th->tid, myproc()->context.ra, myproc()->context.sp, myproc()->trapframe->epc, myproc()->trapframe);
#else
            DEBUG_LOG_LEVEL(LOG_INFO, "futex_wait: 保存上下文前 pid=%d, tid=%d, ra=%p, sp=%p, epc=%p, trapframe=%p\n",
                            th->p->pid, th->tid, myproc()->context.ra, myproc()->context.sp, myproc()->trapframe->era, myproc()->trapframe);
#endif
            /* 设置进程状态为可运行，让调度器可以选择其他线程 */
            th->p->state = RUNNABLE;

            /* 切换到调度器 */
            sched();

            /* 当线程被唤醒时会回到这里 */
#ifdef RISCV
            DEBUG_LOG_LEVEL(LOG_INFO, "futex_wait: 恢复后 tid=%d, ra=%p, sp=%p, epc=%p, trapframe=%p\n",
                            th->tid, myproc()->context.ra, myproc()->context.sp, myproc()->trapframe->epc, myproc()->trapframe);
#else
            DEBUG_LOG_LEVEL(LOG_INFO, "futex_wait: 恢复后 tid=%d, ra=%p, sp=%p, epc=%p, trapframe=%p\n",
                            th->tid, myproc()->context.ra, myproc()->context.sp, myproc()->trapframe->era, myproc()->trapframe);
#endif
            release(&th->p->lock);

            /* 清除futex队列中的条目 */
            futex_queue[i].valid = 0;
            return;
        }
    }
    panic("No futex Resource!\n");
}

/**
 * @brief 唤醒一个或多个等待futex的线程
 *
 * @param addr futex的地址
 * @param n 要唤醒的线程数量
 * @return 实际唤醒的线程数量
 */
int futex_wake(uint64 addr, int n)
{
    int woken = 0;
    for (int i = 0; i < FUTEX_COUNT && n > 0; i++)
    {
        if (futex_queue[i].valid && futex_queue[i].addr == addr)
        {
            futex_queue[i].thread->state = t_RUNNABLE;
            futex_queue[i].thread->timeout_occurred = 0; // 正常唤醒，清除超时标志
            DEBUG_LOG_LEVEL(LOG_DEBUG, "futex wake up addr %p, tid is %d\n", futex_queue[i].addr, futex_queue[i].thread->tid);
            futex_queue[i].valid = 0; ///< 清除futex等待
            n--;
            woken++;
        }
    }
    return woken;
}

/**
 * @brief 将等待futex的线程重新排队到新的futex上
 *
 * @param addr 原futex的地址
 * @param n 唤醒的数量
 * @param newAddr 新futex的地址
 */
void futex_requeue(uint64 addr, int n, uint64 newAddr)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "futex_requeue: addr=%p, n=%d, newAddr=%p\n", addr, n, newAddr);
    for (int i = 0; i < FUTEX_COUNT && n; i++)
    {
        if (futex_queue[i].valid && futex_queue[i].addr == addr)
        {
            futex_queue[i].thread->state = t_RUNNABLE;
            futex_queue[i].valid = 0;
            n--;
        }
    }
    for (int i = 0; i < FUTEX_COUNT; i++)
        if (futex_queue[i].valid && futex_queue[i].addr == addr)
            futex_queue[i].addr = newAddr;
}

/**
 * @brief 清除一个线程在futex上的等待
 *
 * @param thread 线程指针
 */
void futex_clear(thread_t *thread)
{
    for (int i = 0; i < FUTEX_COUNT; i++)
        if (futex_queue[i].valid && futex_queue[i].thread == thread)
            futex_queue[i].valid = 0;
}

/**
 * @brief 初始化futex
 * 这个函数在系统启动时调用，初始化futex队列
 */
void futex_init(void)
{
    for (int i = 0; i < FUTEX_COUNT; i++)
    {
        futex_queue[i].valid = 0;
        futex_queue[i].addr = 0;
        futex_queue[i].thread = 0;
    }
}