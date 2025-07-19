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
    if (pid_len <= 0 || pid_len >= sizeof(pid_str)) return 0;
    strncpy(pid_str, pid_start, pid_len);
    pid_str[pid_len] = '\0';
    for (int i = 0; i < pid_len; i++) {
        if (pid_str[i] < '0' || pid_str[i] > '9') return 0;
    }
    *pid = atoi(pid_str);
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
    // 这里只简单填充部分字段，其他填0
    int written = snprintf(buf, size, "%d (init) R 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n", pid);
    release(&p->lock);
    return written;
} 