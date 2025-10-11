
unsigned long __write(int fd, const void *buf, int len);
unsigned long __read(int fd, const void *buf, int len);
unsigned long __openat(int fd, const char *pathname, int flags, unsigned short mode);
unsigned long __creat(const char *pathname, unsigned short mode);
unsigned long __close(int fd);

#include "tlibc.h" // for struct linux_dirent64
unsigned long __getdents64(unsigned int fd, struct linux_dirent64 *dirp, unsigned int count);
unsigned long fstat(int fd, struct stat *statbuf);

//进程操作
unsigned long __fork();
unsigned long __exit(int status);
unsigned long __waitpid(int pid, int *wstatus, int options);
unsigned long __wait(int *wstatus);
unsigned long __execve(const char *pathname, char *const argv[], char *const envp[]);

//printf
void print_int(int num);
void __printf(const char *fmt, ...);

//自定义
void tlibc_shutdown();