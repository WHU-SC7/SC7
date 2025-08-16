#include "signal.h"
#include "process.h"
#include "printf.h"
#include "string.h"
#include "thread.h"
#include "futex.h"
#include "hsai.h"
#include "cpu.h"
#include "print.h"
#include "pmem.h"
#include "list.h"

// +++ 新增：定义全信号集常量 +++
const __sigset_t full_sigset = {
    .__val = {0xFFFFFFFFFFFFFFFF}
};

// 新增：验证用户栈地址的有效性
static int is_valid_user_stack_addr(uint64 addr, struct proc *p) {
    
    // 检查地址是否在栈VMA范围内
    struct vma *vma = p->vma->next;
    while (vma != p->vma) {
        if (addr >= vma->addr && addr < vma->end) {
            // 检查是否有写权限
            return (vma->perm & PTE_W) != 0;
        }
        vma = vma->next;
    }
    
    return 1;
}

// 新增：安全地写入用户栈
static int safe_write_user_stack(uint64 addr, void *data, size_t size, struct proc *p) {
    if (!is_valid_user_stack_addr(addr, p)) {
        return -1;
    }
    
    
    // 使用copyout安全地写入用户空间
    if (copyout(p->pagetable, addr, (char*)data, size) < 0) {
        return -1;
    }
    
    return 0;
}

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
                // DEBUG_LOG_LEVEL(LOG_DEBUG, "set_sigaction: 保存旧的信号处理函数\n");
            }
            
            // 设置新的信号处理函数
            if (act)
            {
                // DEBUG_LOG_LEVEL(LOG_DEBUG, "set_sigaction: 设置新信号处理配置, handler=%p, flags=0x%x\n",act->__sigaction_handler.sa_handler, act->sa_flags);
                t->sigaction[signum] = *act; ///< 如果act非NULL，设置新处理配置
                // memcpy(&t->sigaction[signum], act, sizeof(sigaction));
            }

    }

    // DEBUG_LOG_LEVEL(LOG_DEBUG, "set_sigaction: 函数返回0\n");
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
    t->sig_set.__val[0] |= (1ul << (SIGKILL - 1)) | (1ul << (SIGSTOP - 1)); // 始终阻塞SIGKILL和SIGSTOP
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
    
    // +++ 新增：检查进程是否正在退出，如果是则跳过信号检查 +++
    if (p->state == ZOMBIE) {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "check_pending_signals: 进程 %d 正在退出，跳过信号检查\n", p->pid);
        return 0;
    }
    
    // 添加详细的调试信息
    // DEBUG_LOG_LEVEL(LOG_DEBUG, "check_pending_signals: 检查线程 %d 的信号\n", t->tid);
    // DEBUG_LOG_LEVEL(LOG_DEBUG, "check_pending_signals: 待处理信号=0x%lx, 信号掩码=0x%lx\n", t->sig_pending.__val[0], t->sig_set.__val[0]);
    
    // 检查是否有未被阻塞的待处理信号，遍历整个信号集
    for (int i = 0; i < SIGSET_LEN; i++) {
        uint64 pending = t->sig_pending.__val[i] & ~t->sig_set.__val[i];
        
        // DEBUG_LOG_LEVEL(LOG_DEBUG, "check_pending_signals: 索引 %d: 待处理=0x%lx, 掩码=0x%lx, 未阻塞=0x%lx\n", i, t->sig_pending.__val[i], t->sig_set.__val[i], pending);
        
        if (pending == 0) {
            continue;
        }

        // 找到最低位的信号，但需要检查是否被忽略
        for (int sig = i * 64 + 1; sig <= (i + 1) * 64 && sig <= SIGRTMAX; sig++)
        {
            uint64 sig_mask = (1ul << (sig - 1 - i * 64));
            if (pending & sig_mask)
            {
                DEBUG_LOG_LEVEL(LOG_DEBUG, "check_pending_signals: 找到信号位 %d, 掩码=0x%lx\n", sig, sig_mask);
                
                // 检查信号是否被设置为忽略
                if (t->sigaction[sig].__sigaction_handler.sa_handler == SIG_IGN)
                {
                    DEBUG_LOG_LEVEL(LOG_DEBUG, "check_pending_signals: 信号 %d (%s) 被忽略，清除并继续查找\n", sig, get_signal_name(sig));
                    // 清除被忽略的信号
                    t->sig_pending.__val[i] &= ~sig_mask;
                    // 重新计算未被阻塞的信号
                    pending = t->sig_pending.__val[i] & ~t->sig_set.__val[i];
                    if (pending == 0)
                    {
                        break;
                    }
                    // 继续查找下一个信号
                    continue;
                }

                DEBUG_LOG_LEVEL(LOG_DEBUG, "check_pending_signals: 找到待处理信号 %d (%s)\n", sig, get_signal_name(sig));
                return sig;
            }
        }
    }

    // DEBUG_LOG_LEVEL(LOG_DEBUG, "check_pending_signals: 未找到有效信号，返回0\n");
    return 0;
}

