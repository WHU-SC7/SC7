
#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include "types.h"
#include <stdbool.h>

typedef struct spinlock 
{
  uint locked;       ///< Is the lock held?

  /* 用来调试 */
  char *name;        ///< Name of lock.
  struct cpu *cpu;   ///< The cpu holding the lock.
} spinlock_t;

void push_off();
void pop_off();

void initlock(struct spinlock*, char*);

void acquire(struct spinlock*);

void release(struct spinlock*);

int holding(struct spinlock*);

#endif