#include "signal.h"
#include "process.h"
#include "printf.h"
#include "string.h"
#include "thread.h"
#include "futex.h"
#include "hsai.h"
#include "cpu.h"
#include "print.h"

// 信号名称数组，用于调试输出
static const char *signal_names[] = {
    [0] = "NONE",
    [SIGHUP] = "SIGHUP",
    [SIGINT] = "SIGINT",
    [SIGQUIT] = "SIGQUIT",
    [SIGILL] = "SIGILL",
    [SIGTRAP] = "SIGTRAP",
    [SIGABRT] = "SIGABRT",
    [SIGBUS] = "SIGBUS",
    [SIGFPE] = "SIGFPE",
    [SIGKILL] = "SIGKILL",
    [SIGUSR1] = "SIGUSR1",
    [SIGSEGV] = "SIGSEGV",
    [SIGUSR2] = "SIGUSR2",
    [SIGPIPE] = "SIGPIPE",
    [SIGALRM] = "SIGALRM",
    [SIGTERM] = "SIGTERM",
    [SIGSTKFLT] = "SIGSTKFLT",
    [SIGCHLD] = "SIGCHLD",
    [SIGCONT] = "SIGCONT",
    [SIGSTOP] = "SIGSTOP",
    [SIGTSTP] = "SIGTSTP",
    [SIGTTIN] = "SIGTTIN",
    [SIGTTOU] = "SIGTTOU",
    [SIGURG] = "SIGURG",
    [SIGXCPU] = "SIGXCPU",
    [SIGXFSZ] = "SIGXFSZ",
    [SIGVTALRM] = "SIGVTALRM",
    [SIGPROF] = "SIGPROF",
    [SIGWINCH] = "SIGWINCH",
    [SIGIO] = "SIGIO",
    [SIGPWR] = "SIGPWR",
    [SIGSYS] = "SIGSYS",
    [SIGRTMIN] = "SIGRTMIN",
    [SIGRTMIN + 1] = "SIGRTMIN+1",
    [SIGRTMIN + 2] = "SIGRTMIN+2",
    [SIGRTMIN + 3] = "SIGRTMIN+3",
    [SIGRTMIN + 4] = "SIGRTMIN+4",
    [SIGRTMIN + 5] = "SIGRTMIN+5",
    [SIGRTMIN + 6] = "SIGRTMIN+6",
    [SIGRTMIN + 7] = "SIGRTMIN+7",
    [SIGRTMIN + 8] = "SIGRTMIN+8",
    [SIGRTMIN + 9] = "SIGRTMIN+9",
    [SIGRTMIN + 10] = "SIGRTMIN+10",
    [SIGRTMIN + 11] = "SIGRTMIN+11",
    [SIGRTMIN + 12] = "SIGRTMIN+12",
    [SIGRTMIN + 13] = "SIGRTMIN+13",
    [SIGRTMIN + 14] = "SIGRTMIN+14",
    [SIGRTMIN + 15] = "SIGRTMIN+15",
    [SIGRTMIN + 16] = "SIGRTMIN+16",
    [SIGRTMIN + 17] = "SIGRTMIN+17",
    [SIGRTMIN + 18] = "SIGRTMIN+18",
    [SIGRTMIN + 19] = "SIGRTMIN+19",
    [SIGRTMIN + 20] = "SIGRTMIN+20",
    [SIGRTMIN + 21] = "SIGRTMIN+21",
    [SIGRTMIN + 22] = "SIGRTMIN+22",
    [SIGRTMIN + 23] = "SIGRTMIN+23",
    [SIGRTMIN + 24] = "SIGRTMIN+24",
    [SIGRTMIN + 25] = "SIGRTMIN+25",
    [SIGRTMIN + 26] = "SIGRTMIN+26",
    [SIGRTMIN + 27] = "SIGRTMIN+27",
    [SIGRTMIN + 28] = "SIGRTMIN+28",
    [SIGRTMIN + 29] = "SIGRTMIN+29",
    [SIGRTMIN + 30] = "SIGRTMIN+30",
    [SIGRTMIN + 31] = "SIGRTMIN+31",
    [SIGRTMAX] = "SIGRTMAX"};

