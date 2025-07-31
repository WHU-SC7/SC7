#include "vf2_driver.h"

/* 全局变量用于存储复位掩码 */
static uint32 g_reset_mask = 0;

/**
 * @brief 写入SDIO控制器寄存器
 * 
 * 该函数向SDIO控制器的指定寄存器写入32位值。
 * 使用volatile关键字确保编译器不会优化掉寄存器访问。
 * 
 * @param reg 寄存器偏移地址
 * @param val 要写入的32位值
 */
void write_reg(uint32 reg, uint32 val) {
    volatile uint32 *addr = (volatile uint32 *)(SDIO_BASE + reg);
    *addr = val;
}

/**
 * @brief 读取SDIO控制器寄存器
 * 
 * 该函数从SDIO控制器的指定寄存器读取32位值。
 * 使用volatile关键字确保编译器不会优化掉寄存器访问。
 * 
 * @param reg 寄存器偏移地址
 * @return uint32 读取到的32位值
 */
uint32 read_reg(uint32 reg) {
    volatile uint32 *addr = (volatile uint32 *)(SDIO_BASE + reg);
    return *addr;
}

/**
 * @brief 向SDIO FIFO写入数据
 * 
 * 该函数向SDIO控制器的FIFO缓冲区写入一个字节。
 * FIFO地址位于SDIO基地址+0x200的位置。
 * 
 * @param offset FIFO内的偏移地址
 * @param val 要写入的字节值
 */
void write_fifo(size_t offset, uint8 val) {
    volatile uint8 *addr = (volatile uint8 *)(SDIO_BASE + 0x200 + offset);
    *addr = val;
}

/**
 * @brief 从SDIO FIFO读取数据
 * 
 * 该函数从SDIO控制器的FIFO缓冲区读取一个字节。
 * FIFO地址位于SDIO基地址+0x200的位置。
 * 
 * @param offset FIFO内的偏移地址
 * @return uint8 读取到的字节值
 */
uint8 read_fifo(size_t offset) {
    volatile uint8 *addr = (volatile uint8 *)(SDIO_BASE + 0x200 + offset);
    return *addr;
}

/**
 * @brief 获取FIFO中的数据计数
 * 
 * 该函数从状态寄存器中提取FIFO中的数据字节数。
 * FIFO计数位于状态寄存器的第17-29位。
 * 
 * @return uint32 FIFO中的数据字节数
 */
uint32 fifo_count(void) {
    uint32 status = read_reg(REG_STATUS);
    return (status >> 17) & 0x1FFF;
}

/**
 * @brief 等待条件满足或超时
 * 
 * 该函数在指定的时间内等待条件函数返回1。
 * 如果超时则返回0，否则返回1。
 * 
 * @param microseconds 等待的最大时间（微秒）
 * @param condition 条件检查函数指针
 * @return 1 如果条件满足，0 如果超时
 */
int wait_for(uint64 microseconds, int (*condition)(void)) {
    timer_t timer = timer_start(microseconds);
    while (!timer_timeout(&timer)) {
        if (condition()) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief 检查命令线是否空闲的辅助函数
 * 
 * @return 1 如果命令线空闲，0 如果命令线忙
 */
static int is_cmd_line_idle(void) {
    return (read_reg(REG_CMD) & CMD_START_CMD) == 0;
}

/**
 * @brief 等待命令线空闲
 * 
 * 该函数等待SDIO命令线变为空闲状态。
 * 通过检查命令寄存器的START_CMD位来判断命令线是否空闲。
 * 
 * @return card_error_t 成功返回CARD_ERROR_NONE，超时返回CARD_ERROR_TIMEOUT
 */
card_error_t wait_for_cmd_line(void) {
    if (!wait_for(0xFF * 1000, is_cmd_line_idle)) {  // 转换为微秒
        return CARD_ERROR_TIMEOUT;
    }
    return CARD_ERROR_NONE;
}

/**
 * @brief 检查数据线是否空闲的辅助函数
 * 
 * @return 1 如果数据线空闲，0 如果数据线忙
 */
static int is_data_line_idle(void) {
    return (read_reg(REG_STATUS) & STATUS_DATA_BUSY) == 0;
}

/**
 * @brief 等待数据线空闲
 * 
 * 该函数等待SDIO数据线变为空闲状态。
 * 通过检查状态寄存器的DATA_BUSY位来判断数据线是否空闲。
 * 
 * @return card_error_t 成功返回CARD_ERROR_NONE，超时返回CARD_ERROR_TIMEOUT
 */
card_error_t wait_for_data_line(void) {
    if (!wait_for(DATA_TMOUT_DEFAULT, is_data_line_idle)) {
        return CARD_ERROR_TIMEOUT;
    }
    return CARD_ERROR_NONE;
}

/**
 * @brief 检查命令是否完成的辅助函数
 * 
 * @return 1 如果命令完成，0 如果命令未完成
 */
static int is_cmd_done(void) {
    return (read_reg(REG_RINTSTS) & INT_CMD) != 0;
}

/**
 * @brief 等待命令完成
 * 
 * 该函数等待SDIO命令执行完成。
 * 通过检查中断状态寄存器的CMD位来判断命令是否完成。
 * 
 * @return card_error_t 成功返回CARD_ERROR_NONE，超时返回CARD_ERROR_TIMEOUT
 */
card_error_t wait_for_cmd_done(void) {
    if (!wait_for(1000 * 1000, is_cmd_done)) {  // 增加到1秒超时
        return CARD_ERROR_TIMEOUT;
    }
    return CARD_ERROR_NONE;
}

/**
 * @brief 检查复位是否完成的辅助函数
 * 
 * @return 1 如果复位完成，0 如果复位未完成
 */
static int is_reset_done(void) {
    return (read_reg(REG_CTRL) & g_reset_mask) == 0;
}

/**
 * @brief 等待复位操作完成
 * 
 * 该函数等待指定的复位位被清除，表示复位操作完成。
 * 
 * @param mask 要等待清除的复位位掩码
 * @return card_error_t 成功返回CARD_ERROR_NONE，超时返回CARD_ERROR_TIMEOUT
 */
card_error_t wait_reset(uint32 mask) {
    g_reset_mask = mask;
    if (!wait_for(10 * 1000, is_reset_done)) {  // 转换为微秒
        return CARD_ERROR_TIMEOUT;
    }
    return CARD_ERROR_NONE;
} 