#ifndef __SH_H__
#define __SH_H__

#include "def.h"
#include "string.h"
#include "print.h"

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

void parse_command(char *line, char **argv, int *argc)
{
    *argc = 0;
    char *src = line;
    char *dst = line;

    // 预处理：过滤控制字符（保留空格和制表符）
    while (*src)
    {
        if (*src == '\n' || *src == '\0')
        {
            break; // 遇到换行或结束符停止
        }
        // 保留可打印字符、空格、制表符
        if (*src >= ' ' || *src == '\t')
        {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0'; // 终止字符串

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
        static char cwd[256] = {0};
        sys_getcwd(cwd, sizeof(cwd));
        int use_glibc = 0, use_musl = 0;
        if (strstr(cwd, "glibc") || (argc > 0 && strstr(argv[0], "glibc")))
        {
            use_glibc = 1;
        }
        else if (strstr(cwd, "musl") || (argc > 0 && strstr(argv[0], "musl")))
        {
            use_musl = 1;
        }
        const char *paths_default[] = {"", "/musl/", "/glibc/", "/musl/basic/", "/glibc/basic/", NULL};
        const char *paths_glibc[] = {"", "/glibc/", "/glibc/basic/", "/musl/", "/musl/basic/", NULL};
        const char *paths_musl[] = {"", "/musl/", "/musl/basic/", "/glibc/", "/glibc/basic/", NULL};
        const char **paths;
        if (use_glibc)
        {
            paths = paths_glibc;
        }
        else if (use_musl)
        {
            paths = paths_musl;
        }
        else
        {
            paths = paths_default;
        }
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

void run_shell(const char *prefix)
{
    char line[256];
    char *argv[32];
    int argc;

    printf("欢迎使用SC7 Shell!\n");
    printf("输入 'help' 查看可用命令，输入 'exit' 退出shell\n\n");
    int auto_executed = 0;
    while (1)
    {
        // 检查是否需要自动执行prefix命令
        if (prefix != NULL && !auto_executed)
        {
            strncpy(line, prefix, sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';      // 确保终止
            auto_executed = 1;                  // 标记已执行
            printf("(自动执行: %s)\n", prefix); // 显示提示信息
            parse_command(line, argv, &argc);
            execute_command(argv, argc);
        }
        else
        {
            break;
        }
    }

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
        else if (strcmp(argv[0], "cd") == 0)
        {
            char *path = (argc > 1) ? argv[1] : "/";
            if (sys_chdir(path) < 0)
            {
                printf("切换目录失败: %s\n", path);
            }
        }
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

#endif