#include "usercall.h"
#include "userlib.h"
int init_main(void) __attribute__((section(".text.user.init")));

typedef struct
{
    int valid;
    char *name[20];
} longtest;
static longtest busybox[];
int _strlen(const char *s)
{
    int n;

    for (n = 0; s[n]; n++)
        ;
    return n;
}
int strlen(const char *s);
void print(const char *s) { write(1, s, _strlen(s)); }
void printf(const char *fmt, ...);
void test_write();
void test_fork();
void test_gettime();
void test_brk();
void test_times();
void test_uname();
void test_waitpid(void);
void test_execve();
void test_wait(void);
void test_open();
void test_mmap(void);
int strlen(const char *s);
void test_dup2();
void test_getcwd();
void test_chdir();
void test_getdents();
void test_basic();
void test_busybox();
void exe(char *path);

char *question_name[] = {};
char *basic_name[] = {
    "brk",
    "chdir",
    "close",
    "dup",
    "dup2",
    "execve",
    "exit",
    "fork",
    "fstat",
    "getcwd",
    "getdents",
    "getpid",
    "mmap",
    "getppid",
    "gettimeofday",
    "mount",
    "umount",
    "munmap",
    "openat",
    "open",
    "pipe",
    "read",
    "sleep",
    "test_echo",
    "times",
    "clone",
    "uname",
    "wait",
    "waitpid",
    "write",
    "yield",
    "mkdir_",
    "unlink",
};

int init_main()
{
    if (openat(AT_FDCWD, "console", O_RDWR) < 0)
    {
        sys_mknod("/dev/tty", CONSOLE, 0);
        openat(AT_FDCWD, "/dev/tty", O_RDWR);
    }
    sys_dup(0); // stdout
    sys_dup(0); // stderr
    
    if (openat(AT_FDCWD, "/proc", O_RDONLY) < 0)
        sys_mkdirat(AT_FDCWD, "/proc", 0555);
    
    if (openat(AT_FDCWD, "/proc/mounts", O_RDONLY) < 0)
        sys_openat(AT_FDCWD, "/proc/mounts", 0777, O_CREATE);
    
    if (openat(AT_FDCWD, "/proc/meminfo", O_RDONLY) < 0)
        sys_openat(AT_FDCWD, "/proc/meminfo", 0777, O_CREATE);
    
    if (openat(AT_FDCWD, "/dev/misc/rtc", O_RDONLY) < 0)
        sys_openat(AT_FDCWD, "/dev/misc/rtc", 0777, O_CREATE);
    
    test_busybox();
    //test_basic();
    // test_busybox();
    // test_basic();
    //[[maybe_unused]]int id = getpid();
    //  test_fork();
    //  test_gettime();
    //  test_brk();
    //  test_write();
    //  test_execve();
    //  test_wait();
    //  test_times();
    //  test_uname();
    //  test_waitpid();
    //  test_execve();
    //  test_open();
    //  test_mmap();
    //  test_getcwd();
    //  test_chdir();
    //  test_getdents();
    //  test_dup2();

    // exe("/glibc/basic/getcwd");
    // exe("/glibc/basic/chdir");
    // exe("/glibc/basic/getdents");
    // test_waitpid();
    // test_execve();
    // test_open();
    // test_mmap();
    // exe("/glibc/basic/mount");
    shutdown();
    while (1)
        ;
    return 0;
}

void test_busybox()
{
    int pid, status;
    sys_chdir("musl");
    // sys_chdir("glibc");
    //  sys_chdir("/sdcard");
    int i;
    for (i = 0; busybox[i].name[1]; i++)
    {
        if (!busybox[i].valid)
            continue;
        pid = fork();
        if (pid < 0)
        {
            printf("init: fork failed\n");
            exit(1);
        }
        if (pid == 0)
        {
            // char *newargv[] = {"busybox","sh","-c", "basic_testcode.sh", 0};
            char *newenviron[] = {NULL};
            // sys_execve("busybox",newargv, newenviron);
            sys_execve("busybox", busybox[i].name, newenviron);
            print("execve error.\n");
            exit(1);
        }
        waitpid(pid, &status, 0);
        if (status == 0)
            printf("testcase %s success.\n", busybox[i].name[1]);
        else
            printf("testcase %s failed.\n", busybox[i].name[1]);
    }
}

