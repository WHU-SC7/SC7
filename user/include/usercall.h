#ifndef _USYSCALL_H_
#define _USYSCALL_H_
#include "def.h"

extern int write(int fd, const void *buf, int len) __attribute__((section(".text.syscall_function")));
extern int getpid(void) __attribute__((section(".text.syscall_function")));
extern int fork(void) __attribute__((section(".text.syscall_function")));
extern int waitpid(int pid, int *code, int options) __attribute__((section(".text.syscall_function")));
extern int exit(int exit_status) __attribute__((section(".text.syscall_function")));
extern int sys_get_time(timeval_t *tv, int sz) __attribute__((section(".text.syscall_function")));
extern int sys_nanosleep(timeval_t *req, timeval_t *rem) __attribute__((section(".text.syscall_function")));
extern int sys_brk(void *addr) __attribute__((section(".text.syscall_function")));
extern long int sys_times(void *mytimes) __attribute__((section(".text.syscall_function")));
extern int sys_uname(void *buf) __attribute__((section(".text.syscall_function")));
extern int sys_sched_yield(void) __attribute__((section(".text.syscall_function")));
extern int getppid(void) __attribute__((section(".text.syscall_function")));

#endif