// 获取信号名称的辅助函数
[[maybe_unused]] static const char *get_signal_name(int sig)
{
    if (sig >= 0 && sig < sizeof(signal_names) / sizeof(signal_names[0]) && signal_names[sig])
    {
        return signal_names[sig];
    }
    return "UNKNOWN";
}

int set_sigaction(int signum, sigaction const *act, sigaction *oldact)
{
    struct proc *p = myproc();
    if (!p || !p->current_thread)
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "set_sigaction: 进程或当前线程不存在\n");
        return -1;
    }
    
    // thread_t *t = p->current_thread;

    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "set_sigaction: 进入函数, signum=%d, pid=%d\n", signum, p->pid);
    
    // 检查信号编号是否有效
    if (signum <= 0 || signum > SIGRTMAX)
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "set_sigaction: 无效的信号编号 %d\n", signum);
        return -1;
    }
    
    // 特殊信号不能被修改
    if (signum == SIGKILL || signum == SIGSTOP)
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "set_sigaction: 信号 %d 不能被修改\n", signum);
        return -1;
    }
    
    struct list_elem *e;
    for (e = list_begin(&p->thread_queue); e != list_end(&p->thread_queue); e = list_next(e))
    {
            thread_t *t = list_entry(e, thread_t, elem);
                // 如果需要返回旧的信号处理函数，先保存
            if (oldact)
            {
                memcpy(oldact, &t->sigaction[signum], sizeof(sigaction));
                DEBUG_LOG_LEVEL(LOG_DEBUG, "set_sigaction: 保存旧的信号处理函数\n");
            }
            
            // 设置新的信号处理函数
            if (act)
            {
                DEBUG_LOG_LEVEL(LOG_DEBUG, "set_sigaction: 设置新信号处理配置, handler=%p, flags=0x%x\n",
                        act->__sigaction_handler.sa_handler, act->sa_flags);
                t->sigaction[signum] = *act; ///< 如果act非NULL，设置新处理配置
                // memcpy(&t->sigaction[signum], act, sizeof(sigaction));
            }

    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "set_sigaction: 函数返回0\n");
    return 0;
}

/**
 * @brief 实际执行信号掩码的修改操作
 *
 * @param how     指定如何修改信号掩码
 * @param set     指向要设置的新信号集（可为NULL表示不修改）
 * @param oldset  用于返回原来的信号集（可为NULL表示不需要返回）
 * @return int    总是返回0
 */
