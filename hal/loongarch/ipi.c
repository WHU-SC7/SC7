#include <stdint.h>

// CSR寄存器地址定义
#define LOONGARCH_IOCSR_IPI_STATUS   0x1000
#define LOONGARCH_IOCSR_IPI_EN       0x1004
#define LOONGARCH_IOCSR_IPI_SET      0x1008
#define LOONGARCH_IOCSR_IPI_CLEAR    0x100c
#define LOONGARCH_CSR_MAIL_BUF0      0x1020
#define LOONGARCH_CSR_MAIL_BUF1      0x1028
#define LOONGARCH_CSR_MAIL_BUF2      0x1030
#define LOONGARCH_CSR_MAIL_BUF3      0x1038

// 位域定义
#define IOCSR_MBUF_SEND_CPU_SHIFT    16
#define IOCSR_MBUF_SEND_BUF_SHIFT    32
#define IOCSR_MBUF_SEND_H32_MASK     0xFFFFFFFF00000000ULL

#define LOONGARCH_IOCSR_IPI_SEND     0x1040
#define IOCSR_IPI_SEND_IP_SHIFT      0
#define IOCSR_IPI_SEND_CPU_SHIFT     16
#define IOCSR_IPI_SEND_BLOCKING      (1U << 31)

#define LOONGARCH_IOCSR_MBUF_SEND    0x1048
#define IOCSR_MBUF_SEND_BLOCKING     (1ULL << 31)
#define IOCSR_MBUF_SEND_BOX_SHIFT    2

// 假设的启动地址常量
#define KERNEL_ENTRY_PA              (0x90000000)
#define KERNEL_ENTRY_VA              (0x9000000000000000 | KERNEL_ENTRY_PA)

// CSR基础读写函数
static inline void iocsr_write_u32(uintptr_t addr, uint32_t value) {
    __asm__ volatile ("iocsrwr.w %0, %1" :: "r"(value), "r"(addr));
}

static inline uint32_t iocsr_read_u32(uintptr_t addr) {
    uint32_t value;
    __asm__ volatile ("iocsrrd.w %0, %1" : "=r"(value) : "r"(addr));
    return value;
}

static inline void iocsr_write_u64(uintptr_t addr, uint64_t value) {
    __asm__ volatile ("iocsrwr.d %0, %1" :: "r"(value), "r"(addr));
}

static inline uint64_t iocsr_read_u64(uintptr_t addr) {
    uint64_t value;
    __asm__ volatile ("iocsrrd.d %0, %1" : "=r"(value) : "r"(addr));
    return value;
}

// 邮箱辅助函数
static inline uintptr_t iocsr_mbuf_send_box_lo(uintptr_t box) {
    return box << 1;
}

static inline uintptr_t iocsr_mbuf_send_box_hi(uintptr_t box) {
    return (box << 1) + 1;
}

// 邮件发送函数
void csr_mail_send(uint64_t entry, uintptr_t cpu, uintptr_t mailbox) {
    uint64_t val;
    
    // 发送高32位
    val = IOCSR_MBUF_SEND_BLOCKING;
    val |= (uint64_t)(iocsr_mbuf_send_box_hi(mailbox) << IOCSR_MBUF_SEND_BOX_SHIFT);
    val |= (uint64_t)(cpu << IOCSR_MBUF_SEND_CPU_SHIFT);
    val |= entry & IOCSR_MBUF_SEND_H32_MASK;
    iocsr_write_u64(LOONGARCH_IOCSR_MBUF_SEND, val);
    
    // 发送低32位
    val = IOCSR_MBUF_SEND_BLOCKING;
    val |= (uint64_t)(iocsr_mbuf_send_box_lo(mailbox) << IOCSR_MBUF_SEND_BOX_SHIFT);
    val |= (uint64_t)(cpu << IOCSR_MBUF_SEND_CPU_SHIFT);
    val |= entry << IOCSR_MBUF_SEND_BUF_SHIFT;
    iocsr_write_u64(LOONGARCH_IOCSR_MBUF_SEND, val);
}

// 位操作辅助函数
static inline int get_bit(uint32_t value, int bit) {
    return (value >> bit) & 1U;
}

// IPI操作函数
void ipi_write_action(uintptr_t cpu, uint32_t action) {
    for (int i = 0; i < 32; i++) {
        if (get_bit(action, i)) {
            uint32_t val = IOCSR_IPI_SEND_BLOCKING;
            val |= (uint32_t)(cpu << IOCSR_IPI_SEND_CPU_SHIFT);
            val |= (uint32_t)i;
            iocsr_write_u32(LOONGARCH_IOCSR_IPI_SEND, val);
            
            // 等待操作完成
            while (iocsr_read_u32(LOONGARCH_IOCSR_IPI_SEND) & IOCSR_IPI_SEND_BLOCKING);
        }
    }
}

void send_ipi_single(uintptr_t cpu, uint32_t action) {
    ipi_write_action(cpu, action);
}

// 启动从核函数
void start_secondary_harts(uintptr_t num_harts) {
    // 初始化IPI状态寄存器
    iocsr_write_u32(LOONGARCH_IOCSR_IPI_CLEAR, 0xFFFFFFFF);
    iocsr_write_u32(LOONGARCH_IOCSR_IPI_EN, 0x1);
    
    // hart0唤醒其他hart
    for (uintptr_t hartid = 1; hartid < num_harts; hartid++) {
        // 1. 通过邮箱发送启动地址
        csr_mail_send(KERNEL_ENTRY_VA, hartid, 0);
        
        // 2. 发送IPI中断(向量1)
        send_ipi_single(hartid, 0x1);
        
        // 3. 等待从核启动完成(可选)
        // while (!(iocsr_read_u32(LOONGARCH_IOCSR_IPI_STATUS) & (1 << hartid)));
    }
}
