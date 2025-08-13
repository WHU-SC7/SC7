#include "types.h"
#include "print.h"
#include "process.h"
#include "thread.h"
#include "string.h"
#include "fs.h"
#include "file.h"
#include "fs_defs.h"
#include "errno-base.h"
#include "spinlock.h"
#include "procfs.h"

#define MAX_PRTH_INFO 100

extern struct proc pool[NPROC];

// 中断计数器
static uint64 interrupt_counts[256]; // 支持256个中断号
static struct spinlock interrupt_lock;
static prth_info_t prth_table[MAX_PRTH_INFO];

void init_prth_info_table(void)
{
    for (int i = 0; i < MAX_PRTH_INFO; ++i)
    {
        prth_table[i].valid = false;
        prth_table[i].pid = 0;
        prth_table[i].tid = 0;
        prth_table[i].type = 0;
    }
}

prth_info_t *allocprthinfo(void)
{
    for (int i = 0; i < MAX_PRTH_INFO; ++i)
    {
        if (!prth_table[i].valid)
        {
            prth_table[i].valid = true;
            return &prth_table[i];
        }
    }
    return NULL;
}

void freeprthinfo(prth_info_t *prth_i)
{
    if (prth_i)
    {
        prth_i->valid = false;
    }
}

// 初始化中断计数器
void init_interrupt_counts(void)
{
    initlock(&interrupt_lock, "interrupt_counts");
    for (int i = 0; i < 256; i++)
    {
        interrupt_counts[i] = 0;
    }
}

// 增加中断计数
void increment_interrupt_count(int irq)
{
    if (irq >= 0 && irq < 256)
    {
        acquire(&interrupt_lock);
        interrupt_counts[irq]++;
        release(&interrupt_lock);
    }
}

// 检查路径是否为 /proc/pid/stat
int is_proc_pid_stat(const char *path, int *pid)
{
    if (!path || !pid)
        return 0;
    if (strncmp(path, "/proc/", 6) != 0)
        return 0;
    const char *pid_start = path + 6;
    const char *slash = strchr(pid_start, '/');
    if (!slash || strcmp(slash, "/stat") != 0)
        return 0;
    char pid_str[16];
    int pid_len = slash - pid_start;
    if (!strncmp(pid_start, "self", 4))
    {
        *pid = myproc()->pid;
    }
    else
    {
        if (pid_len <= 0 || pid_len >= sizeof(pid_str))
            return 0;
        strncpy(pid_str, pid_start, pid_len);
        pid_str[pid_len] = '\0';
        for (int i = 0; i < pid_len; i++)
        {
            if (pid_str[i] < '0' || pid_str[i] > '9')
                return 0;
        }
        *pid = atoi(pid_str);
    }
    return 1;
}

// 检查路径是否为 /proc/pid/status
int is_proc_pid_status(const char *path, int *pid)
{
    if (!path || !pid)
        return 0;
    if (strncmp(path, "/proc/", 6) != 0)
        return 0;
    const char *pid_start = path + 6;
    const char *slash = strchr(pid_start, '/');
    if (!slash || strcmp(slash, "/status") != 0)
        return 0;
    char pid_str[16];
    int pid_len = slash - pid_start;
    if (!strncmp(pid_start, "self", 4))
    {
        *pid = myproc()->pid;
    }
    else
    {
        if (pid_len <= 0 || pid_len >= sizeof(pid_str))
            return 0;
        strncpy(pid_str, pid_start, pid_len);
        pid_str[pid_len] = '\0';
        for (int i = 0; i < pid_len; i++)
        {
            if (pid_str[i] < '0' || pid_str[i] > '9')
                return 0;
        }
        *pid = atoi(pid_str);
    }
    return 1;
}

// 检查路径是否为 /proc/sys/kernel/pid_max
int is_proc_kernel_pidmax(const char *path)
{
    if (!path)
        return 0;
    if (strncmp(path, "/proc/sys/kernel/pid_max", 24) != 0)
        return 0;
    return 1;
}

// 检查路径是否为 /proc/sys/kernel/tainted
int is_proc_kernel_tainted(const char *path)
{
    if (!path)
        return 0;
    char *addr = "/proc/sys/kernel/tainted";
    if (strncmp(path, addr, sizeof(addr)) != 0)
        return 0;
    return 1;
}

// 检查路径是否为 /proc/interrupts
int is_proc_interrupts(const char *path)
{
    if (!path)
        return 0;
    if (strcmp(path, "/proc/interrupts") == 0)
        return 1;
    return 0;
}

