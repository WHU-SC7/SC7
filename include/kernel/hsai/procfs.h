#ifndef _PROCFS_H_
#define _PROCFS_H_

int is_proc_pid_stat(const char *path, int *pid);
int generate_proc_stat_content(int pid, char *buf, int size);

#endif 