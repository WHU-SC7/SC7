#include "signal.h"
#include "process.h"
#include "cpu.h"

int set_sigaction(int signum, sigaction const *act, sigaction *oldact)
{
    proc_t *p = myproc();
    if(p->sigaction[signum].__sigaction_handler.sa_handler !=NULL && oldact !=NULL){ ///<  如果oldact非NULL且当前信号已有处理函数，保存旧配置
        *oldact = p->sigaction[signum];
    }
    if(act != NULL){
        p->sigaction[signum] = *act; ///< 如果act非NULL，设置新处理配置
    }
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
    proc_t *p = myproc();
    for (int i = 0; i < SIGSET_LEN; i++)  ///< 遍历信号集的每个元素（通常是64位数组）
    {
         // 如果需要返回旧信号集，先保存当前值
        if (oldset)
        {
            oldset->__val[i] = p->sig_set.__val[i];
        }
        // 如果set为NULL，跳过修改操作
        if (set == NULL)
            continue;
        // 根据how参数执行不同的信号集操作
        switch (how)
        {
        case SIG_BLOCK:     ///<  阻塞指定信号（或操作）
            p->sig_set.__val[i] |= set->__val[i];
            break;
        case SIG_UNBLOCK:   ///<  解除阻塞指定信号（与非操作）
            p->sig_set.__val[i] &= ~set->__val[i];
            break;
        case SIG_SETMASK:   ///<  直接设置新信号掩码
            p->sig_set.__val[i] = set->__val[i];
            break;
        default:
            break;
        }
    }
    /* 
    特殊处理：确保SIGTERM、SIGKILL和SIGSTOP信号始终不被阻塞
    通过位操作保留这三个信号的屏蔽位，其他位清零
    */
    p->sig_set.__val[0] &= 1ul << SIGTERM | 1ul << SIGKILL | 1ul << SIGSTOP;
    return 0;
}