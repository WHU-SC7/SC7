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
#define SYS_settimer 103
#define SYS_pread   67
#define SYS_ppoll 73

// busybox补全调用
#define SYS_sendfile64 71
#define SYS_llseek 62
#define SYS_renameat2 276
#define SYS_clock_nanosleep 115

// libc-test调用
#define SYS_rt_sigtimedwait 137
#define SYS_mprotect    226
#define SYS_getegid         177
#define SYS_socket      198
#define SYS_bind        200
#define SYS_listen      201
#define SYS_getsockname 204
#define SYS_sendto      206
#define SYS_recvfrom    207
#define SYS_setsockopt  208
#define SYS_connect     203
#define SYS_accept      202
#define SYS_statfs      43
#define SYS_setsid      157 

// libc-test glibc调用
#define SYS_mremap      216

// libcbench调用
#define SYS_madvise     233

//iozone 调用
#define SYS_sync        81
#define SYS_ftruncate   46
#define SYS_fsync       82
#define SYS_shmget      194 
#define SYS_shmctl      195
#define SYS_shmat       196
#define SYS_pselect6_time32 72
#define SYS_sigreturn   715 //先设置为715,改的时候记得改sigtrampoline

// lmbench
#define SYS_getrusage   165
#define SYS_umask       166

/* pthread */
#define SYS_membarrier  283
#define SYS_tkill       130
#define SYS_get_robust_list 100
#define SYS_clone3      435


/* ltp */
#define SYS_sched_setaffinity 123
#define SYS_fchmodat    53
#define SYS_fchownat    54
#define SYS_setpgid     154
#define SYS_getpgid     155
#define SYS_msync       227
#define SYS_waitid      95
#define SYS_fallocate   47

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
        case SYS_waitid: return "waitid";
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
        case SYS_settimer: return "settimer";
        case SYS_pread: return "pread";
        case SYS_ppoll: return "ppoll";
        case SYS_sendfile64: return "sendfile64";
        case SYS_llseek: return "llseek";
        case SYS_renameat2: return "renameat2";
        case SYS_clock_nanosleep: return "clock_nanosleep";
        case SYS_rt_sigtimedwait: return "rt_sigtimedwait";
        case SYS_mprotect       : return "mprotect";
        case SYS_getegid        : return "getegid";
        case SYS_socket         : return "socket";
        case SYS_bind           : return "bind";
        case SYS_getsockname    : return "getsockname";
        case SYS_setsockopt     : return "setsockopt";
        case SYS_sendto         : return "sendto";
        case SYS_recvfrom       : return "recvfrom";
        case SYS_listen         : return "listen";
        case SYS_membarrier     : return "membarrier";
        case SYS_connect        : return "connect";
        case SYS_accept         : return "accept";
        case SYS_tkill          : return "tkill";
        case SYS_get_robust_list: return "get_robust_list";
        case SYS_statfs         : return "statfs";
        case SYS_setsid         : return "setsid";
        case SYS_madvise        : return "madvise";
        case SYS_sync           : return "sync";
        case SYS_ftruncate      : return "ftruncate";
        case SYS_fsync          : return "fsync";
        case SYS_getrusage      : return "getrusage";
        case SYS_mremap         : return "mremap";
        case SYS_clone3         : return "clone3";
        case SYS_shmget         : return "shmget";
        case SYS_shmat          : return "shmat";  
        case SYS_shmctl         : return "shmctl";
        case SYS_pselect6_time32: return "pselect6_time32 ";
        case SYS_umask          : return "umask";
        case SYS_sigreturn      : return "sigreturn";
        case SYS_sched_setaffinity : return "sched_setaffinity";
        case SYS_fchmodat       : return "fchmodat";
        case SYS_fchownat       : return "fchownat";
        case SYS_setpgid        : return "setpgid";
        case SYS_getpgid        : return "getpgid";
        case SYS_msync          : return "msync"; 
        case SYS_fallocate      : return "fallocate";
        default: return "unknown";
    }
}

#endif