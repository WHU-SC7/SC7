#ifndef PROC_H
#define PROC_H

#include "types.h"

#define NPROC (16)

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
	enum procstate state; // Process state
	int pid; // Process ID
	uint64 ustack; // Virtual address of user stack 现在是物理地址
	uint64 kstack; // Virtual address of kernel stack
	struct trapframe *trapframe; // data page for trampoline.S
	
};



struct proc *curr_proc();
void proc_init();
// void scheduler() __attribute__((noreturn));
struct proc *allocproc();

#endif // PROC_H