int sigprocmask(int how, __sigset_t *set, __sigset_t *oldset)
{
    struct proc *p = myproc();
    if (!p || !p->current_thread)
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "sigprocmask: 进程或当前线程不存在\n");
        return -1;
    }
    
    thread_t *t = p->current_thread;
    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: 进入函数, how=%d, pid=%d, tid=%d\n", how, p->pid, t->tid);
    
    // 保存旧的信号掩码
    if (oldset)
    {
        for (int i = 0; i < SIGSET_LEN; i++)
        {
            oldset->__val[i] = t->sig_set.__val[i];
        }
        DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: 保存旧信号掩码=0x%lx\n", oldset->__val[0]);
    }
    
    // 根据how参数修改信号掩码
    for (int i = 0; i < SIGSET_LEN; i++)
    {
        switch (how)
        {
        case SIG_BLOCK: ///< 阻塞指定的信号
            DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: SIG_BLOCK操作, 新信号集[%d]=0x%lx\n", i, set->__val[i]);
            t->sig_set.__val[i] |= set->__val[i];
            break;
        case SIG_UNBLOCK: ///< 解除阻塞指定的信号
            DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: SIG_UNBLOCK操作, 新信号集[%d]=0x%lx\n", i, set->__val[i]);
            t->sig_set.__val[i] &= ~set->__val[i];
            break;
        case SIG_SETMASK: ///<  直接设置新信号掩码
            DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: SIG_SETMASK操作, 新信号集[%d]=0x%lx\n", i, set->__val[i]);
            t->sig_set.__val[i] = set->__val[i];
            break;
        default:
            DEBUG_LOG_LEVEL(LOG_ERROR, "sigprocmask: 无效的how参数 %d\n", how);
            return -1;
        }
    }
    
    // 应用特殊信号规则：SIGKILL和SIGSTOP不能被阻塞
    DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: 应用特殊信号规则前, 信号掩码=0x%lx\n", t->sig_set.__val[0]);
    t->sig_set.__val[0] |= (1ul << SIGKILL) | (1ul << SIGSTOP); // 始终阻塞
    t->sig_set.__val[0] &= ~(1ul << SIGTERM);                   // 允许SIGTERM被解除阻塞
    DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: 应用特殊信号规则后, 信号掩码=0x%lx\n", t->sig_set.__val[0]);
    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: 函数返回0\n");
    return 0;
}

/**
 * @brief 检查是否有待处理的信号需要处理
 *
 * @param p 当前进程
 * @return int 返回待处理的信号编号，如果没有则返回0
 */
int check_pending_signals(struct proc *p)
{
    if (!p || !p->current_thread)
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "check_pending_signals: 进程或当前线程不存在\n");
        return 0;
    }
    
    thread_t *t = p->current_thread;
    
    // 检查是否有未被阻塞的待处理信号
    uint64 pending = t->sig_pending.__val[0] & ~t->sig_set.__val[0];

    if (pending == 0)
    {
        return 0;
    }

    // 找到最低位的信号，但需要检查是否被忽略
    for (int sig = 1; sig <= SIGRTMAX; sig++)
    {
        uint64 sig_mask = (1ul << (sig - 1));
        if (pending & sig_mask)
        {
            // 检查信号是否被设置为忽略
            if (t->sigaction[sig].__sigaction_handler.sa_handler == SIG_IGN)
            {
                DEBUG_LOG_LEVEL(LOG_DEBUG, "check_pending_signals: 信号 %d (%s) 被忽略，清除并继续查找\n", sig, get_signal_name(sig));
                // 清除被忽略的信号
                t->sig_pending.__val[0] &= ~sig_mask;
                // 重新计算未被阻塞的信号
                pending = t->sig_pending.__val[0] & ~t->sig_set.__val[0];
                if (pending == 0)
                {
                    return 0;
                }
                // 继续查找下一个信号
                continue;
            }

            DEBUG_LOG_LEVEL(LOG_DEBUG, "check_pending_signals: 找到待处理信号 %d (%s)\n", sig, get_signal_name(sig));
            return sig;
        }
    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "check_pending_signals: 未找到有效信号，返回0\n");
    return 0;
}

/**
 * @brief 处理指定的信号
 *
 * @param p 当前进程
 * @param sig 信号编号
 * @return int 0表示成功处理，-1表示处理失败
 */
