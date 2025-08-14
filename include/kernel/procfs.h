#ifndef _PROCFS_H_
#define _PROCFS_H_

#include "types.h"

typedef struct thread thread_t; // 前向声明

typedef struct prth_info
{
    int type;
    tid_t tid;
    pid_t pid;
    bool valid;
} prth_info_t;

#define FD_PROC_STAT 100       // /proc/pid/stat 虚拟文件类型
#define FD_PROC_STATUS 101     // /proc/pid/status 虚拟文件类型
#define FD_PROC_PIDMAX 110     // /proc/sys/kernel/pidmax
#define FD_PROC_TAINTED 111    // /proc/sys/kernel/tainted
#define FD_PROC_INTERRUPTS 112 // /proc/interrupts 虚拟文件类型
#define FD_PROC_CPUINFO 113    // /proc/cpuinfo 虚拟文件类型
#define FD_PROC_TASK_DIR 114   // /proc/pid/task/ 目录类型
#define FD_PROC_TASK_STAT 115  // /proc/pid/task/tid/stat 虚拟文件类型

void init_prth_info_table(void);
prth_info_t *allocprthinfo(void);
void freeprthinfo(prth_info_t *prth_i);
int is_proc_pid_stat(const char *path, int *pid);
int is_proc_kernel_pidmax(const char *path);
int is_proc_kernel_tainted(const char *path);
int is_proc_pid_status(const char *path, int *pid);
int is_proc_interrupts(const char *path);
int is_proc_cpuinfo(const char *path);
int is_proc_pid_task_dir(const char *path, int *pid);
int is_proc_pid_task_tid_stat(const char *path, int *pid, int *tid);
int generate_proc_stat_content(int pid, char *buf, int size);
int generate_proc_status_content(int pid, char *buf, int size);
int generate_proc_interrupts_content(char *buf, int size);
int generate_proc_cpuinfo_content(char *buf, int size);
int generate_proc_task_dir_content(int pid, char *buf, int size);
int generate_proc_task_tid_stat_content(int pid, int tid, char *buf, int size);
thread_t *get_thread_by_pid_tid(int pid, int tid);
int check_proc_path(const char *path, int *pid, int *tid);
int check_proc_task_tid_stat(const char *path, int *pid, int *tid);
void init_interrupt_counts(void);
void increment_interrupt_count(int irq);
#endif