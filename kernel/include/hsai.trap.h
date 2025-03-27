//
//
//

#include "types.h"

void hsai_swtch(struct context *idle, struct context *p);//context与架构相关，暂时没有更好的办法

void hsai_set_trapframe_kernel_sp(struct trapframe *trapframe, uint64 value);//修改线程内核栈

void hsai_set_trapframe_kernel_trap(struct trapframe *trapframe, uint64 value);//修改 陷入时异常处理函数的地址

void hsai_set_trapframe_epc(struct trapframe *trapframe, uint64 value);//修改返回地址，loongarch的Trapframe为era,意义相同