/**
 * @brief 测试函数：向当前进程发送一个测试信号
 * 用于调试信号处理系统
 */
int send_test_signal(struct proc *p, int sig)
{
    if (!p || !p->current_thread) {
        DEBUG_LOG_LEVEL(LOG_ERROR, "send_test_signal: 进程或当前线程不存在\n");
        return -1;
    }
    
    thread_t *t = p->current_thread;
    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "send_test_signal: 向线程 %d 发送测试信号 %d\n", t->tid, sig);
    
    // 设置待处理信号
    t->sig_pending.__val[0] |= (1ul << (sig - 1));
    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "send_test_signal: 信号已设置，新的待处理信号=0x%lx\n", t->sig_pending.__val[0]);
    
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
    
    // +++ 新增：检查进程是否正在退出，如果是则丢弃信号 +++
    if (p->state == ZOMBIE) {
        DEBUG_LOG_LEVEL(LOG_WARNING, "handle_signal: 进程 %d 正在退出，丢弃信号 %d\n", p->pid, sig);
        // 清除信号待处理标志
        t->sig_pending.__val[0] &= ~(1ul << (sig - 1));
        return 0;
    }
    
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
            
            // +++ 关键修复：设置取消请求标志 +++
            t->cancel_requested = 1;
            
            // 唤醒等待此线程的pthread_join
            if (t->join_futex_addr != 0) {
                futex_wake(t->join_futex_addr, INT_MAX);
            }
            
            DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 线程 %d 取消请求标志已设置\n", t->tid);
            
            // +++ 关键修复：立即标记线程需要退出 +++
            t->should_exit = 1;
            t->exit_value = (void*)PTHREAD_CANCELED;
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
            p->stopped = 1;
            p->stop_signal = sig;
            // 清除待处理信号
            t->sig_pending.__val[0] &= ~(1ul << (sig - 1));
            return 0;
        }

        // 特殊处理SIGCONT信号
        if (sig == SIGCONT)
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: SIGCONT信号，继续进程\n");
            // SIGCONT信号继续被停止的进程
            if (p->stopped)
            {
                p->stopped = 0;    // 清除停止标志
                p->stop_signal = 0; // 清除停止信号
                p->continued = 1;   // 设置继续标志
            }
            // 清除待处理信号
            t->sig_pending.__val[0] &= ~(1ul << (sig - 1));
            return 0;
        }

        // 默认处理：对于SIGCHLD，忽略；对于其他信号，终止进程
        if (sig != SIGCHLD)
        {
            // 对于实时信号（SIGRTMIN到SIGRTMAX），默认终止进程
            if (sig >= SIGRTMIN && sig <= SIGRTMAX)
            {
                DEBUG_LOG_LEVEL(LOG_DEBUG, "handle_signal: 实时信号 %d 默认终止\n", sig);
                p->killed = sig;
                t->sig_pending.__val[0] &= ~(1ul << (sig - 1));
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

    // +++ 新增：检查进程退出状态，避免在进程退出过程中处理信号 +++
    if (p->state == ZOMBIE) {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 进程 %d 正在退出，跳过信号处理\n", p->pid);
        return 0;
    }

    // 检查线程取消状态
    if (thread_check_cancellation(t))
    {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 线程 %d 已被取消\n", t->tid);
        thread_exit((void*)PTHREAD_CANCELED);
        return 1; // 表示需要处理
    }
    
    // +++ 新增：检查线程退出标志 +++
    if (t->should_exit) {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 线程 %d 需要退出，退出值: %p\n", t->tid, t->exit_value);
        thread_exit(t->exit_value);
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
            // 打印信号处理上下文调试信息
            debug_print_signal_context(p, trapframe, sig);
            
            // +++ 新增：检查进程是否正在退出，如果是则丢弃信号 +++
            if (p->state == ZOMBIE) {
                DEBUG_LOG_LEVEL(LOG_WARNING, "check_and_handle_signals: 进程 %d 正在退出，丢弃信号 %d\n", p->pid, sig);
                // 清除信号待处理标志
                t->sig_pending.__val[0] &= ~(1ul << (sig - 1));
                return 0;
            }
            
            // 保存信号处理前的上下文到线程级信号帧链表
            struct signal_frame *new_frame = kalloc();
            if (new_frame) {
                new_frame->signal_number = sig;
                new_frame->return_address = trapframe->ra;
                new_frame->signal_mask = t->sig_set.__val[0];
                new_frame->tf = kmalloc(sizeof(struct trapframe));
                if (new_frame->tf) {
                    copytrapframe(new_frame->tf, trapframe);
                    list_push_front(&t->signal_frames, &new_frame->elem);
                    DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 保存信号帧到线程链表\n");
                } else {
                    kfree(new_frame);
                    // 内存分配失败，回退到进程级存储
                    copytrapframe(&p->sig_trapframe, trapframe);
                    DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 内存分配失败，使用进程级存储\n");
                }
            } else {
                // 内存分配失败，回退到进程级存储
                copytrapframe(&p->sig_trapframe, trapframe);
                DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 内存分配失败，使用进程级存储\n");
            }
            
            DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 设置trapframe以调用信号处理函数\n");

            // 设置信号处理函数的参数（信号编号）
            trapframe->a0 = sig;
            
            // 根据SA_SIGINFO标志决定参数,只有设置了siginfo和ucontext后处理信号才不会缺页异常
            if (t->sigaction[sig].sa_flags) {
                // 需要设置siginfo和ucontext参数
                // 在用户栈上分配信号帧空间
                uint64_t sp = trapframe->sp - sizeof(siginfo_t) - sizeof(ucontext_t);
                
                // 确保栈地址对齐且有效
                if (!is_valid_user_stack_addr(sp, p)) {
                    DEBUG_LOG_LEVEL(LOG_ERROR, "check_and_handle_signals: 栈地址无效 0x%lx\n", sp);
                    
                    // 尝试使用备用信号栈
                    if (t->altstack.ss_sp && t->altstack.ss_size >= sizeof(siginfo_t) + sizeof(ucontext_t)) {
                        sp = (uint64_t)t->altstack.ss_sp + t->altstack.ss_size - sizeof(siginfo_t) - sizeof(ucontext_t);
                        sp = ALIGN_DOWN(sp, 16);
                        DEBUG_LOG_LEVEL(LOG_WARNING, "check_and_handle_signals: 使用备用信号栈 0x%lx\n", sp);
                    } else {
                        DEBUG_LOG_LEVEL(LOG_ERROR, "check_and_handle_signals: 无有效栈可用，终止进程\n");
                        p->killed = SIGSEGV;
                        return 0;
                    }
                }
                
                // 构造siginfo结构
                siginfo_t info;
                info.si_signo = sig;
                info.si_errno = 0;
                info.si_code = 0;
                info.si_pid = p->pid;
                info.si_uid = 0;
                info.si_status = 0;
                info.si_addr = 0;
                info.si_value.sival_int = 0;
                
                // 安全地写入siginfo到用户栈
                if (safe_write_user_stack(sp, &info, sizeof(siginfo_t), p) < 0) {
                    DEBUG_LOG_LEVEL(LOG_ERROR, "check_and_handle_signals: 写入siginfo失败\n");
                    return 0;
                }
                
                // 构造ucontext结构
                ucontext_t context;
                context.uc_flags = 0;
                context.uc_link = NULL;
                context.uc_stack.ss_sp = (void*)trapframe->sp;
                context.uc_stack.ss_size = PGSIZE;
                context.uc_stack.ss_flags = 0;
                context.uc_sigmask = t->sig_set;
                context.uc_mcontext = NULL;
                
                // 安全地写入ucontext到用户栈
                if (safe_write_user_stack(sp + sizeof(siginfo_t), &context, sizeof(ucontext_t), p) < 0) {
                    DEBUG_LOG_LEVEL(LOG_ERROR, "check_and_handle_signals: 写入ucontext失败\n");
                    return 0;
                }
                
                trapframe->a1 = sp;  // siginfo指针
                trapframe->a2 = sp + sizeof(siginfo_t); // ucontext指针
                trapframe->sp = sp;  // 更新栈指针
                
                DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: SA_SIGINFO标志设置，a1=0x%lx, a2=0x%lx, sp=0x%lx\n", 
                               trapframe->a1, trapframe->a2, trapframe->sp);
            } else {
                // 标准信号处理 - 确保足够的栈空间
                uint64_t sp = trapframe->sp;
                sp -= 128; // 分配128字节的最小栈空间
                sp = ALIGN_DOWN(sp, 16); // 16字节对齐

                // 确保新栈地址有效
                if (!is_valid_user_stack_addr(sp, p)) {
                    DEBUG_LOG_LEVEL(LOG_ERROR, "check_and_handle_signals: 栈地址无效 0x%lx\n", sp);
                    
                    // 尝试使用备用信号栈
                    if (t->altstack.ss_sp && t->altstack.ss_size >= 128) {
                        sp = (uint64_t)t->altstack.ss_sp + t->altstack.ss_size - 128;
                        sp = ALIGN_DOWN(sp, 16);
                        DEBUG_LOG_LEVEL(LOG_WARNING, "check_and_handle_signals: 使用备用信号栈 0x%lx\n", sp);
                    } else {
                        DEBUG_LOG_LEVEL(LOG_ERROR, "check_and_handle_signals: 无有效栈可用，终止进程\n");
                        p->killed = SIGSEGV;
                        return 0;
                    }
                }

                trapframe->a1 = 0;
                trapframe->a2 = 0;
                trapframe->sp = sp;
                DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 标准信号处理，a1=0, a2=0, sp=0x%lx\n", trapframe->sp);
            }
            
            DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 设置a0=%d\n", sig);

            // 设置返回地址为信号处理函数
            uint64 handler_addr = (uint64)t->sigaction[sig].__sigaction_handler.sa_handler;
            
            // 验证信号处理函数地址的有效性
            if (handler_addr == 0 || handler_addr < 0x1000) {
                DEBUG_LOG_LEVEL(LOG_ERROR, "check_and_handle_signals: 无效的信号处理函数地址 0x%lx\n", handler_addr);
                return 0;
            }
            
#ifdef RISCV
            trapframe->epc = handler_addr;
            DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 设置epc=0x%lx (RISC-V)\n", trapframe->epc);
#else
            trapframe->era = handler_addr;
            DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 设置era=0x%lx (LoongArch)\n", trapframe->era);
#endif
            trapframe->ra = SIGTRAMPOLINE;
            
            // 备用信号栈支持：如果设置了SA_ONSTACK标志且存在备用栈则使用备用栈
            if (t->sigaction[sig].sa_flags & SA_ONSTACK && t->altstack.ss_sp) {
                trapframe->sp = (uint64)t->altstack.ss_sp + t->altstack.ss_size;
                DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 使用备用信号栈，sp=0x%lx\n", trapframe->sp);
            }
            // 注意：栈指针已在上面根据信号类型设置，这里不再重复设置
            
            // 信号自动阻塞：执行处理函数时应自动阻塞当前信号（除非设置了SA_NODEFER）
            if (!(t->sigaction[sig].sa_flags & SA_NODEFER)) {
                t->sig_set.__val[0] |= (1ul << (sig - 1));
                DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 自动阻塞信号 %d\n", sig);
            } else {
                DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: SA_NODEFER标志设置，不阻塞信号 %d\n", sig);
            }
            
            // 保存原始返回地址到用户栈，以便信号处理函数返回后继续执行
            // 这里简化处理，实际应该保存到用户栈

            // 记录当前处理的信号，以便在信号处理完成后设置killed标志
            t->current_signal = sig;
            // 设置信号中断标志，表示pselect等系统调用应该返回EINTR
            t->signal_interrupted = 1;
            t->handling_signal = sig;
            DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 设置signal_interrupted=1\n");
            
            // +++ 新增：检查线程退出标志 +++
            if (t->should_exit) {
                DEBUG_LOG_LEVEL(LOG_DEBUG, "check_and_handle_signals: 信号处理前检查：线程 %d 需要退出\n", t->tid);
                // 清理信号帧
                if (new_frame) {
                    kfree(new_frame->tf);
                    kfree(new_frame);
                }
                thread_exit(t->exit_value);
                return 1; // 表示需要处理
            }

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
        DEBUG_LOG_LEVEL(LOG_ERROR, "%s: 进程或线程不存在\n", prefix);
        return;
    }
    
    thread_t *t = p->current_thread;
    
    // 打印信号掩码
    DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 信号掩码=0x%lx\n", prefix, t->sig_set.__val[0]);
    
    // 打印待处理信号
    DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 待处理信号=0x%lx\n", prefix, t->sig_pending.__val[0]);
    
    // 打印信号处理函数
    for (int i = 1; i <= SIGRTMAX; i++)
    {
        if (t->sigaction[i].__sigaction_handler.sa_handler != NULL)
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 信号%d处理函数=0x%lx, 标志=0x%x\n", 
                           prefix, i, (uint64)t->sigaction[i].__sigaction_handler.sa_handler, 
                           t->sigaction[i].sa_flags);
        }
    }
    
    // 打印信号处理状态
    DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 当前信号=%d, 信号中断=%d, 处理中信号=%d\n", 
                   prefix, t->current_signal, t->signal_interrupted, t->handling_signal);
    DEBUG_LOG_LEVEL(LOG_DEBUG, "%s: 信号帧数量=%d\n", prefix, list_size(&t->signal_frames));
}

