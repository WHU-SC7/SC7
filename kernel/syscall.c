#include "types.h"
#include "print.h"
#include "trap.h" //这个必须要再hsai_trap.h前。syscall(struct trapframe *trapframe)参数表中的trapframe对外部不可见，所以先声明trapframe
#include "hsai_trap.h"
#include "defs.h"
#include "vmem.h"
#include "cpu.h"
#include "process.h"
#include "syscall_ids.h"

#ifndef DEBUG
#define DEBUG 1
#endif
void sys_write(int fd, uint64 va, int len)
{
    struct proc *p = curr_proc();
    char str[200];
    int size = copyinstr(p->pagetable, str, va, MIN(len, 200));
    printf("write系统调用,str:%s,size:%d\n", str, size);
}

uint64 sys_getpid(void)
{
    return myproc()->pid;
}

uint64 sys_fork(void)
{
    return fork();
}

int sys_wait(uint64 va)
{
    return wait(va);
}

uint64 sys_exit(int n)
{
    exit(n);
    return 0;
}

uint64 a[8]; // 8个a寄存器，a7是系统调用号
void syscall(struct trapframe *trapframe)
{
    for (int i = 0; i < 8; i++)
        a[i] = hsai_get_arg(trapframe, i);
    int ret = -1;
#ifdef DEBUG
    LOG("syscall: a7: %d\n", a[7]);
#endif
    switch (a[7])
    {
    case SYS_write:
        sys_write(a[0], a[1], a[2]);
        break;
    case SYS_getpid:
        ret = sys_getpid();
        break;
    case SYS_fork:
        ret = sys_fork();
        break;
    case SYS_wait:
        ret = sys_wait(a[0]);
        break;
    case SYS_exit:
        sys_exit(a[0]);
        break;

    default:
        ret = -1;
        panic("unknown syscall with a7: %d", a[7]);
    }
    trapframe->a0 = ret;
}
