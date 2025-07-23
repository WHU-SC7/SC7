#include "signal.h"
#include "process.h"
#include "cpu.h"
#include "print.h"

// 信号名称数组，用于调试输出
static const char* signal_names[] = {
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
    [SIGRTMIN+1] = "SIGRTMIN+1",
    [SIGRTMIN+2] = "SIGRTMIN+2",
    [SIGRTMIN+3] = "SIGRTMIN+3",
    [SIGRTMIN+4] = "SIGRTMIN+4",
    [SIGRTMIN+5] = "SIGRTMIN+5",
    [SIGRTMIN+6] = "SIGRTMIN+6",
    [SIGRTMIN+7] = "SIGRTMIN+7",
    [SIGRTMIN+8] = "SIGRTMIN+8",
    [SIGRTMIN+9] = "SIGRTMIN+9",
    [SIGRTMIN+10] = "SIGRTMIN+10",
    [SIGRTMIN+11] = "SIGRTMIN+11",
    [SIGRTMIN+12] = "SIGRTMIN+12",
    [SIGRTMIN+13] = "SIGRTMIN+13",
    [SIGRTMIN+14] = "SIGRTMIN+14",
    [SIGRTMIN+15] = "SIGRTMIN+15",
    [SIGRTMIN+16] = "SIGRTMIN+16",
    [SIGRTMIN+17] = "SIGRTMIN+17",
    [SIGRTMIN+18] = "SIGRTMIN+18",
    [SIGRTMIN+19] = "SIGRTMIN+19",
    [SIGRTMIN+20] = "SIGRTMIN+20",
    [SIGRTMIN+21] = "SIGRTMIN+21",
    [SIGRTMIN+22] = "SIGRTMIN+22",
    [SIGRTMIN+23] = "SIGRTMIN+23",
    [SIGRTMIN+24] = "SIGRTMIN+24",
    [SIGRTMIN+25] = "SIGRTMIN+25",
    [SIGRTMIN+26] = "SIGRTMIN+26",
    [SIGRTMIN+27] = "SIGRTMIN+27",
    [SIGRTMIN+28] = "SIGRTMIN+28",
    [SIGRTMIN+29] = "SIGRTMIN+29",
    [SIGRTMIN+30] = "SIGRTMIN+30",
    [SIGRTMIN+31] = "SIGRTMIN+31",
    [SIGRTMAX] = "SIGRTMAX"
};

// 获取信号名称的辅助函数
[[maybe_unused]]static const char* get_signal_name(int sig) {
    if (sig >= 0 && sig < sizeof(signal_names)/sizeof(signal_names[0]) && signal_names[sig]) {
        return signal_names[sig];
    }
    return "UNKNOWN";
}

