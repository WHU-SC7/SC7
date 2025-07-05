#include "uart.h"
#include "types.h"
#include <stdarg.h>
#include <stdbool.h>
#include "spinlock.h"
#include "hsai_service.h"
#include "defs.h"
#include "print.h"

#define COLOR_RESET "\033[0m" // 重置所有属性
#define RED_COLOR_PRINT "\033[31;1m"
extern void shutdown();

volatile int panicked = 0;

// 外部变量，控制是否启用打印锁
extern int pr_locking_enable;

static struct
{
    struct spinlock lock;
    int locking;
} pr;

// /** 放一个char到终端 */
// void
// consputc(char c)//来自xv6-2021.没有backspace功能
// {
//     put_char_sync(c);
// }

// /** 终端回退一个字符 */
// void
// cons_back()//backspace一次
// {
//     put_char_sync('\b');//光标向前移动
//     put_char_sync(' ');//输出空格，覆盖原来这格的字符。同时光标后移
//     put_char_sync('\b');//光标向前移动。最终实现backspace
// }

/** 往终端放一行 */
void print_line(const char *str) // should receive a str end with \0. Like "print line"
{
    while (*str)
        put_char_sync(*str++);
}

/*
以下来自xv6-2021
*/
static char digits[] = "0123456789abcdef";

/** 往终端放int */
static void
printint(long long xx, int base, int sign)
{
    char buf[16];
    int i;
    unsigned long long x;

    if (sign && (sign = xx < 0))
        x = -xx;
    else
        x = xx;

    i = 0;
    do
    {
        buf[i++] = digits[x % base];
    } while ((x /= base) != 0);

    if (sign)
        buf[i++] = '-';

    while (--i >= 0)
        consputc(buf[i]);
}