static longtest busybox[] = {
    {1, {"busybox", "echo", "#### independent command test", 0}},
    {1, {"busybox", "ash", "-c", "exit", 0}},
    {1, {"busybox", "sh", "-c", "exit", 0}},
    {1, {"busybox", "basename", "/aaa/bbb", 0}},
    {1, {"busybox", "cal", 0}},
    {1, {"busybox", "clear", 0}},
    {1, {"busybox", "date", 0}},
    {1, {"busybox", "df", 0}},
    {1, {"busybox", "dirname", "/aaa/bbb", 0}},
    {1, {"busybox", "dmesg", 0}},
    {1, {"busybox", "du", 0}}, //< 无法访问是对的，不需要扫描半天，反正都是s1ccess
    {1, {"busybox", "expr", "1", "+", "1", 0}},
    {1, {"busybox", "false", 0}},
    {1, {"busybox", "true", 0}},
    {1, {"busybox", "which", "ls", 0}},
    {1, {"busybox", "uname", 0}},
    {1, {"busybox", "uptime", 0}},
    {1, {"busybox", "printf", "abc\n", 0}},
    {1, {"busybox", "ps", 0}},
    {1, {"busybox", "pwd", 0}},
    {1, {"busybox", "free", 0}},
    {1, {"busybox", "hwclock", 0}},
    {1, {"busybox", "kill", "10", 0}},
    {1, {"busybox", "ls", 0}}, //< 虽然无法访问. 但是testcase success，可以通过
    {0, {"busybox", "sleep", "1", 0}}, //< [glibc] syscall 115, c1ock_nanosleep
    {1, {"busybox", "echo", "#### file opration test", 0}},
    {1, {"busybox", "touch", "test.txt", 0}},
    {1, {"busybox", "echo", "hello world", ">", "test.txt", 0}},
    {1, {"busybox", "cat", "test.txt", 0}}, //<完成 [glibc] syscall 71   //< [musl] syscall 71
    {1, {"busybox", "cut", "-c", "3", "test.txt", 0}},
    {1, {"busybox", "od", "test.txt", 0}}, //< 能过[musl] syscall 65
    {1, {"busybox", "head", "test.txt", 0}},
    {1, {"busybox", "tail", "test.txt", 0}}, //< 完成[glibc] syscall 61  //< [musl] syscall 62
    {1, {"busybox", "hexdump", "-C", "test.txt", 0}}, //< 能过[musl] s1scall 65
    {1, {"busybox", "md5sum", "test.txt", 0}},
    {1, {"busybox", "echo", "ccccccc", ">>", "test.txt", 0}},
    {1, {"busybox", "echo", "bbbbbbb", ">>", "test.txt", 0}},
    {1, {"busybox", "echo", "aaaaaaa", ">>", "test.txt", 0}},
    {1, {"busybox", "echo", "2222222", ">>", "test.txt", 0}},
    {1, {"busybox", "echo", "1111111", ">>", "test.txt", 0}},
    {1, {"busybox", "sort", "test.txt", "|", "./busybox", "uniq", 0}},
    {1, {"busybox", "stat", "test.txt", 0}},
    {1, {"busybox", "strings", "test.txt", 0}},
    {1, {"busybox", "wc", "test.txt", 0}},
    {1, {"busybox", "[", "-f", "test.txt", "]", 0}},
    {1, {"busybox", "more", "test.txt", 0}}, //< 完成[glibc] syscall 71 //< [musl] syscall 71
    {1, {"busybox", "rm", "test.txt", 0}},
    {1, {"busybox", "mkdir", "test_dir", 0}},
    {1, {"busybox", "mv", "test_dir", "test", 0}}, //< 这个不需要s1scall 276，猜想是实现和riscv不一样
    {1, {"busybox", "rmdir", "test", 0}},
    {1, {"busybox", "grep", "hello", "busybox_cmd.txt", 0}},
    {1, {"busybox", "cp", "busybox_cmd.txt", "busybox_cmd.bak", 0}},
    {1, {"busybox", "rm", "busybox_cmd.bak", 0}},
    {1, {"busybox", "find", "-name", "busybox_cmd.txt", 0}},
    {0, {0, 0}},
};
void test_basic()
{
    printf("#### OS COMP TEST GROUP START basic-glibc ####\n");
    int basic_testcases = sizeof(basic_name) / sizeof(basic_name[0]);
    int pid;
    sys_chdir("/glibc/basic");
    for (int i = 0; i < basic_testcases; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf("init: fork failed\n");
            exit(1);
        }
        if (pid == 0)
        {
            exe(basic_name[i]);
            exit(1);
        }
        wait(0);
    }
    printf("#### OS COMP TEST GROUP END basic-glibc ####\n");


    printf("#### OS COMP TEST GROUP START basic-musl ####\n");
    sys_chdir("/musl/basic");
    for (int i = 0; i < basic_testcases; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf("init: fork failed\n");
            exit(1);
        }
        if (pid == 0)
        {
            exe(basic_name[i]);
            exit(1);
        }
        wait(0);
    }
    printf("#### OS COMP TEST GROUP END basic-musl ####\n");

}

