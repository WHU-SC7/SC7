#define SYS_write 64
#define SYS_getpid 172
#define SYS_fork 300
#define SYS_clone 220
#define SYS_exit 93
#define SYS_wait4 260
#define SYS_gettimeofday 169
#define SYS_sleep 101
#define SYS_brk 214
#define SYS_times 153
#define SYS_uname 160
#define SYS_sched_yield 124
#define SYS_getppid 173
#define SYS_execve 221
#define SYS_close 57
#define SYS_pipe2 59
#define SYS_read 63
#define SYS_dup 23
#define SYS_openat 56
#define SYS_mknod 16
#define SYS_dup3 24
#define SYS_mmap 222
#define SYS_munmap 215
#define SYS_fstat 80
#define SYS_statx 291
#define SYS_getcwd 17
#define SYS_mkdirat 34
#define SYS_chdir 49
#define SYS_fchdir 50
#define SYS_getdents64 61
#define SYS_mount 40
#define SYS_umount 39
#define SYS_unlinkat 35
#define SYS_shutdown 1000
// busybox
#define SYS_set_tid_address 96
#define SYS_getuid 174
#define SYS_geteuid 175
#define SYS_ioctl 29
#define SYS_exit_group 94
#define SYS_clock_gettime 113
#define SYS_clock_getres 114
#define SYS_writev 66
#define SYS_readv 65
#define SYS_syslog 116
#define SYS_fstatat 79
#define SYS_faccessat 48
#define SYS_sysinfo 179
#define SYS_fcntl 25
#define SYS_kill 129
#define SYS_utimensat 88
#define SYS_futex 98
#define SYS_futex_waitv 449

// 信号
#define SYS_rt_sigaction 134
#define SYS_rt_sigprocmask 135

// glibc
#define SYS_set_robust_list 99
#define SYS_gettid 178
#define SYS_tgkill 131
#define SYS_prlimit64 261
#define SYS_getrlimit 163
#define SYS_readlinkat 78
#define SYS_getrandom 278
#define SYS_getgid 176 //< getuid返回值从0改成1,la glibc需要这个
#define SYS_setgid 144
#define SYS_setuid 146
#define SYS_settimer 103
#define SYS_pread 67
#define SYS_ppoll 73

// busybox补全调用
#define SYS_sendfile64 71
#define SYS_llseek 62
#define SYS_renameat2 276
#define SYS_clock_nanosleep 115

// libc-test调用
#define SYS_rt_sigtimedwait 137
#define SYS_mprotect 226
#define SYS_getegid 177
#define SYS_socket 198
#define SYS_bind 200
#define SYS_listen 201
#define SYS_getsockname 204
#define SYS_sendto 206
#define SYS_recvfrom 207
#define SYS_setsockopt 208
#define SYS_connect 203
#define SYS_accept 202
#define SYS_statfs 43
#define SYS_setsid 157
#define SYS_getsid 156

// libc-test glibc调用
#define SYS_mremap 216

// libcbench调用
#define SYS_madvise 233

// iozone 调用
#define SYS_sync 81
#define SYS_ftruncate 46
#define SYS_fsync 82
#define SYS_shmget 194
#define SYS_shmctl 195
#define SYS_shmat 196
#define SYS_shmdt 197
#define SYS_pselect6_time32 72
#define SYS_sigreturn 715 // 先设置为715,改的时候记得改sigtrampoline

// lmbench
#define SYS_getrusage 165
#define SYS_umask 166

/* pthread */
#define SYS_membarrier 283
#define SYS_tkill 130
#define SYS_get_robust_list 100
#define SYS_clone3 435

/* ltp */
#define SYS_sched_setaffinity 122
#define SYS_sched_getaffinity 123
#define SYS_getcpu 168
#define SYS_fchmod 52
#define SYS_fchmodat 53
#define SYS_fchmodat2 452
#define SYS_fchownat 54
#define SYS_setpgid 154
#define SYS_getpgid 155
#define SYS_msync 227
#define SYS_waitid 95
#define SYS_fallocate 47
#define SYS_mknodat 33
#define SYS_linkat 37
#define SYS_setresuid 147
#define SYS_getresuid 148
#define SYS_setresgid 149
#define SYS_pwrite64 68
#define SYS_preadv 69
#define SYS_pwritev 70
#define SYS_sched_get_priority_max 125
#define SYS_sched_get_priority_min 126
#define SYS_setuid 146
#define SYS_symlinkat 36
#define SYS_setgroups 159
#define SYS_getgroups 158
#define SYS_faccessat2 439
#define SYS_chroot 51
#define SYS_setreuid 145
#define SYS_setregid 143
#define SYS_getresgid 150
#define SYS_fchown 55
#define SYS_fgetxattr 10
#define SYS_copy_file_range 285
#define SYS_preadv2 286
#define SYS_pwritev2 287
#define SYS_splice 76
#define SYS_prctl 167
#define SYS_personality 92
#define SYS_unshare 97
#define SYS_sethostname 161
#define SYS_getitimer 102