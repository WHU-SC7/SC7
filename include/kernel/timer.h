#ifndef __TIMER_H__
#define __TIMER_H__

#include "types.h"
#include "spinlock.h"
#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

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

void timer_init(void);
#ifdef RISCV
void set_next_timeout(void);
#else
void countdown_timer_init(void);
#endif
void timer_tick();
uint64 get_times(uint64 utmsj);
timeval_t timer_get_time();

#endif
