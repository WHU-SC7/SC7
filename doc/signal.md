# 系统信号处理机制文档

## 1. 概述

本系统实现了完整的 POSIX 信号处理机制，支持标准信号（1-31）和实时信号（SIGRTMIN-SIGRTMAX），提供了信号发送、接收、处理和阻塞等功能。

## 2. 关键数据结构

### 2.1 信号集（sigset_t）

```c
#define SIGSET_LEN 1
typedef struct {
    unsigned long __val[SIGSET_LEN];
} __sigset_t;
```

- 使用位图表示信号集，每个位对应一个信号
- 支持最多 64 个信号（当前实现）
- 位 0 表示信号 1，位 1 表示信号 2，以此类推

### 2.2 信号处理函数（sigaction）

```c
typedef struct sigaction {
    union {
        __sighandler_t sa_handler;  // 信号处理函数指针
    } __sigaction_handler;
    __sigset_t sa_mask;            // 信号处理期间要阻塞的信号
    int sa_flags;                  // 信号处理标志
} sigaction;
```

### 2.3 进程信号相关字段

```c
typedef struct proc {
    // ... 其他字段 ...
    
    /* 信号相关 */
    __sigset_t sig_set;                    // 信号掩码（被阻塞的信号）
    sigaction sigaction[SIGRTMAX + 1];     // 每个信号的处理函数
    __sigset_t sig_pending;                // 待处理的信号
    struct trapframe sig_trapframe;        // 信号处理上下文
    int current_signal;                    // 当前正在处理的信号
    int signal_interrupted;                // 是否被信号中断
    int continued;                         // 是否被SIGCONT继续
    int killed;                            // 进程终止信号
} proc_t;
```

## 3. 核心字段详解

### 3.1 sigaction 数组

**作用**: 存储每个信号的处理函数和属性

**索引**: `sigaction[信号编号]`
- 信号编号 1-31：标准信号
- 信号编号 SIGRTMIN-SIGRTMAX：实时信号

**处理函数类型**:
- `SIG_DFL` (0)：默认处理
- `SIG_IGN` (1)：忽略信号
- 用户函数指针：自定义处理函数

### 3.2 sig_pending

**作用**: 记录待处理的信号

**位图表示**: 
- 位 0 表示信号 1 待处理
- 位 1 表示信号 2 待处理
- 以此类推

**特点**:
- 同一信号多次到达时只记录一次（非实时信号）
- 实时信号可以排队，但当前实现中也是位图表示

### 3.3 sig_set（信号掩码）

**作用**: 记录被阻塞的信号

**位图表示**:
- 位 0 表示信号 1 被阻塞
- 位 1 表示信号 2 被阻塞
- 以此类推

**特殊规则**:
- `SIGKILL` 和 `SIGSTOP` 始终被阻塞（不可被阻塞）
- 被阻塞的信号不会立即处理，但会保留在 `sig_pending` 中

## 4. 信号处理流程

### 4.1 信号发送流程

```c
int kill(int pid, int sig) {
    // 1. 查找目标进程
    // 2. 设置待处理信号位
    p->sig_pending.__val[0] |= (1 << sig);
    
    // 3. 特殊信号处理
    if (sig == SIGCONT && p->killed == SIGSTOP) {
        p->killed = 0;
        p->continued = 1;
    }
    
    // 4. 设置 killed 标志（仅对默认处理的信号）
    if (p->sigaction[sig].sa_handler == NULL || 
        p->sigaction[sig].sa_handler == SIG_DFL) {
        if (sig < SIGRTMIN || sig > SIGRTMAX) {
            p->killed = sig;
        }
    }
    
    // 5. 唤醒睡眠的进程
    if (p->state == SLEEPING) {
        p->state = RUNNABLE;
    }
}
```

### 4.2 信号检测流程

```c
int check_pending_signals(struct proc *p) {
    // 1. 计算未被阻塞的待处理信号
    uint64 pending = p->sig_pending.__val[0] & ~p->sig_set.__val[0];
    
    // 2. 查找最低位的有效信号
    for (int sig = 1; sig <= SIGRTMAX; sig++) {
        if (pending & (1ul << sig)) {
            // 3. 检查是否被忽略
            if (p->sigaction[sig].sa_handler == SIG_IGN) {
                p->sig_pending.__val[0] &= ~(1ul << sig);
                continue;
            }
            return sig;
        }
    }
    return 0;
}
```

### 4.3 信号处理流程

```c
int handle_signal(struct proc *p, int sig) {
    // 1. 检查信号处理函数类型
    if (p->sigaction[sig].sa_handler == SIG_IGN) {
        // 忽略信号，清除待处理标志
        p->sig_pending.__val[0] &= ~(1ul << sig);
        return 0;
    }
    
    if (p->sigaction[sig].sa_handler == SIG_DFL || 
        p->sigaction[sig].sa_handler == NULL) {
        // 默认处理
        if (sig != SIGCHLD) {
            p->killed = sig;  // 设置终止标志
        }
        p->sig_pending.__val[0] &= ~(1ul << sig);
        return 0;
    }
    
    // 2. 自定义处理函数
    p->sig_pending.__val[0] &= ~(1ul << sig);
    return 0;  // 实际调用在 check_and_handle_signals 中
}
```

