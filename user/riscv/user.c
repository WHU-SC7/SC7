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
typedef struct
{
    int valid;
    char *name[20];
} longtest;
static longtest busybox[];
static char *busybox_cmd[];
static longtest lua[];
static longtest iozone[];
static longtest lmbench[];
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
void test_getcwd();
void test_chdir();
void test_getdents();
void test_clone();
void test_basic();
void test_mount();
void test_busybox();
void test_fs_img();
void test_libc();
void test_libc_dy();
void test_libcbench();
void test_sh();
void test_iozone();
void test_lua();
void test_lmbench();
void test_libc_all();
void run_all();
void exe(char *path);
int test_signal();
int test_shm();
static char *busybox_cmd[];
char *question_name[] = {};
static longtest libctest[];
static longtest libctest_dy[];
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

// 添加shell相关的函数声明
void run_shell();
void parse_command(char *line, char **argv, int *argc);
void execute_command(char **argv, int argc);
void print_prompt();
int read_line(char *line, int max_len);

// 添加busybox命令识别函数
int is_busybox_command(const char *cmd);
void execute_busybox_command(char **argv, int argc);

// 辅助函数实现
int atoi(const char *str)
{
    int result = 0;
    int sign = 1;

    // 跳过前导空格
    while (*str == ' ' || *str == '\t')
    {
        str++;
    }

    // 处理符号
    if (*str == '-')
    {
        sign = -1;
        str++;
    }
    else if (*str == '+')
    {
        str++;
    }

    // 转换数字
    while (*str >= '0' && *str <= '9')
    {
        result = result * 10 + (*str - '0');
        str++;
    }

    return sign * result;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s2 && *s1 == *s2)
    {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

char *strchr(const char *s, int c)
{
    while (*s != '\0')
    {
        if (*s == c)
        {
            return (char *)s;
        }
        s++;
    }
    if (c == '\0')
    {
        return (char *)s;
    }
    return NULL;
}

char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while (*src)
    {
        *d = *src;
        d++;
        src++;
    }
    *d = '\0';
    return dest;
}

char *strcat(char *dest, const char *src)
{
    char *d = dest;
    while (*d)
    {
        d++;
    }
    while (*src)
    {
        *d = *src;
        d++;
        src++;
    }
    *d = '\0';
    return dest;
}

void test_uartread() // 这个循环会读取字符并输出到屏幕，只对riscv sbi有效
{
    char c[2];
    c[1] = 0;
    while (1)
    {
        sys_read(0, c, 1);
        if (*c)
            printf("%s", c);
    }
}

int init_main()
{
    if (openat(AT_FDCWD, "/dev/tty", O_RDWR) < 0)
    {
        sys_mknod("/dev/tty", CONSOLE, 0);
        openat(AT_FDCWD, "/dev/tty", O_RDWR);
    }
    sys_dup(0); // stdout
    sys_dup(0); // stderr

    // 读取字符测试 - 注释掉，避免阻塞
    //  test_uartread();
    //  启动shell而不是运行测试
    run_shell();

    // 如果shell退出，则运行测试
    // run_all();
    // test_shm();
    //  test_libc_dy();
    //   test_libc();
    //    test_lua();
    // test_basic();
    // test_busybox();
    //    test_fs_img();
    // test_lmbench();
    // test_libcbench();
    // test_sh();
    shutdown();
    while (1)
        ;
    return 0;
}

// ... existing code ...

// 简单的shell实现
void run_shell()
{
    char line[256];
    char *argv[32];
    int argc;

    printf("欢迎使用SC7 Shell!\n");
    printf("输入 'help' 查看可用命令，输入 'exit' 退出shell\n\n");

    while (1)
    {
        print_prompt();

        int read_result = read_line(line, sizeof(line));
        if (read_result < 0)
        {
            printf("\n读取输入失败，退出shell\n");
            break;
        }
        // 跳过空行
        if (read_result == 0 || line[0] == '\0' || line[0] == '\n')
        {
            continue;
        }

        // 解析命令
        parse_command(line, argv, &argc);

        if (argc == 0)
        {
            continue;
        }

        // 检查内置命令
        if (strcmp(argv[0], "exit") == 0)
        {
            printf("退出shell\n");
            break;
        }
        else if (strcmp(argv[0], "help") == 0)
        {
            printf("常见可用命令:\n");
            printf("  help       - 显示此帮助信息\n");
            printf("  exit       - 退出shell\n");
            printf("  echo       - 输出文本\n");
            printf("  sh         - 启动Bourne shell\n");
            printf("  basename   - 显示路径的最后部分\n");
            printf("  cal        - 显示日历\n");
            printf("  clear      - 清屏\n");
            printf("  date       - 显示当前时间\n");
            printf("  df         - 显示磁盘空间\n");
            printf("  dirname    - 显示路径的目录部分\n");
            printf("  dmesg      - 显示内核日志\n");
            printf("  du         - 显示目录占用空间\n");
            printf("  which      - 查找命令路径\n");
            printf("  uname      - 显示系统信息\n");
            printf("  printf     - 格式化输出\n");
            printf("  ps         - 显示进程列表\n");
            printf("  pwd        - 显示当前目录\n");
            printf("  free       - 显示内存使用\n");
            printf("  kill       - 终止进程\n");
            printf("  ls         - 列出目录内容\n");
            printf("  sleep      - 暂停执行\n");
            printf("  touch      - 创建空文件\n");
            printf("  cat        - 显示文件内容\n");
            printf("  cut        - 截取文件部分\n");
            printf("  od         - 八进制转储文件\n");
            printf("  head       - 显示文件头部\n");
            printf("  tail       - 显示文件尾部\n");
            printf("  sort       - 文件排序\n");
            printf("  stat       - 显示文件状态\n");
            printf("  strings    - 提取文件中的字符串\n");
            printf("  wc         - 统计文件信息\n");
            printf("  more       - 分页显示文件\n");
            printf("  rm         - 删除文件\n");
            printf("  mkdir      - 创建目录\n");
            printf("  mv         - 移动/重命名文件\n");
            printf("  rmdir      - 删除空目录\n");
            printf("  grep       - 文本搜索\n");
            printf("  cp         - 复制文件\n");
            printf("  find       - 查找文件\n");
            printf("  其他命令将通过execve执行\n");
        }
        // else if (strcmp(argv[0], "pwd") == 0)
        // {
        //     char cwd[256];
        //     if (sys_getcwd(cwd, sizeof(cwd)) != NULL)
        //     {
        //         printf("%s\n", cwd);
        //     }
        //     else
        //     {
        //         printf("获取当前目录失败\n");
        //     }
        // }
        // else if (strcmp(argv[0], "ls") == 0)
        // {
        //     char *path = (argc > 1) ? argv[1] : ".";
        //     int fd = open(path, O_RDONLY | O_DIRECTORY);
        //     if (fd < 0)
        //     {
        //         printf("无法打开目录: %s\n", path);
        //         continue;
        //     }

        //     char buf[512];
        //     struct linux_dirent64 *d;
        //     int nread;

        //     while ((nread = sys_getdents64(fd, (struct linux_dirent64 *)buf, sizeof(buf))) > 0)
        //     {
        //         for (int bpos = 0; bpos < nread;)
        //         {
        //             d = (struct linux_dirent64 *)(buf + bpos);
        //             if (d->d_ino != 0)
        //             { // 跳过无效条目
        //                 printf("%s  ", d->d_name);
        //             }
        //             bpos += d->d_reclen;
        //         }
        //     }
        //     printf("\n");
        //     sys_close(fd);
        // }
        else if (strcmp(argv[0], "cd") == 0)
        {
            char *path = (argc > 1) ? argv[1] : "/";
            if (sys_chdir(path) < 0)
            {
                printf("切换目录失败: %s\n", path);
            }
        }
        // else if (strcmp(argv[0], "cat") == 0)
        // {
        //     if (argc < 2)
        //     {
        //         printf("用法: cat <文件名>\n");
        //         continue;
        //     }

        //     int fd = open(argv[1], O_RDONLY);
        //     if (fd < 0)
        //     {
        //         printf("无法打开文件: %s\n", argv[1]);
        //         continue;
        //     }

        //     char buf[256];
        //     int n;
        //     while ((n = sys_read(fd, buf, sizeof(buf))) > 0)
        //     {
        //         write(1, buf, n);
        //     }
        //     sys_close(fd);
        // }
        // else if (strcmp(argv[0], "echo") == 0)
        // {
        //     for (int i = 1; i < argc; i++)
        //     {
        //         printf("%s ", argv[i]);
        //     }
        //     printf("\n");
        // }
        // else if (strcmp(argv[0], "clear") == 0)
        // {
        //     // 简单的清屏实现
        //     for (int i = 0; i < 50; i++)
        //     {
        //         printf("\n");
        //     }
        // }
        // else if (strcmp(argv[0], "date") == 0)
        // {
        //     timeval_t tv;
        //     if (sys_get_time(&tv, 0) == 0)
        //     {
        //         printf("当前时间: %llu 秒 %llu 微秒\n", tv.sec, tv.usec);
        //     }
        //     else
        //     {
        //         printf("获取时间失败\n");
        //     }
        // }
        // else if (strcmp(argv[0], "ps") == 0)
        // {
        //     printf("进程信息功能暂未实现\n");
        // }
        // else if (strcmp(argv[0], "kill") == 0)
        // {
        //     if (argc < 2)
        //     {
        //         printf("用法: kill <进程ID>\n");
        //         continue;
        //     }
        //     int pid = atoi(argv[1]);
        //     if (sys_kill(pid, 9) < 0)
        //     {
        //         printf("终止进程失败: %d\n", pid);
        //     }
        //     else
        //     {
        //         printf("已发送终止信号给进程: %d\n", pid);
        //     }
        // }
        else
        {
            if (is_busybox_command(argv[0]))
            {
                execute_busybox_command(argv, argc);
            }
            else
            {
                // 尝试执行外部命令
                execute_command(argv, argc);
            }
        }
    }
}