// 检查路径是否为 /proc/cpuinfo
int is_proc_cpuinfo(const char *path)
{
    if (!path)
        return 0;
    if (strcmp(path, "/proc/cpuinfo") == 0)
        return 1;
    return 0;
}

// 检查路径是否为 /proc/pid/task
int is_proc_pid_task_dir(const char *path, int *pid)
{
    if (!path || !pid)
        return 0;
    if (strncmp(path, "/proc/", 6) != 0)
        return 0;

    const char *pid_start = path + 6;
    const char *slash = strchr(pid_start, '/');
    if (!slash)
        return 0;

    // 处理 /task 或 /task/ 两种情况
    if (strncmp(slash, "/task", 5) != 0)
        return 0;

    // 检查是否是 /task 结尾或 /task/ 结尾
    if (slash[5] != '\0' && !(slash[5] == '/' && slash[6] == '\0'))
        return 0;

    char pid_str[16];
    int pid_len = slash - pid_start;
    if (!strncmp(pid_start, "self", 4))
    {
        *pid = myproc()->pid;
    }
    else
    {
        if (pid_len <= 0 || pid_len >= sizeof(pid_str))
            return 0;
        strncpy(pid_str, pid_start, pid_len);
        pid_str[pid_len] = '\0';
        for (int i = 0; i < pid_len; i++)
        {
            if (pid_str[i] < '0' || pid_str[i] > '9')
                return 0;
        }
        *pid = atoi(pid_str);
    }
    return 1;
}

// 检查路径是否为 /proc/pid/task/tid/stat
int is_proc_pid_task_tid_stat(const char *path, int *pid, int *tid)
{
    if (!path || !pid || !tid)
        return 0;
    if (strncmp(path, "/proc/", 6) != 0)
        return 0;

    const char *pid_start = path + 6;
    const char *first_slash = strchr(pid_start, '/');
    if (!first_slash || strncmp(first_slash, "/task/", 6) != 0)
        return 0;

    const char *tid_start = first_slash + 6;
    const char *second_slash = strchr(tid_start, '/');
    if (!second_slash || strcmp(second_slash, "/stat") != 0)
        return 0;

    // 解析PID
    char pid_str[16];
    int pid_len = first_slash - pid_start;
    if (!strncmp(pid_start, "self", 4))
    {
        *pid = myproc()->pid;
    }
    else
    {
        if (pid_len <= 0 || pid_len >= sizeof(pid_str))
            return 0;
        strncpy(pid_str, pid_start, pid_len);
        pid_str[pid_len] = '\0';
        for (int i = 0; i < pid_len; i++)
        {
            if (pid_str[i] < '0' || pid_str[i] > '9')
                return 0;
        }
        *pid = atoi(pid_str);
    }

    // 解析TID
    char tid_str[16];
    int tid_len = second_slash - tid_start;
    if (tid_len <= 0 || tid_len >= sizeof(tid_str))
        return 0;
    strncpy(tid_str, tid_start, tid_len);
    tid_str[tid_len] = '\0';
    for (int i = 0; i < tid_len; i++)
    {
        if (tid_str[i] < '0' || tid_str[i] > '9')
            return 0;
    }
    *tid = atoi(tid_str);

    return 1;
}

// 生成 /proc/interrupts 文件内容
int generate_proc_interrupts_content(char *buf, int size)
{
    if (!buf || size <= 0)
        return -1;

    acquire(&interrupt_lock);

    int written = 0;
    int first = 1;

    // 遍历所有中断号，只输出有计数的中断
    for (int i = 0; i < 256; i++)
    {
        if (interrupt_counts[i] > 0)
        {
            int len = snprintf(buf + written, size - written, "%s%d: %*lu\n",
                               first ? "" : "", i, 8, interrupt_counts[i]);
            if (len >= size - written)
            {
                // 缓冲区不够，停止写入
                break;
            }
            written += len;
            first = 0;
        }
    }

    release(&interrupt_lock);
    return written;
}

