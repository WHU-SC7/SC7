#include "vmem.h"
#include "pmem.h"
#include "print.h"
#include "hsai_mem.h"
#include "string.h"
#include "process.h"
#if defined RISCV
#include "riscv.h"
#include "riscv_memlayout.h"
#else
#include "loongarch.h"
#endif

pgtbl_t kernel_pagetable;
bool debug_trace_walk = false;

extern char KERNEL_DATA;
extern char KERNEL_TEXT;
extern char USER_END;
extern char trampoline;
void vmem_init()
{
    kernel_pagetable = pmem_alloc_pages(1); ///< 分配一个页,存放内核页表 分配时已清空页面
    LOG("kernel_pagetable address: %p\n", kernel_pagetable);
    // RISCV需要将内核映射到外部，LA由于映射窗口，不需要映射
#if defined RISCV
    /*UART映射*/
    mappages(kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);

    /*PLIC映射*/
    mappages(kernel_pagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

    /*kernel代码区映射 映射为可读可执行*/
    mappages(kernel_pagetable, KERNEL_BASE, KERNEL_BASE, (uint64)&KERNEL_TEXT - KERNEL_BASE, PTE_R | PTE_X);
    LOG("[MAP] KERNEL_BASE:%p ->  KERNEL_TEXT:%p  len: 0x%x\n", KERNEL_BASE, (uint64)&KERNEL_TEXT, (uint64)&KERNEL_TEXT - KERNEL_BASE);

    /*kernel数据区映射，映射为可读可写*/
    mappages(kernel_pagetable, (uint64)&KERNEL_TEXT, (uint64)&KERNEL_TEXT, (uint64)&USER_END - (uint64)&KERNEL_TEXT, PTE_R | PTE_W);
    LOG("[MAP] KERNEL_TEXT :%p ->  USER_END:%p  len: 0x%x\n", (uint64)&KERNEL_TEXT, (uint64)&USER_END, (uint64)&USER_END - (uint64)&KERNEL_TEXT);

#endif
    /*trampoline映射为虚拟地址最高位的一个页面*/
    mappages(kernel_pagetable, TRAMPOLINE, (uint64)&trampoline, PGSIZE, PTE_TRAMPOLINE);
    LOG("[MAP] TRAMPOLINE :%p ->  trampoline:%p  len: 0x%x\n", TRAMPOLINE, (uint64)&trampoline, PGSIZE);

    proc_mapstacks(kernel_pagetable);

    hsai_config_pagetable(kernel_pagetable);
    LOG("Kernel page table configuration completed successfully\n");
}


/**
 * @brief 在页表中遍历虚拟地址对应的页表项（PTE）
 * 功能说明：
 * 1. 通过多级页表逐级解析虚拟地址va，最终返回对应的PTE。
 * 2. 若alloc=1且中间页表项不存在，函数会动态分配物理页作为下级页表。
 * 3. 若alloc=0且中间页表项缺失，函数返回NULL表示遍历失败。
 *
 * @param pt    页表基地址
 * @param va    待查询的虚拟地址
 * @param alloc 是否自动分配缺失的页表层级（1=分配，0=不分配）
 * @return      返回虚拟地址对应的页表项（PTE）指针，若失败返回NULL
 *
 */
pte_t *walk(pgtbl_t pt, uint64 va, int alloc)
{
    assert(va < MAXVA, "va out of range");
    pte_t *pte;
    if (debug_trace_walk)
        printf("[walk trace] 0x%p:", va);
    /*riscv 三级页表  loongarch 四级页表*/
    for (int level = PT_LEVEL - 1; level > 0; level--)
    {
        /*pt是页表项指针的数组,存放的是物理地址,需要to_vir*/
        pte = &pt[PX(level, va)];
        //pte = to_vir(pte);
        if (debug_trace_walk)
            printf("0x%p->", pte);
        if (*pte & PTE_V)
        {
            pt = ((pgtbl_t)(PTE2PA(*pte) | dmwin_win0)); ///< 如果页表项有效，更新pt，找下一级页表
        }
        else if (alloc) ///< 无效且允许分配，则分配一个物理页作为，将物理地址存放在页表项中
        {
            pt = (pgtbl_t)pmem_alloc_pages(1);
            if (pt == NULL)
                return NULL;
            /*写页表的时候，需要把分配的页的物理地址写入pte中*/
            //pt = to_phy(pt);
            *pte = PA2PTE(pt) | PTE_WALK;
            if (debug_trace_walk)
                printf("0x%p->", pte);

            /// @todo TLB刷新？
        }
        else
        {
            return NULL;
        }
    }
    pte = &pt[PX(0, va)];
    /*最后需要返回PTE的虚拟地址*/
    //pte = to_vir(pte);
    if (debug_trace_walk)
        printf("0x%p\n", pte);
    return pte;
}

// uint64 walkaddr(pgtbl_t pt, uint64 va)
// {
//     pte_t *pte;
//     uint64 pa;
//     if(va > MAXVA) return 0;
//     pte = walk(pagetable,va,0);
//     if(pte == NULL) return 0;
//     if((*pte&PTE_V) == 0) return 0;
//     if((*pte&PTE_U) == 0) return 0;
// }

/**
 * @brief 在pt中建立映射 [va, va+len)->[pa, pa+len)
 * @param va :  要映射到的起始虚拟地址
 * @param pa :  需要映射的起始物理地址  输入时需要对齐
 * @param len:  映射长度
 * @param perm: 权限位 loongarch权限可能超过Int范围,修改为uint64
 * @return   映射成功返回1，失败返回-1
 *
 */
int mappages(pgtbl_t pt, uint64 va, uint64 pa, uint64 len, uint64 perm)
{
    assert(va < MAXVA, "va out of range");
    assert((pa != 0) | (pa % PGSIZE != 0), "pa need be aligned");
    pte_t *pte;
    /*将要分配的虚拟地址首地址和尾地址对齐*/
    uint64 begin = PGROUNDDOWN(va);
    uint64 end = PGROUNDDOWN(va + len - 1);
    uint64 current = begin;
    for (;;)
    {
        if ((pte = walk(pt, current, true)) == NULL)
        {
            assert(0, "pte allock error");
            return -1;
        }
        if (*pte & PTE_V)
        {
            assert(0, "pte remap!");
            return -1;
        }
        /*给页表项写上控制位，置有效*/
        *pte = PA2PTE(pa) | perm | PTE_V;

        /// @todo : 刷新TLB
        if (current == end)
            break;
        current += PGSIZE;
        pa += PGSIZE;
    }
    return 1;
}
