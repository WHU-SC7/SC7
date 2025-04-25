#include "cpu.h"
#include "spinlock.h"
#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

cpu_t cpus[NCPU];

/**
 * @brief 初始化cpu
 */
void 
cpuinit(void) 
{
    struct cpu *it;
    for (it = cpus; it < &cpus[NCPU]; it++) 
    {
      it->proc = 0;
      it->intena = 0;
      it->noff = 0;
    }
}

/**
 * @brief 返回当前CPU
 */
cpu_t* 
mycpu(void) 
{
    uint64 hartid = r_tp(); //< 解决了，使用sbi时设置tp为0就可以
    return &cpus[hartid];
}

/**
 * @brief 返回当前CPU对应的process
 */
proc_t* 
myproc(void) 
{
    push_off();
    proc_t* p = mycpu()->proc;
    pop_off();
    return p;
}
