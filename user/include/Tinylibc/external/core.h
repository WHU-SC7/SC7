
ssize_t __write(int fd, const void *buf, int len);
ssize_t __read(int fd, const void *buf, int len);
int __openat(int fd, const char *pathname, int flags, unsigned short mode);
int __creat(const char *pathname, unsigned short mode);
int __close(int fd);

#include "tlibc.h" // for struct linux_dirent64
long __getdents64(unsigned int fd, struct linux_dirent64 *dirp, unsigned int count);
int __fstat(int fd, struct stat *statbuf);

//进程操作
pid_t __fork();
void __exit(int status);
pid_t __waitpid(int pid, int *wstatus, int options);
pid_t __wait(int *wstatus);
int __execve(const char *pathname, char *const argv[], char *const envp[]);

//printf
void print_int(int num);
void __printf(const char *fmt, ...);

//自定义
void tlibc_shutdown();