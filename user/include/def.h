#ifndef __DEF_H__
#define __DEF_H__

/* Explicitly-sized versions of integer types */
typedef char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;
typedef unsigned int uint;
typedef int pid_t;
typedef long unsigned int size_t;

#define NULL ((void *)0)
#define stdout 1
typedef __SIZE_TYPE__ size_t;

#define SS (sizeof(size_t))
#define HASZERO(x) (((x) - ONES) & ~(x) & HIGHS) // lib/string.c
#define UCHAR_MAX (0xffU)
#define ONES ((size_t)-1 / UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX / 2 + 1))

typedef struct timeval {
    uint64 sec;      // 秒
    uint64 usec;     // 微秒
} timeval_t;

typedef struct tms
{
    long tms_utime;  ///< 用户态时间
    long tms_stime;  ///< 内核态时间
    long tms_cutime; ///< 子进程用户态时间
    long tms_cstime; ///< 子进程内核态时间
} tms_t;

struct utsname
{
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};



struct kstat
{
    uint64 st_dev;
    uint64 st_ino;
    uint32 st_mode;
    uint32 st_nlink;
    uint32 st_uid;
    uint32 st_gid;
    uint64 st_rdev;
    uint64 __pad;
    uint64 st_size;
    uint32 st_blksize;
    uint32 __pad2;
    uint64 st_blocks;
    uint64 st_atime_sec;
    uint64 st_atime_nsec;
    uint64 st_mtime_sec;
    uint64 st_mtime_nsec;
    uint64 st_ctime_sec;
    uint64 st_ctime_nsec;
    // unsigned __unused[2];
};

typedef struct
{
    int valid;
    char *name[20];
} longtest;



struct statx
{
    uint32 stx_mask;
    uint32 stx_blksize;
    uint64 stx_attributes;
    uint32 stx_nlink;
    uint32 stx_uid;
    uint32 stx_gid;
    uint16 stx_mode;
    uint16 pad1;
    uint64 stx_ino;
    uint64 stx_size;
    uint64 stx_blocks;
    uint64 stx_attributes_mask;
    struct
    {
        int64 tv_sec;
        uint32 tv_nsec;
        int32 pad;
    } stx_atime, stx_btime, stx_ctime, stx_mtime;
    uint32 stx_rdev_major;
    uint32 stx_rdev_minor;
    uint32 stx_dev_major;
    uint32 stx_dev_minor;
    uint64 spare[14];
};

struct linux_dirent64
{
    uint64 d_ino;
    int64 d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

/* Structure for scatter/gather I/O.  */
struct iovec
  {
    void *iov_base;	/* Pointer to data.  */
    size_t iov_len;	/* Length of data.  */
  };

  #define SIGSET_LEN 1
  typedef void (*__sighandler_t)(int);
  typedef struct
{
    unsigned long __val[SIGSET_LEN];
} __sigset_t;

