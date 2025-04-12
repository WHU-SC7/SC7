
#include "types.h"
#if defined RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif
/**
 * @brief 获取对于架构的内核内存起始地址
 *
 * @return uint64
 */
uint64 hsai_get_mem_start()
{
#if defined RISCV // riscv起始地址 从ld中获取
    extern char KERNEL_DATA;
    return (uint64)&KERNEL_DATA;
#else
    extern char KERNEL_DATA;
    return (uint64)&KERNEL_DATA;
#endif
}

/**
 * @brief 传入内核页表，开启分页模式
 *
 * @param kernel_pagetable  内核页表地址
 */
void hsai_config_pagetable(pgtbl_t kernel_pagetable)
{
#if defined RISCV
    // 写入内核页表,开启sv39模式的分页
    w_satp(MAKE_SATP(kernel_pagetable));
    // flush the TLB
    sfence_vma();
#else
    w_csr_pgdl((uint64)kernel_pagetable & (~dmwin_mask));
    w_csr_pgdh((uint64)kernel_pagetable & (~dmwin_mask));
    asm volatile("invtlb  0x0,$zero,$zero");
    // 设置tlb页大小
    w_csr_stlbps(0xcU);
    // 设置asid 表项
    w_csr_asid(0x0U);
    // 设置高位tlbehi项， 虚页号相关消息
    w_csr_tlbrehi(0xcU);
    w_csr_pwcl((PTEWIDTH << 30)|(DIR2WIDTH << 25)|(DIR2BASE << 20)|(DIR1WIDTH << 15)|(DIR1BASE << 10)|(PTWIDTH << 5)|(PTBASE << 0)); //0x13e4d52c
    w_csr_pwch((DIR4WIDTH << 18)|(DIR3WIDTH << 6)|(DIR3BASE << 0)); //0x00000267


#endif
}