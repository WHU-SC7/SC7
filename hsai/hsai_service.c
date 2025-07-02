#include "types.h"
#if defined RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

/**
 * 一些不知道放在哪里的接口或服务，先放到这里
 */

int hsai_get_cpuid()
{
    return r_tp();
}