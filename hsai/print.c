#include "uart.h"
#include "types.h"
#include <stdarg.h>
#include <stdbool.h>

#define COLOR_RESET    "\033[0m"  // 重置所有属性
#define RED_COLOR_PRINT		"\033[31;1m"

/** 放一个char到终端 */
void 
consputc(char c)//来自xv6-2021.没有backspace功能
{
    put_char_sync(c);
}

/** 终端回退一个字符 */
void 
cons_back()//backspace一次
{
    put_char_sync('\b');//光标向前移动
    put_char_sync(' ');//输出空格，覆盖原来这格的字符。同时光标后移
    put_char_sync('\b');//光标向前移动。最终实现backspace
}

/** 往终端放一行 */
void 
print_line(char *str)//should receive a str end with \0. Like "print line"
{
    while(*str)
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

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
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
void
printf(const char *fmt, ...)
{
  va_list ap;
  int i, cx, c0, c1, c2;
  char *s;
  // TODO: Here add locking.
  va_start(ap, fmt);
  for(i = 0; (cx = fmt[i] & 0xff) != 0; i++){
    if(cx != '%'){
      consputc(cx);
      continue;
    }
    i++;
    c0 = fmt[i+0] & 0xff;
    c1 = c2 = 0;
    if(c0) c1 = fmt[i+1] & 0xff;
    if(c1) c2 = fmt[i+2] & 0xff;
    if(c0 == 'd'){
      printint(va_arg(ap, int), 10, 1);
    } else if(c0 == 'l' && c1 == 'd'){
      printint(va_arg(ap, uint64), 10, 1);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'd'){
      printint(va_arg(ap, uint64), 10, 1);
      i += 2;
    } else if(c0 == 'u'){
      printint(va_arg(ap, uint32), 10, 0);
    } else if(c0 == 'l' && c1 == 'u'){
      printint(va_arg(ap, uint64), 10, 0);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'u'){
      printint(va_arg(ap, uint64), 10, 0);
      i += 2;
    } else if(c0 == 'x'){
      printint(va_arg(ap, uint32), 16, 0);
    } else if(c0 == 'l' && c1 == 'x'){
      printint(va_arg(ap, uint64), 16, 0);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'x'){
      printint(va_arg(ap, uint64), 16, 0);
      i += 2;
    } else if(c0 == 'p'){
      printptr(va_arg(ap, uint64));
    } else if(c0 == 's'){
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
    } else if(c0 == '%'){
      consputc('%');
    } else if(c0 == 0){
      consputc('%');
      break;
    } else {
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c0);
    }
  }
}

/** PANIC输出，支持占位符，红色输出 */
void 
vpanic(const char* file, int line,const char* fmt, va_list ap)
{
    print_line(RED_COLOR_PRINT);
    printf("panic:[%s:%d] ",file,line);
    // 手动处理格式化字符串
    int i = 0;
    while (fmt[i]) {
        if (fmt[i] == '%') {
            i++;
            if (fmt[i] == 'd') {
                printint(va_arg(ap, int), 10, 1);
            } else if (fmt[i] == 'x') {
                printint(va_arg(ap, uint32), 16, 0);
            } else if (fmt[i] == 's') {
                char* s = va_arg(ap, char*);
                if (s == 0) s = "(null)";
                while (*s) consputc(*s++);
            } else if (fmt[i] == 'p') {
                printptr(va_arg(ap, uint64));
            } else {
                consputc('%');
                if (fmt[i]) consputc(fmt[i]);
            }
        } else {
            consputc(fmt[i]);
        }
        i++;
    }
    printf("\n");
    print_line(COLOR_RESET);
    for(;;); // 进入死循环
}

/** PANIC */
void 
panic_impl(const char* file, int line,const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vpanic(file, line,fmt, ap);
    va_end(ap);
}

/** PANIC */
void 
assert_impl(const char* file, int line,bool condition, const char* format, ...)
{
    if (!condition) {
        if (format != NULL) {
            va_list ap;
            va_start(ap, format);
            vpanic(file,line,format, ap); // 直接调用 vpanic 处理格式化
            va_end(ap);
        } else {
          panic_impl(file,line,"failed assert");
        }
    }
}