/** 往终端放ptr */
static void
printptr(uint64 x)
{
    int i;
    consputc('0');
    consputc('x');
    for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
        consputc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the console. only understands %d, %x, %p, %s.
// 10进制，16进制；64位指针，字符串，长整型
void printf(const char *fmt, ...)
{
    va_list ap;
    int i, cx, c0, c1, c2, locking;
    char *s;
    locking = pr.locking;
    if (locking){
        acquire(&pr.lock);
    }
    // if (locking) {
    //     // 添加调试信息
    //     if (holding(&pr.lock)) {
    //         // 如果已经持有锁，直接输出而不获取锁
    //         locking = 0;
    //     } else {
    //         acquire(&pr.lock);
    //     }
    // }

    va_start(ap, fmt);
    for (i = 0; (cx = fmt[i] & 0xff) != 0; i++)
    {
        if (cx != '%')
        {
            consputc(cx);
            continue;
        }
        i++;
        c0 = fmt[i + 0] & 0xff;
        c1 = c2 = 0;
        if (c0)
            c1 = fmt[i + 1] & 0xff;
        if (c1)
            c2 = fmt[i + 2] & 0xff;
        if (c0 == 'd')
        {
            printint(va_arg(ap, int), 10, 1);
        }
        else if (c0 == 'l' && c1 == 'd')
        {
            printint(va_arg(ap, uint64), 10, 1);
            i += 1;
        }
        else if (c0 == 'l' && c1 == 'l' && c2 == 'd')
        {
            printint(va_arg(ap, uint64), 10, 1);
            i += 2;
        }
        else if (c0 == 'u')
        {
            printint(va_arg(ap, uint32), 10, 0);
        }
        else if (c0 == 'l' && c1 == 'u')
        {
            printint(va_arg(ap, uint64), 10, 0);
            i += 1;
        }
        else if (c0 == 'l' && c1 == 'l' && c2 == 'u')
        {
            printint(va_arg(ap, uint64), 10, 0);
            i += 2;
        }
        else if (c0 == 'x')
        {
            printint(va_arg(ap, uint32), 16, 0);
        }
        else if (c0 == 'l' && c1 == 'x')
        {
            printint(va_arg(ap, uint64), 16, 0);
            i += 1;
        }
        else if (c0 == 'l' && c1 == 'l' && c2 == 'x')
        {
            printint(va_arg(ap, uint64), 16, 0);
            i += 2;
        }
        else if (c0 == 'p')
        {
            printptr(va_arg(ap, uint64));
        }
        else if (c0 == 's')
        {
            if ((s = va_arg(ap, char *)) == 0)
                s = "(null)";
            for (; *s; s++)
                consputc(*s);
        }
        else if (c0 == '%')
        {
            consputc('%');
        }
        else if (c0 == 0)
        {
            consputc('%');
            break;
        }
        else
        {
            // Print unknown % sequence to draw attention.
            consputc('%');
            consputc(c0);
        }
    }
    va_end(ap);

    if (locking)
        release(&pr.lock);
}

/** PANIC输出，支持占位符，红色输出 */
void vpanic(const char *file, int line, const char *fmt, va_list ap)
{
    // pr.locking = 0;
    int locking = pr.locking && pr_locking_enable;  // 只有在两个条件都满足时才使用锁
    if (locking) {
        // 添加调试信息
        if (holding(&pr.lock)) {
            // 如果已经持有锁，直接输出而不获取锁
            locking = 0;
        } else {
            acquire(&pr.lock);
        }
    }
    print_line(RED_COLOR_PRINT);
    
    int hartid = hsai_get_cpuid();
    char hartid_str[3];
    // 直接打印panic信息，避免调用printf导致锁重入
    print_line("panic:[");
    print_line(file);
    print_line(":");
    char line_str[16];
    int_to_str(line, line_str);
    print_line(line_str);
    print_line(" hartid:");
    int_to_str(hartid,hartid_str);
    print_line(hartid_str);
    print_line("] ");
    
    // 手动处理格式化字符串
    int i = 0;
    while (fmt[i])
    {
        if (fmt[i] == '%')
        {
            i++;
            if (fmt[i] == 'd')
            {
                printint(va_arg(ap, int), 10, 1);
            }
            else if (fmt[i] == 'x')
            {
                printint(va_arg(ap, uint32), 16, 0);
            }
            else if (fmt[i] == 's')
            {
                char *s = va_arg(ap, char *);
                if (s == 0)
                    s = "(null)";
                while (*s)
                    consputc(*s++);
            }
            else if (fmt[i] == 'p')
            {
                printptr(va_arg(ap, uint64));
            }
            else
            {
                consputc('%');
                if (fmt[i])
                    consputc(fmt[i]);
            }
        }
        else
        {
            consputc(fmt[i]);
        }
        i++;
    }
    print_line("\n");
    print_line(COLOR_RESET);
    if (locking)
        release(&pr.lock);
    panicked = 1; ///< 冻结来自其他CPU的uart输出
#ifdef RISCV
    shutdown();
#else
    *(volatile uint8 *)(0x8000000000000000 | 0x100E001C) = 0x34;
#endif
    for (;;)
        ; // 进入死循环
}

/** PANIC */
void panic_impl(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vpanic(file, line, fmt, ap);
    va_end(ap);
}

/** ASSERT */
void assert_impl(const char *file, int line, bool condition, const char *format, ...)
{
    if (!condition)
    {
        if (format != NULL)
        {
            va_list ap;
            va_start(ap, format);
            vpanic(file, line, format, ap); // 直接调用 vpanic 处理格式化
            va_end(ap);
        }
        else
        {
            panic_impl(file, line, "failed assert");
        }
    }
}

void printfinit(void)
{
    initlock(&pr.lock, "pr");
    // 延迟启用打印锁，避免多核启动时的锁竞争
    pr.locking = 0;  // 初始时不使用锁
}

// 添加一个函数来启用打印锁
void enable_print_lock(void)
{
    pr.locking = 1;
}

// 将整数转换为字符串
void int_to_str(int num, char *str)
{
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    int i = 0;
    int temp = num;
    while (temp > 0) {
        temp /= 10;
        i++;
    }
    
    str[i] = '\0';
    i--;
    
    temp = num;
    while (temp > 0) {
        str[i] = '0' + (temp % 10);
        temp /= 10;
        i--;
    }
}

// 将uint64转换为十六进制字符串
void uint64_to_hex_str(uint64 num, char *str)
{
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    int i = 0;
    uint64 temp = num;
    while (temp > 0) {
        temp /= 16;
        i++;
    }
    
    str[i] = '\0';
    i--;
    
    temp = num;
    while (temp > 0) {
        int digit = temp % 16;
        str[i] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        temp /= 16;
        i--;
    }
}

// 将uint32转换为十六进制字符串
static void uint32_to_hex_str(uint32 num, char *str)
{
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    int i = 0;
    uint32 temp = num;
    while (temp > 0) {
        temp /= 16;
        i++;
    }
    
    str[i] = '\0';
    i--;
    
    temp = num;
    while (temp > 0) {
        int digit = temp % 16;
        str[i] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        temp /= 16;
        i--;
    }
}

// 根据ecode和esubcode查找异常描述
const char *get_exception_name(unsigned int ecode, unsigned int esubcode)
{
    for (int i = 0; exception_table[i].name != NULL; i++)
    {
        if (exception_table[i].ecode == ecode &&
            exception_table[i].esubcode == esubcode)
        {
            return exception_table[i].name;
        }
    }
    return "UNKNOWN";
}

const char *get_exception_description(unsigned int ecode, unsigned int esubcode)
{
    for (int i = 0; exception_table[i].description != NULL; i++)
    {
        if (exception_table[i].ecode == ecode &&
            exception_table[i].esubcode == esubcode)
        {
            return exception_table[i].description;
        }
    }
    return "未知异常类型";
}

// 异常打印函数
void handle_exception(unsigned int ecode, unsigned int esubcode)
{
    const char *name = get_exception_name(ecode, esubcode);
    const char *desc = get_exception_description(ecode, esubcode);

    // LOG_LEVEL(3, "\nEcode=0x%x, EsubCode=0x%x\n类型=%s, 描述: %s\n", ecode, esubcode, name, desc);
    // 在异常处理中直接输出，避免获取打印锁
    print_line("\nEcode=0x");
    char ecode_str[16];
    uint32_to_hex_str(ecode, ecode_str);
    print_line(ecode_str);
    print_line(", EsubCode=0x");
    uint32_to_hex_str(esubcode, ecode_str);
    print_line(ecode_str);
    print_line("\n类型=");
    print_line(name);
    print_line(", 描述: ");
    print_line(desc);
    print_line("\n");
}