/**
 * @brief 调试函数：打印信号处理上下文信息
 *
 * @param p 进程指针
 * @param trapframe 陷阱帧
 * @param sig 信号编号
 */
void debug_print_signal_context(struct proc *p, struct trapframe *trapframe, int sig)
{
    if (!p || !p->current_thread || !trapframe)
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "debug_print_signal_context: 参数无效\n");
        return;
    }
    
    thread_t *t = p->current_thread;
    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "=== 信号处理上下文调试信息 ===\n");
    DEBUG_LOG_LEVEL(LOG_DEBUG, "进程ID: %d, 线程ID: %d\n", p->pid, t->tid);
    DEBUG_LOG_LEVEL(LOG_DEBUG, "信号编号: %d (%s)\n", sig, get_signal_name(sig));
    DEBUG_LOG_LEVEL(LOG_DEBUG, "信号处理函数: 0x%lx\n", 
                   (uint64)t->sigaction[sig].__sigaction_handler.sa_handler);
    DEBUG_LOG_LEVEL(LOG_DEBUG, "信号标志: 0x%x\n", t->sigaction[sig].sa_flags);
    DEBUG_LOG_LEVEL(LOG_DEBUG, "原始栈指针: 0x%lx\n", trapframe->sp);
    
    // 检查SA_SIGINFO标志
    if (t->sigaction[sig].sa_flags & SA_SIGINFO) {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "SA_SIGINFO标志已设置\n");
        DEBUG_LOG_LEVEL(LOG_DEBUG, "siginfo大小: %zu\n", sizeof(siginfo_t));
        DEBUG_LOG_LEVEL(LOG_DEBUG, "ucontext大小: %zu\n", sizeof(ucontext_t));
    } else {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "标准信号处理\n");
    }
    
    // 检查栈地址有效性
    [[maybe_unused]]uint64_t test_sp = trapframe->sp - sizeof(siginfo_t) - sizeof(ucontext_t);
    DEBUG_LOG_LEVEL(LOG_DEBUG, "测试栈地址: 0x%lx\n", test_sp);
    DEBUG_LOG_LEVEL(LOG_DEBUG, "栈地址有效性: %s\n", 
                   is_valid_user_stack_addr(test_sp, p) ? "有效" : "无效");
    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "==============================\n");
}

