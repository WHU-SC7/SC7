//把中断，异常，陷入转发给hal层的trap处理
//
//
#include "types.h"
#include "trap.h"

extern char uservec[];

// Supervisor Trap-Vector Base Address
// low two bits are mode.
static inline void w_stvec(uint64 x)
{
	asm volatile("csrw stvec, %0" : : "r"(x));
}
void hsai_set_usertrap()
{
	#if defined RISCV //trap_init
        w_stvec((uint64)uservec & ~0x3);
    #else

    #endif
}

void hsai_usertrap()
{

}

void hsai_usertrapret()
{

}

void hsai_get_arg()//从trapframe获取参数a0-a5
{
    #if defined RISCV

    #else

    #endif
}

extern void swtch(struct context *idle, struct context *p);//把idle和p交换

void hsai_swtch(struct context *idle, struct context *p)
{
    #if defined RISCV
        swtch(idle,p);
    #else
        swtch(idle,p);
    #endif
}

void hsai_set_trapframe_kernel_sp(struct trapframe *trapframe, uint64 value)//修改线程内核栈
{
    #if defined RISCV
        trapframe->kernel_sp=value;
    #else

    #endif
}

extern void syscall(); //在kernel中
void usertrap(struct trapframe *trapframe)
{
    syscall();
    while(1) ;
}



//为给定的trapframe设置usertrap,在trampoline保存状态后usertrap处理陷入或异常
//这个usertrap地址是固定的
void hsai_set_trapframe_kernel_trap(struct trapframe *trapframe)
{
    #if defined RISCV
        trapframe->kernel_trap=(uint64)usertrap;
    #else

    #endif
}

void hsai_set_trapframe_epc(struct trapframe *trapframe, uint64 value)//修改返回地址，loongarch的Trapframe为era,意义相同
{
    #if defined RISCV
        trapframe->epc=value;
    #else

    #endif
}

void hsai_set_trapframe_user_sp(struct trapframe *trapframe, uint64 value)//修改用户态的栈
{
    #if defined RISCV
        trapframe->sp=value;
    #else

    #endif
}

void hsai_set_trapframe_pagetable(struct trapframe *trapframe, uint64 value)//修改页表
{
    #if defined RISCV
        trapframe->sp=0;
    #else

    #endif
}