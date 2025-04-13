//
//
//
#ifndef __HSAI_TRAP_H__
#define __HSAI_TRAP_H__

#include "types.h"
#include "process.h"


void hsai_trap_init(); ///< 设置loongarch的ecfg

void hsai_set_usertrap();//设置异常跳转地址，trap_init

void hsai_set_csr_to_usermode(); //设置好csr寄存器，准备进入U态

void hsai_set_csr_sepc(uint64 addr); //设置sepc, sret时跳转

uint64 hsai_get_arg(struct trapframe *trapframe, uint64 register_num); //从trapframe获取参数a0-a5

void hsai_swtch(struct context *idle, struct context *p);//context与架构相关，暂时没有更好的办法

void hsai_set_trapframe_kernel_sp(struct trapframe *trapframe, uint64 value);//修改线程内核栈

void hsai_set_trapframe_kernel_trap(struct trapframe *trapframe);//为给定的trapframe设置usertrap,在trampoline保存状态后usertrap处理陷入或异常

void hsai_set_trapframe_epc(struct trapframe *trapframe, uint64 value);//修改返回地址，loongarch的Trapframe为era,意义相同

void hsai_set_trapframe_user_sp(struct trapframe *trapframe, uint64 value);//修改用户态的栈

void hsai_set_trapframe_pagetable(struct trapframe *trapframe);//修改页表

void hsai_usertrapret(); ///< 返回用户态，一个线程第一次启动时执行

void forkret(void);


#endif