  typedef struct sigaction
  {
      union
      { // let's make it simple, only sa_handler is supported
          __sighandler_t sa_handler;
          // void (*sa_sigaction)(int, siginfo_t *, void *);
      } __sigaction_handler;
      __sigset_t sa_mask; // signals to be blocked during handling
      int sa_flags;
      // void (*sa_restorer)(void);	// this field is not used on risc-v
  } sigaction;

#define O_RDONLY 0x000    ///< 只读
#define O_WRONLY 0x001    ///< 只写
#define O_RDWR 0x002      ///< 读写
#define O_CREATE 0100     ///< 如果指定的文件不存在，则创建该文件。 64
#define O_TRUNC 0x400     ///< 如果文件已存在且以写方式打开，则将文件长度截断为0，即清空文件内容
#define O_DIRECTORY 0x004 ///< 要求打开的目标必须是一个目录，否则打开失败
#define O_CLOEXEC 0x008   ///< 在执行 exec 系列函数时，自动关闭该文件描述符（close on exec）

#define CONSOLE 1
#define DEVNULL 0
#define SIGCHLD 17
#define SIGUSR1 10

// waitid 相关常量
#define P_ALL    0  // 等待任意子进程
#define P_PID    1  // 等待指定PID的子进程
#define P_PGID   2  // 等待指定进程组的子进程

// waitid 选项
#define WNOHANG		0x00000001
#define WUNTRACED	0x00000002
#define WSTOPPED	WUNTRACED
#define WEXITED		0x00000004
#define WCONTINUED	0x00000008
#define WNOWAIT		0x01000000	/* Don't reap, just poll status.  */

// siginfo_t 信号代码
#define CLD_EXITED  1  // 子进程正常退出
#define CLD_KILLED  2  // 子进程被信号杀死
#define CLD_DUMPED  3  // 子进程异常退出并产生core dump
#define CLD_TRAPPED 4  // 子进程被跟踪停止
#define CLD_STOPPED 5  // 子进程停止
#define CLD_CONTINUED 6 // 子进程继续运行

#define SIGHUP 1      // Hangup
#define SIGINT 2      // Interrupt
#define SIGQUIT 3     // Quit
#define SIGILL 4      // Illegal instruction
#define SIGTRAP 5     // Trace trap
#define SIGABRT 6     // Abort (abort)
#define SIGIOT 6      // IOT trap (on some systems)
#define SIGBUS 7      // BUS error (bad memory access)
#define SIGFPE 8      // Floating-point exception
#define SIGKILL 9     // Kill, unblockable
#define SIGUSR1 10    // User-defined signal 1
#define SIGSEGV 11    // Segmentation violation (invalid memory reference)
#define SIGUSR2 12    // User-defined signal 2
#define SIGPIPE 13    // Broken pipe: write to pipe with no readers
#define SIGALRM 14    // Timer signal from alarm(2)
#define SIGTERM 15    // Termination signal
#define SIGSTKFLT 16  // Stack fault on coprocessor (unused)
#define SIGCHLD 17    // Child stopped or terminated
#define SIGCONT 18    // Continue if stopped
#define SIGSTOP 19    // Stop process unblockable
#define SIGTSTP 20    // Keyboard stop
#define SIGTTIN 21    // Background read from tty
#define SIGTTOU 22    // Background write to tty
#define SIGURG 23     // Urgent condition on socket (4.2BSD)
#define SIGXCPU 24    // CPU limit exceeded (4.2BSD)
#define SIGXFSZ 25    // File size limit exceeded (4.2BSD)
#define SIGVTALRM 26  // Virtual alarm clock (4.2BSD)
#define SIGPROF 27    // Profiling alarm clock (4.2BSD)
#define SIGWINCH 28   // Window size change (4.3BSD, Sun)
#define SIGIO 29      // I/O now possible (4.2BSD)
#define SIGPOLL SIGIO // Pollable event occurred (System V)
#define SIGPWR 30     // Power failure restart (System V)
#define SIGSYS 31     // Bad system call

// Real-time signals
#define SIGRTMIN 32 // First real-time signal
#define SIGRTMAX 64 // Last real-time signal

// Signal Flags
#define SA_NOCLDSTOP 0x00000001
#define SA_NOCLDWAIT 0x00000002
#define SA_NODEFER 0x08000000
#define SA_RESETHAND 0x80000000
#define SA_RESTART 0x10000000
#define SA_SIGINFO 0x00000004


// siginfo_t 结构体定义
union sigval
{
  int sival_int;
  void *sival_ptr;
};


typedef struct {
    int si_signo;       // 信号编号
    int si_errno;       // 关联的错误码
    int si_code;        // 信号来源代码
    int si_pid;       // 发送进程的PID
    uint si_uid;       // 发送进程的真实用户ID
    int si_status;      // 退出值或信号
    void *si_addr;      // 触发信号的地址
    long si_band;       // I/O 事件类型
    union sigval si_value; // 伴随数据
    int __si_flags;     // 内部标志
    int __pad[3];       // 填充字段
} siginfo_t;

// msync flags
#define MS_ASYNC      1  // 异步同步
#define MS_SYNC       2  // 同步同步
#define MS_INVALIDATE 4  // 使缓存无效

#define AT_FDCWD -100
// for mmap
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define PROT_GROWSDOWN 0X01000000
#define PROT_GROWSUP 0X02000000

#define MAP_FILE 0
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0X02
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ALLOC  0xf00    //自定义，mmap时不使用懒加载
#define MAP_FAILED ((void *)-1)

#endif