char getdents_buf[512];
void test_getdents()
{ //< 看描述sys_getdents64只获取目录自身的信息，比ls简单
    int fd, nread;
    struct linux_dirent64 *dirp64;
    dirp64 = (struct linux_dirent64 *)getdents_buf;
    // fd = open(".", O_DIRECTORY); //< 测例中本来就注释掉了
    fd = open(".", O_RDONLY);
    printf("open fd:%d\n", fd);

    nread = sys_getdents64(fd, dirp64, 512);
    printf("getdents fd:%d\n", nread); //< 好令人困惑的写法，是指文件描述符？应该是返回的长度
    // assert(nread != -1);
    printf("getdents success.\n%s\n", dirp64->d_name);
    /*下面一行是我测试用的*/
    // printf("inode: %d, type: %d, reclen: %d\n",dirp64->d_ino,dirp64->d_type,dirp64->d_reclen);

    /*
    下面是测例注释掉的，看来是为了降低难度，不需要显示一个目录下的所有文件
    不过我们内核的list_file已经实现了
    */
    /*
    for(int bpos = 0; bpos < nread;){
        d = (struct dirent *)(buf + bpos);
        printf(  "%s\t", d->d_name);
        bpos += d->d_reclen;
    }
    */

    printf("\n");
    sys_close(fd);
}

// static char buffer[30];
void test_chdir()
{
    mkdir("test_chdir", 0666); //< mkdir使用相对路径, sys_mkdirat可以是相对也可以是绝对
    //< 先做mkdir
    int ret = sys_chdir("test_chdir");
    printf("chdir ret: %d\n", ret);
    // assert(ret == 0); 初赛测例用了assert
    char buffer[30];
    sys_getcwd(buffer, 30);
    printf("  current working dir : %s\n", buffer);
}

void test_getcwd()
{
    char *cwd = NULL;
    char buf[128]; //= {0}; //<不初始化也可以，虽然比赛测例初始化buf了，但是我们这样做会缺memset函数报错，无所谓了
    cwd = sys_getcwd(buf, 128);
    if (cwd != NULL)
        printf("getcwd: %s successfully!\n", buf);
    else
        printf("getcwd ERROR.\n");
    // sys_getcwd(NULL,128); 这两个是我为了测试加的，测例并无
    // sys_getcwd(buf,0);
}

void exe(char *path)
{
    // printf("开始执行测例\n");
    int pid = fork();
    if (pid < 0)
    {
        print("fork failed\n");
    }
    else if (pid == 0)
    {
        // 子进程
        char *newargv[] = {path, "/dev/sda2", "./mnt"};
        char *newenviron[] = {NULL};
        sys_execve(path, newargv, newenviron);
        print("execve error.\n");
        exit(1);
    }
    else
    {
        int status;
        wait(&status);
        // print("测例执行成功\n");
    }
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
        sys_execve("/glibc/basic/waitpid", newargv, newenviron);
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

void test_dup2()
{
    int fd = sys_dup3(stdout, 100, 0);
    if (fd < 0)
    {
        print("dup2 error.\n");
    }
    else
    {
        print("dup2 success.\n");
    }
    const char *str = "  from fd 100\n";
    write(100, str, strlen(str));
}

void *memset(void *s, int c, int n)
{
    for (unsigned char *p = s; n--; *p++ = (unsigned char)c)
        ;
    return s;
}
void test_mmap(void)
{
}

void test_write()
{
    const char *str = "Hello operating system contest.\n";
    int str_len = _strlen(str);
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
        if (ret == cpid)
        {
            print("waitpid test Success!\n");
        }
        else
            print("waitpid error.\n");
    }
}

// void test_write()
// {
//     char *str = "user program write\n";
//     write(0, str, 20);
//     char *str1 = "第二次调用write,来自user\n";
//     write(0, str1, 33);
// }
void test_gettime()
{
    int test_ret1 = get_time();
    // volatile int i = 100000; // qemu时钟频率12500000
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
    sys_brk((void *)(cur_pos + 2 * 4006));

    alloc_pos = sys_brk(0);
    sys_brk((void *)(alloc_pos + 2 * 4006));

    alloc_pos_1 = sys_brk(0);
    alloc_pos_1++;
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
            print("wait child success.\nwstatus: ");
        else
            print("wait child error.\n");
    }
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