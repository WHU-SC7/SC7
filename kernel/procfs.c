#include "types.h"
#include "print.h"
#include "process.h"
#include "string.h"
#include "fs.h"
#include "file.h"
#include "fs_defs.h"
#include "errno-base.h"

extern struct proc pool[NPROC];

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
    
    // 修正的格式：前13字段 + utime(14) + stime(15)
    int written = snprintf(buf, size, 
        "%d (init) R %d %d %d %d %d %d %d %d %d %d "  // 字段1-13
        "%lu %lu "  // 字段14-15: utime/stime
        "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n",  // 后续字段
        pid,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 字段4-13填0
        utime, stime);
    
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
    else if (is_proc_kernel_pidmax(path))
    {
        return FD_PROC_PIDMAX;
    }
    else if (is_proc_kernel_tainted(path))
    {
        return FD_PROC_TAINTED;
    }
    else
        return 0;    ///< 非procfs文件 
}