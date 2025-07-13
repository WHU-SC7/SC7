// 目前支持riscv架构，初始化plic后能处理磁盘的外部中断
// loongarch使用pcie而不是mmio,写法可能很不一样
// 因为是架构相关，所以在hsai目录
#include "types.h"
#include "riscv_memlayout.h"
// #include "hsai.h"
#include "print.h"


void plicinit(void)
{
    // set desired IRQ priorities non-zero (otherwise disabled).
    // 先禁用uart的中断源，似乎是没有设置优先级，uart会抢占替换掉virtio的中断
    *(uint32 *)(PLIC + UART0_IRQ * 4) = 1;
    *(uint32 *)(PLIC + VIRTIO0_IRQ * 4) = 1;
}

void plicinithart(void)
{
    int hart = hsai_get_cpuid();
    // LOG_LEVEL(LOG_INFO,"hart: %d",hart);

    // set uart's enable bit for this hart's S-mode.
    *(uint32 *)PLIC_SENABLE(hart) = (1 << UART0_IRQ) | (1 << VIRTIO0_IRQ);

    // set this hart's S-mode priority threshold to 0.
    *(uint32 *)PLIC_SPRIORITY(hart) = 0;
}

int plic_claim(void)
{
    int hart = hsai_get_cpuid();
    int irq = *(uint32 *)PLIC_SCLAIM(hart);
    return irq;
}
void plic_complete(int irq)
{
    int hart = hsai_get_cpuid();
    DEBUG_LOG_LEVEL(LOG_INFO,"hart: %d\n",hart);
    *(uint32 *)PLIC_SCLAIM(hart) = irq;
}