### 4.4 用户态信号处理

```c
int check_and_handle_signals(struct proc *p, struct trapframe *trapframe) {
    int sig = check_pending_signals(p);
    if (sig == 0) return 0;
    
    if (handle_signal(p, sig) == 0) {
        if (p->sigaction[sig].sa_handler != NULL) {
            // 1. 保存当前上下文
            copytrapframe(&p->sig_trapframe, p->trapframe);
            
            // 2. 设置信号处理函数参数
            trapframe->a0 = sig;
            
            // 3. 设置返回地址为信号处理函数
            trapframe->epc = (uint64)p->sigaction[sig].sa_handler;
            trapframe->ra = SIGTRAMPOLINE;
            
            // 4. 记录当前处理的信号
            p->current_signal = sig;
            p->signal_interrupted = 1;
            
            return 1;  // 需要处理信号
        }
    }
    return 0;
}
```

## 5. 系统调用中的信号处理

### 5.1 可中断系统调用

某些系统调用在收到信号时应该返回 `EINTR`，例如：

```c
// clock_nanosleep 中的信号检测
while (r_time() < target_time) {
    // 检查进程是否被杀死
    if (myproc()->killed) {
        // 计算剩余时间并返回 EINTR
        return -EINTR;
    }
    
    // 检查是否有待处理的信号
    if (myproc()->sig_pending.__val[0] != 0) {
        // 计算剩余时间并返回 EINTR
        return -EINTR;
    }
    
    // 继续睡眠
    sleep_on_chan(&tickslock, &tickslock);
}
```

### 5.2 ppoll 中的信号处理

```c
// 检查是否有待处理的信号
int pending_sig = check_pending_signals(p);
if (pending_sig != 0) {
    ret = -EINTR;  // 被信号中断
    break;
}

// 检查是否被信号中断过
if (p->signal_interrupted) {
    ret = -EINTR;
    p->signal_interrupted = 0;
    break;
}
```

## 6. 信号处理函数返回机制

### 6.1 sigreturn 系统调用

```c
uint64 sys_sigreturn() {
    struct proc *p = myproc();
    
    // 1. 恢复原始上下文
    copytrapframe(p->trapframe, &p->sig_trapframe);
    
    // 2. 清除信号处理状态
    p->current_signal = 0;
    p->signal_interrupted = 0;
    
    // 3. 返回用户态继续执行
    return 0;
}
```

### 6.2 信号处理函数调用约定

1. **参数传递**: 信号编号通过 `a0` 寄存器传递
2. **返回地址**: 信号处理函数返回时跳转到 `SIGTRAMPOLINE`
3. **上下文保存**: 原始执行上下文保存在 `sig_trapframe` 中
4. **栈管理**: 信号处理函数使用独立的栈空间

## 7. 特殊信号处理

### 7.1 SIGKILL 和 SIGSTOP

- 始终被阻塞（不可被阻塞）
- 不能被忽略或自定义处理
- 直接终止或停止进程

### 7.2 SIGCONT

- 用于恢复被 `SIGSTOP` 停止的进程
- 立即设置 `continued` 标志
- 清除 `killed` 标志（如果等于 `SIGSTOP`）

### 7.3 SIGCHLD

- 默认忽略
- 用于通知父进程子进程状态变化

## 8. 调试和监控

### 8.1 调试函数

```c
void debug_print_signal_info(struct proc *p, const char *prefix) {
    // 打印待处理信号
    // 打印信号掩码
    // 打印未被阻塞的待处理信号
}
```

### 8.2 日志输出

系统提供了详细的调试日志，包括：
- 信号发送和接收
- 信号处理函数调用
- 信号掩码变化
- 进程状态转换

## 9. 性能考虑

### 9.1 位图操作

- 使用位图高效表示信号集
- 位操作快速检查信号状态
- 支持批量信号处理

### 9.2 延迟处理

- 信号在返回用户态时处理
- 避免在中断上下文中执行复杂操作
- 保证系统调用的原子性

## 10. 兼容性

### 10.1 POSIX 兼容

- 支持标准信号（1-31）
- 支持实时信号（SIGRTMIN-SIGRTMAX）
- 实现 `sigaction`、`sigprocmask` 等标准接口

### 10.2 Linux 兼容

- 支持 `rt_sigaction`、`rt_sigprocmask` 等 Linux 特有接口
- 兼容 Linux 信号编号和语义
- 支持信号队列和实时信号

## 11. 总结

本系统的信号处理机制提供了：

1. **完整的信号支持**: 标准信号和实时信号
2. **灵活的处理方式**: 默认处理、忽略、自定义处理函数
3. **可靠的传递机制**: 位图表示、延迟处理、上下文保存
4. **良好的兼容性**: POSIX 和 Linux 兼容
5. **高效的实现**: 位图操作、延迟处理、调试支持

该机制为系统提供了强大的进程间通信和异常处理能力，是操作系统的重要组成部分。 