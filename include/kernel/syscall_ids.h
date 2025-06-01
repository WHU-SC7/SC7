#ifndef __SYSCALL_IDS_H__
#define __SYSCALL_IDS_H__

#define SYS_write   64
#define SYS_getpid  172
#define SYS_fork    300
#define SYS_clone   220
#define SYS_exit    93
#define SYS_wait    260
#define SYS_gettimeofday 169
#define SYS_sleep   101 
#define SYS_brk     214 
#define SYS_times   153
#define SYS_uname   160
#define SYS_sched_yield 124
#define SYS_getppid 173
#define SYS_execve  221
#define SYS_close   57
#define SYS_pipe2   59
#define SYS_read    63
#define SYS_dup     23
#define SYS_openat  56
#define SYS_mknod   16
#define SYS_dup3    24
#define SYS_mmap    222
#define SYS_munmap  215
#define SYS_fstat   80
#define SYS_statx   291
#define SYS_getcwd  17
#define SYS_mkdirat 34
#define SYS_chdir   49
#define SYS_getdents64   61
#define SYS_mount 40
#define SYS_umount 39
#define SYS_unlinkat 35
#define SYS_shutdown 1000
// busybox
#define SYS_set_tid_address 96
#define SYS_getuid      174
#define SYS_geteuid     175
#define SYS_ioctl       29
#define SYS_exit_group  94
#define SYS_clock_gettime  113 
#define SYS_writev      66
#define SYS_readv       65
#define SYS_syslog      116
#define SYS_fstatat     79
#define SYS_faccessat   48
#define SYS_sysinfo     179 
#define SYS_fcntl       25
#define SYS_kill        129
#define SYS_utimensat   88

//信号
#define SYS_rt_sigaction 134
#define SYS_rt_sigprocmask 135

// glibc
#define SYS_set_robust_list   99
#define SYS_gettid   178
#define SYS_tgkill   131
#define SYS_prlimit64   261
#define SYS_readlinkat   78
#define SYS_getrandom  278
#define SYS_getgid  176 //< getuid返回值从0改成1,la glibc需要这个
#define SYS_setgid  144
#define SYS_setuid  146
#define SYS_settimer 103
#define SYS_pread   67

// busybox补全调用
#define SYS_sendfile64 71
#define SYS_readv 65
#define SYS_llseek 62
#define SYS_renameat2 276

#endif