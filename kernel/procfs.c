#include "types.h"
#include "print.h"
#include "process.h"
#include "string.h"
#include "fs.h"
#include "file.h"
#include "fs_defs.h"
#include "errno-base.h"
#include "spinlock.h"

extern struct proc pool[NPROC];

// 中断计数器
static uint64 interrupt_counts[256];  // 支持256个中断号
static struct spinlock interrupt_lock;

// 初始化中断计数器
void init_interrupt_counts(void) {
    initlock(&interrupt_lock, "interrupt_counts");
    for (int i = 0; i < 256; i++) {
        interrupt_counts[i] = 0;
    }
}

// 增加中断计数
void increment_interrupt_count(int irq) {
    if (irq >= 0 && irq < 256) {
        acquire(&interrupt_lock);
        interrupt_counts[irq]++;
        release(&interrupt_lock);
    }
}

// 检查路径是否为 /proc/pid/stat
int is_proc_pid_stat(const char *path, int *pid) {
    if (!path || !pid) return 0;
    if (strncmp(path, "/proc/", 6) != 0) return 0;
    const char *pid_start = path + 6;
    const char *slash = strchr(pid_start, '/');
    if (!slash || strcmp(slash, "/stat") != 0) return 0;
    char pid_str[16];
    int pid_len = slash - pid_start;
    if(!strncmp(pid_start,"self",4)){
        *pid = myproc()->pid;
    }else{
        if (pid_len <= 0 || pid_len >= sizeof(pid_str)) return 0;
        strncpy(pid_str, pid_start, pid_len);
        pid_str[pid_len] = '\0';
        for (int i = 0; i < pid_len; i++) {
            if (pid_str[i] < '0' || pid_str[i] > '9') return 0;
        }
        *pid = atoi(pid_str);
    }
    return 1;
}


// 检查路径是否为 /proc/pid/stat
int is_proc_pid_status(const char *path, int *pid) {
    if (!path || !pid) return 0;
    if (strncmp(path, "/proc/", 6) != 0) return 0;
    const char *pid_start = path + 6;
    const char *slash = strchr(pid_start, '/');
    if (!slash || strcmp(slash, "/status") != 0) return 0;
    char pid_str[16];
    int pid_len = slash - pid_start;
    if(!strncmp(pid_start,"self",4)){
        *pid = myproc()->pid;
    }else{
        if (pid_len <= 0 || pid_len >= sizeof(pid_str)) return 0;
        strncpy(pid_str, pid_start, pid_len);
        pid_str[pid_len] = '\0';
        for (int i = 0; i < pid_len; i++) {
            if (pid_str[i] < '0' || pid_str[i] > '9') return 0;
        }
        *pid = atoi(pid_str);
    }
    return 1;
}


//检查路径是否为 /proc/sys/kernel/pid_max
int is_proc_kernel_pidmax(const char *path){
    if (!path) return 0;
    if (strncmp(path, "/proc/sys/kernel/pid_max", 24) != 0) return 0;
    return 1;
}

//检查路径是否为 /proc/sys/kernel/tainted
int is_proc_kernel_tainted(const char *path){
    if (!path) return 0;
    char *addr = "/proc/sys/kernel/tainted";
    if (strncmp(path, addr, sizeof(addr)) != 0) return 0;
    return 1;
}

// 检查路径是否为 /proc/interrupts
int is_proc_interrupts(const char *path) {
    if (!path) return 0;
    if (strcmp(path, "/proc/interrupts") == 0) return 1;
    return 0;
}