/**
 * @brief 设置信号帧，用于信号处理
 * @param p 当前进程
 * @param sig 信号编号
 * @param trapframe 陷阱帧
 * @return int 0表示成功，-1表示失败
 */
int setup_signal_frame(struct proc *p, int sig, struct trapframe *trapframe)
{
    if (!p || !p->current_thread) {
        return -1;
    }
    
    thread_t *t = p->current_thread;
    
    // +++ 新增：检查进程是否正在退出，如果是则丢弃信号 +++
    if (p->state == ZOMBIE) {
        DEBUG_LOG_LEVEL(LOG_WARNING, "setup_signal_frame: 进程 %d 正在退出，丢弃信号 %d\n", p->pid, sig);
        return -1;
    }
    
    // 创建新的信号帧
    struct signal_frame *new_frame = kmalloc(sizeof(*new_frame));
    if (!new_frame) {
        return -1;
    }
    
    // 初始化信号帧
    new_frame->signal_number = sig;
    new_frame->return_address = trapframe->ra;
    new_frame->signal_mask = t->sig_set.__val[0];
    new_frame->tf = kmalloc(sizeof(struct trapframe));
    if (new_frame->tf) {
        copytrapframe(new_frame->tf, trapframe);
        // 添加到线程的信号帧链表
        list_push_front(&t->signal_frames, &new_frame->elem);
    } else {
        kfree(new_frame);
        return -1;
    }
    
    return 0;
}

