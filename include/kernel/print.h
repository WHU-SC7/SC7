//hsai提供
//
// 颜色打印宏
#ifndef __PRINT_H__
#define __PRINT_H__
#include <stdbool.h>
#include "types.h"


// 异常码与描述信息结构体
typedef struct {
    unsigned int ecode;
    unsigned int esubcode;
    const char *name;
    const char *description;
} LoongArchException;

// 龙芯架构异常码表（基于用户提供的完整列表）
static const LoongArchException exception_table[] = {
    {0x0,  0,      "INT",  "中断 (仅当 CSR.ECFG.VS=0 时)"},
    {0x1,  0,      "PIL",  "load操作页无效例外"},
    {0x2,  0,      "PIS",  "store操作页无效例外"},
    {0x3,  0,      "PIF",  "取指操作页无效例外"},
    {0x4,  0,      "PME",  "页修改例外"},
    {0x5,  0,      "PNR",  "页不可读例外"},
    {0x6,  0,      "PNX",  "页不可执行例外"},
    {0x7,  0,      "PPI",  "页特权等级不合规例外"},
    {0x8,  0,      "ADE",  "取指地址错例外 (ADEF)"},
    {0x8,  1,      "ADE",  "访存指令地址错例外 (ADEM)"},
    {0x9,  0,      "ALE",  "地址非对齐例外"},
    {0xA,  0,      "BCE",  "边界检查错例外"},
    {0xB,  0,      "SYS",  "系统调用例外"},
    {0xC,  0,      "BRK",  "断点例外"},
    {0xD,  0,      "INE",  "指令不存在例外"},
    {0xE,  0,      "IPE",  "指令特权等级错例外"},
    {0xF,  0,      "FPD",  "浮点指令未使能例外"},
    {0x10, 0,      "SXD",  "128位向量扩展指令未使能例外"},
    {0x11, 0,      "ASXD", "256位向量扩展指令未使能例外"},
    {0x12, 0,      "FPE",  "基础浮点指令例外"},
    {0x12, 1,      "VFPE", "向量浮点指令例外"},
    {0x13, 0,      "WPE",  "取指监测点例外 (WPEF)"},
    {0x13, 1,      "WPE",  "load/store操作监测点例外 (WPEM)"},
    {0x14, 0,      "BTD",  "二进制翻译扩展指令未使能例外"},
    {0x15, 0,      "BTE",  "二进制翻译相关例外"},
    {0x16, 0,      "GSPR", "客户机敏感特权资源例外"},
    {0x17, 0,      "HVC",  "虚拟机监控调用例外"},
    {0x18, 0,      "GCC",  "客户机CSR软件修改例外 (GCSC)"},
    {0x18, 1,      "GCC",  "客户机CSR硬件修改例外 (GCHC)"},
    // 结束标记
    {0xFFFFFFFF, 0xFFFFFFFF, NULL, NULL}
};

const char *get_exception_name(unsigned int ecode, unsigned int esubcode);
const char *get_exception_description(unsigned int ecode, unsigned int esubcode);
void handle_exception(unsigned int ecode, unsigned int esubcode);

// 日志级别枚举（可选）
enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
};

#define COLOR_RESET    "\033[0m"  // 重置所有属性
#define RED_COLOR_PRINT		"\033[31;1m"
#define GREEN_COLOR_PRINT	"\033[32;1m"
#define YELLOW_COLOR_PRINT	"\033[33;1m"
#define BLUE_COLOR_PRINT		"\033[34;1m"
#define MAGANTA_COLOR_PRINT 	"\033[35;1m"
#define CYAN_COLOR_PRINT		"\033[36;1m"
#define CLEAR_COLOR_PRINT	"\033[0m"

#define BRIGHT_RED_COLOR_PINRT	   "\033[91m"
#define BRIGHT_GREEN_COLOR_PRINT   "\033[92m"
#define BRIGHT_YELLOW_COLOR_PRINT  "\033[93m"
#define BRIGHT_BLUE_COLOR_PRINT	   "\033[94m"
#define BRIGHT_MAGANTA_COLOR_PRINT "\033[95m"
#define BRIGHT_CYAN_COLOR_PINRT	   "\033[96m"


// 颜色打印宏
#define PRINT_COLOR(color, format, ...) \
    do { \
        printf("%s" format "%s", color, ##__VA_ARGS__, COLOR_RESET); \
    } while(0)  // do-while结构避免宏展开问题

// 默认日志宏（蓝色，INFO级别）
#define LOG(format, ...) \
    PRINT_COLOR(BLUE_COLOR_PRINT, "[INFO][%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__)

// 带级别的日志宏
#define LOG_LEVEL(level, format, ...) do { \
    const char *color = BLUE_COLOR_PRINT; \
    const char *prefix = "INFO"; \
    switch (level) { \
        case LOG_DEBUG:   color = CYAN_COLOR_PRINT; prefix = "DEBUG"; break; \
        case LOG_INFO:    color = BLUE_COLOR_PRINT;  prefix = "INFO";  break; \
        case LOG_WARNING: color = YELLOW_COLOR_PRINT; prefix = "WARN";  break; \
        case LOG_ERROR:   color = RED_COLOR_PRINT;    prefix = "ERROR"; break; \
    } \
    PRINT_COLOR(color, "[%s][%s:%d] " format, prefix, __FILE__, __LINE__, ##__VA_ARGS__); \
} while (0)

#if DEBUG
#define DEBUG_LOG_LEVEL(level, format, ...) do { \
    const char *color = BLUE_COLOR_PRINT; \
    const char *prefix = "INFO"; \
    switch (level) { \
        case LOG_DEBUG:   color = CYAN_COLOR_PRINT; prefix = "DEBUG"; break; \
        case LOG_INFO:    color = BLUE_COLOR_PRINT;  prefix = "INFO";  break; \
        case LOG_WARNING: color = YELLOW_COLOR_PRINT; prefix = "WARN";  break; \
        case LOG_ERROR:   color = RED_COLOR_PRINT;    prefix = "ERROR"; break; \
    } \
    PRINT_COLOR(color, "[%s][%s:%d] " format, prefix, __FILE__, __LINE__, ##__VA_ARGS__); \
} while (0)
#else
#define DEBUG_LOG_LEVEL(level, format, ...) do { } while (0)
#endif


// 将panic定义为宏，自动捕获文件、行号和参数
#define panic(format, ...) \
    panic_impl(__FILE__, __LINE__, format, ##__VA_ARGS__)

#define assert(condition, format, ...) \
    assert_impl(__FILE__, __LINE__, condition, format, ##__VA_ARGS__)

#define ASSERT(condition) \
    assert_impl(__FILE__, __LINE__, condition, "%s", #condition)

// void consputc();
// void cons_back();
void print_line(const char *str);
void printf(const char *fmt, ...);
void assert_impl(const char* file, int line,bool condition, const char *fmt, ...);
void panic_impl(const char* file, int line,const char* fmt, ...);
void printfinit(void);
void enable_print_lock(void);
void int_to_str(int num, char *str);
void uint64_to_hex_str(uint64 num, char *str);
#endif