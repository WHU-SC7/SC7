#include "usercall.h"
#include "userlib.h"
#include "fcntl.h"

int init_main(void) __attribute__((section(".text.user.init")));
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
void test_times();
void test_uname();
void test_waitpid();
void test_execve();
void test_wait(void);
void test_open();
void test_openat();
void test_fstat();
void test_mmap(void);
int strlen(const char *s);
int init_main()
{
    if(openat(AT_FDCWD, "console", O_RDWR) < 0)
    {
        sys_mknod("console", CONSOLE, 0);
        openat(AT_FDCWD, "console", O_RDWR);
    }
    sys_dup(0);  // stdout
    sys_dup(0);  // stderr

    //[[maybe_unused]]int id = getpid();
    // test_fork();
    // test_wait();
    // test_gettime();
    // test_brk();
    // test_times();
    // test_waitpid();
    // test_uname();
    // test_write();
    // test_execve();
    // test_openat();
    //test_fstat();
    test_mmap();
    while (1)
        ;
    return 0;
}

void test_execve()
{
    int pid = fork();
    if (pid < 0)
    {
        print("fork failed\n");
    }
    else if (pid == 0)
    {
        // 子进程

        char *newargv[] = {"/dup2", NULL};
        char *newenviron[] = {NULL};
        sys_execve("/glibc/basic/dup2", newargv, newenviron);
        print("execve error.\n");
        exit(1);
    }
    else
    {
        int status;
        wait(&status);
        print("child process is over\n");
    }
}

void test_write(){
	const char *str = "Hello operating system contest.\n";
	int str_len = strlen(str);
	int reallylen = write(1, str, str_len);
    if (reallylen != str_len)
    {
        print("write error.\n");
    }
    else
    {
        print("write success.\n");
    }
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
        pid_t ppid = getppid();
        if (ppid > 0)
            print("getppid success. ppid");
        else
            print("  getppid error.\n");
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

void test_open()
{
    // O_RDONLY = 0, O_WRONLY = 1
    int fd = open("./text.txt", 0);
    char buf[256];
    int size = sys_read(fd, buf, 256);
    if (size < 0)
    {
        size = 0;
    }
    write(stdout, buf, size);
    sys_close(fd);
}

void test_openat(void)
{
    // int fd_dir = open(".", O_RDONLY | O_CREATE);
    int fd_dir = open("./mnt", O_DIRECTORY);
    print("open dir fd: \n");
    int fd = openat(fd_dir, "test_openat.txt", O_CREATE | O_RDWR);
    print("openat fd: \n");
    print("openat success");
    /*(
    char buf[256] = "openat text file";
    write(fd, buf, strlen(buf));
    int size = read(fd, buf, 256);
    if (size > 0) printf("  openat success.\n");
    else printf("  openat error.\n");
    */
    sys_close(fd);
}

static struct kstat kst;
void test_fstat()
{
    int fd = open("./text.txt", 0);
    int ret = sys_fstat(fd, &kst);
    ret++;
    print("fstat ret: \n");
    // printf("fstat: dev: %d, inode: %d, mode: %d, nlink: %d, size: %d, atime: %d, mtime: %d, ctime: %d\n",
    //    kst.st_dev, kst.st_ino, kst.st_mode, kst.st_nlink, kst.st_size, kst.st_atime_sec, kst.st_mtime_sec, kst.st_ctime_sec);
}

void test_mmap(void)
{
    char *array;
    const char *str = "Hello, mmap successfully!";
    int fd;

    fd = open("test_mmap.txt", O_RDWR | O_CREATE);
    write(fd, str, strlen(str));
    sys_fstat(fd, &kst);
    // printf("file len: %d\n", kst.st_size);
    array = sys_mmap(NULL, kst.st_size, PROT_WRITE | PROT_READ, MAP_FILE | MAP_SHARED, fd, 0);
    // printf("return array: %x\n", array);

    if (array == MAP_FAILED)
    {
        print("mmap error.\n");
    }
    else
    {
        printf("mmap content: %s\n", str);
        // munmap(array, kst.st_size);
    }

    sys_close(fd);
}

int i = 1000;
void test_waitpid(void)
{
    int cpid, wstatus;
    cpid = fork();
    if (cpid != -1)
    {
        print("fork test Success!\n");
    };
    if (cpid == 0)
    {
        while (i--)
            ;
        sys_sched_yield();
        print("This is child process\n");
        exit(3);
    }
    else
    {
        pid_t ret = waitpid(cpid, &wstatus, 0);
        if (ret != -1)
        {
            print("waitpid test Success!\n");
        }
        else
            print("waitpid error.\n");
    }
}
void test_wait(void)
{
    int cpid, wstatus;
    cpid = fork();
    if (cpid == 0)
    {
        print("This is child process\n");
        exit(0);
    }
    else
    {
        pid_t ret = wait(&wstatus);
        if (ret == cpid)
            print("wait child success.\n");
        else
            print("wait child error.\n");
    }
}

void test_gettime()
{
    int test_ret1 = get_time();
    // volatile int i = 100000; // qemu时钟频率12500000
    // while (i > 0)
    //     i--;
    sleep(2);
    int test_ret2 = get_time();
    if (test_ret1 >= 0 && test_ret2 >= 0)
    {
        print("get_time test success\n");
    }
}

// void test_write()
// {
//     char *str = "user program write\n";
//     write(0, str, 20);
//     char *str1 = "第二次调用write,来自user\n";
//     write(0, str1, 33);
// }

void test_brk()
{
    int64 cur_pos, alloc_pos, alloc_pos_1;

    cur_pos = sys_brk(0);
    sys_brk((void *)(cur_pos + 2 * 4006));

    alloc_pos = sys_brk(0);
    sys_brk((void *)(alloc_pos + 2 * 4006));

    alloc_pos_1 = sys_brk(0);
    alloc_pos_1++;
}
struct tms mytimes;
void test_times()
{

    for (int i = 0; i < 1000000; i++)
    {
    }
    uint64 test_ret = sys_times(&mytimes);
    mytimes.tms_cstime++;
    if (test_ret == 0)
    {
        print("test_times Success!");
    }
    else
    {
        print("test_times Failed!");
    }
}

struct utsname un;
void test_uname()
{
    int test_ret = sys_uname(&un);

    if (test_ret >= 0)
    {
        print("test_uname Success!");
    }
    else
    {
        print("test_uname Failed!");
    }
}

#include "def.h"
#include <stdarg.h>
#include <stddef.h>


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
#define HASZERO(x) (((x) - ONES) & ~(x) & HIGHS) // lib/string.c

typedef __SIZE_TYPE__ size_t;
#define SS (sizeof(size_t))

int strlen(const char *s)
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
    do
    {
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
    int f = stdout;

    va_start(ap, fmt);
    for (;;)
    {
        if (!*s)
            break;
        for (a = s; *s && *s != '%'; s++)
            ;
        for (z = s; s[0] == '%' && s[1] == '%'; z++, s += 2)
            ;
        l = z - a;
        out(f, a, l);
        if (l)
            continue;
        if (s[1] == 0)
            break;
        switch (s[1])
        {
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
        	if ((a = va_arg(ap, char *)) == 0)
        		a = "(null)";
        	l = strlen(a);
            l = l > 200 ? 200 : l;
        	out(f, a, l);
        	break;
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