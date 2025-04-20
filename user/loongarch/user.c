#include "usercall.h"
#include "userlib.h"
int			init_main( void ) __attribute__( ( section( ".text.user.init" ) ) );


int _strlen(const char *s)
{
    int n;

    for (n = 0; s[n]; n++)
        ;
    return n;
}
void print(const char *s) { write(1, s, _strlen(s)); }
void printf(const char *fmt, ...);
void test_write();
void test_fork();
void test_gettime();
void test_brk();
int init_main()
{
    //[[maybe_unused]]int id = getpid();
    //test_fork();
    //test_gettime();
	test_brk();
    //test_write();
    while (1)
        ;
    return 0;
}

void test_fork()
{
    int pid = fork();
    if (pid < 0)
    {
        // fork失败
        print("fork failed\n");
    }
    else if (pid == 0)
    {
        // 子进程
        print("child process\n");
        exit(1);
    }
    else
    {
        // 父进程
        print("parent process is waiting\n");
        int status;
        wait(&status);
        print("child process is over\n");
    }
}

void test_write()
{
    char *str = "user program write\n";
    write(0, str, 20);
    char *str1 = "第二次调用write,来自user\n";
    write(0, str1, 33);
}
void test_gettime()
{
    int test_ret1 = get_time();
    //volatile int i = 100000; // qemu时钟频率12500000
    sleep(1);
    int test_ret2 = get_time();
    if (test_ret1 >= 0 && test_ret2 >= 0)
    {
        print("get_time test success\n");
    }
}
void test_brk()
{
    int64 cur_pos, alloc_pos, alloc_pos_1;

    cur_pos = sys_brk(0);
    sys_brk((void*)(cur_pos + 2*4006));

    alloc_pos = sys_brk(0);
    sys_brk((void*)(alloc_pos + 2*4006));

    alloc_pos_1 = sys_brk(0);
    alloc_pos_1 ++;
}
#include "def.h"
#include <stdarg.h>
#include <stddef.h>


#define stdout 0

static int out(int f, const char *s, size_t l)
{
	write(f, s, l);
    return 0;
	// int len = 0;
	// if (buffer_lock_enabled == 1) {
	// 	// for multiple threads io
	// 	mutex_lock(buffer_lock);
	// 	len = out_unlocked(s, l);
	// 	mutex_unlock(buffer_lock);
	// } else {
	// 	len = out_unlocked(s, l);
	// }
	// return len;
}

int putchar(int c)
{
	char byte = c;
	return out(stdout, &byte, 1);
}

#define UCHAR_MAX (0xffU)
#define ONES ((size_t)-1 / UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX / 2 + 1))
#define HASZERO(x) (((x)-ONES) & ~(x)&HIGHS) // lib/string.c

typedef __SIZE_TYPE__ size_t;
#define SS (sizeof(size_t))

size_t strlen(const char *s)
{
	const char *a = s;
	typedef size_t __attribute__((__may_alias__)) word;
	const word *w;
	for (; (uint64)s % SS; s++)
		if (!*s)
			return s - a;
	for (w = (const void *)s; !HASZERO(*w); w++)
		;
	s = (const void *)w;
	for (; *s; s++)
		;
	return s - a;
}

int puts(const char *s)
{
	int r;
	r = -(out(stdout, s, strlen(s)) < 0 || putchar('\n') < 0);
	return r;
}

static char digits[] = "0123456789abcdef";

static void printint(int xx, int base, int sign)
{
	char buf[16 + 1];
	int i;
	uint x;

	if (sign && (sign = xx < 0))
		x = -xx;
	else
		x = xx;

	buf[16] = 0;
	i = 15;
	do {
		buf[i--] = digits[x % base];
	} while ((x /= base) != 0);

	if (sign)
		buf[i--] = '-';
	i++;
	if (i < 0)
		puts("printint error");
	out(stdout, buf + i, 16 - i);
}

static void printptr(uint64 x)
{
	int i = 0, j;
	char buf[32 + 1];
	buf[i++] = '0';
	buf[i++] = 'x';
	for (j = 0; j < (sizeof(uint64) * 2); j++, x <<= 4)
		buf[i++] = digits[x >> (sizeof(uint64) * 8 - 4)];
	buf[i] = 0;
	out(stdout, buf, i);
}

// Print to the console. only understands %d, %x, %p, %s.
void printf(const char *fmt, ...)
{
	va_list ap;
	int l = 0;
	char *a, *z, *s = (char *)fmt;
	//int f = stdout;

	va_start(ap, fmt);
	for (;;) {
		if (!*s)
			break;
		for (a = s; *s && *s != '%'; s++)
			;
		for (z = s; s[0] == '%' && s[1] == '%'; z++, s += 2)
			;
		l = z - a;
		//out(f, a, l);
		if (l)
			continue;
		if (s[1] == 0)
			break;
		switch (s[1]) {
		case 'd':
			printint(va_arg(ap, int), 10, 1);
			break;
		case 'x':
			printint(va_arg(ap, int), 16, 1);
			break;
		case 'p':
			printptr(va_arg(ap, uint64));
			break;
		// case 's':
		// 	if ((a = va_arg(ap, char *)) == 0)
		// 		a = "(null)";
		// 	l = strnlen(a, 200);
		// 	out(f, a, l);
		// 	break;
		default:
			// Print unknown % sequence to draw attention.
			putchar('%');
			putchar(s[1]);
			break;
		}
		s += 2;
	}
	va_end(ap);
}