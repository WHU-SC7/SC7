#ifndef _USYSCALL_H_
#define _USYSCALL_H_
#include "def.h"

extern int write(int fd, const void *buf, int len) __attribute__((section(".text.syscall_function")));
extern int getpid(void) __attribute__((section(".text.syscall_function")));
extern int fork(void) __attribute__((section(".text.syscall_function")));
extern int clone(int (*fn)(void *arg), void *arg, void *stack, uint64 stack_size, uint64 flags) __attribute__((section(".text.syscall_function")));
extern int waitpid(int pid, int *code, int options) __attribute__((section(".text.syscall_function")));
extern int exit(int exit_status) __attribute__((section(".text.syscall_function")));
extern int sys_get_time(timeval_t *tv, int sz) __attribute__((section(".text.syscall_function")));
extern int sys_nanosleep(timeval_t *req, timeval_t *rem) __attribute__((section(".text.syscall_function")));
extern int sys_brk(void *addr) __attribute__((section(".text.syscall_function")));
extern long int sys_times(void *mytimes) __attribute__((section(".text.syscall_function")));
extern int sys_uname(void *buf) __attribute__((section(".text.syscall_function")));
extern int sys_sched_yield(void) __attribute__((section(".text.syscall_function")));
extern int getppid(void) __attribute__((section(".text.syscall_function")));
extern int sys_execve(const char *name, char *const argv[], char *const argp[]) __attribute__((section(".text.syscall_function")));
extern int sys_pipe2(int *fd, int flags) __attribute__((section(".text.syscall_function")));
extern int sys_close(int fd) __attribute__((section(".text.syscall_function")));
extern int sys_read(int fd, void *buf, int len) __attribute__((section(".text.syscall_function")));
extern int sys_dup(int fd) __attribute__((section(".text.syscall_function")));
extern int sys_openat(int fd, const char *upath, int flags, uint16 mode) __attribute__((section(".text.syscall_function")));
extern int sys_mknod(const char *path, int major, int minor) __attribute__((section(".text.syscall_function")));
extern int sys_dup3(int oldfd, int newfd, int flags) __attribute__((section(".text.syscall_function")));
extern void *sys_mmap(void *start, int len, int prot, int flags, int fd, int off) __attribute__((section(".text.syscall_function")));
extern int sys_munmap(void *start, int len)__attribute__((section(".text.syscall_function")));
extern int sys_fstat(int fd, struct kstat *st) __attribute__((section(".text.syscall_function")));
extern int sys_statx(int fd, const char *path,int flags,int mode, struct  statx *statx) __attribute__((section(".text.syscall_function")));
extern void *sys_getcwd(char *buf,int size) __attribute__((section(".text.syscall_function")));
extern int sys_mkdirat(int dirfd, const char *path, uint16 mode) __attribute__((section(".text.syscall_function")));
extern int sys_chdir(const char *path) __attribute__((section(".text.syscall_function")));
extern int sys_getdents64(int fd, struct linux_dirent64 *buf, int len) __attribute__((section(".text.syscall_function"))); 
extern int mount (const char *special, const char *dir, const char *fstype, unsigned long flags, const void *data) __attribute__((section(".text.syscall_function"))); 
extern int umount (const char *special) __attribute__((section(".text.syscall_function")));
extern int sys_unlinkat(int dirfd, char *path, unsigned int flags) __attribute__((section(".text.syscall_function")));
extern void shutdown(void) __attribute__((section(".text.syscall_function")));
extern uint64 sys_readv(int fd, const struct iovec *iov, int iovcnt) __attribute__((section(".text.syscall_function")));
#endif