int handle_signal(struct proc *p, int sig)
{
    if (!p || !p->current_thread)
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "handle_signal: 进程或当前线程不存在\n");
        return -1;
    }
    
    thread_t *t = p->current_thread;
    
    // 检查信号是否有效
    if (sig <= 0 || sig > SIGRTMAX)
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "handle_signal: 无效的信号编号 %d\n", sig);
        return -1;
    }

    // 特殊处理线程取消信号
    if (sig == SIGCANCEL || sig == SIGCANCEL + 1) // SIGCANCEL 通常是 SIGRTMIN
    {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 处理线程取消信号 %d\n", sig);
        
        if (t->cancel_state == PTHREAD_CANCEL_ENABLE)
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 线程 %d 取消状态为启用，设置取消请求标志\n", t->tid);
            
            // 设置线程取消请求标志
            
            // 唤醒等待此线程的pthread_join
            if (t->join_futex_addr != 0) {
                futex_wake(t->join_futex_addr, INT_MAX);
            }
            
            DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 线程 %d 取消请求标志已设置\n", t->tid);
        }
        else
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 线程取消状态为禁用\n");
        }
        
        // 清除待处理信号
        t->sig_pending.__val[0] &= ~(1ul << (sig - 1));
        return 0;
    }

    // 检查是否有信号处理函数
    // 检查是否是SIG_IGN（忽略信号）
    if (t->sigaction[sig].__sigaction_handler.sa_handler == SIG_IGN)
    {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 信号 %d 设置为忽略\n", sig);
        // 忽略信号，直接清除待处理信号
        t->sig_pending.__val[0] &= ~(1ul << (sig - 1));
        DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 清除待处理信号, 新的待处理信号=0x%lx\n", t->sig_pending.__val[0]);
        return 0;
    }

    // 检查是否是SIG_DFL（默认处理）或NULL
    if (t->sigaction[sig].__sigaction_handler.sa_handler == SIG_DFL ||
        t->sigaction[sig].__sigaction_handler.sa_handler == NULL)
    {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 信号 %d 使用默认处理\n", sig);

        // 特殊处理SIGSTOP信号
        if (sig == SIGSTOP)
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: SIGSTOP信号，停止进程\n");
            // SIGSTOP信号停止进程，设置停止标志而不是killed标志
            p->killed = SIGSTOP; // 使用killed字段存储停止信号
            // 清除待处理信号
            t->sig_pending.__val[0] &= ~(1ul << (sig - 1));
            return 0;
        }

        // 特殊处理SIGCONT信号
        if (sig == SIGCONT)
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: SIGCONT信号，继续进程\n");
            // SIGCONT信号继续被停止的进程
            if (p->killed == SIGSTOP)
            {
                p->killed = 0;    // 清除停止标志
                p->continued = 1; // 设置继续标志
            }
            // 清除待处理信号
            t->sig_pending.__val[0] &= ~(1ul << (sig - 1));
            return 0;
        }

        // 默认处理：对于SIGCHLD，忽略；对于其他信号，终止进程
        if (sig != SIGCHLD)
        {
            // 对于实时信号（SIGRTMIN到SIGRTMAX），默认忽略而不是终止进程
            if (sig >= SIGRTMIN && sig <= SIGRTMAX)
            {
                DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 实时信号 %d 默认忽略\n", sig);
                // 清除待处理信号
                t->sig_pending.__val[0] &= ~(1ul << (sig - 1));
                DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 清除待处理信号, 新的待处理信号=0x%lx\n", t->sig_pending.__val[0]);
                return 0;
            }
            DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 设置进程终止标志, killed=%d\n", sig);
            p->killed = sig;
        }
        else
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: SIGCHLD信号，忽略处理\n");
        }
        // 清除待处理信号
        t->sig_pending.__val[0] &= ~(1ul << (sig - 1));
        return 0;
    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 信号 %d 有自定义处理函数 %p\n", sig, t->sigaction[sig].__sigaction_handler.sa_handler);

    // 清除待处理信号
    t->sig_pending.__val[0] &= ~(1ul << (sig - 1));
    DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 清除待处理信号, 新的待处理信号=0x%lx\n", t->sig_pending.__val[0]);

    // 调用信号处理函数
    // 注意：这里需要保存当前上下文，设置信号处理函数的参数，然后调用
    // 由于信号处理函数在用户态执行，我们需要在返回用户态时处理

    DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 函数返回0\n");
    return 0;
}

#define SIGTRAMPOLINE (MAXVA - 0x10000000) // 先定这么多
/**
 * @brief 在返回用户态前检查和处理信号
 *
 * @param p 当前进程
 * @param trapframe 陷阱帧
 * @return int 1表示需要处理信号，0表示不需要
 */
