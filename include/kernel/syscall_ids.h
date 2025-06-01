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
#define SYS_getuid          174
#define SYS_geteuid         175
#define SYS_ioctl           29
#define SYS_exit_group      94
#define SYS_clock_gettime   113 
#define SYS_writev          66
#define SYS_readv           65
#define SYS_syslog          116
#define SYS_fstatat         79
#define SYS_faccessat       48
#define SYS_sysinfo         179 
#define SYS_fcntl           25
#define SYS_kill            129
#define SYS_utimensat       88
#define SYS_futex           98

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

// busybox补全调用
#define SYS_sendfile64 71
#define SYS_llseek 62
#define SYS_renameat2 276

/**
 * @brief 根据系统调用号返回对应系统调用
 * 
 */
static inline const char* get_syscall_name(int num) 
{
    switch(num) {
        case SYS_write: return "write";
        case SYS_getpid: return "getpid";
        case SYS_fork: return "fork";
        case SYS_clone: return "clone";
        case SYS_exit: return "exit";
        case SYS_wait: return "wait";
        case SYS_gettimeofday: return "gettimeofday";
        case SYS_sleep: return "sleep";
        case SYS_brk: return "brk";
        case SYS_times: return "times";
        case SYS_uname: return "uname";
        case SYS_sched_yield: return "sched_yield";
        case SYS_getppid: return "getppid";
        case SYS_execve: return "execve";
        case SYS_close: return "close";
        case SYS_pipe2: return "pipe2";
        case SYS_read: return "read";
        case SYS_dup: return "dup";
        case SYS_openat: return "openat";
        case SYS_mknod: return "mknod";
        case SYS_dup3: return "dup3";
        case SYS_mmap: return "mmap";
        case SYS_munmap: return "munmap";
        case SYS_fstat: return "fstat";
        case SYS_statx: return "statx";
        case SYS_getcwd: return "getcwd";
        case SYS_mkdirat: return "mkdirat";
        case SYS_chdir: return "chdir";
        case SYS_getdents64: return "getdents64";
        case SYS_mount: return "mount";
        case SYS_umount: return "umount";
        case SYS_unlinkat: return "unlinkat";
        case SYS_shutdown: return "shutdown";
        case SYS_set_tid_address: return "set_tid_address";
        case SYS_getuid: return "getuid";
        case SYS_geteuid: return "geteuid";
        case SYS_ioctl: return "ioctl";
        case SYS_exit_group: return "exit_group";
        case SYS_clock_gettime: return "clock_gettime";
        case SYS_writev: return "writev";
        case SYS_readv: return "readv";
        case SYS_syslog: return "syslog";
        case SYS_fstatat: return "fstatat";
        case SYS_faccessat: return "faccessat";
        case SYS_sysinfo: return "sysinfo";
        case SYS_fcntl: return "fcntl";
        case SYS_kill: return "kill";
        case SYS_utimensat: return "utimensat";
        case SYS_futex: return "futex";
        case SYS_rt_sigaction: return "rt_sigaction";
        case SYS_rt_sigprocmask: return "rt_sigprocmask";
        case SYS_set_robust_list: return "set_robust_list";
        case SYS_gettid: return "gettid";
        case SYS_tgkill: return "tgkill";
        case SYS_prlimit64: return "prlimit64";
        case SYS_readlinkat: return "readlinkat";
        case SYS_getrandom: return "getrandom";
        case SYS_getgid: return "getgid";
        case SYS_setgid: return "setgid";
        case SYS_setuid: return "setuid";
        default: return "unknown";
    }
}

#endif