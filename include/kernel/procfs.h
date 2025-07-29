#ifndef _PROCFS_H_
#define _PROCFS_H_

int is_proc_pid_stat(const char *path, int *pid);
int is_proc_kernel_pidmax(const char *path);
int is_proc_kernel_tainted(const char *path);
int is_proc_pid_status(const char *path, int *pid);
int generate_proc_stat_content(int pid, char *buf, int size);
int generate_proc_status_content(int pid, char *buf, int size);
int check_proc_path(const char *path, int *pid);
#endif 