int check_and_handle_signals(struct proc *p, struct trapframe *trapframe)
{
    if (!p || !p->current_thread)
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "check_and_handle_signals: 进程或当前线程不存在\n");
        return 0;
    }
    
    thread_t *t = p->current_thread;

    // 检查线程取消状态
    if (thread_check_cancellation(t))
    {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 线程 %d 已被取消\n", t->tid);
        thread_exit(PTHREAD_CANCELED);
        return 1; // 表示需要处理
    }

    int sig = check_pending_signals(p);
    if (sig == 0)
    {
        return 0;
    }

    // 处理信号
    if (handle_signal(p, sig) == 0)
    {
        // 如果有信号处理函数，需要设置trapframe以便在用户态调用
        if (t->sigaction[sig].__sigaction_handler.sa_handler != NULL)
        {
            // 保存信号处理前的上下文
            void copytrapframe(struct trapframe * f1, struct trapframe * f2);
            copytrapframe(&p->sig_trapframe, p->trapframe);
            DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 设置trapframe以调用信号处理函数\n");

            // 设置信号处理函数的参数（信号编号）
            trapframe->a0 = sig;
            DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 设置a0=%d\n", sig);

            // 设置返回地址为信号处理函数
#ifdef RISCV
            trapframe->epc = (uint64)t->sigaction[sig].__sigaction_handler.sa_handler;
            DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 设置epc=0x%lx (RISC-V)\n", trapframe->epc);
#else
            trapframe->era = (uint64)t->sigaction[sig].__sigaction_handler.sa_handler;
            DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 设置era=0x%lx (LoongArch)\n", trapframe->era);
#endif
            trapframe->ra = SIGTRAMPOLINE;
            trapframe->sp -= PGSIZE;
            // 保存原始返回地址到用户栈，以便信号处理函数返回后继续执行
            // 这里简化处理，实际应该保存到用户栈

            // 记录当前处理的信号，以便在信号处理完成后设置killed标志
            p->current_signal = sig;
            // 设置信号中断标志，表示pselect等系统调用应该返回EINTR
            p->signal_interrupted = 1;
            DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 设置signal_interrupted=1\n");

            DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 需要处理信号，返回1\n");
            return 1;
        }
    }
    else
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "check_and_handle_signals: 信号处理失败\n");
    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 函数返回0\n");
    return 0;
}

/**
 * @brief 调试函数：打印信号集的详细信息
 *
 * @param p 进程指针
 * @param prefix 输出前缀
 */
void debug_print_signal_info(struct proc *p, const char *prefix)
{
    if (!p || !p->current_thread)
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "%s: 进程或当前线程不存在\n", prefix);
        return;
    }
    
    thread_t *t = p->current_thread;

    DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 进程pid=%d, 线程tid=%d\n", prefix, p->pid, t->tid);
    DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 待处理信号=0x%lx\n", prefix, t->sig_pending.__val[0]);
    DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 信号掩码=0x%lx\n", prefix, t->sig_set.__val[0]);

    // 打印待处理的信号
    DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 待处理的信号:\n", prefix);
    for (int sig = 1; sig <= SIGRTMAX; sig++)
    {
        if (t->sig_pending.__val[0] & (1ul << sig))
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "%s:   - %d (%s)\n", prefix, sig, get_signal_name(sig));
        }
    }

    // 打印未被阻塞的待处理信号
    uint64 unblocked_pending = t->sig_pending.__val[0] & ~t->sig_set.__val[0];
    DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 未被阻塞的待处理信号=0x%lx\n", prefix, unblocked_pending);
    for (int sig = 1; sig <= SIGRTMAX; sig++)
    {
        if (unblocked_pending & (1ul << sig))
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "%s:   - %d (%s)\n", prefix, sig, get_signal_name(sig));
        }
    }
}