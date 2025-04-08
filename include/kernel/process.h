#ifndef __PROCESS_H__
#define __PROCESS_H__

#include "types.h"
#include "spinlock.h"

#define NPROC (16)

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };


#if defined RISCV
typedef struct context { //riscv 14个
	uint64 ra;
	uint64 sp;

	// callee-saved
	uint64 s0;
	uint64 s1;
	uint64 s2;
	uint64 s3;
	uint64 s4;
	uint64 s5;
	uint64 s6;
	uint64 s7;
	uint64 s8;
	uint64 s9;
	uint64 s10;
	uint64 s11;
} context_t;
#else
typedef struct context //loongarch 12个
{
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 fp;
} context_t;
#endif

// Per-process state
typedef struct proc {
	spinlock_t lock;		///< 自旋锁限制修改
	void *chan;				///< 如果 non-zero，sleeping on chan

	enum procstate state; // Process state
	int pid; // Process ID
	uint64 ustack; // Virtual address of user stack 现在是物理地址
	uint64 kstack; // Virtual address of kernel stack
	struct trapframe *trapframe; // data page for trampoline.S
	struct context context; // swtch() here to run process
} proc_t;



struct proc *curr_proc();
void proc_init();
void scheduler() __attribute__((noreturn));
struct proc *allocproc();
void proc_mapstacks(pgtbl_t pagetable);
void            sleep_on_chan(void*, struct spinlock*);
void            wakeup(void*);
#endif // PROC_H