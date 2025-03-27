//把中断，异常，陷入转发给hal层的trap处理
//
//
#include "types.h"
#include "trap.h"

void hsai_set_usertrap(void)
{
	
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

void hsai_set_trapframe_kernel_trap(struct trapframe *trapframe, uint64 value)//修改 陷入时异常处理函数的地址
{
    #if defined RISCV
        trapframe->kernel_trap=value;
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