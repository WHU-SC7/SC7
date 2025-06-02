#ifndef __CPU_H__
#define __CPU_H__

typedef struct proc proc_t; // 前置声明，保证proc_t已知

#include "process.h"
#include "context.h"

#define NCPU 1

typedef struct cpu 
{
    proc_t* proc;       ///< 当前运行的proc
    context_t context;  ///< 调度器的上下文(proc_schedule中)
    int noff;           ///< push_off的深度
    int intena;         ///< push_off之前中断是打开的吗
} cpu_t;

cpu_t*  mycpu(void);
proc_t* myproc(void);

#endif  ///< CPU_H