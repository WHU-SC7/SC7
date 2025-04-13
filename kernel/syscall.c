#include "types.h"
#include "print.h"
#include "trap.h" //这个必须要再hsai_trap.h前。syscall(struct trapframe *trapframe)参数表中的trapframe对外部不可见，所以先声明trapframe
#include "hsai_trap.h"
#include "vmem.h"


void sys_write(int fd, uint64 va, int len)
{
    struct proc *p = curr_proc();
    char str[200];
    int size = copyinstr(p->pagetable, str, va, len);
    printf("write系统调用,str:%s,size:%d\n",str,size);
    //printf("write系统调用");
}

uint64 a[8]; // 8个a寄存器，a7是系统调用号
void syscall(struct trapframe *trapframe)
{
    // uint64 a[8];
    for (int i = 0; i < 8; i++)
        a[i] = hsai_get_arg(trapframe, i);

    switch (a[7])
    {
    case 64:
        sys_write(a[0], a[1], a[2]);
        break;

    default:
        printf("unknown syscall with a7: %d", a[7]);
        break;
    }

}

