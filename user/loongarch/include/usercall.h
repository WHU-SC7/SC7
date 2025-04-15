//
// Created by Li Shuang ( pseudonym ) on 2024-05-21
// --------------------------------------------------------------
// | Note: This code file just for study, not for commercial use
// | Contact Author: lishuang.mk@whu.edu.cn
// --------------------------------------------------------------
//

#ifndef _USYSCALL_H_
#define _USYSCALL_H_
#include "def.h"

extern int write(int fd, const void *buf, int len) __attribute__((section(".text.syscall_function")));
extern int getpid(void) __attribute__((section(".text.syscall_function")));
extern int fork(void) __attribute__((section(".text.syscall_function")));
extern int wait(int *code) __attribute__((section(".text.syscall_function")));
extern int exit(int exit_status) __attribute__((section(".text.syscall_function")));
extern int sys_get_time(timeval_t *tv, int sz) __attribute__((section(".text.syscall_function")));
#endif