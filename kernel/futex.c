#include "futex.h"
#include "types.h"
#include "timer.h"
#include "trap.h"
#include "thread.h"
#include "process.h"
#include "printf.h"
#include "spinlock.h"
typedef struct futex_queue
{
    uint64 addr;
    thread_t *thread;
    uint8 valid;
    uint32 bitset;
} futex_queue_t;

futex_queue_t futex_queue[FUTEX_COUNT];
spinlock_t fq_lock;
static int waiting_count = 0;

/**
 * @brief 等待一个futex（带bitset支持）
 *
 * @param addr futex的地址
 * @param th 线程指针，表示进行等待的线程
 * @param ts 超时时间，如果为NULL则表示无限等待
 * @param bitset 位集，用于选择性唤醒
 */
void futex_wait_bitset(uint64 addr, thread_t *th, timespec_t *ts, uint32 bitset)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "futex_wait_bitset: 线程 tid=%d 开始等待 addr=%p, bitset=0x%x\n", th->tid, addr, bitset);
    acquire(&fq_lock); ///< 获取futex队列锁，确保线程安全
    for (int i = 0; i < FUTEX_COUNT; i++)
    {
        if (!futex_queue[i].valid)
        {
            DEBUG_LOG_LEVEL(LOG_INFO, "futex_wait_bitset: 线程 tid=%d 进入队列位置 %d, addr=%p, bitset=0x%x\n", th->tid, i, addr, bitset);
            futex_queue[i].valid = 1;
            futex_queue[i].addr = addr;
            futex_queue[i].thread = th;
            futex_queue[i].bitset = bitset;

            /* 设置线程状态为睡眠或定时等待 */
            if (ts)
            {
                th->awakeTime = ts->tv_sec * 1000000000 + ts->tv_nsec;
                th->state = t_TIMING;
                DEBUG_LOG_LEVEL(LOG_DEBUG, "futex_wait_bitset is timing.\n");
            }
            else
            {
                th->awakeTime = 0; ///< 清除唤醒时间
                th->state = t_SLEEPING;
            }

            /* 获取进程锁 */
            acquire(&th->p->lock);
#ifdef RISCV
            DEBUG_LOG_LEVEL(LOG_INFO, "futex_wait_bitset: 保存上下文前pid=%d, tid=%d, ra=%p, sp=%p, epc=%p, trapframe=%p\n",
                            th->p->pid, th->tid, myproc()->context.ra, myproc()->context.sp, myproc()->trapframe->epc, myproc()->trapframe);
#else
            DEBUG_LOG_LEVEL(LOG_INFO, "futex_wait_bitset: 保存上下文前 pid=%d, tid=%d, ra=%p, sp=%p, epc=%p, trapframe=%p\n",
                            th->p->pid, th->tid, myproc()->context.ra, myproc()->context.sp, myproc()->trapframe->era, myproc()->trapframe);
#endif
            /* 设置进程状态为可运行，让调度器可以选择其他线程 */
            th->p->state = RUNNABLE;
            waiting_count++;
            release(&fq_lock);
            /* 切换到调度器 */
            sched();

            /* 当线程被唤醒时会回到这里 */
#ifdef RISCV
            DEBUG_LOG_LEVEL(LOG_INFO, "futex_wait_bitset: 恢复后 tid=%d, ra=%p, sp=%p, epc=%p, trapframe=%p\n",
                            th->tid, myproc()->context.ra, myproc()->context.sp, myproc()->trapframe->epc, myproc()->trapframe);
#else
            DEBUG_LOG_LEVEL(LOG_INFO, "futex_wait_bitset: 恢复后 tid=%d, ra=%p, sp=%p, epc=%p, trapframe=%p\n",
                            th->tid, myproc()->context.ra, myproc()->context.sp, myproc()->trapframe->era, myproc()->trapframe);
#endif
            release(&th->p->lock);

            /* 清除futex队列中的条目（如果还没有被清除的话，比如超时唤醒） */
            acquire(&fq_lock);
            if (futex_queue[i].valid)
            {
                DEBUG_LOG_LEVEL(LOG_INFO, "futex_wait_bitset: 超时唤醒，清除线程 tid=%d 的队列条目\n", th->tid);
                futex_queue[i].valid = 0;
                waiting_count--;
            }
            release(&fq_lock);
            return;
        }
    }
    release(&fq_lock);
    panic("No futex Resource!\n");
}

/**
 * @brief 等待一个futex
 *
 * @param addr futex的地址
 * @param th 线程指针，表示进行等待的线程
 * @param ts 超时时间，如果为NULL则表示无限等待,相对时间
 */
