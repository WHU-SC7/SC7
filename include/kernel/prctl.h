#ifndef _PRCTL_H_
#define _PRCTL_H_

/* prctl 命令选项 */
#define PR_SET_PDEATHSIG  1  /* 设置父进程死亡信号 */
#define PR_GET_PDEATHSIG  2  /* 获取父进程死亡信号 */
#define PR_GET_DUMPABLE   3  /* 获取是否可 dump */
#define PR_SET_DUMPABLE   4  /* 设置是否可 dump */
#define PR_GET_UNALIGN    5  /* 获取未对齐内存访问策略 */
#define PR_SET_UNALIGN    6  /* 设置未对齐内存访问策略 */
#define PR_GET_KEEPCAPS   7  /* 获取是否保持能力 */
#define PR_SET_KEEPCAPS   8  /* 设置是否保持能力 */
#define PR_GET_FPEMU      9  /* 获取浮点模拟策略 */
#define PR_SET_FPEMU     10  /* 设置浮点模拟策略 */
#define PR_GET_FPEXC     11  /* 获取浮点异常模式 */
#define PR_SET_FPEXC     12  /* 设置浮点异常模式 */
#define PR_GET_TIMING    13  /* 获取进程时序方式 */
#define PR_SET_TIMING    14  /* 设置进程时序方式 */
#define PR_SET_NAME      15  /* 设置进程名称 */
#define PR_GET_NAME      16  /* 获取进程名称 */
#define PR_GET_ENDIAN    19  /* 获取字节序 */
#define PR_SET_ENDIAN    20  /* 设置字节序 */
#define PR_GET_SECCOMP   21  /* 获取 seccomp 模式 */
#define PR_SET_SECCOMP   22  /* 设置 seccomp 模式 */
#define PR_CAPBSET_READ  23  /* 读取能力边界集 */
#define PR_CAPBSET_DROP  24  /* 删除能力边界集 */
#define PR_GET_TSC       25  /* 获取 TSC 启用状态 */
#define PR_SET_TSC       26  /* 设置 TSC 启用状态 */
#define PR_GET_SECUREBITS 27 /* 获取安全位 */
#define PR_SET_SECUREBITS 28 /* 设置安全位 */
#define PR_SET_TIMERSLACK 29 /* 设置定时器松弛 */
#define PR_GET_TIMERSLACK 30 /* 获取定时器松弛 */

#define PR_TASK_PERF_EVENTS_DISABLE 31 /* 禁用性能事件 */
#define PR_TASK_PERF_EVENTS_ENABLE  32 /* 启用性能事件 */

#define PR_MCE_KILL     33 /* 内存错误处理 */
#define PR_MCE_KILL_GET 34 /* 获取内存错误处理策略 */

#define PR_SET_MM              35 /* 设置内存映射 */
#define PR_SET_CHILD_SUBREAPER 36 /* 设置子进程回收器 */
#define PR_GET_CHILD_SUBREAPER 37 /* 获取子进程回收器 */

#define PR_SET_NO_NEW_PRIVS 38 /* 设置不获取新权限 */
#define PR_GET_NO_NEW_PRIVS 39 /* 获取不获取新权限 */

#define PR_GET_TID_ADDRESS 40 /* 获取 TID 地址 */

#define PR_SET_THP_DISABLE 41 /* 禁用透明大页 */
#define PR_GET_THP_DISABLE 42 /* 获取透明大页状态 */

/* prctl 状态值 */
#define PR_UNALIGN_NOPRINT   1  /* 不打印未对齐访问警告 */
#define PR_UNALIGN_SIGBUS    2  /* 未对齐访问产生 SIGBUS */

#define PR_FPEMU_NOPRINT     1  /* 不打印浮点模拟警告 */
#define PR_FPEMU_SIGFPE      2  /* 浮点模拟产生 SIGFPE */

#define PR_TIMING_STATISTICAL 0 /* 统计时序 */
#define PR_TIMING_TIMESTAMP   1 /* 时间戳时序 */

#define PR_ENDIAN_BIG        0  /* 大端序 */
#define PR_ENDIAN_LITTLE     1  /* 小端序 */
#define PR_ENDIAN_PPC_LITTLE 2  /* PPC 小端序 */

#define PR_TSC_ENABLE        1  /* 启用 TSC */
#define PR_TSC_SIGSEGV       2  /* TSC 访问产生 SIGSEGV */

#endif /* _PRCTL_H_ */
