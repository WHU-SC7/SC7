#ifndef __TIMER_H__
#define __TIMER_H__

#include "types.h"
#include "spinlock.h"
#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

/* 不知道为什么华科时钟频率定成这样的 */
#define FREQUENCY 10000000L // qemu时钟频率12500000
#define TIME2NS(time) (time * 1000 * 1000 * 1000 / FREQUENCY)
#define TIMESEPC2NS(sepc) (sepc.tv_nsec + sepc.tv_sec * 1000 * 1000 * 1000)

#define CLK_FREQ 10000000ul     
#define INTERVAL (CLK_FREQ / 1) ///< 0.1s

extern struct spinlock tickslock;
extern uint ticks;


typedef struct tms
{
    long tms_utime;  ///< 用户态时间
    long tms_stime;  ///< 内核态时间
    long tms_cutime; ///< 子进程用户态时间
    long tms_cstime; ///< 子进程内核态时间
} tms_t;

// 时间间隔
typedef struct timeval
{
    uint64 sec;  // 秒
    uint64 usec; // 微秒
} timeval_t;

struct itimerval {
    struct timeval it_interval; // 间隔时间
    struct timeval it_value;    // 当前定时器值
};


#define _STRUCT_TIMESPEC    ///< struct timespec系统又定义了
typedef struct timespec {
    uint64 tv_sec; /* Seconds */
    uint64 tv_nsec; /* Nanoseconds */
} timespec_t;

void timer_init(void);
#ifdef RISCV
void set_next_timeout(void);
#else
void countdown_timer_init(void);
#endif
void timer_tick();
uint64 get_times(uint64 utmsj);
timeval_t timer_get_time();
timespec_t timer_get_ntime();
#endif
