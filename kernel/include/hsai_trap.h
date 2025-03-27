//
//
//

#include "types.h"

void hsai_set_usertrap();//设置异常跳转地址，trap_init

void hsai_swtch(struct context *idle, struct context *p);//context与架构相关，暂时没有更好的办法

void hsai_set_trapframe_kernel_sp(struct trapframe *trapframe, uint64 value);//修改线程内核栈

void hsai_set_trapframe_kernel_trap(struct trapframe *trapframe);//为给定的trapframe设置usertrap,在trampoline保存状态后usertrap处理陷入或异常

void hsai_set_trapframe_epc(struct trapframe *trapframe, uint64 value);//修改返回地址，loongarch的Trapframe为era,意义相同

void hsai_set_trapframe_user_sp(struct trapframe *trapframe, uint64 value);//修改用户态的栈

void hsai_set_trapframe_pagetable(struct trapframe *trapframe, uint64 value);//修改页表
