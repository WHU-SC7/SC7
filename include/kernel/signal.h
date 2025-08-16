#ifndef _SIGNAL_H__
#define _SIGNAL_H__

#include "types.h"
#include "list.h"  // 包含list_elem的完整定义

// 宏定义
#define ALIGN_DOWN(addr, align) ((addr) & ~((align) - 1))

// 前向声明
struct proc;
struct trapframe;
struct sigcontext;

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
#define SIGCANCEL SIGRTMIN // 线程取消信号
#define SIGRTMAX 64 // Last real-time signal

// Signal action constants
#define SIG_DFL ((__sighandler_t)0)  // Default action
#define SIG_IGN ((__sighandler_t)1)  // Ignore signal

// Signal Flags
#define SA_NOCLDSTOP 0x00000001
#define SA_NOCLDWAIT 0x00000002
#define SA_NODEFER 0x08000000
#define SA_RESETHAND 0x80000000
#define SA_RESTART 0x10000000
#define SA_SIGINFO 0x00000004
#define SA_ONSTACK 0x08000000

#define SIGSET_LEN 1
typedef struct
{
    unsigned long __val[SIGSET_LEN];
} __sigset_t;

typedef void (*__sighandler_t)(int);

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

// Flags for sigprocmask
#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

// 注意：siginfo_t 已在 defs.h 中定义，这里不再重复定义

// 新增：stack_t 结构体定义
typedef struct stack {
    void *ss_sp;       // 栈指针
    int ss_flags;      // 栈标志
    size_t ss_size;    // 栈大小
} stack_t;

// 新增：ucontext_t 结构体定义
typedef struct ucontext {
    unsigned long uc_flags;     // 上下文标志
    struct ucontext *uc_link;   // 链接上下文
    stack_t uc_stack;           // 栈信息
    __sigset_t uc_sigmask;      // 信号掩码
    char __pad[1024 / 8 - sizeof(__sigset_t)]; // 填充字段
    void *uc_mcontext;          // 机器上下文指针
} ucontext_t;

// 新增：信号帧结构体，用于嵌套信号处理
typedef struct signal_frame {
    struct list_elem elem;      // 链表元素
    void *tf;                   // 保存的陷阱帧指针
    int signal_number;          // 信号编号
    uint64 return_address;      // 返回地址
    uint64 signal_mask;         // 信号掩码
} signal_frame_t;

int sigprocmask(int how, __sigset_t *set, __sigset_t *oldset);
int set_sigaction(int signum, sigaction const *act, sigaction *oldact);

// 新增的信号处理函数声明
int check_pending_signals(struct proc *p);
int handle_signal(struct proc *p, int sig);
int check_and_handle_signals(struct proc *p, struct trapframe *trapframe);

// 新增：信号栈和上下文处理函数
int setup_signal_frame(struct proc *p, int sig, struct trapframe *trapframe);
int restore_signal_context(struct proc *p, struct trapframe *trapframe);

// 调试函数声明
void debug_print_signal_info(struct proc *p, const char *prefix);
void debug_print_signal_context(struct proc *p, struct trapframe *trapframe, int sig);

// 测试函数声明
int send_test_signal(struct proc *p, int sig);

#endif