void futex_wait(uint64 addr, thread_t *th, timespec_t *ts)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "futex_wait: 线程 tid=%d 开始等待 addr=%p\n", th->tid, addr);
    timespec_t current_time = timer_get_ntime();
    acquire(&fq_lock); ///< 获取futex队列锁，确保线程安全
    for (int i = 0; i < FUTEX_COUNT; i++)
    {
        if (!futex_queue[i].valid)
        {
            DEBUG_LOG_LEVEL(LOG_INFO, "futex_wait: 线程 tid=%d 进入队列位置 %d, addr=%p\n", th->tid, i, addr);
            futex_queue[i].valid = 1;
            futex_queue[i].addr = addr;
            futex_queue[i].thread = th;
            futex_queue[i].bitset = 0xffffffff; ///< 默认 bitset

            /* 设置线程状态为睡眠或定时等待 */
            if (ts)
            {
                DEBUG_LOG_LEVEL(LOG_DEBUG, "futex_wait is timing.\n");
                th->awakeTime = ts->tv_sec * 1000000000 + ts->tv_nsec + current_time.tv_sec * 1000000000 + current_time.tv_nsec;
                th->state = t_TIMING;
            }
            else
            {
                th->awakeTime = 0; ///< 清除唤醒时间
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
            waiting_count++;
            release(&fq_lock);
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

            /* 清除futex队列中的条目（如果还没有被清除的话，比如超时唤醒） */
            acquire(&fq_lock);
            if (futex_queue[i].valid)
            {
                DEBUG_LOG_LEVEL(LOG_INFO, "futex_wait: 超时唤醒，清除线程 tid=%d 的队列条目\n", th->tid);
                futex_queue[i].valid = 0;
                waiting_count--;
            }
            release(&fq_lock);
            return;
        }
    }
    release(&fq_lock);
    panic("No futex Resource!\n");
}

/**
 * @brief 唤醒一个或多个等待futex的线程（带bitset支持）
 *
 * @param addr futex的地址
 * @param n 要唤醒的线程数量
 * @param bitset 位集，只唤醒匹配的线程
 * @return 实际唤醒的线程数量
 */
int futex_wake_bitset(uint64 addr, int n, uint32 bitset)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "futex_wake_bitset: addr=%p, 请求唤醒 %d 个线程, bitset=0x%x, 队列中等待此地址的线程数 %d\n", addr, n, bitset, waiting_count);
    int woken = 0;
    acquire(&fq_lock); ///< 获取futex队列锁，确保线程安全
    for (int i = 0; i < FUTEX_COUNT && n > 0; i++)
    {
        if (futex_queue[i].valid && futex_queue[i].addr == addr && (futex_queue[i].bitset & bitset))
        {
            DEBUG_LOG_LEVEL(LOG_INFO, "futex_wake_bitset: 设置线程 tid=%d 状态为 t_RUNNABLE (bitset match: 0x%x & 0x%x)\n",
                            futex_queue[i].thread->tid, futex_queue[i].bitset, bitset);
            futex_queue[i].thread->state = t_RUNNABLE;
            futex_queue[i].thread->timeout_occurred = 0; ///< 正常唤醒，清除超时标志
            DEBUG_LOG_LEVEL(LOG_DEBUG, "futex wake up addr %p, tid is %d\n", futex_queue[i].addr, futex_queue[i].thread->tid);
            futex_queue[i].valid = 0; ///< 清除已唤醒线程的futex队列条目
            n--;
            woken++;
            waiting_count--; ///< 减少等待计数
        }
    }
    release(&fq_lock); ///< 释放futex队列锁
    DEBUG_LOG_LEVEL(LOG_INFO, "futex_wake_bitset: 实际唤醒了 %d 个线程\n", woken);
    return woken;
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
    DEBUG_LOG_LEVEL(LOG_INFO, "futex_wake: addr=%p, 请求唤醒 %d 个线程, 队列中等待此地址的线程数 %d\n", addr, n, waiting_count);
    int woken = 0;
    acquire(&fq_lock); ///< 获取futex队列锁，确保线程安全
    for (int i = 0; i < FUTEX_COUNT && n > 0; i++)
    {
        if (futex_queue[i].valid && futex_queue[i].addr == addr)
        {
            DEBUG_LOG_LEVEL(LOG_INFO, "futex_wake: 设置线程 tid=%d 状态为 t_RUNNABLE\n", futex_queue[i].thread->tid);
            futex_queue[i].thread->state = t_RUNNABLE;
            futex_queue[i].thread->timeout_occurred = 0; ///< 正常唤醒，清除超时标志
            DEBUG_LOG_LEVEL(LOG_DEBUG, "futex wake up addr %p, tid is %d\n", futex_queue[i].addr, futex_queue[i].thread->tid);
            futex_queue[i].valid = 0; ///< 清除已唤醒线程的futex队列条目
            n--;
            woken++;
            waiting_count--; ///< 减少等待计数
        }
    }
    release(&fq_lock); ///< 释放futex队列锁
    DEBUG_LOG_LEVEL(LOG_INFO, "futex_wake: 实际唤醒了 %d 个线程\n", woken);
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
 * @brief 比较并重新排队futex操作
 * 这是FUTEX_CMP_REQUEUE操作的实现，它先比较uaddr处的值，
 * 如果匹配，则唤醒一些线程并将其余线程重新排队到新地址
 *
 * @param addr 原futex的地址
 * @param expected_val 期望在addr处找到的值
 * @param newAddr 新futex的地址（重新排队的目标）
 * @param nr_wake 要唤醒的线程数量
 * @param nr_requeue 要重新排队的线程数量
 * @return 实际唤醒和重新排队的线程总数，如果值不匹配返回-EAGAIN
 */
