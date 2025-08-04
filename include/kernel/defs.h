#ifndef DEFS_H
#define DEFS_H

#include "types.h"

struct file;
struct pipe;

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define INT_MIN	(-INT_MAX - 1)
#define INT_MAX	2147483647


// console.c
void            chardev_init(void);
void            consoleintr(int);
void            consputc(int);

// pipe.c
int             pipealloc(struct file**, struct file**);
void            pipeclose(struct pipe*, int);
int             piperead(struct pipe*, uint64, int, struct file*);
int             pipewrite(struct pipe*, uint64, int, struct file*);

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

// 定时器相关常量
#define ITIMER_REAL    0  // 实时定时器
#define ITIMER_VIRTUAL 1  // 虚拟定时器（用户态时间）
#define ITIMER_PROF    2  // 性能分析定时器（用户态+内核态时间）

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
    int pad;            // 填充位，经实践得出       
    int si_pid;       // 发送进程的PID
    uint si_uid;       // 发送进程的真实用户ID
    int si_status;      // 退出值或信号
    void *si_addr;      // 触发信号的地址
    long si_band;       // I/O 事件类型
    union sigval si_value; // 伴随数据
    int __si_flags;     // 内部标志
    int __pad[3];       // 填充字段
} siginfo_t;

#endif // DEF_H
