#include "vf2_driver.h"
#include "print.h"

/**
 * @brief 创建SD命令结构体
 * 
 * 该函数根据参数创建一个完整的SD命令结构体。
 * 根据命令类型设置相应的寄存器标志位。
 * 
 * @param index 命令索引（0-63）
 * @param resp_type 期望的响应类型
 * @param arg 命令参数
 * @param data_expected 是否期望数据传输
 * @param write_mode 是否为写操作（仅在data_expected为true时有效）
 * @return sd_command_t 配置好的命令结构体
 */
sd_command_t command_create(uint32 index, response_type_t resp_type, uint32 arg, int data_expected, int write_mode) {
    sd_command_t cmd = {0};
    cmd.index = index;
    cmd.arg = arg;
    cmd.resp_type = resp_type;
    
    // 设置基本命令标志
    cmd.reg_flags |= CMD_START_CMD | CMD_USE_HOLD_REG | CMD_WAIT_PRVDATA_COMPLETE;
    
    // 如果期望响应，设置响应相关标志
    if (resp_type != RESP_NONE) {
        cmd.reg_flags |= CMD_RESPONSE_EXPECT | CMD_CHECK_RESPONSE_CRC;
        // 对于R2响应，设置响应长度标志（按照Rust驱动的逻辑）
        if (resp_type == RESP_R2) {
            cmd.reg_flags |= CMD_RESPONSE_LENGTH;
        }
    }
    
    // 如果期望数据传输，设置数据相关标志
    if (data_expected) {
        cmd.reg_flags |= CMD_DATA_EXPECTED;
        if (write_mode) {
            cmd.reg_flags |= CMD_WRITE;
        }
    }
    
    return cmd;
}

/**
 * @brief 将命令结构体转换为寄存器值
 * 
 * 该函数将命令结构体的各个字段组合成32位寄存器值。
 * 
 * @param cmd 指向命令结构体的指针
 * @return uint32 32位寄存器值
 */
uint32 command_to_reg(sd_command_t *cmd) {
    return cmd->reg_flags | cmd->index;
}

/**
 * @brief 检查中断状态并返回错误类型
 * 
 * 该函数检查中断状态寄存器的各个位，确定具体的错误类型。
 * 按优先级顺序检查各种错误条件。
 * 
 * @param mask 中断状态寄存器的值
 * @return interrupt_type_t 检测到的中断错误类型
 */
static interrupt_type_t check_interrupt(uint32 mask) {
    // +++ 修复：参考Rust版本的Interrupt::check逻辑 +++
    // 只检查特定的错误中断，如果没有错误就返回NONE
    
    // 检查数据传输错误（优先级最高）
    if (mask & INT_DCRC) return INTERRUPT_DATA_CRC;
    if (mask & INT_DRTO) return INTERRUPT_DATA_READ_TIMEOUT;
    
    // 检查FIFO错误
    if (mask & INT_FRUN) return INTERRUPT_FIFO;
    
    // 检查硬件错误
    if (mask & INT_HLE) return INTERRUPT_HARDWARE_LOCK;
    if (mask & INT_SBE) return INTERRUPT_START_BIT_ERR;
    if (mask & INT_EBE) return INTERRUPT_END_BIT_ERR;
    
    // 检查命令响应错误（在数据传输阶段，这些错误可能不相关）
    if (mask & INT_RTO) return INTERRUPT_RESPONSE_TIMEOUT;
    if (mask & INT_RE) return INTERRUPT_RESPONSE_ERR;
    
    // 如果没有错误，返回NONE（表示正常状态）
    return INTERRUPT_NONE;
}

