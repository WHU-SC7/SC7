#ifndef __SLEEPLOCK_H_
#define __SLEEPLOCK_H_

#include "types.h"
#include "spinlock.h" 

/** Long-term locks for processes */
struct sleeplock 
{
  uint locked;        ///< Is the lock held?
  spinlock_t lk;      ///< spinlock protecting this sleep lock
  
  /* For debugging: */
  char *name;        ///< Name of lock.
  int pid;           ///< Process holding lock
};

void            acquiresleep(struct sleeplock*);
void            releasesleep(struct sleeplock*);
int             holdingsleep(struct sleeplock*);
void            initsleeplock(struct sleeplock*, char*);
#endif ///< SLEEPLOCK_H