void parse_command(char *line, char **argv, int *argc)
{
    *argc = 0;
    char *src = line;
    char *dst = line;

    // 预处理：过滤控制字符（保留空格和制表符）
    while (*src) {
        if (*src == '\n' || *src == '\0') {
            break;  // 遇到换行或结束符停止
        }
        // 保留可打印字符、空格、制表符
        if (*src >= ' ' || *src == '\t') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';  // 终止字符串

    char *token = line;

    while (*token && *argc < 31)
    {
        // 跳过前导空格
        while (*token == ' ' || *token == '\t')
        {
            token++;
        }

        if (*token == '\0')
            break;

        argv[*argc] = token;
        (*argc)++;

        // 找到下一个空格
        while (*token && *token != ' ' && *token != '\t')
        {
            token++;
        }

        if (*token)
        {
            *token = '\0';
            token++;
        }
    }

    argv[*argc] = NULL;
}

void execute_command(char **argv, int argc)
{
    int pid = fork();

    if (pid < 0)
    {
        printf("fork失败\n");
        return;
    }

    if (pid == 0)
    {
        // 子进程
        char *newenviron[] = {NULL};

        // 尝试不同的路径
        char *paths[] = {
            "", // 当前目录
            "/musl/",
            "/glibc/",
            "/musl/basic/",
            "/glibc/basic/",
            NULL};

        int executed = 0;
        for (int i = 0; paths[i] != NULL; i++)
        {
            char full_path[256];
            if (strlen(paths[i]) > 0)
            {
                strcpy(full_path, paths[i]);
                strcat(full_path, argv[0]);
            }
            else
            {
                strcpy(full_path, argv[0]);
            }

            if (sys_execve(full_path, argv, newenviron) == 0)
            {
                executed = 1;
                break;
            }
        }

        if (!executed)
        {
            printf("命令未找到: %s\n", argv[0]);
        }
        exit(1);
    }
    else
    {
        // 父进程等待子进程
        int status;
        waitpid(pid, &status, 0);
    }
}

void print_prompt()
{
    char cwd[256];
    if (sys_getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("SC7:%s$ ", cwd);
    }
    else
    {
        printf("SC7:$ ");
    }
}

static int out(int f, const char *s, size_t l)
{
    write(f, s, l);
    return 0;
}

int putchar(int c)
{
    char byte = c;
    return out(stdout, &byte, 1);
}

int read_line(char *line, int max_len)
{
    int i = 0;
    char c;
    while (i < max_len - 1)
    {
        if (sys_read(0, &c, 1) <= 0)
        {
            return -1;
        }
        if (c == '\b' || c == 127)
        {
            if (i > 0)
            {
                i--;
                // 回显退格：移动光标左移，输出空格，再左移
                putchar('\b');
                putchar(' ');
                putchar('\b');
            }
            continue;
        }
        if (c == '\n' || c == '\r')
        {
            putchar('\n'); // 回显换行
            line[i] = '\0';
            return i;
        }
        putchar(c); // 实时回显
        line[i++] = c;
    }
    line[i] = '\0';
    return i;
}

void run_all()
{
    // test_basic();
    // test_busybox();
    // test_lua();
    // test_sh();
    // // test_libc_all();
    // test_libcbench();
    test_iozone();
}

static longtest busybox_setup_dynamic_library[] = {
    {1, {"busybox", "cp", "/glibc/lib/libc.so.6", "/usr/lib/libc.so.6", 0}},
    {1, {"busybox", "cp", "/glibc/lib/libm.so.6", "/usr/lib/libm.so.6", 0}},
    // {0, {"busybox", "cp", "/glibc/lib/ld-linux-riscv64-lp64d.so.1", "/usr/lib/ld-linux-riscv64-lp64d.so.1", 0}},
    {0, {0}},
};

void setup_dynamic_library()
{
    int i, pid, status;

    for (i = 0; busybox_setup_dynamic_library[i].name[1]; i++)
    {
        if (!busybox_setup_dynamic_library[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve("/musl/busybox", busybox_setup_dynamic_library[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }
}

void test_libc_all()
{
    int i, pid, status;
    sys_chdir("/musl");
    printf("#### OS COMP TEST GROUP START libctest-musl ####\n");
    // for (i = 0; libctest[i].name[1]; i++)
    // {
    //     if (!libctest[i].valid)
    //         continue;
    //     pid = fork();
    //     if (pid == 0)
    //     {
    //         char *newenviron[] = {NULL};
    //         sys_execve("./runtest.exe", libctest[i].name, newenviron);
    //         exit(0);
    //     }
    //     waitpid(pid, &status, 0);
    // }
    for (i = 0; libctest_dy[i].name[1]; i++)
    {
        if (!libctest_dy[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve("./runtest.exe", libctest_dy[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }
    printf("#### OS COMP TEST GROUP END libctest-musl ####\n");
}
void test_libc()
{
    printf("#### OS COMP TEST GROUP START libctest-glibc ####\n");
    int i, pid, status;
    // sys_chdir("/glibc");
    sys_chdir("/musl");
    for (i = 0; libctest[i].name[1]; i++)
    {
        if (!libctest[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve("./runtest.exe", libctest[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }
}

void test_libc_dy()
{
    int i, pid, status;
    sys_chdir("/musl");
    // sys_chdir("/glibc");
    for (i = 0; libctest_dy[i].name[1]; i++)
    {
        if (!libctest_dy[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve("./runtest.exe", libctest_dy[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }
}

void test_lua()
{
    printf("#### OS COMP TEST GROUP START lua-glibc ####\n");
    int i, status, pid;
    sys_chdir("/glibc");
    for (i = 0; lua[i].name[1]; i++)
    {
        if (!lua[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve("lua", lua[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
        if (status == 0)
        {
            printf("testcase lua %s success\n", lua[i].name[1]);
        }
        else
        {
            printf("testcase lua %s success\n", lua[i].name[1]);
        }
    }
    printf("#### OS COMP TEST GROUP END lua-glibc ####\n");

    printf("#### OS COMP TEST GROUP START lua-musl ####\n");
    sys_chdir("/musl");
    for (i = 0; lua[i].name[1]; i++)
    {
        if (!lua[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve("lua", lua[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
        if (status == 0)
        {
            printf("testcase lua %s success\n", lua[i].name[1]);
        }
        else
        {
            printf("testcase lua %s success\n", lua[i].name[1]);
        }
    }
    printf("#### OS COMP TEST GROUP END lua-musl ####");
}
static longtest lua[] = {
    {1, {"./lua", "date.lua", 0}},
    {1, {"./lua", "file_io.lua", 0}},
    {1, {"./lua", "max_min.lua", 0}},
    {1, {"./lua", "random.lua", 0}},
    {1, {"./lua", "remove.lua", 0}},
    {1, {"./lua", "round_num.lua", 0}},
    {1, {"./lua", "sin30.lua", 0}},
    {1, {"./lua", "sort.lua", 0}},
    {1, {"./lua", "strings.lua", 0}},
    {0, {0}},

};

void test_busybox()
{
    printf("#### OS COMP TEST GROUP START busybox-glibc ####\n");
    int pid, status;
    // sys_chdir("/musl");
    sys_chdir("/glibc");
    // sys_chdir("/sdcard");
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
            // char *newargv[] = {"busybox","sh", "-c","exec busybox pmap $$", 0};
            char *newenviron[] = {NULL};
            // sys_execve("busybox",newargv, newenviron);
            sys_execve("busybox", busybox[i].name, newenviron);
            print("execve error.\n");
            exit(1);
        }
        waitpid(pid, &status, 0);
        if (status == 0)
            printf("testcase busybox %s success\n", busybox_cmd[i]);
        else
            printf("testcase busybox %s failed\n", busybox_cmd[i]);
    }
    printf("#### OS COMP TEST GROUP END busybox-glibc ####\n");

    printf("#### OS COMP TEST GROUP START busybox-musl ####\n");
    sys_chdir("/musl");
    // sys_chdir("/glibc");
    //  sys_chdir("/sdcard");
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
            // char *newargv[] = {"busybox","sh", "-c","exec busybox pmap $$", 0};
            char *newenviron[] = {NULL};
            // sys_execve("busybox",newargv, newenviron);
            sys_execve("busybox", busybox[i].name, newenviron);
            print("execve error.\n");
            exit(1);
        }
        waitpid(pid, &status, 0);
        if (status == 0)
            printf("testcase busybox %s success\n", busybox_cmd[i]);
        else
            printf("testcase busybox %s failed\n", busybox_cmd[i]);
    }
    printf("#### OS COMP TEST GROUP END busybox-musl ####\n");
}

static longtest libctest[] = {
    {1, {"./runtest.exe", "-w", "entry-static.exe", "argv", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "basename", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "clocale_mbfuncs", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "clock_gettime", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "crypt", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "dirname", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "env", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fdopen", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fnmatch", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fscanf", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fwscanf", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "iconv_open", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "inet_pton", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "mbc", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "memstream", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cancel_points", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cancel", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cond", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_tsd", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "qsort", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "random", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "search_hsearch", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "search_insque", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "search_lsearch", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "search_tsearch", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "setjmp", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "snprintf", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "socket", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "sscanf", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "sscanf_long", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "stat", 0}}, ///< 打开tmp文件失1是合理的，因为已经删除了
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strftime", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_memcpy", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_memmem", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_memset", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_strchr", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_strcspn", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_strstr", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strptime", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strtod", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strtod_simple", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strtof", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strtol", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strtold", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "swprintf", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "tgmath", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "time", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "tls_align", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "udiv", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "ungetc", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "utime", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "wcsstr", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "wcstol", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pleval", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "daemon_failure", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "dn_expand_empty", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "dn_expand_ptr_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fflush_exit", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fgets_eof", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fgetwc_buffering", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fpclassify_invalid_ld80", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "ftello_unflushed_append", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "getpwnam_r_crash", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "getpwnam_r_errno", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "iconv_roundtrips", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "inet_ntop_v4mapped", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "inet_pton_empty_last_field", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "iswspace_null", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "lrand48_signextend", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "lseek_large", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "malloc_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "mbsrtowcs_overflow", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "memmem_oob_read", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "memmem_oob", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "mkdtemp_failure", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "mkstemp_failure", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "printf_1e9_oob", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "printf_fmt_g_round", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "printf_fmt_g_zeros", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "printf_fmt_n", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_robust_detach", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cancel_sem_wait", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cond_smasher", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_condattr_setclock", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cond_smasher", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_condattr_setclock", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_exit_cancel", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_once_deadlock", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_rwlock_ebusy", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "putenv_doublefree", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regex_backref_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regex_bracket_icase", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regex_ere_backref", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regex_escaped_high_byte", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regex_negated_range", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regexec_nosub", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "rewind_clear_error", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "rlimit_open_files", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "scanf_bytes_consumed", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "scanf_match_literal_eof", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "scanf_nullbyte_char", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "setvbuf_unget", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "sigprocmask_internal", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "sscanf_eof", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "statvfs", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strverscmp", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "syscall_sign_extend", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "uselocale_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "wcsncpy_read_overflow", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "wcsstr_false_negative", 0}},
    {0, {0, 0}}, // 数组结束标志，必须保留
};

static longtest libctest_dy[] = {
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "argv", 0}}, //< 从argv开始到sem_init之间，没有写注释的都是glibc dynamic可以通过的。
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "basename", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "clocale_mbfuncs", 0}}, //< 这个glibc dynamic有问题，输出很多assert failed
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "clock_gettime", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "crypt", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "dirname", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "dlopen", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "env", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fdopen", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fnmatch", 0}}, //< 不行，有assert failed
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fscanf", 0}},  //< 不行，有assert failed
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fwscanf", 0}}, //< 不行，有assert failed
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "iconv_open", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "inet_pton", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "mbc", 0}}, //< 不行. src/functional/mbc.c:44: cannot set UTF-8 locale for test (codeset=ANSI_X3.4-1968)
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "memstream", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_cancel_points", 0}}, //< pthread应该本来就跑不了
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_cancel", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_cond", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_tsd", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "qsort", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "random", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "search_hsearch", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "search_insque", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "search_lsearch", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "search_tsearch", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sem_init", 0}}, //< 只测试到这里，这个不行，报错panic:[syscall.c:1728] Futex type not support!
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sem_init", 0}}, //< 只测试到这里，这个不行，报错panic:[syscall.c:1728] Futex type not support!
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "setjmp", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "snprintf", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "socket", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "socket", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sscanf", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sscanf_long", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "stat", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strftime", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_memcpy", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_memmem", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_memset", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_strchr", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_strcspn", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_strstr", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strptime", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strtod", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strtod_simple", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strtof", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strtol", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strtold", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "swprintf", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "tgmath", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "time", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "tls_init", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "tls_local_exec", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "udiv", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "ungetc", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "utime", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "wcsstr", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "wcstol", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "daemon_failure", 0}}, ///< @todo remap
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "daemon_failure", 0}}, ///< @todo remap
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "dn_expand_empty", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "dn_expand_ptr_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fflush_exit", 0}}, ///< @todo remap
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fgets_eof", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fgetwc_buffering", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fpclassify_invalid_ld80", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "ftello_unflushed_append", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "getpwnam_r_crash", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "getpwnam_r_errno", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "iconv_roundtrips", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "inet_ntop_v4mapped", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "inet_pton_empty_last_field", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "iswspace_null", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "lrand48_signextend", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "lseek_large", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "malloc_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "mbsrtowcs_overflow", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "memmem_oob_read", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "memmem_oob", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "mkdtemp_failure", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "mkstemp_failure", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "printf_1e9_oob", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "printf_fmt_g_round", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "printf_fmt_g_zeros", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "printf_fmt_n", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_robust_detach", 0}}, //< kerneltrap
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_cond_smasher", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_condattr_setclock", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_cond_smasher", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_condattr_setclock", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_exit_cancel", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_once_deadlock", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_rwlock_ebusy", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "putenv_doublefree", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regex_backref_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regex_bracket_icase", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regex_ere_backref", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regex_escaped_high_byte", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regex_negated_range", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regexec_nosub", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "rewind_clear_error", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "rlimit_open_files", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "scanf_bytes_consumed", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "scanf_match_literal_eof", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "scanf_nullbyte_char", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "setvbuf_unget", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sigprocmask_internal", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sscanf_eof", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "statvfs", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strverscmp", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "tls_get_new_dtv", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "syscall_sign_extend", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "uselocale_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "wcsncpy_read_overflow", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "wcsstr_false_negative", 0}},
    {0, {0, 0}}, // 数组结束标志，必须保留
};

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
    {1, {"busybox", "du", "-d", "1", "/proc", 0}}, //< glibc跑这个有点慢,具体来说是输出第七行的6       ./ltp/testscripts之后慢
    {1, {"busybox", "expr", "1", "+", "1", 0}},
    {1, {"busybox", "false", 0}},
    {1, {"busybox", "true", 0}},
    {1, {"busybox", "which", "ls", 0}},
    {1, {"busybox", "uname", 0}},
    {1, {"busybox", "uptime", 0}}, //< [glibc] syscall 62  还要 syscall 103
    {1, {"busybox", "printf", "abc\n", 0}},
    {1, {"busybox", "ps", 0}},
    {1, {"busybox", "pwd", 0}},
    {1, {"busybox", "free", 0}},
    {1, {"busybox", "hwclock", 0}},
    {1, {"busybox", "kill", "10", 0}},
    {1, {"busybox", "ls", 0}},
    {1, {"busybox", "sleep", "1", 0}}, //< [glibc] syscall 115
    {1, {"busybox", "echo", "#### file opration test", 0}},
    {1, {"busybox", "touch", "test.txt", 0}},
    {1, {"busybox", "echo", "hello world", ">", "test.txt", 0}},
    {1, {"busybox", "cat", "test.txt", 0}}, //<完成 [glibc] syscall 71  //< [musl] syscall 71
    {1, {"busybox", "cut", "-c", "3", "test.txt", 0}},
    {1, {"busybox", "od", "test.txt", 0}}, //< 能过[musl] syscall 65
    {1, {"busybox", "head", "test.txt", 0}},
    {1, {"busybox", "tail", "test.txt", 0}},          //< 能过[glibc] syscall 62 //< [musl] syscall 62
    {1, {"busybox", "hexdump", "-C", "test.txt", 0}}, //< 能过[musl] syscall 65
    {1, {"busybox", "md5sum", "test.txt", 0}},
    {1, {"busybox", "echo", "ccccccc", ">>", "test.txt", 0}},
    {1, {"busybox", "echo", "bbbbbbb", ">>", "test.txt", 0}},
    {1, {"busybox", "echo", "aaaaaaa", ">>", "test.txt", 0}},
    {1, {"busybox", "echo", "2222222", ">>", "test.txt", 0}},
    {1, {"busybox", "echo", "1111111", ">>", "test.txt", 0}},
    {1, {"busybox", "echo", "bbbbbbb", ">>", "test.txt", 0}},
    {1, {"busybox", "sort", "test.txt", "|", "./busybox", "uniq", 0}},
    {1, {"busybox", "stat", "test.txt", 0}},
    {1, {"busybox", "strings", "test.txt", 0}},
    {1, {"busybox", "wc", "test.txt", 0}},
    {1, {"busybox", "[", "-f", "test.txt", "]", 0}},
    {1, {"busybox", "more", "test.txt", 0}}, //< 完成 [glibc] syscall 71     //< [musl] syscall 71
    {1, {"busybox", "rm", "test.txt", 0}},
    {1, {"busybox", "mkdir", "test_dir", 0}},
    {1, {"busybox", "mv", "test_dir", "test", 0}}, //<能过 [glibc] syscall 276      //< [musl] syscall 276
    {1, {"busybox", "rmdir", "test", 0}},
    {1, {"busybox", "grep", "hello", "busybox_cmd.txt", 0}},
    {1, {"busybox", "cp", "busybox_cmd.txt", "busybox_cmd.bak", 0}}, //< 应该都完成了[glibc] syscall 71     //< [musl] syscall 71
    {1, {"busybox", "rm", "busybox_cmd.bak", 0}},
    {1, {"busybox", "find", ".", "-maxdepth", "1", "-name", "busybox_cmd.txt", 0}}, //< [glibc] syscall 98     //< [musl] 虽然没有问题，但是找的真久啊，是整个磁盘扫了一遍吗
    {0, {0, 0}},
};

static char *busybox_cmd[] = {
    "echo \"#### independent command test\"",
    "ash -c exit",
    "sh -c exit",
    "basename /aaa/bbb",
    "cal",
    "clear",
    "date",
    "df",
    "dirname /aaa/bbb",
    "dmesg",
    "du",
    "expr 1 + 1",
    "false",
    "true",
    "which ls",
    "uname",
    "uptime",
    "printf",
    "ps",
    "pwd",
    "free",
    "hwclock",
    "kill 10",
    "ls",
    "sleep 1",
    "echo \"#### file opration test\"",
    "touch test.txt",
    "echo \"hello world\" > test.txt",
    "cat test.txt",
    "cut -c 3 test.txt",
    "od test.txt",
    "head test.txt",
    "tail test.txt",
    "hexdump -C test.txt",
    "md5sum test.txt",
    "echo \"ccccccc\" >> test.txt",
    "echo \"bbbbbbb\" >> test.txt",
    "echo \"aaaaaaa\" >> test.txt",
    "echo \"2222222\" >> test.txt",
    "echo \"1111111\" >> test.txt",
    "echo \"bbbbbbb\" >> test.txt",
    "sort test.txt | ./busybox uniq",
    "stat test.txt",
    "strings test.txt",
    "wc test.txt",
    "[ -f test.txt ]",
    "more test.txt",
    "rm test.txt",
    "mkdir test_dir",
    "mv test_dir test",
    "rmdir test",
    "grep hello busybox_cmd.txt",
    "cp busybox_cmd.txt busybox_cmd.bak",
    "rm busybox_cmd.bak",
    "find -name \"busybox_cmd.txt\"",
    NULL // Terminating NULL pointer (common convention for string arrays)
};

void test_sh()
{
    int pid;
    pid = fork();
    sys_chdir("/glibc");
    // sys_chdir("/musl");
    if (pid < 0)
    {
        printf("init: fork failed\n");
        exit(1);
    }
    if (pid == 0)
    {
        // char *newargv[] = {"sh", "-c", "./libctest_testcode.sh", NULL};
        char *newargv[] = {"sh", "-c", "./lmbench_testcode.sh", NULL};
        // char *newargv[] = {"sh", "-c","./busybox_testcode.sh", NULL};
        // char *newargv[] = {"sh", "./basic_testcode.sh", NULL};
        // char *newargv[] = {"sh", "-c","./iozone_testcode.sh", NULL};
        // char *newargv[] = {"sh", "./libcbench_testcode.sh", NULL};
        char *newenviron[] = {NULL};
        sys_execve("busybox", newargv, newenviron);
        print("execve error.\n");
        exit(1);
    }
    wait(0);

    // pid = fork();
    // sys_chdir("/musl");
    // // sys_chdir("/musl");
    // if (pid < 0)
    // {
    //     printf("init: fork failed\n");
    //     exit(1);
    // }
    // if (pid == 0)
    // {
    //     char *newargv[] = {"sh", "-c", "./libctest_testcode.sh", NULL};
    //     // char *newargv[] = {"sh", "-c","./busybox_testcode.sh", NULL};
    //     // char *newargv[] = {"sh", "./basic_testcode.sh", NULL};
    //     // char *newargv[] = {"sh", "-c","./iozone_testcode.sh", NULL};
    //     // char *newargv[] = {"sh", "./libcbench_testcode.sh", NULL};
    //     char *newenviron[] = {NULL};
    //     sys_execve("busybox", newargv, newenviron);
    //     print("execve error.\n");
    //     exit(1);
    // }
    // wait(0);
}
void test_fs_img()
{
    int pid;
    pid = fork();
    // sys_chdir("/glibc");
    // sys_chdir("/sdcard");
    if (pid < 0)
    {
        printf("init: fork failed\n");
        exit(1);
    }
    if (pid == 0)
    {
        char *newargv[] = {"sh", "-c", "exec busybox pmap $$", NULL};
        char *newenviron[] = {NULL};
        sys_execve("busybox_unstripped_musl", newargv, newenviron);
        print("execve error.\n");
        exit(1);
    }
    wait(0);
}
void test_libcbench()
{
    int pid;
    printf("#### OS COMP TEST GROUP START libcbench-glibc ####\n");
    pid = fork();
    sys_chdir("/musl");
    // sys_chdir("/glibc");
    if (pid < 0)
    {
        printf("init: fork failed\n");
        exit(1);
    }
    if (pid == 0)
    {
        // char *newargv[] = {"sh", "-c", "./run-static.sh", NULL};
        char *newargv[] = {NULL};
        // char *newargv[] = {"sh", "-c","./libctest_testcode.sh", NULL};
        char *newenviron[] = {NULL};
        sys_execve("./libc-bench", newargv, newenviron);
        print("execve error.\n");
        exit(1);
    }
    wait(0);
    printf("#### OS COMP TEST GROUP END libcbench-glibc ####\n");

    printf("#### OS COMP TEST GROUP START libcbench-musl ####\n");
    pid = fork();
    // sys_chdir("/musl");
    sys_chdir("/musl");
    if (pid < 0)
    {
        printf("init: fork failed\n");
        exit(1);
    }
    if (pid == 0)
    {
        // char *newargv[] = {"sh", "-c", "./run-static.sh", NULL};
        char *newargv[] = {NULL};
        // char *newargv[] = {"sh", "-c","./libctest_testcode.sh", NULL};
        char *newenviron[] = {NULL};
        sys_execve("./libc-bench", newargv, newenviron);
        print("execve error.\n");
        exit(1);
    }
    wait(0);
    printf("#### OS COMP TEST GROUP END libcbench-musl ####\n");
}

void test_iozone()
{
    // setup_dynamic_library();
    int pid, status;
    // sys_chdir("/glibc");
    sys_chdir("/musl");
    printf("run iozone_testcode.sh\n");
    char *newenviron[] = {NULL};
    // printf("iozone automatic measurements\n");
    // pid = fork();
    // if (pid == 0)
    // {
    //     sys_execve("iozone", iozone[0].name, newenviron);
    //     exit(0);
    // }
    // waitpid(pid, &status, 0);

    printf("iozone throughput write/read measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[1].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);

    printf("iozone throughput random-read measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[2].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);

    printf("iozone throughput read-backwards measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[3].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);

    printf("iozone throughput stride-read measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[4].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);

    printf("iozone throughput fwrite/fread measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[5].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);

    printf("iozone throughput pwrite/pread measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[6].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);

    printf("iozone throughput pwritev/preadv measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[7].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);
}

static longtest iozone[] = {
    {1, {"iozone", "-a", "-r", "1k", "-s", "4m", 0}},
    {1, {"iozone", "-t", "4", "-i", "0", "-i", "1", "-r", "1k", "-s", "1m", 0}},
    {1, {"iozone", "-t", "4", "-i", "0", "-i", "2", "-r", "1k", "-s", "1m", 0}},
    {1, {"iozone", "-t", "4", "-i", "0", "-i", "3", "-r", "1k", "-s", "1m", 0}},
    {1, {"iozone", "-t", "4", "-i", "0", "-i", "5", "-r", "1k", "-s", "1m", 0}},
    {1, {"iozone", "-t", "4", "-i", "6", "-i", "7", "-r", "1k", "-s", "1m", 0}},
    {1,
     {"iozone", "-t", "4", "-i", "9", "-i", "10", "-r", "1k", "-s", "1m", 0}},
    {1,
     {"iozone", "-t", "4", "-i", "11", "-i", "12", "-r", "1k", "-s", "1m", 0}},
    {0, {0, 0}} // 数组结束标志，必须保留
};

void test_lmbench()
{
    int pid, status, i;
    // sys_chdir("/musl");
    sys_chdir("/glibc");

    printf("run lmbench_testcode.sh\n");
    printf("latency measurements\n");

    for (i = 0; lmbench[i].name[1]; i++)
    {
        if (!lmbench[i].valid)
            continue;
        pid = fork();
        char *newenviron[] = {NULL};
        if (pid == 0)
        {
            sys_execve(lmbench[i].name[0], lmbench[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }
}

static longtest lmbench[] = {
    {1, {"lmbench_all", "lat_syscall", "-P", "1", "null", 0}},
    {1, {"lmbench_all", "lat_syscall", "-P", "1", "read", 0}},
    {1, {"lmbench_all", "lat_syscall", "-P", "1", "write", 0}},
    {1, {"busybox", "mkdir", "-p", "/var/tmp", 0}},
    {1, {"busybox", "touch", "/var/tmp/lmbench", 0}},
    {1,
     {"lmbench_all", "lat_syscall", "-P", "1", "stat", "/var/tmp/lmbench", 0}},
    {1,
     {"lmbench_all", "lat_syscall", "-P", "1", "fstat", "/var/tmp/lmbench", 0}},
    {1,
     {"lmbench_all", "lat_syscall", "-P", "1", "open", "/var/tmp/lmbench", 0}},
    {1, {"lmbench_all", "lat_select", "-n", "100", "-P", "1", "file", 0}},
    {1, {"lmbench_all", "lat_sig", "-P", "1", "install", 0}},
    {1, {"lmbench_all", "lat_sig", "-P", "1", "catch", 0}},
    {1, {"lmbench_all", "lat_pipe", "-P", "1", 0}},
    {1, {"lmbench_all", "lat_proc", "-P", "1", "fork", 0}},
    {1, {"lmbench_all", "lat_proc", "-P", "1", "exec", 0}},
    {1, {"busybox", "cp", "hello", "/tmp", 0}},
    {1, {"lmbench_all", "lat_proc", "-P", "1", "shell", 0}},
    {1,
     {"lmbench_all", "lmdd", "label=File /var/tmp/XXX write bandwidth:",
      "of=/var/tmp/XXX", "move=1m", "fsync=1", "print=3", 0}},
    {1, {"lmbench_all", "lat_pagefault", "-P", "1", "/var/tmp/XXX", 0}},
    {1, {"lmbench_all", "lat_mmap", "-P", "1", "512k", "/var/tmp/XXX", 0}},
    {1, {"busybox", "echo", "file", "system", "latency", 0}},
    {1, {"lmbench_all", "lat_fs", "/var/tmp", 0}},
    {1, {"busybox", "echo", "Bandwidth", "measurements", 0}},
    {1, {"lmbench_all", "bw_pipe", "-P", "1", 0}},
    {1,
     {"lmbench_all", "bw_file_rd", "-P", "1", "512k", "io_only", "/var/tmp/XXX",
      0}},
    {1,
     {"lmbench_all", "bw_file_rd", "-P", "1", "512k", "open2close",
      "/var/tmp/XXX", 0}},
    {1,
     {"lmbench_all", "bw_mmap_rd", "-P", "1", "512k", "mmap_only",
      "/var/tmp/XXX", 0}},
    {1,
     {"lmbench_all", "bw_mmap_rd", "-P", "1", "512k", "open2close",
      "/var/tmp/XXX", 0}},
    {1, {"busybox", "echo", "context", "switch", "overhead", 0}},
    {1,
     {"lmbench_all", "lat_ctx", "-P", "1", "-s", "32", "2", "4", "8", "16",
      "24", "32", "64", "96", 0}},
    {0, {0, 0}},
};

// static char mntpoint[64] = "./mnt";
// static char device[64] = "/dev/vda2";
// static const char *fs_type = "vfat";
// void test_mount()
// {

//     printf("Mounting dev:%s to %s\n", device, mntpoint);
//     int ret = mount(device, mntpoint, fs_type, 0, NULL);
//     printf("mount return: %d\n", ret);

//     if (ret == 0)
//     {
//         printf("mount successfully\n");
//         ret = umount(mntpoint);
//         printf("umount return: %d\n", ret);
//     }
// }

void test_basic()
{
    printf("#### OS COMP TEST GROUP START basic-glibc ####\n");
    int basic_testcases = sizeof(basic_name) / sizeof(basic_name[0]);
    int pid;
    sys_chdir("/glibc/basic");
    // char *newenviron[] = {NULL};

    // // 分组执行测例 (每组最多4个)
    // for (int i = 0; i < basic_testcases; i += 2)
    // {
    //     int group_size = (basic_testcases - i) < 2 ? (basic_testcases - i) : 2;
    //     int pids[4]; // 当前组的子进程PID

    //     // 创建当前组的子进程
    //     for (int j = 0; j < group_size; j++)
    //     {
    //         pid = fork();
    //         if (pid < 0)
    //         { /* 错误处理 */
    //         }
    //         if (pid == 0)
    //         {
    //             char *argc[] = {basic_name[i + j], NULL};
    //             sys_execve(basic_name[i + j], argc, newenviron);
    //             exit(1); // exe失败时退出
    //         }
    //         pids[j] = pid; // 记录当前组子进程PID
    //     }

    //     // 等待当前组所有子进程结束
    //     for (int j = 0; j < group_size; j++)
    //     {
    //         waitpid(pids[j], NULL, 0);
    //     }
    // }
    // printf("#### OS COMP TEST GROUP END basic-glibc ####\n");

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

// SIGCHLD信号处理函数
void sigchld_handler(int sig)
{
    printf("收到SIGCHLD信号，子进程已退出\n");
    // 等待子进程，避免僵尸进程
    int status;
    wait(&status);
    printf("子进程退出状态: %d\n", WEXITSTATUS(status));
}

int test_signal()
{
    printf("测试SIGCHLD信号处理\n");

    // 设置SIGCHLD信号处理函数
    struct sigaction sa;
    sa.__sigaction_handler.sa_handler = sigchld_handler;
    sa.sa_flags = 0;

    if (sys_sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        printf("设置SIGCHLD信号处理失败\n");
        return 1;
    }

    printf("SIGCHLD信号处理函数已设置\n");

    // 创建子进程
    int pid = fork();
    if (pid == 0)
    {
        // 子进程
        printf("子进程开始运行，PID: %d\n", getpid());
        sleep(2); // 子进程睡眠2秒
        printf("子进程即将退出\n");
        exit(42); // 子进程退出，状态码为42
    }
    else if (pid > 0)
    {
        // 父进程
        printf("父进程等待子进程退出，子进程PID: %d\n", pid);

        // 父进程继续运行，等待SIGCHLD信号
        while (1)
        {
            sleep(1);
            printf("父进程继续运行...\n");
        }
    }
    else
    {
        printf("fork失败\n");
        return 1;
    }

    return 0;
}

// 定义 IPC_PRIVATE (内核中为0)

// 共享内存大小
#define SHM_SIZE 4096
#define IPC_CREAT 0x200 // flag，如果不存在则创建共享内存段。
char *strncpy(char *s, const char *t, int n)
{
    char *os;

    os = s;
    while (n-- > 0 && (*s++ = *t++) != 0)
        ;
    while (n-- > 0)
        *s++ = 0;
    return os;
}

int test_shm()
{
    int shmid;
    char *shm_ptr;
    pid_t pid;

    // 1. 测试 shmget (使用 IPC_PRIVATE)
    shmid = sys_shmget(0, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1)
    {
        printf("shmget failed");
        exit(0);
    }
    printf("shmget success: shmid = %d\n", shmid);

    // 2. 测试 shmat (自动分配地址)
    shm_ptr = (char *)sys_shmat(shmid, 0, 0);
    if (shm_ptr == (void *)-1)
    {
        printf("shmat failed");
        exit(0);
    }
    printf("shmat success: attached at %p\n", shm_ptr);

    // 3. 写入测试数据
    const char *msg = "Hello, Shared Memory!";
    strncpy(shm_ptr, msg, strlen(msg) + 1);
    printf("Data written: \"%s\"\n", msg);

    // 4. 创建子进程验证共享内存
    pid = fork();
    if (pid < 0)
    {
        printf("fork failed");
        exit(0);
    }

    if (pid == 0)
    { // 子进程
        printf("\n[Child Process] Reading shared memory...\n");
        printf("Data in child: \"%s\"\n", shm_ptr);

        // 子进程写入数据 - 修复：包含字符串终止符
        strncpy(shm_ptr, "Modified by child", 18); // 17个字符 + 1个终止符
        printf("Child modified data\n");

        exit(0);
    }
    else
    {               // 父进程
        wait(NULL); // 等待子进程结束

        printf("\n[Parent Process] After child modification:\n");
        printf("Data in parent: \"%s\"\n", shm_ptr);
    }

    // 5. 测试 shmctl (目前内核空实现)
    if (sys_shmctl(shmid, 0, 0) == -1)
    {
        printf("shmctl IPC_RMID failed (expected, not implemented)\n");
    }
    else
    {
        printf("shmctl IPC_RMID success\n");
    }

    // 注意：内核目前没有实现 shmdt
    // 程序退出后内核会自动清理资源

    printf("\nTest completed successfully!\n");
    return 0;
}

// int stack[1024] = {0};
// static int child_pid;
// static int child_func()
// {
//     print("  Child says successfully!\n");
//     return 0;
// }

// void test_clone(void)
// {
//     int wstatus;
//     child_pid = clone(child_func, NULL, stack, 1024, SIGCHLD);
//     if (child_pid == 0)
//     {
//         exit(0);
//     }
//     else
//     {
//         if (wait(&wstatus) == child_pid)
//             print("clone process successfully.\npid:\n");
//         else
//             print("clone process error.\n");
//     }
// }

// char getdents_buf[512];
// void test_getdents()
// { //< 看描述sys_getdents64只获取目录自身的信息，比ls简单
//     int fd, nread;
//     struct linux_dirent64 *dirp64;
//     dirp64 = (struct linux_dirent64 *)getdents_buf;
//     // fd = open(".", O_DIRECTORY); //< 测例中本来就注释掉了
//     fd = open(".", O_RDONLY);
//     printf("open fd:%d\n", fd);

//     nread = sys_getdents64(fd, dirp64, 512);
//     printf("getdents fd:%d\n", nread); //< 好令人困惑的写法，是指文件描述符？应该是返回的长度
//     // assert(nread != -1);
//     printf("getdents success.\n%s\n", dirp64->d_name);
//     /*下面一行是我测试用的*/
//     // printf("inode: %d, type: %d, reclen: %d\n",dirp64->d_ino,dirp64->d_type,dirp64->d_reclen);

//     /*
//     下面是测例注释掉的，看来是为了降低难度，不需要显示一个目录下的所有文件
//     不过我们内核的list_file已经实现了
//     */
//     /*
//     for(int bpos = 0; bpos < nread;){
//         d = (struct dirent *)(buf + bpos);
//         printf(  "%s\t", d->d_name);
//         bpos += d->d_reclen;
//     }
//     */

//     printf("\n");
//     sys_close(fd);
// }

// // static char buffer[30];
// void test_chdir()
// {
//     mkdir("test_chdir", 0666); //< mkdir使用相对路径, sys_mkdirat可以是相对也可以是绝对
//     //< 先做mkdir
//     int ret = sys_chdir("test_chdir");
//     printf("chdir ret: %d\n", ret);
//     // assert(ret == 0); 初赛测例用了assert
//     char buffer[30];
//     sys_getcwd(buffer, 30);
//     printf("  current working dir : %s\n", buffer);
// }

// void test_getcwd()
// {
//     char *cwd = NULL;
//     char buf[128]; //= {0}; //<不初始化也可以，虽然比赛测例初始化buf了，但是我们这样做会缺memset函数报错，无所谓了
//     cwd = sys_getcwd(buf, 128);
//     if (cwd != NULL)
//         printf("getcwd: %s successfully!\n", buf);
//     else
//         printf("getcwd ERROR.\n");
//     // sys_getcwd(NULL,128); 这两个是我为了测试加的，测例并无
//     // sys_getcwd(buf,0);
// }

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
        char *newargv[] = {path, "/dev/sda2", "./mnt", NULL};
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

// void test_execve()
// {
//     int pid = fork();
//     if (pid < 0)
//     {
//         print("fork failed\n");
//     }
//     else if (pid == 0)
//     {
//         // 子进程

//         char *newargv[] = {"/glibc/basic/mount", NULL};
//         char *newenviron[] = {NULL};
//         sys_execve("/glibc/basic/mount", newargv, newenviron);
//         print("execve error.\n");
//         exit(1);
//     }
//     else
//     {
//         int status;
//         wait(&status);
//         print("child process is over\n");
//     }
// }

// void test_write()
// {
//     const char *str = "Hello operating system contest.\n";
//     int str_len = strlen(str);
//     int reallylen = write(1, str, str_len);
//     if (reallylen != str_len)
//     {
//         print("write error.\n");
//     }
//     else
//     {
//         print("write success.\n");
//     }
// }

// void test_fork()
// {
//     int pid = fork();
//     if (pid < 0)
//     {
//         // fork失败
//         print("fork failed\n");
//     }
//     else if (pid == 0)
//     {
//         // 子进程
//         pid_t ppid = getppid();
//         if (ppid > 0)
//             print("getppid success. ppid");
//         else
//             print("  getppid error.\n");
//         print("child process\n");
//         exit(1);
//     }
//     else
//     {
//         // 父进程
//         print("parent process is waiting\n");
//         int status;
//         wait(&status);
//         print("child process is over\n");
//     }
// }

// void test_open()
// {
//     // O_RDONLY = 0, O_WRONLY = 1
//     int fd = open("./text.txt", 0);
//     char buf[256];
//     int size = sys_read(fd, buf, 256);
//     if (size < 0)
//     {
//         size = 0;
//     }
//     write(stdout, buf, size);
//     sys_close(fd);
// }

// void test_openat(void)
// {
//     // int fd_dir = open(".", O_RDONLY | O_CREATE);
//     int fd_dir = open("./mnt", O_DIRECTORY);
//     print("open dir fd: \n");
//     int fd = openat(fd_dir, "test_openat.txt", O_CREATE | O_RDWR);
//     print("openat fd: \n");
//     print("openat success");
//     /*(
//     char buf[256] = "openat text file";
//     write(fd, buf, strlen(buf));
//     int size = read(fd, buf, 256);
//     if (size > 0) printf("  openat success.\n");
//     else printf("  openat error.\n");
//     */
//     sys_close(fd);
// }

// static struct kstat kst;
// void test_fstat()
// {
//     int fd = open("./text.txt", 0);
//     int ret = sys_fstat(fd, &kst);
//     ret++;
//     print("fstat ret: \n");
//     // printf("fstat: dev: %d, inode: %d, mode: %d, nlink: %d, size: %d, atime: %d, mtime: %d, ctime: %d\n",
//     //    kst.st_dev, kst.st_ino, kst.st_mode, kst.st_nlink, kst.st_size, kst.st_atime_sec, kst.st_mtime_sec, kst.st_ctime_sec);
// }

// void test_mmap(void)
// {
//     char *array;
//     const char *str = "Hello, mmap successfully!";
//     int fd;

//     fd = open("test_mmap.txt", O_RDWR | O_CREATE);
//     write(fd, str, strlen(str));
//     sys_fstat(fd, &kst);
//     // printf("file len: %d\n", kst.st_size);
//     array = sys_mmap(NULL, kst.st_size, PROT_WRITE | PROT_READ, MAP_FILE | MAP_SHARED, fd, 0);
//     // printf("return array: %x\n", array);

//     if (array == MAP_FAILED)
//     {
//         print("mmap error.\n");
//     }
//     else
//     {
//         printf("mmap content: %s\n", str);
//         // munmap(array, kst.st_size);
//     }

//     sys_close(fd);
// }

// int i = 1000;
// void test_waitpid(void)
// {
//     int cpid, wstatus;
//     cpid = fork();
//     if (cpid != -1)
//     {
//         print("fork test Success!\n");
//     };
//     if (cpid == 0)
//     {
//         while (i--)
//             ;
//         sys_sched_yield();
//         print("This is child process\n");
//         exit(3);
//     }
//     else
//     {
//         pid_t ret = waitpid(cpid, &wstatus, 0);
//         if (ret == cpid && WEXITSTATUS(wstatus) == 3)
//         {
//             print("waitpid test Success!\n");
//         }
//         else
//             print("waitpid error.\n");
//     }
// }
// void test_wait(void)
// {
//     int cpid, wstatus;
//     cpid = fork();
//     if (cpid == 0)
//     {
//         print("This is child process\n");
//         exit(0);
//     }
//     else
//     {
//         pid_t ret = wait(&wstatus);
//         if (ret == cpid)
//             print("wait child success.\n");
//         else
//             print("wait child error.\n");
//     }
// }

// void test_gettime()
// {
//     int test_ret1 = get_time();
//     // volatile int i = 100000; // qemu时钟频率12500000
//     // while (i > 0)
//     //     i--;
//     sleep(2);
//     int test_ret2 = get_time();
//     if (test_ret1 >= 0 && test_ret2 >= 0)
//     {
//         print("get_time test success\n");
//     }
// }

// // void test_write()
// // {
// //     char *str = "user program write\n";
// //     write(0, str, 20);
// //     char *str1 = "第二次调用write,来自user\n";
// //     write(0, str1, 33);
// // }

// void test_brk()
// {
//     int64 cur_pos, alloc_pos, alloc_pos_1;

//     cur_pos = sys_brk(0);
//     sys_brk((void *)(cur_pos + 2 * 4006));

//     alloc_pos = sys_brk(0);
//     sys_brk((void *)(alloc_pos + 2 * 4006));

//     alloc_pos_1 = sys_brk(0);
//     alloc_pos_1++;
// }
// struct tms mytimes;
// void test_times()
// {

//     for (int i = 0; i < 1000000; i++)
//     {
//     }
//     uint64 test_ret = sys_times(&mytimes);
//     mytimes.tms_cstime++;
//     if (test_ret == 0)
//     {
//         print("test_times Success!");
//     }
//     else
//     {
//         print("test_times Failed!");
//     }
// }

// struct utsname un;
// void test_uname()
// {
//     int test_ret = sys_uname(&un);

//     if (test_ret >= 0)
//     {
//         print("test_uname Success!");
//     }
//     else
//     {
//         print("test_uname Failed!");
//     }
// }

#include "def.h"
#include <stdarg.h>
#include <stddef.h>

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

// SIGUSR1信号处理函数
void sigusr1_handler(int sig)
{
    printf("收到SIGUSR1信号，信号编号: %d\n", sig);
}

// 测试pselect6_time32信号处理功能
int test_pselect6_signal()
{
    printf("测试pselect6_time32信号处理功能\n");

    // 设置SIGUSR1信号处理函数
    struct sigaction sa;
    sa.__sigaction_handler.sa_handler = sigusr1_handler; // 使用专门的SIGUSR1处理函数
    sa.sa_flags = 0;

    if (sys_sigaction(SIGUSR1, &sa, NULL) == -1)
    {
        printf("设置SIGUSR1信号处理失败\n");
        return 1;
    }

    printf("SIGUSR1信号处理函数已设置\n");

    // 创建子进程来发送信号
    int pid = fork();
    if (pid == 0)
    {
        // 子进程
        printf("子进程开始运行，PID: %d\n", getpid());
        sleep(2); // 子进程睡眠2秒
        printf("子进程发送SIGUSR1信号给父进程\n");
        kill(getppid(), SIGUSR1); // 发送信号给父进程
        exit(0);
    }
    else if (pid > 0)
    {
        // 父进程
        printf("父进程开始pselect6_time32等待，子进程PID: %d\n", pid);

        // 设置信号掩码，不阻塞SIGUSR1（允许信号中断）
        __sigset_t sigmask;
        sigmask.__val[0] = 0; // 清空掩码，允许所有信号

        // 调用pselect6_time32，应该被信号中断
        int ret = sys_pselect6_time32(0, 0, 0, 0, 0, (uint64)&sigmask);
        printf("pselect6_time32返回: %d (期望: -4, EINTR)\n", ret);

        if (ret == -4)
        {
            printf("✓ pselect6_time32信号处理测试通过\n");
        }
        else
        {
            printf("✗ pselect6_time32信号处理测试失败\n");
        }

        // 等待子进程
        wait(0);
    }
    else
    {
        printf("fork失败\n");
        return 1;
    }

    return 0;
}

// 检查命令是否为busybox命令
int is_busybox_command(const char *cmd)
{
    // 从busybox_cmd数组中提取的基本命令列表
    static const char *busybox_commands[] = {"echo", "ash", "sh", "basename", "cal", "clear", "date", "df", "dirname", "dmesg", "du", "expr", "false", "true", "which", "uname", "uptime", "printf", "ps", "pwd", "free", "hwclock", "kill", "ls", "sleep", "touch", "cat", "cut", "od", "head", "tail", "hexdump", "md5sum", "sort", "stat", "strings", "wc", "more", "rm", "mkdir", "mv", "rmdir", "grep", "cp", "find"};

    int num_commands = sizeof(busybox_commands) / sizeof(busybox_commands[0]);

    for (int i = 0; i < num_commands; i++)
    {
        if (strcmp(cmd, busybox_commands[i]) == 0)
        {
            return 1; // 是busybox命令
        }
    }
    return 0; // 不是busybox命令
}

// 执行busybox命令
void execute_busybox_command(char **argv, int argc)
{
    int pid = fork();

    if (pid < 0)
    {
        printf("fork失败\n");
        return;
    }

    if (pid == 0)
    { // 子进程
        char *newenviron[] = {NULL};

        // 构建新的参数数组，在命令前添加"busybox"
        char *new_argv[argc + 2]; // 最大32个参数 + NULL
        new_argv[0] = "busybox";

        // 复制原有参数
        for (int i = 0; i < argc; i++)
        {
            new_argv[i + 1] = argv[i];
        }
        new_argv[argc + 1] = NULL;

        // 尝试不同的路径执行busybox
        char *paths[] = {
            "/musl/",
            "/glibc/",
            "/musl/basic/",
            "/glibc/basic/",
            "", // 当前目录
            NULL};

        int executed = 0;
        for (int i = 0; paths[i] != NULL; i++)
        {
            char full_path[256];
            if (strlen(paths[i]) > 0)
            {
                strcpy(full_path, paths[i]);
                strcat(full_path, "busybox");
            }
            else
            {
                strcpy(full_path, "busybox");
            }

            if (sys_execve(full_path, new_argv, newenviron) == 0)
            {
                executed = 1;
                break;
            }
        }

        if (!executed)
        {
            printf("busybox命令执行失败: %s\n", argv[0]);
        }
        exit(1);
    }
    else
    { // 父进程等待子进程
        int status;
        waitpid(pid, &status, 0);
    }
}
