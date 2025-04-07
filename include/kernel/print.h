//hsai提供
//
// 颜色打印宏
#ifndef __PRINT_H__
#define __PRINT_H__
#include <stdbool.h>

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


//颜色打印宏
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

// 将panic定义为宏，自动捕获文件、行号和参数
#define panic(format, ...) \
    panic_impl(__FILE__, __LINE__, format, ##__VA_ARGS__)

#define assert(format, ...) \
    assert_impl(__FILE__, __LINE__, format, ##__VA_ARGS__)

void consputc();
void cons_back();
void print_line(char *str);
void printf(const char *fmt, ...);
void assert_impl(const char* file, int line,bool condition, const char *fmt, ...);
void panic_impl(const char* file, int line,const char* fmt, ...);

#endif