/**
 * @brief 发送SD命令并处理响应
 * 
 * 该函数执行完整的SD命令发送流程：
 * 1. 等待命令线和数据线空闲
 * 2. 清除中断状态
 * 3. 发送命令和参数
 * 4. 等待命令完成
 * 5. 处理响应数据
 * 6. 设置数据传输相关寄存器（如果需要）
 * 
 * @param cmd 指向要发送的命令结构体的指针
 * @param response 指向响应结构体的指针，用于存储响应数据
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
card_error_t send_command(sd_command_t *cmd, sd_response_t *response) {
    // +++ 增加状态检查 +++
    uint32 status;
    int retry = 0;
    do {
        status = read_reg(REG_STATUS);
        if (status & STATUS_DATA_BUSY) {
            // printf("[WARN] Data busy before CMD%d, retry %d\n", cmd->index, retry);
            delay_us(1000);
        }
    } while ((status & STATUS_DATA_BUSY) && ++retry < 10);
    
    if (status & STATUS_DATA_BUSY) {
        printf("[ERROR] Data busy timeout before CMD%d\n", cmd->index);
        return CARD_ERROR_TIMEOUT;
    }
    
    // 等待数据线和命令线空闲
    card_error_t err = wait_for_data_line();
    if (err != CARD_ERROR_NONE) return err;
    
    err = wait_for_cmd_line();
    if (err != CARD_ERROR_NONE) return err;
    
    // 清除中断状态寄存器（按照Rust驱动的逻辑，清除所有中断状态）
    // InterruptMask::all().bits() = 0xFFFF7FFF
    write_reg(REG_RINTSTS, 0xFFFF7FFF);
    
    // 优化：只在调试模式下打印详细信息
    #ifdef DEBUG_SDIO
    printf("[SDIO] Send command: index=%d, arg=0x%x, reg_flags=0x%x\n", cmd->index, cmd->arg, cmd->reg_flags);
    #endif
    
    // 修复：先等待HLE清除，再发送命令（避免重复发送）
    while (read_reg(REG_RINTSTS) & INT_HLE) {
        delay_us(10);
    }
    
    // 发送命令参数和命令（单次发送）
    write_reg(REG_CMDARG, cmd->arg);
    write_reg(REG_CMD, command_to_reg(cmd));
    
    // 等待命令执行完成
    err = wait_for_cmd_done();
    if (err != CARD_ERROR_NONE) {
        printf("[SDIO] wait_for_cmd_done failed!\n");
        return err;
    }
    
    // 处理命令响应
    if (cmd->resp_type != RESP_NONE) {
        uint32 mask = read_reg(REG_RINTSTS);
        
        // 优化：只在调试模式下打印详细信息
        #ifdef DEBUG_SDIO
        printf("[SDIO] Response mask: 0x%x, RTO: %d, RE: %d\n", 
               mask, (mask & INT_RTO) ? 1 : 0, (mask & INT_RE) ? 1 : 0);
        #endif
        
        // 检查响应超时错误
        if (mask & INT_RTO) {
            write_reg(REG_RINTSTS, mask);
            printf("[SDIO] CMD%d timeout! STATUS=0x%x, CLKDIV=0x%x\n", 
                   cmd->index, read_reg(REG_STATUS), read_reg(REG_CLKDIV));
            printf("[SDIO] FIFO status: cnt=%d, empty=%d, full=%d\n",
                   fifo_count(), (read_reg(REG_STATUS) & STATUS_FIFO_EMPTY) ? 1 : 0,
                   (read_reg(REG_STATUS) & STATUS_FIFO_FULL) ? 1 : 0);
            printf("[SDIO] Response timeout error\n");
            // 打印所有相关寄存器用于调试
            printf("[SDIO] CMD=0x%x, CMDARG=0x%x\n", read_reg(REG_CMD), read_reg(REG_CMDARG));
            printf("[SDIO] STATUS=0x%x, RINTSTS=0x%x\n", read_reg(REG_STATUS), read_reg(REG_RINTSTS));
            printf("[SDIO] RESP0=0x%x, RESP1=0x%x, RESP2=0x%x, RESP3=0x%x\n", 
                read_reg(REG_RESP0), read_reg(REG_RESP1), read_reg(REG_RESP2), read_reg(REG_RESP3));
            return CARD_ERROR_INTERRUPT;
        }
        
        // 检查响应错误
        if (mask & INT_RE) {
            write_reg(REG_RINTSTS, mask);
            printf("[SDIO] Response error\n");
            return CARD_ERROR_INTERRUPT;
        }
        
        // 读取响应数据（按照Rust驱动的逻辑，R2响应是长响应）
        if (cmd->resp_type == RESP_R2) {
            // R2响应包含4个32位寄存器（长响应）
            response->r136.resp0 = read_reg(REG_RESP0);
            response->r136.resp1 = read_reg(REG_RESP1);
            response->r136.resp2 = read_reg(REG_RESP2);
            response->r136.resp3 = read_reg(REG_RESP3);
            
            // 优化：只在调试模式下打印详细信息
            #ifdef DEBUG_SDIO
            printf("[SDIO] RESP0=0x%x, RESP1=0x%x, RESP2=0x%x, RESP3=0x%x\n", 
                response->r136.resp0, response->r136.resp1, response->r136.resp2, response->r136.resp3);
            #endif
        } else {
            // 其他响应类型只使用第一个响应寄存器（短响应）
            response->r48 = read_reg(REG_RESP0);
            
            // 优化：只在调试模式下打印详细信息
            #ifdef DEBUG_SDIO
            printf("[SDIO] RESP0=0x%x\n", response->r48);
            #endif
        }
    }
    
    // 如果命令期望数据传输，设置相关寄存器
    if (cmd->reg_flags & CMD_DATA_EXPECTED) {
        // 复位FIFO
        err = wait_reset(CTRL_FIFO_RESET);
        if (err != CARD_ERROR_NONE) return err;
        
        // 设置块大小和字节计数
        write_reg(REG_BLKSIZ, BLKSIZ_DEFAULT);
        write_reg(REG_BYTCNT, BLKSIZ_DEFAULT);
        
        // 优化：只在调试模式下打印详细信息
        #ifdef DEBUG_SDIO
        printf("[SDIO] Set BLKSIZ=0x%x, BYTCNT=0x%x\n", BLKSIZ_DEFAULT, BLKSIZ_DEFAULT);
        #endif
    }
    
    return CARD_ERROR_NONE;
}

/**
 * @brief 从SD卡读取数据块
 * 
 * 该函数通过FIFO从SD卡读取512字节的数据块。
 * 使用轮询方式检查中断状态，处理数据接收和错误条件。
 * 
 * @param buf 指向接收缓冲区的指针，必须至少512字节
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
card_error_t read_data(uint8 *buf) {
    size_t offset = 0;
    timer_t timer = timer_start(DATA_TMOUT_DEFAULT);
    
    while (1) {
        uint32 mask = read_reg(REG_RINTSTS);
        
        // +++ 修复：参考Rust版本的退出条件 +++
        // 只有当offset达到BLKSIZ_DEFAULT且DTO置位时才退出
        if (offset == BLKSIZ_DEFAULT && (mask & INT_DTO)) {
            break;
        }
        
        // +++ 修复：优先检查错误状态 +++
        // 优先检查数据传输错误（优先级最高）
        if (mask & (INT_DCRC | INT_DRTO | INT_HLE)) {
            printf("[DATA ERROR] Mask=0x%x, FIFO=%d\n", mask, fifo_count());
            if (mask & INT_DCRC) {
                printf("[DATA ERROR] Data CRC error\n");
            }
            if (mask & INT_DRTO) {
                printf("[DATA ERROR] Data read timeout\n");
            }
            if (mask & INT_HLE) {
                printf("[DATA ERROR] Hardware lock error\n");
            }
            return CARD_ERROR_INTERRUPT;
        }
        
        // +++ 修复：参考Rust版本的中断检查逻辑 +++
        // 只在真正的错误时才返回错误，而不是在正常状态时
        interrupt_type_t int_type = check_interrupt(mask);
        if (int_type == INTERRUPT_NONE) {
            // 这是正常状态，继续处理
        } else {
            // 这是真正的错误
            return CARD_ERROR_INTERRUPT;
        }
        
        // +++ 修复：参考Rust版本的FIFO读取逻辑 +++
        // 在rxdr或dto中断时都读取FIFO
        if (mask & INT_RXDR || mask & INT_DTO) {
            // +++ 修正FIFO读取逻辑 +++
            while (fifo_count() > 0) { // 读取所有可用数据
                buf[offset] = read_fifo(offset);
                offset++;
            }
            // 清除中断时保留DTO标志
            write_reg(REG_RINTSTS, INT_RXDR);
        }
        
        // 检查超时
        if (timer_timeout(&timer)) {
            printf("[DATA ERROR] Data transfer timeout, offset=%d\n", offset);
            return CARD_ERROR_DATA_TRANSFER_TIMEOUT;
        }
        
        // 短暂延时避免过度占用CPU
        delay_us(10);
    }
    
    // +++ 修复：参考Rust版本的寄存器清除策略 +++
    // 清除所有中断状态
    write_reg(REG_RINTSTS, read_reg(REG_RINTSTS));
    return CARD_ERROR_NONE;
}

/**
 * @brief 向SD卡写入数据块
 * 
 * 该函数通过FIFO向SD卡写入512字节的数据块。
 * 使用轮询方式检查中断状态，处理数据发送和错误条件。
 * 
 * @param buf 指向要写入的数据缓冲区的指针，必须包含512字节数据
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
card_error_t write_data(const uint8 *buf) {
    timer_t timer = timer_start(DATA_TMOUT_DEFAULT);
    
    while (1) {
        uint32 mask = read_reg(REG_RINTSTS);
        
        // +++ 修复：参考Rust版本的退出条件 +++
        // 只有当DTO置位时才退出（写入时不需要检查offset）
        if (mask & INT_DTO) {
            break;
        }
        
        // +++ 修复：参考Rust版本的中断检查逻辑 +++
        // 只在真正的错误时才返回错误，而不是在正常状态时
        interrupt_type_t int_type = check_interrupt(mask);
        if (int_type == INTERRUPT_NONE) {
            // 这是正常状态，继续处理
        } else {
            // 这是真正的错误
            return CARD_ERROR_INTERRUPT;
        }
        
        // +++ 修复：参考Rust版本的FIFO写入逻辑 +++
        // 在txdr中断时写入FIFO
        if (mask & INT_TXDR) {
            // 参考Rust版本：一次性写入所有数据
            for (size_t i = 0; i < BLKSIZ_DEFAULT; i++) {
                write_fifo(i, buf[i]);
            }
            // 清除发送数据中断
            write_reg(REG_RINTSTS, INT_TXDR);
        }
        
        // 检查超时
        if (timer_timeout(&timer)) {
            return CARD_ERROR_DATA_TRANSFER_TIMEOUT;
        }
        
        // 短暂延时避免过度占用CPU
        delay_us(10);
    }
    
    // +++ 修复：参考Rust版本的寄存器清除策略 +++
    // 清除所有中断状态
    write_reg(REG_RINTSTS, read_reg(REG_RINTSTS));
    return CARD_ERROR_NONE;
} 