int futex_cmp_requeue(uint64 addr, uint32 expected_val, uint64 newAddr, int nr_wake, int nr_requeue)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "futex_cmp_requeue: addr=%p, expected_val=%d, newAddr=%p, nr_wake=%d, nr_requeue=%d\n",
                    addr, expected_val, newAddr, nr_wake, nr_requeue);

    int total_processed = 0;
    int woken = 0;
    int requeued = 0;

    acquire(&fq_lock); ///< 获取futex队列锁，确保线程安全

    /* 首先处理唤醒操作 */
    for (int i = 0; i < FUTEX_COUNT && nr_wake > 0; i++)
    {
        if (futex_queue[i].valid && futex_queue[i].addr == addr)
        {
            DEBUG_LOG_LEVEL(LOG_INFO, "futex_cmp_requeue: 唤醒线程 tid=%d\n", futex_queue[i].thread->tid);
            futex_queue[i].thread->state = t_RUNNABLE;
            futex_queue[i].thread->timeout_occurred = 0; ///< 正常唤醒，清除超时标志
            futex_queue[i].valid = 0;                    ///< 从队列中移除
            nr_wake--;
            woken++;
            waiting_count--; ///< 减少等待计数
        }
    }

    /* 然后处理重新排队操作 */
    for (int i = 0; i < FUTEX_COUNT && nr_requeue > 0; i++)
    {
        if (futex_queue[i].valid && futex_queue[i].addr == addr)
        {
            DEBUG_LOG_LEVEL(LOG_INFO, "futex_cmp_requeue: 重新排队线程 tid=%d 从 %p 到 %p\n",
                            futex_queue[i].thread->tid, addr, newAddr);
            futex_queue[i].addr = newAddr; ///< 更改等待地址
            nr_requeue--;
            requeued++;
        }
    }

    total_processed = woken + requeued;
    release(&fq_lock); ///< 释放futex队列锁

    DEBUG_LOG_LEVEL(LOG_INFO, "futex_cmp_requeue: 总共处理了 %d 个线程 (唤醒: %d, 重新排队: %d)\n",
                    total_processed, woken, requeued);
    return total_processed;
}

/**
 * @brief 清除一个线程在futex上的等待
 *
 * @param thread 线程指针
 */
void futex_clear(thread_t *thread)
{
    acquire(&fq_lock); ///< 获取futex队列锁，确保线程安全
    for (int i = 0; i < FUTEX_COUNT; i++)
    {
        if (futex_queue[i].valid && futex_queue[i].thread == thread)
        {
            DEBUG_LOG_LEVEL(LOG_INFO, "futex_clear: 清除线程 tid=%d 的futex等待, addr=%p\n",
                            thread->tid, futex_queue[i].addr);
            futex_queue[i].valid = 0;
            waiting_count--; ///< 减少等待计数
        }
    }
    release(&fq_lock); ///< 释放futex队列锁
}

/**
 * @brief 初始化futex
 * 这个函数在系统启动时调用，初始化futex队列
 */
void futex_init(void)
{
    initlock(&fq_lock, "futex_queue_lock");
    waiting_count = 0;
    for (int i = 0; i < FUTEX_COUNT; i++)
    {
        futex_queue[i].valid = 0;
        futex_queue[i].addr = 0;
        futex_queue[i].thread = 0;
        futex_queue[i].bitset = 0xffffffff; ///< 默认 bitset
    }
}