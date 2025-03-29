
#include "types.h"

uint64 hsai_get_mem_start() {
    #if defined RISCV //riscv起始地址 从ld中获取
        extern char end;
        return (uint64)&end;
    #else
        extern char xn6_end;
        return (uint64)&xn6_end ;
    #endif

}
