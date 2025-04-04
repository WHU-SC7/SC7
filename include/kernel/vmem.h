#ifndef __VMEM_H__
#define __VMEM_H__

#include "types.h"
#include <stdbool.h>
#include <stdint.h>

void vmem_init();
static inline pte_t *to_vir(pte_t *pte);
static inline pte_t *to_phy(pte_t *pte);
pte_t *vmem_walk(pgtbl_t pt, uint64 va, int alloc);
int vmem_mappages(pgtbl_t pt, uint64 va, uint64 pa, uint64 len, int perm);

/**
 * @brief  将页表项转换为物理地址
 *         loongarch要求物理地址高位为0
 * @param pte 页表项地址
 * @return pte_t*
 */
static inline pte_t *to_phy(pte_t *pte)
{
#if defined(RISCV)
    return pte;
#else
    return (pte_t *)((uintptr_t)pte & ~(0xFUL << 60));
#endif
}

/**
 * @brief  将物理地址转换为虚拟地址
 *         loongarch要求虚拟地址高位为9
 * @param pte
 * @return pte_t*
 */
static inline pte_t *to_vir(pte_t *pte)
{
#if defined(RISCV)
    return pte;
#else
    pte = to_phy(pte);
    return (pte_t *)((uintptr_t)pte | (0x9UL << 60));
#endif
}

#endif