int set_sigaction(int signum, sigaction const *act, sigaction *oldact)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "set_sigaction: 进入函数, signum=%d (%s)\n", signum, get_signal_name(signum));
    
    proc_t *p = myproc();
    if (!p) {
        DEBUG_LOG_LEVEL(LOG_ERROR, "set_sigaction: 当前进程为空\n");
        return -1;
    }
    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "set_sigaction: 进程pid=%d\n", p->pid);
    
    // 打印修改前的信号处理配置
    if (p->sigaction[signum].__sigaction_handler.sa_handler != NULL) {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "set_sigaction: 当前信号 %d 已有处理函数 %p\n", 
                       signum, p->sigaction[signum].__sigaction_handler.sa_handler);
    }
    
    if(p->sigaction[signum].__sigaction_handler.sa_handler !=NULL && oldact !=NULL){ ///<  如果oldact非NULL且当前信号已有处理函数，保存旧配置
        DEBUG_LOG_LEVEL(LOG_DEBUG, "set_sigaction: 保存旧信号处理配置\n");
        *oldact = p->sigaction[signum];
    }
    if(act != NULL){
        DEBUG_LOG_LEVEL(LOG_DEBUG, "set_sigaction: 设置新信号处理配置, handler=%p, flags=0x%x\n", 
                       act->__sigaction_handler.sa_handler, act->sa_flags);
        p->sigaction[signum] = *act; ///< 如果act非NULL，设置新处理配置
        DEBUG_LOG_LEVEL(LOG_DEBUG, "set_sigaction: 设置后验证, 信号%d的处理函数=%p\n", 
                       signum, p->sigaction[signum].__sigaction_handler.sa_handler);
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
    DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: 进入函数, how=%d\n", how);
    
    proc_t *p = myproc();
    if (!p) {
        DEBUG_LOG_LEVEL(LOG_ERROR, "sigprocmask: 当前进程为空\n");
        return -1;
    }
    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: 进程pid=%d\n", p->pid);
    // 打印修改前的信号状态
    debug_print_signal_info(p, "sigprocmask_before");
    
    // 打印当前信号掩码
    DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: 当前信号掩码=0x%lx\n", p->sig_set.__val[0]);
    
    for (int i = 0; i < SIGSET_LEN; i++)  ///< 遍历信号集的每个元素（通常是64位数组）
    {
         // 如果需要返回旧信号集，先保存当前值
        if (oldset)
        {
            oldset->__val[i] = p->sig_set.__val[i];
            DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: 保存旧信号集[%d]=0x%lx\n", i, oldset->__val[i]);
        }
        // 如果set为NULL，跳过修改操作
        if (set == NULL) {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: set为NULL，跳过修改\n");
            continue;
        }
        // 根据how参数执行不同的信号集操作
        switch (how)
        {
        case SIG_BLOCK:     ///<  阻塞指定信号（或操作）
            DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: SIG_BLOCK操作, 新信号集[%d]=0x%lx\n", i, set->__val[i]);
            p->sig_set.__val[i] |= set->__val[i];
            break;
        case SIG_UNBLOCK:   ///<  解除阻塞指定信号（与非操作）
            DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: SIG_UNBLOCK操作, 新信号集[%d]=0x%lx\n", i, set->__val[i]);
            p->sig_set.__val[i] &= ~set->__val[i];
            break;
        case SIG_SETMASK:   ///<  直接设置新信号掩码
            DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: SIG_SETMASK操作, 新信号集[%d]=0x%lx\n", i, set->__val[i]);
            p->sig_set.__val[i] = set->__val[i];
            break;
        default:
            DEBUG_LOG_LEVEL(LOG_ERROR, "sigprocmask: 无效的how参数=%d\n", how);
            break;
        }
    }
    /* 
    特殊处理：确保SIGTERM、SIGKILL和SIGSTOP信号始终不被阻塞
    通过位操作保留这三个信号的屏蔽位，其他位清零
    */
    // 修改后：仅强制保留关键信号的阻塞状态
    DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: 应用特殊信号规则前, 信号掩码=0x%lx\n", p->sig_set.__val[0]);
    p->sig_set.__val[0] |= (1ul << SIGKILL) | (1ul << SIGSTOP); // 始终阻塞
    p->sig_set.__val[0] &= ~(1ul << SIGTERM); // 允许SIGTERM被解除阻塞
    DEBUG_LOG_LEVEL(LOG_DEBUG, "sigprocmask: 应用特殊信号规则后, 信号掩码=0x%lx\n", p->sig_set.__val[0]);
    
    // 打印修改后的信号状态
    debug_print_signal_info(p, "sigprocmask_after");
    
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
    // DEBUG_LOG_LEVEL(LOG_DEBUG, "check_pending_signals: 进入函数, pid=%d\n", p->pid);
    
    // 检查是否有未被阻塞的待处理信号
    uint64 pending = p->sig_pending.__val[0] & ~p->sig_set.__val[0];
    
    // DEBUG_LOG_LEVEL(LOG_DEBUG, "check_pending_signals: 待处理信号=0x%lx, 信号掩码=0x%lx, 未被阻塞信号=0x%lx\n", p->sig_pending.__val[0], p->sig_set.__val[0], pending);
    
    if (pending == 0) {
        // DEBUG_LOG_LEVEL(LOG_DEBUG, "check_pending_signals: 没有待处理的信号\n");
        return 0;
    }
    
    // 找到最低位的信号，但需要检查是否被忽略
    for (int sig = 1; sig <= SIGRTMAX; sig++) {
        if (pending & (1ul << sig)) {
            // 检查信号是否被设置为忽略
            if (p->sigaction[sig].__sigaction_handler.sa_handler == SIG_IGN) {
                DEBUG_LOG_LEVEL(LOG_DEBUG, "check_pending_signals: 信号 %d (%s) 被忽略，清除并继续查找\n", sig, get_signal_name(sig));
                // 清除被忽略的信号
                p->sig_pending.__val[0] &= ~(1ul << sig);
                // 重新计算未被阻塞的信号
                pending = p->sig_pending.__val[0] & ~p->sig_set.__val[0];
                if (pending == 0) {
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
    // DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 进入函数, pid=%d, sig=%d (%s)\n", p->pid, sig, get_signal_name(sig));
    
    // 检查信号是否有效
    if (sig <= 0 || sig > SIGRTMAX) {
        DEBUG_LOG_LEVEL(LOG_ERROR, "handle_signal: 无效的信号编号 %d\n", sig);
        return -1;
    }
    
    // 检查是否有信号处理函数
    // DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 检查信号 %d 的处理函数, 当前值=%p\n", sig, p->sigaction[sig].__sigaction_handler.sa_handler);
    
    // 检查是否是SIG_IGN（忽略信号）
    if (p->sigaction[sig].__sigaction_handler.sa_handler == SIG_IGN) {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 信号 %d 设置为忽略\n", sig);
        // 忽略信号，直接清除待处理信号
        p->sig_pending.__val[0] &= ~(1ul << sig);
        DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 清除待处理信号, 新的待处理信号=0x%lx\n", p->sig_pending.__val[0]);
        return 0;
    }
    
    // 检查是否是SIG_DFL（默认处理）或NULL
    if (p->sigaction[sig].__sigaction_handler.sa_handler == SIG_DFL || 
        p->sigaction[sig].__sigaction_handler.sa_handler == NULL) {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 信号 %d 使用默认处理\n", sig);
        
        // 特殊处理SIGSTOP信号
        if (sig == SIGSTOP) {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: SIGSTOP信号，停止进程\n");
            // SIGSTOP信号停止进程，设置停止标志而不是killed标志
            p->killed = SIGSTOP;  // 使用killed字段存储停止信号
            // 清除待处理信号
            p->sig_pending.__val[0] &= ~(1ul << sig);
            return 0;
        }
        
        // 特殊处理SIGCONT信号
        if (sig == SIGCONT) {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: SIGCONT信号，继续进程\n");
            // SIGCONT信号继续被停止的进程
            if (p->killed == SIGSTOP) {
                p->killed = 0;  // 清除停止标志
                p->continued = 1;  // 设置继续标志
            }
            // 清除待处理信号
            p->sig_pending.__val[0] &= ~(1ul << sig);
            return 0;
        }
        
        // 默认处理：对于SIGCHLD，忽略；对于其他信号，终止进程
        if (sig != SIGCHLD) {
            // 对于实时信号（SIGRTMIN到SIGRTMAX），默认忽略而不是终止进程
            if (sig >= SIGRTMIN && sig <= SIGRTMAX) {
                DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 实时信号 %d 默认忽略\n", sig);
                // 清除待处理信号
                p->sig_pending.__val[0] &= ~(1ul << sig);
                DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 清除待处理信号, 新的待处理信号=0x%lx\n", p->sig_pending.__val[0]);
                return 0;
            }
            DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 设置进程终止标志, killed=%d\n", sig);
            p->killed = sig;
        } else {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: SIGCHLD信号，忽略处理\n");
        }
        // 清除待处理信号
        p->sig_pending.__val[0] &= ~(1ul << sig);
        // DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 清除待处理信号, 新的待处理信号=0x%lx\n", p->sig_pending.__val[0]);
        return 0;
    }
    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 信号 %d 有自定义处理函数 %p\n", sig, p->sigaction[sig].__sigaction_handler.sa_handler);
    
    // 清除待处理信号
    p->sig_pending.__val[0] &= ~(1ul << sig);
    DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 清除待处理信号, 新的待处理信号=0x%lx\n", p->sig_pending.__val[0]);
    
    // 调用信号处理函数
    // 注意：这里需要保存当前上下文，设置信号处理函数的参数，然后调用
    // 由于信号处理函数在用户态执行，我们需要在返回用户态时处理
    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 函数返回0\n");
    return 0;
}

#define SIGTRAMPOLINE (MAXVA - 0x10000000) //先定这么多
/**
 * @brief 在返回用户态前检查和处理信号
 * 
 * @param p 当前进程
 * @param trapframe 陷阱帧
 * @return int 1表示需要处理信号，0表示不需要
 */
int check_and_handle_signals(struct proc *p, struct trapframe *trapframe)
{
    // DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 进入函数, pid=%d\n", p->pid);
    
    // 打印当前信号状态
    // debug_print_signal_info(p, "check_and_handle_signals");
    
    int sig = check_pending_signals(p);
    if (sig == 0) {
        // DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 没有待处理信号，返回0\n");
        return 0;
    }
    
    
    // 处理信号
    if (handle_signal(p, sig) == 0) {
        // 如果有信号处理函数，需要设置trapframe以便在用户态调用
        if (p->sigaction[sig].__sigaction_handler.sa_handler != NULL) {
            // LOG("跳转到信号处理！");
            //保存信号处理前的上下文
            void copytrapframe(struct trapframe *f1, struct trapframe *f2);
            copytrapframe(&p->sig_trapframe,p->trapframe);
            DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 设置trapframe以调用信号处理函数\n");
            
            // 设置信号处理函数的参数（信号编号）
            trapframe->a0 = sig;
            DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 设置a0=%d\n", sig);
            
            // 设置返回地址为信号处理函数
#ifdef RISCV
            trapframe->epc = (uint64)p->sigaction[sig].__sigaction_handler.sa_handler;
            DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 设置epc=0x%lx (RISC-V)\n", trapframe->epc);
#else
            trapframe->era = (uint64)p->sigaction[sig].__sigaction_handler.sa_handler;
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
    } else {
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
    if (!p) {
        DEBUG_LOG_LEVEL(LOG_ERROR, "%s: 进程指针为空\n", prefix);
        return;
    }
    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 进程pid=%d\n", prefix, p->pid);
    DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 待处理信号=0x%lx\n", prefix, p->sig_pending.__val[0]);
    DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 信号掩码=0x%lx\n", prefix, p->sig_set.__val[0]);
    
    // 打印被阻塞的信号
    // DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 被阻塞的信号:\n", prefix);
    // for (int sig = 1; sig <= SIGRTMAX; sig++) {
    //     if (p->sig_set.__val[0] & (1ul << sig)) {
    //         DEBUG_LOG_LEVEL(LOG_DEBUG, "%s:   - %d (%s)\n", prefix, sig, get_signal_name(sig));
    //     }
    // }
    
    // 打印待处理的信号
    DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 待处理的信号:\n", prefix);
    for (int sig = 1; sig <= SIGRTMAX; sig++) {
        if (p->sig_pending.__val[0] & (1ul << sig)) {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "%s:   - %d (%s)\n", prefix, sig, get_signal_name(sig));
        }
    }
    
    // 打印未被阻塞的待处理信号
    uint64 unblocked_pending = p->sig_pending.__val[0] & ~p->sig_set.__val[0];
    DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 未被阻塞的待处理信号=0x%lx\n", prefix, unblocked_pending);
    for (int sig = 1; sig <= SIGRTMAX; sig++) {
        if (unblocked_pending & (1ul << sig)) {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "%s:   - %d (%s)\n", prefix, sig, get_signal_name(sig));
        }
    }
    
    // 打印信号处理函数配置
    // DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 信号处理函数配置:\n", prefix);
    // for (int sig = 1; sig <= 31; sig++) { // 只打印标准信号
    //     if (p->sigaction[sig].__sigaction_handler.sa_handler != NULL) {
    //         [[maybe_unused]]const char *handler_type = "CUSTOM";
    //         if (p->sigaction[sig].__sigaction_handler.sa_handler == SIG_IGN) {
    //             handler_type = "IGNORE";
    //         } else if (p->sigaction[sig].__sigaction_handler.sa_handler == SIG_DFL) {
    //             handler_type = "DEFAULT";
    //         }
    //         DEBUG_LOG_LEVEL(LOG_DEBUG, "%s:   - %d (%s): %s handler=%p flags=0x%x\n", 
    //                        prefix, sig, get_signal_name(sig), handler_type,
    //                        p->sigaction[sig].__sigaction_handler.sa_handler,
    //                        p->sigaction[sig].sa_flags);
    //     }
    // }
}