#include "types.h"
#include "print.h"
#include "trap.h"//这个必须要再hsai_trap.h前。syscall(struct trapframe *trapframe)参数表中的trapframe对外部不可见，所以先声明trapframe
#include "hsai_trap.h"


void sys_write(int fd,char *str,int len)
{
  printf("write系统调用\n");
}

uint64 a[8];//8个a寄存器，a7是系统调用号
void syscall(struct trapframe *trapframe)
{
  #if defined RISCV
  //uint64 a[8];
  for(int i=0;i<8;i++)
    a[i]=hsai_get_arg(trapframe,i);

  switch (a[7])
  {
  case 64:
    sys_write(a[0],(char *)a[1],a[2]);
    break;
  
  default:
    printf("unknown syscall with a7: %d",a[7]);
    break;
  }
	#else
  
  for(int i=0;i<8;i++)
    a[i]=hsai_get_arg(trapframe,i);

  switch (a[7])
  {
  case 64:
    sys_write(a[0],(char *)a[1],a[2]);
    break;
  
  default:
    printf("unknown syscall with a7: %d",a[7]);
    break;
  }
    
  #endif
}