/**
 * @brief 恢复信号上下文，用于信号处理完成后的恢复
 * @param p 当前进程
 * @param trapframe 陷阱帧
 * @return int 0表示成功，-1表示失败
 */
int restore_signal_context(struct proc *p, struct trapframe *trapframe)
{
    if (!p || !p->current_thread) {
        DEBUG_LOG_LEVEL(LOG_ERROR, "restore_signal_context: 进程或当前线程不存在\n");
        return -1;
    }
    
    thread_t *t = p->current_thread;
    
    // +++ 新增：检查进程是否正在退出，如果是则跳过信号上下文恢复 +++
    if (p->state == ZOMBIE) {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "restore_signal_context: 进程 %d 正在退出，跳过信号上下文恢复\n", p->pid);
        return -1;
    }
    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "restore_signal_context: 恢复线程 %d 的信号上下文\n", t->tid);
    
    // 检查是否有待恢复的信号帧
    if (list_empty(&t->signal_frames)) {
        DEBUG_LOG_LEVEL(LOG_ERROR, "restore_signal_context: 无待恢复的信号帧\n");
        return -1;
    }
    
    // 获取并移除最新的信号帧
    struct list_elem *e = list_pop_front(&t->signal_frames);
    struct signal_frame *frame = list_entry(e, struct signal_frame, elem);
    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "restore_signal_context: 恢复信号 %d 的上下文\n", frame->signal_number);
    
    // 恢复陷阱帧
    if (frame->tf) {
        copytrapframe(trapframe, frame->tf);
        // DEBUG_LOG_LEVEL(LOG_DEBUG, "restore_signal_context: 陷阱帧已恢复, epc=0x%lx, sp=0x%lx\n", trapframe->epc, trapframe->sp);
    } else {
        DEBUG_LOG_LEVEL(LOG_ERROR, "restore_signal_context: 信号帧中的陷阱帧为空\n");
        kfree(frame);
        return -1;
    }
    
    // 恢复信号掩码
    t->sig_set.__val[0] = frame->signal_mask;
    DEBUG_LOG_LEVEL(LOG_DEBUG, "restore_signal_context: 信号掩码已恢复: 0x%lx\n", t->sig_set.__val[0]);
    
    // +++ 关键修复：检查线程是否需要退出 +++
    DEBUG_LOG_LEVEL(LOG_DEBUG, "restore_signal_context: 检查线程退出标志: %d, 退出值: %p\n", t->should_exit, t->exit_value);
    if (t->should_exit) {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "restore_signal_context: 线程 %d 需要退出，退出值: %p\n", t->tid, t->exit_value);
        // 释放信号帧内存
        kfree(frame->tf);
        kfree(frame);
        // 调用线程退出函数
        thread_exit(t->exit_value);
        // 不会返回
    }
    
    // 清除信号处理状态
    t->current_signal = 0;
    t->signal_interrupted = 0;
    t->handling_signal = -1;
    
    // 释放信号帧内存
    kfree(frame->tf);
    kfree(frame);
    
    DEBUG_LOG_LEVEL(LOG_DEBUG, "restore_signal_context: 信号上下文恢复完成\n");
    return 0;
}