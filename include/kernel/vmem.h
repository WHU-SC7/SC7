#ifndef __VMEM_H__
#define __VMEM_H__

#include "types.h"
#include <stdbool.h>
#include <stdint.h>

void vmem_init();
static inline pte_t *to_vir(pte_t *pte);
static inline pte_t *to_phy(pte_t *pte);

pte_t *walk(pgtbl_t pt, uint64 va, int alloc);
int mappages(pgtbl_t pt, uint64 va, uint64 pa, uint64 len, uint64 perm);
uint64 walkaddr(pgtbl_t pt, uint64 va);
void vmunmap(pgtbl_t pt, uint64 va, uint64 npages, int do_free);
uint64 uvmalloc(pgtbl_t pt, uint64 oldsz,uint64 newsz,int perm);
uint64 uvmdealloc(pgtbl_t pt, uint64 oldsz, uint64 newsz);

pgtbl_t uvmcreate();
void uvminit(pgtbl_t pt, uchar *src, uint sz);
int uvmcopy(pgtbl_t old, pgtbl_t new, uint64 sz);
void uvmfree(pgtbl_t pagetable, uint64 start, uint64 sz);
uint64 uvm_grow(pgtbl_t pagetable, uint64 oldsz, uint64 newsz, int xperm);
int fetchaddr(uint64 addr, uint64 *ip);
int fetchstr(uint64 addr, char *buf, int max);
int copyin(pgtbl_t pt, char *dst, uint64 srcva, uint64 len);
int copyout(pgtbl_t pt, uint64 dstva, char *src, uint64 len);
int copyinstr(pgtbl_t pagetable, char *dst, uint64 srcva, uint64 max);

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