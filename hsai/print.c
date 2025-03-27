#include "uart.h"
#include "types.h"
#include <stdarg.h>

static void consputc(char c)//来自xv6-2021.没有backspace功能
{
    put_char_sync(c);
}

void cons_back()//backspace一次
{
    put_char_sync('\b');//光标向前移动
    put_char_sync(' ');//输出空格，覆盖原来这格的字符。同时光标后移
    put_char_sync('\b');//光标向前移动。最终实现backspace
}

void print_line(char *str)//should receive a str end with \0. Like "print line"
{
    while(*str)
        put_char_sync(*str++);
}

/*
以下来自xv6-2021
*/
static char digits[] = "0123456789abcdef";

void panic(char *s); //交叉引用

static void
printint(int xx, int base, int sign)
{
  char buf[16];
  int i;
  uint x;

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
//10进制，16进制；64位指针，字符串
void
printf(char *fmt, ...)
{
  va_list ap;
  int i, c;
  char *s;

  if (fmt == 0)
    panic("null fmt");

  va_start(ap, fmt);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(va_arg(ap, int), 10, 1);
      break;
    case 'x':
      printint(va_arg(ap, int), 16, 1);
      break;
    case 'p':
      printptr(va_arg(ap, uint64));
      break;
    case 's':
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }
}

void
panic(char *s)
{
  printf("panic: ");
  printf(s);
  printf("\n");
  for(;;)
    ;
}