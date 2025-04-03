#ifndef __VMEM_H__
#define __VMEM_H__

#include "types.h"
#include <stdbool.h>
#include <stdint.h>


void vmem_init();
static inline pte_t *to_vir(pte_t *pte); 
static inline pte_t *to_phy(pte_t *pte);  
pte_t* vmem_walk(pgtbl_t pt, uint64 va, int alloc);
int vmem_mappages(pgtbl_t pt, uint64 va, uint64 pa,uint64 len,int perm);


static inline pte_t *to_phy(pte_t *pte) {
    #if defined(RISCV)
        return pte;  // RISC-V物理地址=虚拟地址
    #else
        // 转换为uintptr_t进行位操作后再转回指针
        return (pte_t *)((uintptr_t)pte & ~(0xFUL << 60));
    #endif
}

static inline pte_t *to_vir(pte_t *pte) {
    #if defined(RISCV)
        return pte;  // RISC-V物理地址=虚拟地址
    #else
        // 转换为uintptr_t进行位操作后再转回指针
        return (pte_t *)((uintptr_t)pte | (0x9UL << 60));
    #endif
}


#endif