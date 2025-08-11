#ifndef __TIMER_H__
#define __TIMER_H__

#include "types.h"
#include "spinlock.h"
#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

// 定时器类型定义
#define TIMER_ONESHOT 0  // 单次定时器
#define TIMER_PERIODIC 1 // 周期定时器

// riscv time base 0x989680

#define FREQUENCY 10000000L // qemu时钟频率12500000
#define TIME2NS(time) (time * 1000 * 1000 * 1000 / FREQUENCY)
#define TIMESEPC2NS(sepc) (sepc.tv_nsec + sepc.tv_sec * 1000 * 1000 * 1000)
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID 3
#define CLOCK_MONOTONIC_RAW 4
#define CLOCK_REALTIME_COARSE 5
#define CLOCK_MONOTONIC_COARSE 6
#define CLOCK_BOOTTIME 7

#define MAX_CLOCKS 16

// clock_nanosleep flags
#define TIMER_ABSTIME 1

#define CLK_FREQ 10000000ul

// getrusage constants
#define RUSAGE_SELF 0
#define RUSAGE_CHILDREN -1
#define INTERVAL (CLK_FREQ / 2) ///< 0.1s

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

struct itimerval
{
    struct timeval it_interval; // 间隔时间
    struct timeval it_value;    // 当前定时器值
};

struct rusage
{
    struct timeval ru_utime; /* user CPU time used */
    struct timeval ru_stime; /* system CPU time used */
    long ru_maxrss;          /* maximum resident set size */
    long ru_ixrss;           /* integral shared memory size */
    long ru_idrss;           /* integral unshared data size */
    long ru_isrss;           /* integral unshared stack size */
    long ru_minflt;          /* page reclaims (soft page faults) */
    long ru_majflt;          /* page faults (hard page faults) */
    long ru_nswap;           /* swaps */
    long ru_inblock;         /* block input operations */
    long ru_oublock;         /* block output operations */
    long ru_msgsnd;          /* IPC messages sent */
    long ru_msgrcv;          /* IPC messages received */
    long ru_nsignals;        /* signals received */
    long ru_nvcsw;           /* voluntary context switches */
    long ru_nivcsw;          /* involuntary context switches */
};

#define _STRUCT_TIMESPEC ///< struct timespec系统又定义了
typedef struct timespec
{
    int64 tv_sec;  /* Seconds */
    int64 tv_nsec; /* Nanoseconds */
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