// 生成 /proc/cpuinfo 文件内容
int generate_proc_cpuinfo_content(char *buf, int size)
{
    if (!buf || size <= 0)
        return -1;

    int written = 0;

    // 为处理器 0 生成 cpuinfo
#ifdef RISCV
    written += snprintf(buf + written, size - written,
                        "processor\t: 0\n"
                        "hart\t\t: 0\n"
                        "isa\t\t: rv64imafdc\n"
                        "mmu\t\t: sv39\n"
                        "uarch\t\t: sifive,u74-mc\n");
#else
    written += snprintf(buf + written, size - written,
                        "system type\t\t: Generic Loongson64 System\n"
                        "machine\t\t\t: Loongson-3A5000\n"
                        "processor\t\t: 0\n"
                        "package\t\t\t: 0\n"
                        "core\t\t\t: 0\n"
                        "cpu family\t\t: Loongson-64bit\n"
                        "model name\t\t: Loongson-3A5000\n"
                        "cache size\t\t: 1024 KB\n"
                        "cpufreq\t\t\t: 2500MHz\n"
                        "ASEs implemented\t:\n"
                        "micro-revision\t\t: 1\n"
                        "flags\t\t\t: cpucfg lam ual fpu lsx lasx crc32 complex crypto\n"
                        "BogoMIPS\t\t: 5000.00\n");
#endif

    return written;
}

// 生成 /proc/pid/stat 内容，未实现字段填0
int generate_proc_stat_content(int pid, char *buf, int size)
{
    if (!buf || size <= 0)
        return -1;
    struct proc *p = getproc(pid);
    if (!p)
        return -1;

    acquire(&p->lock);
    if (p->state == UNUSED)
    {
        release(&p->lock);
        return -1;
    }

    // 确保utime在第14字段，stime在第15字段
    unsigned long utime = p->utime * 100;
    unsigned long stime = p->ktime * 100;

    // 根据主线程状态判断进程状态
    char state_str = ' ';
    if (!p->main_thread)
    {
        state_str = 'Z'; // 没有主线程的进程视为僵尸进程
    }
    else
    {
        int thread_state = p->main_thread->state;
        if (thread_state == t_RUNNING)
            state_str = 'R';
        else if (thread_state == t_RUNNABLE)
            state_str = 'R';
        else if (thread_state == t_SLEEPING || thread_state == t_TIMING)
            state_str = 'S';
        else if (thread_state == t_ZOMBIE)
            state_str = 'Z';
        else if (thread_state == t_UNUSED)
            state_str = 'Z';
        else
        {
            state_str = 'S'; // 默认为睡眠状态
        }
    }
    int ppid;
    if (p->parent)
    {
        ppid = p->parent->pid;
    }
    else
    {
        ppid = 0;
    }
    int pgid = p->pgid;

    // 修正的格式：增加更多字段以符合标准 /proc/pid/stat 格式
    int written = snprintf(buf, size,
                           "%d (init) %c %d %d %d %d %d %d %u %lu %lu %lu "        // 字段1-13
                           "%lu %lu "                                              // 字段14-15: utime/stime
                           "%ld %ld %ld %ld %ld %ld %llu %lu %ld %lu %lu %lu %lu " // 字段16-28
                           "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %d %d %u %u "  // 字段29-42
                           "%llu %lu %ld %lu %lu %lu %lu %lu %lu %lu %ld %ld\n",   // 字段43-52
                           pid, state_str,
                           ppid, pgid, 0, 0, 0, 0, 0, 0UL, 0UL, 0UL,                       // 字段4-13
                           utime, stime,                                                   // 字段14-15
                           0L, 0L, 0L, 0L, 0L, 0L, 0ULL, 0UL, 0L, 0UL, 0UL, 0UL, 0UL,      // 字段16-28
                           0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0, 0, 0U, 0U, // 字段29-42
                           0ULL, 0UL, 0L, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0L, 0L);      // 字段43-52

    release(&p->lock);
    return written;
}

