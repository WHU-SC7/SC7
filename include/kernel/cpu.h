#ifndef __CPU_H__
#define __CPU_H__

typedef struct proc proc_t; // 前置声明，保证proc_t已知

#include "process.h"
#include "context.h"

#define NCPU 8

// CPU亲和性相关宏定义
typedef uint64 cpu_set_t;
#define CPU_SETSIZE NCPU
#define CPU_ZERO(cpusetp) do { *(cpusetp) = 0; } while(0)
#define CPU_SET(cpu, cpusetp) do { *(cpusetp) |= (1ULL << (cpu)); } while(0)
#define CPU_CLR(cpu, cpusetp) do { *(cpusetp) &= ~(1ULL << (cpu)); } while(0)
#define CPU_ISSET(cpu, cpusetp) (*(cpusetp) & (1ULL << (cpu)))

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