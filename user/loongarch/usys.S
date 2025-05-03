//
// Created by Li shuang ( pseudonym ) on 2024-05-21 
// --------------------------------------------------------------
// | Note: This code file just for study, not for commercial use 
// | Contact Author: lishuang.mk@whu.edu.cn 
// --------------------------------------------------------------
//

#include "usys_macro.inc"



GEN_USER_SYSCALL getcwd 17
GEN_USER_SYSCALL unlinkat 35
GEN_USER_SYSCALL chdir 49
GEN_USER_SYSCALL pipe 59
GEN_USER_SYSCALL fstat 80
GEN_USER_SYSCALL poweroff 2024

GEN_USER_SYSCALL write  64
GEN_USER_SYSCALL getpid 172
GEN_USER_SYSCALL fork   220
GEN_USER_SYSCALL waitpid   260
GEN_USER_SYSCALL exit   93
GEN_USER_SYSCALL sys_get_time 169
GEN_USER_SYSCALL sys_nanosleep 101
GEN_USER_SYSCALL sys_brk 214
GEN_USER_SYSCALL sys_times 153
GEN_USER_SYSCALL sys_uname 160
GEN_USER_SYSCALL sys_sched_yield 124
GEN_USER_SYSCALL getppid 173
GEN_USER_SYSCALL sys_execve 221
GEN_USER_SYSCALL sys_read 63
GEN_USER_SYSCALL sys_openat 56
GEN_USER_SYSCALL sys_close 57
GEN_USER_SYSCALL sys_dup   23
GEN_USER_SYSCALL sys_mknod 16
GEN_USER_SYSCALL sys_dup3  24
GEN_USER_SYSCALL sys_mmap  222
GEN_USER_SYSCALL sys_fstat 80
GEN_USER_SYSCALL sys_getcwd 17
GEN_USER_SYSCALL sys_mkdirat 34
GEN_USER_SYSCALL sys_chdir 49
GEN_USER_SYSCALL sys_getdents64 61