// 生成 /proc/pid/task 目录内容
int generate_proc_task_dir_content(int pid, char *buf, int size)
{
    if (!buf || size <= 0)
        return -1;

    struct proc *p = getproc(pid);
    if (!p)
        return -1;

    acquire(&p->lock);
    if (p->state == UNUSED)
    {
        release(&p->lock);
        return -1;
    }

    int written = 0;

    int thread_count = 0;

    // 添加. 和 .. 目录项
    if (written + 24 < size)
    {
        written += snprintf(buf + written, size - written,
                            "%d %s %d %d\n", 1, ".", DT_DIR, 24);
    }
    if (written + 24 < size)
    {
        written += snprintf(buf + written, size - written,
                            "%d %s %d %d\n", 2, "..", DT_DIR, 24);
    }

    // // 添加主线程
    // if (p->main_thread && written + 32 < size)
    // {
    //     written += snprintf(buf + written, size - written,
    //                         "%d %d %d %d\n", p->main_thread->tid, p->main_thread->tid, DT_DIR, 32);
    //     thread_count++;
    //     DEBUG_LOG_LEVEL(LOG_DEBUG, "[task_dir] 添加主线程 tid=%d, 状态=%d\n", p->main_thread->tid, p->main_thread->state);
    // }

    // 遍历进程的其他线程
    struct list_elem *e;
    for (e = list_begin(&p->thread_queue); e != list_end(&p->thread_queue); e = list_next(e))
    {
        if (written + 32 >= size)
            break;

        thread_t *t = list_entry(e, thread_t, elem);
        if (t != p->main_thread && t->state != t_UNUSED)
        {
            written += snprintf(buf + written, size - written,
                                "%d %d %d %d\n", t->tid, t->tid, DT_DIR, 32);
            thread_count++;
            DEBUG_LOG_LEVEL(LOG_DEBUG, "[task_dir] 添加线程 tid=%d, 状态=%d\n", t->tid, t->state);
        }
    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[task_dir] 进程 %d 总共有 %d 个线程\n", pid, thread_count);
    release(&p->lock);
    return written;
}

// 根据PID和TID查找线程
thread_t *get_thread_by_pid_tid(int pid, int tid)
{
    struct proc *p = getproc(pid);
    if (!p)
        return NULL;

    acquire(&p->lock);
    if (p->state == UNUSED)
    {
        release(&p->lock);
        return NULL;
    }

    thread_t *result = NULL;

    // 检查主线程
    if (p->main_thread && p->main_thread->tid == tid)
    {
        result = p->main_thread;
        release(&p->lock);
        return result;
    }

    // 检查其他线程
    struct list_elem *e;
    for (e = list_begin(&p->thread_queue); e != list_end(&p->thread_queue); e = list_next(e))
    {
        thread_t *t = list_entry(e, thread_t, elem);
        if (t->tid == tid && t->state != t_UNUSED)
        {
            result = t;
            break;
        }
    }

    release(&p->lock);
    return result;
}

// 生成 /proc/pid/task/tid/stat 内容
int generate_proc_task_tid_stat_content(int pid, int tid, char *buf, int size)
{
    if (!buf || size <= 0)
        return -1;

    thread_t *t = get_thread_by_pid_tid(pid, tid);
    if (!t)
        return -1;

    struct proc *p = t->p;
    if (!p)
        return -1;

    acquire(&p->lock);

    // 确保线程时间在第14字段，线程时间在第15字段
    unsigned long utime = p->utime * 100;
    unsigned long stime = p->ktime * 100;

    // 根据线程状态判断状态字符
    char state_str = ' ';
    int thread_state = t->state;
    if (thread_state == t_RUNNING)
        state_str = 'R';
    else if (thread_state == t_RUNNABLE)
        state_str = 'R';
    else if (thread_state == t_SLEEPING || thread_state == t_TIMING)
        state_str = 'S';
    else if (thread_state == t_ZOMBIE)
        state_str = 'Z';
    else if (thread_state == t_UNUSED)
        state_str = 'Z';
    else
        state_str = 'S'; // 默认为睡眠状态

    int ppid = p->parent ? p->parent->pid : 0;
    int pgid = p->pgid;

    // 生成线程stat文件内容，格式与进程stat相同，但使用线程ID
    int written = snprintf(buf, size,
                           "%d (thread) %c %d %d %d %d %d %d %u %lu %lu %lu %lu "  // 字段1-13
                           "%lu %lu "                                              // 字段14-15: utime/stime
                           "%ld %ld %ld %ld %ld %ld %llu %lu %ld %lu %lu %lu %lu " // 字段16-28
                           "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %d %d %u %u "  // 字段29-42
                           "%llu %lu %ld %lu %lu %lu %lu %lu %lu %lu %ld %ld\n",   // 字段43-52
                           tid, state_str,
                           ppid, pgid, 0, 0, 0, 0, 0, 0UL, 0UL, 0UL, 0UL,                  // 字段4-13
                           utime, stime,                                                   // 字段14-15
                           0L, 0L, 0L, 0L, 0L, 0L, 0ULL, 0UL, 0L, 0UL, 0UL, 0UL, 0UL,      // 字段16-28
                           0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0, 0, 0U, 0U, // 字段29-42
                           0ULL, 0UL, 0L, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0L, 0L);      // 字段43-52

    release(&p->lock);
    return written;
}

// 生成 /proc/self/status 文件内容
int generate_proc_status_content(int pid, char *buf, int size)
{
    if (!buf || size <= 0)
        return -1;

    struct proc *p = getproc(pid);
    if (!p)
        return -1;

    acquire(&p->lock);
    if (p->state == UNUSED)
    {
        release(&p->lock);
        return -1;
    }

    // 准备进程状态信息
    char state_char = '?';
    switch (p->state)
    {
    case RUNNING:
    case RUNNABLE:
        state_char = 'R';
        break;
    case SLEEPING:
        state_char = 'S';
        break;
    case ZOMBIE:
        state_char = 'Z';
        break;
    default:
        state_char = 'U'; // 未知状态
    }

    int ppid = p->parent ? p->parent->pid : 0;
    int uid = p->euid; // 用户ID
    int gid = p->egid; // 组ID
    int threads = 1;   // 单线程系统

    // 内存使用统计 (单位: kB)
    unsigned long vm_size = p->sz / 1024; // 虚拟内存大小
    unsigned long vm_rss = 10 * 4;        // 驻留集大小 (假设页大小4KB)

    // 生成状态文件内容
    int written = snprintf(buf, size,
                           "Name:\t%s\n"                        // 1. 进程名
                           "State:\t%c\n"                       // 2. 状态 (R/S/Z)
                           "Tgid:\t%d\n"                        // 3. 线程组ID (同PID)
                           "Pid:\t%d\n"                         // 4. 进程ID
                           "PPid:\t%d\n"                        // 5. 父进程ID
                           "Uid:\t%d\t%d\t%d\t%d\n"             // 6. 用户ID (真实/有效/保存/文件系统)
                           "Gid:\t%d\t%d\t%d\t%d\n"             // 7. 组ID (同上)
                           "VmSize:\t%lu kB\n"                  // 8. 虚拟内存大小
                           "VmRSS:\t%lu kB\n"                   // 9. 驻留集大小
                           "Threads:\t%d\n"                     // 10. 线程数
                           "FDSize:\t%d\n"                      // 11. 文件描述符数量
                           "SigQ:\t0/0\n"                       // 12. 信号队列
                           "CapInh:\t0000000000000000\n"        // 13. 继承的能力集
                           "Seccomp:\t0\n"                      // 14. seccomp模式
                           "voluntary_ctxt_switches:\t%u\n"     // 15. 主动上下文切换
                           "nonvoluntary_ctxt_switches:\t%u\n", // 16. 非主动上下文切换

                           // 字段值
                           "init",             // Name
                           state_char,         // State
                           pid,                // Tgid (同PID)
                           pid,                // Pid
                           ppid,               // PPid
                           uid, uid, uid, uid, // Uid (四组相同)
                           gid, gid, gid, gid, // Gid (四组相同)
                           vm_size,            // VmSize
                           vm_rss,             // VmRSS
                           threads,            // Threads
                           0,                  // FDSize
                           0,                  // voluntary_ctxt_switches
                           0                   // nonvoluntary_ctxt_switches
    );

    release(&p->lock);
    return written;
}

int check_proc_path(const char *path, int *stat_pid, int *stat_tid)
{
    if (is_proc_pid_stat(path, stat_pid))
    {
        return FD_PROC_STAT;
    }
    else if (is_proc_pid_status(path, stat_pid))
    {
        return FD_PROC_STATUS;
    }
    else if (is_proc_pid_task_tid_stat(path, stat_pid, stat_tid))
    {
        return FD_PROC_TASK_STAT;
    }
    else if (is_proc_pid_task_dir(path, stat_pid))
    {
        return FD_PROC_TASK_DIR;
    }
    else if (is_proc_kernel_pidmax(path))
    {
        return FD_PROC_PIDMAX;
    }
    else if (is_proc_kernel_tainted(path))
    {
        return FD_PROC_TAINTED;
    }
    else if (is_proc_interrupts(path))
    {
        return FD_PROC_INTERRUPTS;
    }
    else if (is_proc_cpuinfo(path))
    {
        return FD_PROC_CPUINFO;
    }
    else
        return 0; ///< 非procfs文件
}

// 新增函数用于检查task/tid/stat文件，需要额外的tid参数
int check_proc_task_tid_stat(const char *path, int *pid, int *tid)
{
    if (is_proc_pid_task_tid_stat(path, pid, tid))
    {
        return FD_PROC_TASK_STAT;
    }
    return 0;
}