// 生成 /proc/interrupts 文件内容
int generate_proc_interrupts_content(char *buf, int size) {
    if (!buf || size <= 0) return -1;
    
    acquire(&interrupt_lock);
    
    int written = 0;
    int first = 1;
    
    // 遍历所有中断号，只输出有计数的中断
    for (int i = 0; i < 256; i++) {
        if (interrupt_counts[i] > 0) {
            int len = snprintf(buf + written, size - written, "%s%d: %*lu\n", 
                              first ? "" : "", i, 8, interrupt_counts[i]);
            if (len >= size - written) {
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


// 生成 /proc/pid/stat 内容，未实现字段填0
int generate_proc_stat_content(int pid, char *buf, int size) {
    if (!buf || size <= 0) return -1;
    struct proc *p = getproc(pid);
    if (!p) return -1;
    
    acquire(&p->lock);
    if (p->state == UNUSED) {
        release(&p->lock);
        return -1;
    }
    
    // 确保utime在第14字段，stime在第15字段
    unsigned long utime = p->utime * 100;
    unsigned long stime = p->ktime * 100;
    int state = p->state;
    char state_str = ' ';
    if(state == RUNNING ) state_str = 'R';
    else if (state == RUNNABLE) state_str = 'R';
    else if(state == SLEEPING) state_str = 'S';
    else if(state == ZOMBIE) state_str = 'Z';
    else {
        panic("state unknown");
    }
    int ppid;
    if(p->parent){
        ppid = p->parent->pid;
    }else{
        ppid = 0;
    }
    int pgid = p->pgid;
    
    // 修正的格式：前13字段 + utime(14) + stime(15)
    int written = snprintf(buf, size, 
        "%d (init) %c %d %d %d %d %d %d %d %d %d %d "  // 字段1-13
        "%lu %lu "  // 字段14-15: utime/stime
        "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n",  // 后续字段
        pid, state_str,
        ppid, pgid, 0, 0, 0, 0, 0, 0, 0, 0,   // 字段4-13填0
        utime, stime);
    
    release(&p->lock);
    return written;
}

// 生成 /proc/self/status 文件内容
int generate_proc_status_content(int pid, char *buf, int size) {
    if (!buf || size <= 0) return -1;
    
    struct proc *p = getproc(pid);
    if (!p) return -1;
    
    acquire(&p->lock);
    if (p->state == UNUSED) {
        release(&p->lock);
        return -1;
    }
    
    // 准备进程状态信息
    char state_char = '?';
    switch(p->state) {
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
            state_char = 'U';  // 未知状态
    }
    
    int ppid = p->parent ? p->parent->pid : 0;
    int uid = p->euid;   // 用户ID
    int gid = p->egid;   // 组ID
    int threads = 1;    // 单线程系统
    
    // 内存使用统计 (单位: kB)
    unsigned long vm_size = p->sz / 1024;            // 虚拟内存大小
    unsigned long vm_rss = 10 * 4;    // 驻留集大小 (假设页大小4KB)
    
    // 生成状态文件内容
    int written = snprintf(buf, size,
        "Name:\t%s\n"                // 1. 进程名
        "State:\t%c\n"               // 2. 状态 (R/S/Z)
        "Tgid:\t%d\n"               // 3. 线程组ID (同PID)
        "Pid:\t%d\n"                // 4. 进程ID
        "PPid:\t%d\n"               // 5. 父进程ID
        "Uid:\t%d\t%d\t%d\t%d\n"    // 6. 用户ID (真实/有效/保存/文件系统)
        "Gid:\t%d\t%d\t%d\t%d\n"    // 7. 组ID (同上)
        "VmSize:\t%lu kB\n"         // 8. 虚拟内存大小
        "VmRSS:\t%lu kB\n"          // 9. 驻留集大小
        "Threads:\t%d\n"            // 10. 线程数
        "FDSize:\t%d\n"             // 11. 文件描述符数量
        "SigQ:\t0/0\n"              // 12. 信号队列
        "CapInh:\t0000000000000000\n" // 13. 继承的能力集
        "Seccomp:\t0\n"             // 14. seccomp模式
        "voluntary_ctxt_switches:\t%u\n"  // 15. 主动上下文切换
        "nonvoluntary_ctxt_switches:\t%u\n", // 16. 非主动上下文切换
        
        // 字段值
        "init",                   // Name
        state_char,                // State
        pid,                       // Tgid (同PID)
        pid,                       // Pid
        ppid,                      // PPid
        uid, uid, uid, uid,        // Uid (四组相同)
        gid, gid, gid, gid,        // Gid (四组相同)
        vm_size,                   // VmSize
        vm_rss,                    // VmRSS
        threads,                   // Threads
        0,                         // FDSize
        0,                         // voluntary_ctxt_switches
        0                          // nonvoluntary_ctxt_switches
    );
    
    release(&p->lock);
    return written;
}


int 
check_proc_path(const char *path, int *stat_pid)
{
    if (is_proc_pid_stat(path, stat_pid))
    {
        return FD_PROC_STAT;
    }
    else if(is_proc_pid_status(path, stat_pid))
    {
        return  FD_PROC_STATUS;
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
    else
        return 0;    ///< 非procfs文件 
}