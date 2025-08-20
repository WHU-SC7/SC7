#ifndef VF2_DRIVER_H
#define VF2_DRIVER_H

#include "types.h"
#include "vf2_param.h"
#include "riscv.h"

// 定义size_t类型
typedef unsigned long size_t;

// 基础地址定义
#define MTIME_BASE          0x0200BFF8
#define UART0_BASE          0x10000000
#define SDIO_BASE           (uint64)0x16020000

// 时间相关常量
#define TIME_BASE           4000000

// SDIO寄存器地址
#define REG_CTRL            0x000
#define REG_PWREN           0x004
#define REG_CLKDIV          0x008
#define REG_CLKENA          0x010
#define REG_TMOUT           0x014
#define REG_CTYPE           0x018
#define REG_BLKSIZ          0x01C
#define REG_BYTCNT          0x020
#define REG_INTMASK         0x024
#define REG_RINTSTS         0x044
#define REG_CMDARG          0x028
#define REG_CMD             0x02C
#define REG_RESP0           0x030
#define REG_RESP1           0x034
#define REG_RESP2           0x038
#define REG_RESP3           0x03C
#define REG_STATUS          0x048
#define REG_HCON            0x070
#define REG_BMOD            0x080

// SDIO寄存器默认值
#define BLKSIZ_DEFAULT      0x200
#define DATA_TMOUT_DEFAULT  (0xFFFFFF << 8)  // 按照Rust驱动的定义

// 控制寄存器位定义
#define CTRL_USE_INTERNAL_DMAC    (1 << 25)
#define CTRL_ENABLE_OD_PULLUP     (1 << 24)
#define CTRL_CARD_VOLTAGE_B       (0xF << 20)
#define CTRL_CARD_VOLTAGE_A       (0xF << 16)
#define CTRL_CEATA_DEVICE_INT     (1 << 11)
#define CTRL_SEND_AUTO_STOP_CCSD  (1 << 10)
#define CTRL_SEND_CCSD            (1 << 9)
#define CTRL_ABORT_READ_DATA      (1 << 8)
#define CTRL_SEND_IRQ_RESPONSE    (1 << 7)
#define CTRL_READ_WAIT            (1 << 6)
#define CTRL_DMA_ENABLE           (1 << 5)
#define CTRL_INT_ENABLE           (1 << 4)
#define CTRL_DMA_RESET            (1 << 2)
#define CTRL_FIFO_RESET           (1 << 1)
#define CTRL_CONTROLLER_RESET     (1 << 0)

// 中断掩码位定义
#define INT_SDIO_INT_MASK         (0xFFFF << 16)
#define INT_EBE                   (1 << 15)
#define INT_ACD                   (1 << 14)
#define INT_SBE                   (1 << 13)
#define INT_HLE                   (1 << 12)
#define INT_FRUN                  (1 << 11)
#define INT_HTO                   (1 << 10)
#define INT_DRTO                  (1 << 9)
#define INT_RTO                   (1 << 8)
#define INT_DCRC                  (1 << 7)
#define INT_RCRC                  (1 << 6)
#define INT_RXDR                  (1 << 5)
#define INT_TXDR                  (1 << 4)
#define INT_DTO                   (1 << 3)
#define INT_CMD                   (1 << 2)
#define INT_RE                    (1 << 1)
#define INT_CD                    (1 << 0)

// 命令寄存器位定义
#define CMD_START_CMD             (1 << 31)
#define CMD_USE_HOLD_REG          (1 << 29)
#define CMD_VOLT_SWITCH           (1 << 28)
#define CMD_BOOT_MODE             (1 << 27)
#define CMD_DISABLE_BOOT          (1 << 26)
#define CMD_EXPECT_BOOT_ACK       (1 << 25)
#define CMD_ENABLE_BOOT           (1 << 24)
#define CMD_CCS_EXPECTED          (1 << 23)
#define CMD_READ_CEATA_DEVICE     (1 << 22)
#define CMD_UPDATE_CLOCK_REG_ONLY (1 << 21)
#define CMD_CARD_NUMBER           (0x1F << 16)
#define CMD_SEND_INITIALIZATION   (1 << 15)
#define CMD_STOP_ABORT_CMD        (1 << 14)
#define CMD_WAIT_PRVDATA_COMPLETE (1 << 13)
#define CMD_SEND_AUTO_STOP        (1 << 12)
#define CMD_TRANSFER_MODE         (1 << 11)
#define CMD_WRITE                 (1 << 10)
#define CMD_DATA_EXPECTED         (1 << 9)
#define CMD_CHECK_RESPONSE_CRC    (1 << 8)
#define CMD_RESPONSE_LENGTH       (1 << 7)
#define CMD_RESPONSE_EXPECT       (1 << 6)
#define CMD_INDEX                 0x3F

