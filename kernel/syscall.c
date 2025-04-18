#include "types.h"
#include "print.h"
#include "trap.h" //这个必须要再hsai_trap.h前。syscall(struct trapframe *trapframe)参数表中的trapframe对外部不可见，所以先声明trapframe
#include "hsai_trap.h"
#include "defs.h"
#include "vmem.h"
#include "cpu.h"
#include "process.h"
#include "timer.h"
#include "syscall_ids.h"
#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

void sys_write(int fd, uint64 va, int len)
{
    struct proc *p = myproc();
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

uint64 sys_gettimeofday(uint64 tv_addr)
{
    struct proc *p = myproc();
    timeval_t tv = timer_get_time();
    return copyout(p->pagetable, tv_addr, (char *)&tv, sizeof(timeval_t));
}
/**
 * @brief 睡眠一段时间
 *        timeval_t* req   目标睡眠时间
 *        timeval_t* rem   未完成睡眠时间
 * @return int 成功返回0 失败返回-1
 */
int sleep(timeval_t *req, timeval_t *rem)
{
    proc_t* p = myproc();
    timeval_t wait; ///<  用于存储从用户空间拷贝的休眠时间
    if(copyin(p->pagetable, (char *)&wait, (uint64)req, sizeof(timeval_t)) == -1){
        return -1;
    }
    timeval_t start,end;
    start = timer_get_time(); ///<  获取休眠开始时间
    acquire(&tickslock);
    while(1){
        end = timer_get_time();
        if(end.sec - start.sec >= wait.sec) break;
        if(myproc()->killed){
            release(&tickslock);
            return -1;
        }
        sleep_on_chan(&ticks,&tickslock);
    }
    release(&tickslock);

    return 0;
}

uint64 a[8]; // 8个a寄存器，a7是系统调用号
void syscall(struct trapframe *trapframe)
{
    for (int i = 0; i < 8; i++)
        a[i] = hsai_get_arg(trapframe, i);
    int ret = -1;
#if DEBUG
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
    case SYS_gettimeofday:
        ret = sys_gettimeofday(a[0]);
        break;
    case SYS_sleep:
        ret = sleep((timeval_t *)a[0], (timeval_t *)a[1]);
        break;

    default:
        ret = -1;
        panic("unknown syscall with a7: %d", a[7]);
    }
    trapframe->a0 = ret;
}
