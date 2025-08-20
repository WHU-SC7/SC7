#include "vf2_driver.h"
#include "print.h"

// 全局RCA变量，存储当前选中的SD卡的相对地址
static uint16 g_rca = 0;

/**
 * @brief 复位和配置SDIO时钟
 * 
 * 该函数按照Rust驱动的逻辑实现时钟复位和配置。
 * 首先等待命令线空闲，然后设置时钟分频器，发送时钟更新命令。
 * 
 * @param ena 时钟使能位（1=使能，0=禁用）
 * @param div 时钟分频值
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
static card_error_t reset_clock(uint32 ena, uint32 div) {
    // 增加时钟切换前的稳定时间
    if (ena && div < 62) { // 加速时增加延时
        delay_us(50000); // 50ms延时
    }
    
    // 等待命令线空闲
    card_error_t err = wait_for_cmd_line();
    if (err != CARD_ERROR_NONE) return err;
    
    // 禁用时钟
    write_reg(REG_CLKENA, 0);
    
    // 设置时钟分频值
    write_reg(REG_CLKDIV, div);
    
    // 发送时钟更新命令
    sd_command_t up_clk_cmd = {0};
    up_clk_cmd.index = 0;  // 按照Rust驱动的逻辑，up_clk命令使用index=0
    up_clk_cmd.reg_flags = CMD_UPDATE_CLOCK_REG_ONLY | CMD_WAIT_PRVDATA_COMPLETE | CMD_START_CMD | CMD_USE_HOLD_REG;
    write_reg(REG_CMDARG, up_clk_cmd.arg);
    write_reg(REG_CMD, up_clk_cmd.reg_flags);
    
    // 如果不需要使能时钟，直接返回
    if (ena == 0) {
        return CARD_ERROR_NONE;
    }
    
    // 等待命令线空闲
    err = wait_for_cmd_line();
    if (err != CARD_ERROR_NONE) return err;
    
    // 再次发送时钟更新命令
    write_reg(REG_CMD, up_clk_cmd.reg_flags);
    
    // 等待命令线空闲
    err = wait_for_cmd_line();
    if (err != CARD_ERROR_NONE) return err;
    
    // 使能时钟
    write_reg(REG_CLKENA, ena);
    write_reg(REG_CMDARG, 0);
    write_reg(REG_CMD, up_clk_cmd.reg_flags);
    
    // 时钟切换后增加稳定时间
    if (ena) {
        delay_us(20000); // 20ms延时
    }
    
    printf("reset clock");
    return CARD_ERROR_NONE;
}

/**
 * @brief 检查SD卡版本和电压兼容性
 * 
 * 该函数发送SEND_IF_COND命令来检查SD卡是否支持指定的电压和模式。
 * 这是SD卡初始化过程中的重要步骤，用于确定卡的类型和兼容性。
 * 
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
static card_error_t check_version(void) {
    // 创建SEND_IF_COND命令，参数0x1AA表示支持2.7-3.6V电压和检查模式0xAA
    sd_command_t cmd = command_create(CMD_SEND_IF_COND, RESP_R7, 0x1AA, 0, 0);
    sd_response_t response;
    
    card_error_t err = send_command(&cmd, &response);
    if (err != CARD_ERROR_NONE) return err;
    
    // 解析响应，检查电压和检查模式
    uint32 cic = response.r48;
    printf("CIC response: 0x%x, voltage: 0x%x, pattern: 0x%x\n", 
           cic, (cic >> 8) & 0xFF, cic & 0xFF);
    // 检查模式应该为0xAA，电压范围应该为1（2.7-3.6V）
    // 按照Rust驱动的逻辑：voltage_accepted()返回(cic >> 8) & 0xFF，pattern()返回cic & 0xFF
    if (((cic >> 8) & 0xFF) == 1 && (cic & 0xFF) == 0xAA) {
        printf("SD version 2.0 detected\n");
        delay_us(10000);
        return CARD_ERROR_NONE;
    }
    
    return CARD_ERROR_VOLTAGE_PATTERN;
}

/**
 * @brief 检查SD卡1.8V和SDHC支持
 * 
 * 该函数通过发送应用命令和操作条件命令来检查SD卡的功能支持。
 * 包括高容量支持（SDHC）和1.8V电压切换支持。
 * 该函数会循环等待直到卡完成初始化。
 * 
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
static card_error_t check_v18_sdhc(void) {
    while (1) {
        // 发送应用命令前缀（按照Rust驱动的逻辑，使用RCA=0）
        sd_command_t app_cmd = command_create(CMD_APP_CMD, RESP_R1, 0, 0, 0);
        sd_response_t app_response;
        card_error_t err = send_command(&app_cmd, &app_response);
        if (err != CARD_ERROR_NONE) return err;
        
        // +++ 修复ACMD41参数构造，参考Rust版本 +++
        // Rust版本: u32::from(host_high_capacity_support) << 30 | u32::from(sr18) << 24 | 1 << 20
        // 参数: 0x40000000 (HCS) | 0x01000000 (S18R) | 0x00100000 (电源管理) = 0x41100000
        uint32 arg = (1 << 30) | (1 << 24) | (1 << 20); // 高容量支持 + 1.8V支持 + 电源管理支持
        
        sd_command_t op_cmd = {0};
        op_cmd.index = CMD_ACMD_SD_SEND_OP_COND;
        op_cmd.arg = arg;
        op_cmd.resp_type = RESP_R3;
        // 按照Rust驱动的标志位设置，不包含CMD_CHECK_RESPONSE_CRC
        op_cmd.reg_flags = CMD_START_CMD | CMD_USE_HOLD_REG | CMD_WAIT_PRVDATA_COMPLETE | CMD_RESPONSE_EXPECT;
        sd_response_t op_response;
        err = send_command(&op_cmd, &op_response);
        if (err != CARD_ERROR_NONE) return err;
        
        // 解析操作条件寄存器（OCR）
        uint32 ocr = op_response.r48;
        printf("OCR response: 0x%x, busy: %d, high_capacity: %d, v18: %d\n", 
               ocr, (ocr >> 31) & 1, (ocr >> 30) & 1, (ocr >> 24) & 1);
        
        // +++ 增强状态检查，参考Rust版本 +++
        if (!(ocr & 0x80000000)) { // 检查忙位，0表示卡已完成初始化
            if (ocr & 0x40000000) {
                printf("Card is high capacity\n");
            } else {
                printf("Warning: Card is not high capacity\n");
            }
            if (ocr & 0x01000000) {
                printf("Card can switch to 1.8V\n");
            } else {
                printf("Warning: Card cannot switch to 1.8V\n");
            }
            break;
        }
        
        // 卡还在初始化中，等待一段时间后重试
        delay_us(10000);
    }
    
    delay_us(10000);
    // 按照Rust驱动的逻辑，在ACMD41完成后添加额外的延时
    delay_us(10000);  // 额外的10ms延时
    
    // +++ 添加电压切换逻辑 +++
    // 重新获取OCR以检查1.8V支持
    sd_command_t app_cmd = command_create(CMD_APP_CMD, RESP_R1, 0, 0, 0);
    sd_response_t app_response;
    card_error_t err = send_command(&app_cmd, &app_response);
    if (err == CARD_ERROR_NONE) {
        uint32 arg = (1 << 30) | (1 << 24) | (1 << 20);
        sd_command_t op_cmd = {0};
        op_cmd.index = CMD_ACMD_SD_SEND_OP_COND;
        op_cmd.arg = arg;
        op_cmd.resp_type = RESP_R3;
        op_cmd.reg_flags = CMD_START_CMD | CMD_USE_HOLD_REG | CMD_WAIT_PRVDATA_COMPLETE | CMD_RESPONSE_EXPECT;
        sd_response_t op_response;
        err = send_command(&op_cmd, &op_response);
        if (err == CARD_ERROR_NONE) {
            uint32 ocr = op_response.r48;
            uint8 v18_supported = (ocr >> 24) & 1; // 明确检查v18位
            
            printf("[SDIO] Secondary OCR: 0x%x, v18: %d\n", ocr, v18_supported);
            
            if (v18_supported) {
                printf("[SDIO] Performing voltage switch to 1.8V\n");
                
                // 发送CMD11进行电压切换
                sd_command_t volt_switch = command_create(CMD_VOLTAGE_SWITCH, RESP_R1, 0, 0, 0);
                sd_response_t volt_response;
                err = send_command(&volt_switch, &volt_response);
                if (err == CARD_ERROR_NONE) {
                    printf("[SDIO] Voltage switch command successful\n");
                    
                    // 等待电压切换完成
                    delay_us(100000); // 100ms延时
                    
                    // +++ 关键修复：电压切换后总线复位 +++
                    printf("[SDIO] Resetting bus after voltage switch\n");
                    write_reg(REG_CTRL, CTRL_CONTROLLER_RESET | CTRL_FIFO_RESET | CTRL_DMA_RESET);
                    delay_us(10000);
                    err = wait_reset(CTRL_CONTROLLER_RESET | CTRL_FIFO_RESET | CTRL_DMA_RESET);
                    if (err != CARD_ERROR_NONE) {
                        printf("[SDIO] Bus reset failed: %d\n", err);
                        return err;
                    }
                    
                    // 重新初始化时钟
                    printf("[SDIO] Re-initializing clock after voltage switch\n");
                    err = reset_clock(1, 62); // 回退到初始400kHz时钟
                    if (err != CARD_ERROR_NONE) {
                        printf("[SDIO] Clock re-initialization failed: %d\n", err);
                        return err;
                    }
                    
                    // 配置1.8V模式（如果需要的话）
                    // 注意：具体的寄存器配置取决于硬件实现
                    printf("[SDIO] Voltage switch to 1.8V completed\n");
                } else {
                    printf("[SDIO] Voltage switch command failed: %d\n", err);
                }
            } else {
                printf("[SDIO] Card does not support 1.8V, using 3.3V mode\n");
                // 配置3.3V时钟模式
                // 注意：这里可以添加3.3V特定的配置
            }
        }
    }
    
    // +++ 移除过早的时钟切换，保持400kHz时钟直到CID获取完成 +++
    // 时钟切换将在CID和RCA获取成功后进行
    
    return CARD_ERROR_NONE;
}

/**
 * @brief 获取SD卡的相对地址（RCA）
 * 
 * 该函数发送SEND_RCA命令来获取SD卡的相对地址。
 * RCA是主机用来识别和选择特定SD卡的16位地址。
 * 
 * @param rca 指向RCA结构体的指针，用于存储获取到的地址
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
static card_error_t check_rca(rca_t *rca) {
    sd_command_t cmd = command_create(CMD_SEND_RCA, RESP_R6, 0, 0, 0);
    sd_response_t response;
    
    card_error_t err = send_command(&cmd, &response);
    if (err != CARD_ERROR_NONE) return err;
    
    // 从响应中提取RCA地址（位于第16-31位）
    rca->address = (response.r48 >> 16) & 0xFFFF;
    printf("RCA: 0x%04X", rca->address);
    delay_us(10000);
    return CARD_ERROR_NONE;
}

/**
 * @brief 获取SD卡的CID（卡识别寄存器）
 * 
 * 该函数发送ALL_SEND_CID命令来获取SD卡的识别信息。
 * CID包含制造商ID、产品名称、序列号等信息。
 * 
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
static card_error_t check_cid(void) {
    printf("[SDIO] Starting CMD2 (ALL_SEND_CID)\n");
    
    // 简单实现，模仿Rust版本
    sd_command_t cmd = command_create(CMD_ALL_SEND_CID, RESP_R2, 0, 0, 0);
    sd_response_t response;
    
    card_error_t err = send_command(&cmd, &response);
    if (err == CARD_ERROR_NONE) {
        // 成功获取CID
        printf("[SDIO] CMD2 successful\n");
        printf("CID: 0x%x%x%x%x", 
              response.r136.resp0, response.r136.resp1, 
              response.r136.resp2, response.r136.resp3);
        delay_us(10000);
        return CARD_ERROR_NONE;
    } else {
        printf("[SDIO] CMD2 failed, error=%d\n", err);
        return err;
    }
}

/**
 * @brief 获取SD卡的CSD（卡特定数据寄存器）
 * 
 * 该函数发送SEND_CSD命令来获取SD卡的特定数据。
 * CSD包含容量、块大小、访问时间等信息。
 * 
 * @param rca 指向RCA结构体的指针，包含卡的相对地址
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
static card_error_t check_csd(rca_t *rca) {
    sd_command_t cmd = command_create(CMD_SEND_CSD, RESP_R2, rca->address << 16, 0, 0);
    sd_response_t response;
    
    card_error_t err = send_command(&cmd, &response);
    if (err != CARD_ERROR_NONE) return err;
    
    // 输出CSD的四个32位寄存器值
    printf("CSD: 0x%x%x%x%x", 
              response.r136.resp0, response.r136.resp1, 
              response.r136.resp2, response.r136.resp3);
    delay_us(10000);
    return CARD_ERROR_NONE;
}

/**
 * @brief 选择指定的SD卡
 * 
 * 该函数发送SELECT_CARD命令来选择指定的SD卡。
 * 选择后，后续的命令将发送给该卡。
 * 
 * @param rca 指向RCA结构体的指针，包含要选择的卡的相对地址
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
static card_error_t select_card(rca_t *rca) {
    sd_command_t cmd = command_create(CMD_SELECT_CARD, RESP_R1, rca->address << 16, 0, 0);
    sd_response_t response;
    
    card_error_t err = send_command(&cmd, &response);
    if (err != CARD_ERROR_NONE) return err;
    
    printf("Card selected, status: 0x%x", response.r48);
    delay_us(10000);
    return CARD_ERROR_NONE;
}

/**
 * @brief 执行SD卡功能切换
 * 
 * 该函数发送SWITCH_FUNCTION命令来切换SD卡的功能模式。
 * 可以用于切换访问模式、驱动强度等功能。
 * 
 * @param arg 功能切换参数
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
static card_error_t function_switch(uint32 arg) {
    sd_command_t cmd = command_create(CMD_SWITCH_FUNCTION, RESP_R1, arg, 0, 0);
    sd_response_t response;
    
    card_error_t err = send_command(&cmd, &response);
    if (err != CARD_ERROR_NONE) return err;
    
    printf("Function switch completed, status: 0x%x", response.r48);
    delay_us(10000);
    return CARD_ERROR_NONE;
}

/**
 * @brief 设置SD卡总线宽度
 * 
 * 该函数通过应用命令设置SD卡的总线宽度。
 * 首先发送应用命令前缀，然后发送设置总线宽度命令。
 * 
 * @param rca 指向RCA结构体的指针，包含卡的相对地址
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
static card_error_t set_bus(rca_t *rca) {
    // 发送应用命令前缀
    sd_command_t app_cmd = command_create(CMD_APP_CMD, RESP_R1, rca->address << 16, 0, 0);
    sd_response_t app_response;
    card_error_t err = send_command(&app_cmd, &app_response);
    if (err != CARD_ERROR_NONE) return err;
    
    // 设置总线宽度为4位（参数2表示4位总线）
    sd_command_t bus_cmd = command_create(CMD_ACMD_SET_BUS, RESP_R1, 2, 0, 0);
    sd_response_t bus_response;
    err = send_command(&bus_cmd, &bus_response);
    if (err != CARD_ERROR_NONE) return err;
    
    printf("Bus width set, status: 0x%x", bus_response.r48);
    delay_us(10000);
    return CARD_ERROR_NONE;
}

/**
 * @brief 停止数据传输
 * 
 * 该函数发送STOP_TRANSMISSION命令来停止当前的数据传输。
 * 通常用于在数据传输过程中出现错误时停止传输。
 * 
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
static card_error_t stop_transmission(void) {
    sd_command_t cmd = command_create(CMD_STOP_TRANSMISSION, RESP_R1B, 0, 0, 0);
    sd_response_t response;
    
    card_error_t err = send_command(&cmd, &response);
    if (err != CARD_ERROR_NONE) return err;
    
    printf("Transmission stopped, status: 0x%x", response.r48);
    return CARD_ERROR_NONE;
}

/**
 * @brief 初始化SD卡
 * 
 * 该函数执行完整的SD卡初始化流程，包括：
 * 1. 复位SDIO控制器
 * 2. 配置时钟和超时
 * 3. 发送初始化命令序列
 * 4. 获取卡信息（CID、CSD、RCA）
 * 5. 选择卡并配置总线宽度
 * 6. 设置高速传输模式
 * 
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
card_error_t sd_init(void) {
    printf("Initializing SDIO...");
    
    // 读取硬件配置信息
    uint32 hconf = read_reg(REG_HCON);
    printf("Hardware config: 0x%x\n", hconf);
    
    // 复位SDIO控制器（控制器、FIFO、DMA）
    uint32 reset_mask = CTRL_CONTROLLER_RESET | CTRL_FIFO_RESET | CTRL_DMA_RESET;
    write_reg(REG_CTRL, reset_mask);
    card_error_t err = wait_reset(reset_mask);
    if (err != CARD_ERROR_NONE) return err;
    
    // 使能SD卡电源
    write_reg(REG_PWREN, 1);
    
    // 使用和Rust版本相同的时钟配置
    printf("[SDIO] Setting identification clock (div=62)\n");
    err = reset_clock(1, 62);
    if (err != CARD_ERROR_NONE) return err;
    
    // 设置数据传输超时时间（按照Rust驱动的设置）
    write_reg(REG_TMOUT, 0xFFFFFFFF);
    
    // 配置中断和传输模式
    write_reg(REG_RINTSTS, 0xFFFFFFFF);  // 清除所有中断状态
    write_reg(REG_INTMASK, 0);           // 禁用中断（轮询模式）
    write_reg(REG_CTYPE, 1);             // 设置卡类型
    write_reg(REG_BMOD, 1);              // 设置突发模式
    
    // +++ 启用中断功能但不使用中断处理程序（轮询模式） +++
    uint32 ctrl = read_reg(REG_CTRL);
    ctrl |= CTRL_INT_ENABLE;
    write_reg(REG_CTRL, ctrl);
    
    // 发送空闲命令，使卡进入SPI模式（按照Rust驱动的逻辑）
    sd_command_t idle_cmd = {0};
    idle_cmd.reg_flags = CMD_SEND_INITIALIZATION | CMD_START_CMD | CMD_WAIT_PRVDATA_COMPLETE | CMD_USE_HOLD_REG;
    sd_response_t idle_response;
    err = send_command(&idle_cmd, &idle_response);
    if (err != CARD_ERROR_NONE) return err;
    
    delay_us(10000);
    
    // 检查SD卡版本和电压兼容性
    err = check_version();
    if (err != CARD_ERROR_NONE) return err;
    
    // 检查1.8V和SDHC支持，等待卡完成初始化
    err = check_v18_sdhc();
    if (err != CARD_ERROR_NONE) return err;
    
    // 获取卡的识别信息（CID）- 仍在低速时钟下
    err = check_cid();
    if (err != CARD_ERROR_NONE) return err;
    
    // 获取卡的相对地址（RCA）
    rca_t rca;
    err = check_rca(&rca);
    if (err != CARD_ERROR_NONE) return err;
    g_rca = rca.address;  // 保存全局RCA
    
    // +++ 在CID和RCA获取成功后切换到高速时钟 +++
    printf("[SDIO] Switching to high speed clock after CID/RCA\n");
    err = reset_clock(1, 1); // 25MHz时钟
    if (err != CARD_ERROR_NONE) return err;
    delay_us(100000); // 100ms延时让卡适应新时钟
    
    // +++ 根据电压模式设置操作时钟 +++
    // 检查OCR中的电压支持位
    sd_command_t app_cmd = command_create(CMD_APP_CMD, RESP_R1, 0, 0, 0);
    sd_response_t app_response;
    err = send_command(&app_cmd, &app_response);
    if (err == CARD_ERROR_NONE) {
        uint32 arg = (1 << 30) | (1 << 24) | (1 << 20);
        sd_command_t op_cmd = {0};
        op_cmd.index = CMD_ACMD_SD_SEND_OP_COND;
        op_cmd.arg = arg;
        op_cmd.resp_type = RESP_R3;
        op_cmd.reg_flags = CMD_START_CMD | CMD_USE_HOLD_REG | CMD_WAIT_PRVDATA_COMPLETE | CMD_RESPONSE_EXPECT;
        sd_response_t op_response;
        err = send_command(&op_cmd, &op_response);
        if (err == CARD_ERROR_NONE) {
            uint32 ocr = op_response.r48;
            uint8 v18_supported = (ocr >> 24) & 1;
            
            if (v18_supported) {
                printf("[SDIO] Using 1.8V mode at 50MHz\n");
                // 1.8V高速模式 (50MHz)
                err = reset_clock(1, 1); // 50MHz (100MHz/2)
            } else {
                printf("[SDIO] Using 3.3V mode at 25MHz\n");
                // 3.3V标准模式 (25MHz)
                err = reset_clock(1, 3); // 25MHz (100MHz/4)
            }
            if (err != CARD_ERROR_NONE) return err;
            delay_us(5000); // 5ms等待时钟稳定
        }
    }
    
    // 获取卡的特定数据（CSD）
    err = check_csd(&rca);
    if (err != CARD_ERROR_NONE) return err;
    
    // 选择指定的卡
    err = select_card(&rca);
    if (err != CARD_ERROR_NONE) return err;
    
    // 执行功能切换（设置访问模式）
    err = function_switch(16777201);
    if (err != CARD_ERROR_NONE) return err;
    
    // 设置总线宽度为4位
    err = set_bus(&rca);
    if (err != CARD_ERROR_NONE) return err;
    
    printf("SDIO initialization successful!");
    return CARD_ERROR_NONE;
}

/**
 * @brief 从SD卡读取单个数据块
 * 
 * 该函数从SD卡的指定地址读取512字节的数据块。
 * 首先发送读取命令，然后通过FIFO读取数据。
 * 如果出现错误，会尝试停止传输。
 * 
 * @param buf 指向接收缓冲区的指针，必须至少512字节
 * @param addr SD卡中的块地址
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
card_error_t sd_read_block(uint8 *buf, uint32 addr) {
    // 创建读取单个块的命令
    sd_command_t cmd = command_create(CMD_READ_SINGLE_BLOCK, RESP_R1, addr, 1, 0);
    sd_response_t response;
    
    // 发送读取命令
    card_error_t err = send_command(&cmd, &response);
    if (err != CARD_ERROR_NONE) {
        printf("Read command failed: %d", err);
        stop_transmission();  // 尝试停止传输
        return err;
    }
    
    printf("Read command status: 0x%x", response.r48);
    
    // 读取数据块
    err = read_data(buf);
    if (err != CARD_ERROR_NONE) {
        stop_transmission();  // 尝试停止传输
        return err;
    }
    
    return CARD_ERROR_NONE;
}

/**
 * @brief 向SD卡写入单个数据块
 * 
 * 该函数向SD卡的指定地址写入512字节的数据块。
 * 首先发送写入命令，然后通过FIFO写入数据。
 * 如果出现错误，会尝试停止传输。
 * 
 * @param buf 指向要写入的数据缓冲区的指针，必须包含512字节数据
 * @param addr SD卡中的块地址
 * @return card_error_t 执行结果，成功返回CARD_ERROR_NONE
 */
card_error_t sd_write_block(const uint8 *buf, uint32 addr) {
    // 创建写入单个块的命令
    sd_command_t cmd = command_create(CMD_WRITE_SINGLE_BLOCK, RESP_R1, addr, 1, 1);
    sd_response_t response;
    
    // 发送写入命令
    card_error_t err = send_command(&cmd, &response);
    if (err != CARD_ERROR_NONE) {
        printf("Write command failed: %d", err);
        stop_transmission();  // 尝试停止传输
        return err;
    }
    
    printf("Write command status: 0x%x", response.r48);
    
    // 写入数据块
    err = write_data(buf);
    if (err != CARD_ERROR_NONE) {
        stop_transmission();  // 尝试停止传输
        return err;
    }
    
    return CARD_ERROR_NONE;
} 