// 状态寄存器位定义
#define STATUS_DMA_REQ            (1 << 31)
#define STATUS_DMA_ACK            (1 << 30)
#define STATUS_FIFO_COUNT         (0x1FFF << 17)
#define STATUS_RESPONSE_INDEX     (0x3F << 11)
#define STATUS_DATA_STATE_MC_BUSY (1 << 10)
#define STATUS_DATA_BUSY          (1 << 9)
#define STATUS_DATA_3_STATUS      (1 << 8)
#define STATUS_COMMAND_FSM_STATES (0xF << 4)
#define STATUS_FIFO_FULL          (1 << 3)
#define STATUS_FIFO_EMPTY         (1 << 2)
#define STATUS_FIFO_TX_WATERMARK  (1 << 1)
#define STATUS_FIFO_RX_WATERMARK  (1 << 0)

// SD命令定义
#define CMD_ALL_SEND_CID          2
#define CMD_SEND_RCA              3
#define CMD_SWITCH_FUNCTION       6
#define CMD_SELECT_CARD           7
#define CMD_SEND_IF_COND          8
#define CMD_SEND_CSD              9
#define CMD_STOP_TRANSMISSION     12
#define CMD_VOLTAGE_SWITCH        11
#define CMD_READ_SINGLE_BLOCK     17
#define CMD_WRITE_SINGLE_BLOCK    24
#define CMD_APP_CMD               55
#define CMD_ACMD_SD_SEND_OP_COND  41
#define CMD_ACMD_SET_BUS          6

// 响应类型
typedef enum {
    RESP_NONE = 0,
    RESP_R1 = 1,
    RESP_R1B = 10,
    RESP_R2 = 2,
    RESP_R3 = 3,
    RESP_R6 = 6,
    RESP_R7 = 7
} response_type_t;

// 错误类型
typedef enum {
    CARD_ERROR_NONE = 0,
    CARD_ERROR_INIT,
    CARD_ERROR_INTERRUPT,
    CARD_ERROR_TIMEOUT,
    CARD_ERROR_VOLTAGE_PATTERN,
    CARD_ERROR_DATA_TRANSFER_TIMEOUT,
    CARD_ERROR_BUS_BUSY
} card_error_t;

typedef enum {
    TIMEOUT_WAIT_RESET,
    TIMEOUT_WAIT_CMD_LINE,
    TIMEOUT_WAIT_CMD_DONE,
    TIMEOUT_WAIT_DATA_LINE,
    TIMEOUT_FIFO_STATUS
} timeout_type_t;

typedef enum {
    INTERRUPT_NONE,
    INTERRUPT_RESPONSE_TIMEOUT,
    INTERRUPT_RESPONSE_ERR,
    INTERRUPT_END_BIT_ERR,
    INTERRUPT_START_BIT_ERR,
    INTERRUPT_HARDWARE_LOCK,
    INTERRUPT_FIFO,
    INTERRUPT_DATA_READ_TIMEOUT,
    INTERRUPT_DATA_CRC
} interrupt_type_t;

// 命令结构体
typedef struct {
    uint32 reg_flags;
    uint32 index;
    uint32 arg;
    response_type_t resp_type;
} sd_command_t;

// 响应结构体
typedef union {
    uint32 r48;
    struct {
        uint32 resp0;
        uint32 resp1;
        uint32 resp2;
        uint32 resp3;
    } r136;
} sd_response_t;

// RCA结构体
typedef struct {
    uint16 address;
} rca_t;

// 定时器结构体
typedef struct {
    uint64 deadline;
} timer_t;

// UART配置结构体
typedef struct {
    uint64 base_address;
} uart_t;

// 函数声明

// 定时器函数
uint64 read_tick(void);
void delay_us(uint64 microseconds);
timer_t timer_start(uint64 microseconds);
int timer_timeout(timer_t *timer);

// SDIO寄存器操作
void write_reg(uint32 reg, uint32 val);
uint32 read_reg(uint32 reg);
void write_fifo(size_t offset, uint8 val);
uint8 read_fifo(size_t offset);
uint32 fifo_count(void);

// SDIO工具函数
int wait_for(uint64 microseconds, int (*condition)(void));
card_error_t wait_for_cmd_line(void);
card_error_t wait_for_data_line(void);
card_error_t wait_for_cmd_done(void);
card_error_t wait_reset(uint32 mask);
card_error_t read_data(uint8 *buf);
card_error_t write_data(const uint8 *buf);

// SDIO命令函数
sd_command_t command_create(uint32 index, response_type_t resp_type, uint32 arg, int data_expected, int write_mode);
uint32 command_to_reg(sd_command_t *cmd);
card_error_t send_command(sd_command_t *cmd, sd_response_t *response);

// SDIO初始化函数
card_error_t sd_init(void);
card_error_t sd_read_block(uint8 *buf, uint32 addr);
card_error_t sd_write_block(const uint8 *buf, uint32 addr);

// UART函数
void vf2_uart_init(uart_t *uart);
void vf2_uart_putc(uart_t *uart, char c);
void vf2_uart_puts(uart_t *uart, const char *str);
void vf2_uart_printf(uart_t *uart, const char *format, ...);


#endif // VF2_DRIVER_H 