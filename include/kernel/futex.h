#ifndef __FUTEX_H
#define __FUTEX_H

#include "timer.h"
#include "thread.h"

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_FD 2
#define FUTEX_REQUEUE 3
#define FUTEX_CMP_REQUEUE 4
#define FUTEX_WAKE_OP 5
#define FUTEX_LOCK_PI 6
#define FUTEX_UNLOCK_PI 7
#define FUTEX_TRYLOCK_PI 8
#define FUTEX_WAIT_BITSET 9
#define FUTEX_WAKE_BITSET 10
#define FUTEX_WAIT_REQUEUE_PI 11
#define FUTEX_CMP_REQUEUE_PI 12
#define FUTEX_LOCK_PI2 13

#define FUTEX_PRIVATE_FLAG 128
#define FUTEX_CLOCK_REALTIME 256
#define FUTEX_COUNT 2048

// Flags for futex_waitv
#define FUTEX_32 2
#define FUTEX_WAITV_MAX 128

/**
 * struct futex_waitv - A waiter for vectorized wait
 * @val:	Expected value at uaddr
 * @uaddr:	User address to wait on
 * @flags:	Flags for this waiter
 * @__reserved:	Reserved member to preserve data alignment. Should be 0.
 */
struct futex_waitv
{
    uint64_t val;
    uint64_t uaddr;
    uint32_t flags;
    uint32_t __reserved;
};

void futex_wait(uint64 addr, thread_t *th, timespec_t *ts);
void futex_wait_bitset(uint64 addr, thread_t *th, timespec_t *ts, uint32 bitset);
int futex_wake(uint64 addr, int n);
int futex_wake_bitset(uint64 addr, int n, uint32 bitset);
void futex_requeue(uint64 addr, int n, uint64 newAddr);
int futex_cmp_requeue(uint64 addr, uint32 expected_val, uint64 newAddr, int nr_wake, int nr_requeue);
void futex_clear(thread_t *thread);
void futex_init(void);
#endif