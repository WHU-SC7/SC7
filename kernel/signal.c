#include "signal.h"
#include "process.h"
#include "cpu.h"

int set_sigaction(int signum, sigaction const *act, sigaction *oldact)
{
    proc_t *p = myproc();
    if(p->sigaction[signum].__sigaction_handler.sa_handler !=NULL && oldact !=NULL){
        *oldact = p->sigaction[signum];
    }
    if(act != NULL){
        p->sigaction[signum] = *act;
    }
    return 0;
}

int sigprocmask(int how, __sigset_t *set, __sigset_t *oldset)
{
    proc_t *p = myproc();
    for (int i = 0; i < SIGSET_LEN; i++)
    {
        if (oldset)
        {
            oldset->__val[i] = p->sig_set.__val[i];
        }
        if (set == NULL)
            continue;
        switch (how)
        {
        case SIG_BLOCK:
            p->sig_set.__val[i] |= set->__val[i];
            break;
        case SIG_UNBLOCK:
            p->sig_set.__val[i] &= ~set->__val[i];
            break;
        case SIG_SETMASK:
            p->sig_set.__val[i] = set->__val[i];
            break;
        default:
            break;
        }
    }
    p->sig_set.__val[0] &= 1ul << SIGTERM | 1ul << SIGKILL | 1ul << SIGSTOP;
    return 0;
}