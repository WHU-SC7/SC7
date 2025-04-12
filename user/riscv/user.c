#include "usercall.h"

int			init_main( void ) __attribute__( ( section( ".text.user.init" ) ) );

#include "print.h"

int init_main()
{
    //asm volatile ("sret"); //非法指令
    //现在用户程序可以直接调用printf,因为用户程序和内核一起链接，能找到printf的地址
    //printf("用户态printf"); 
    char *str="user program write\n";//先不管str被放到user.init段还是rodata段
    write(0,str,19);
    char *str1="第二次调用write,来自user\n";
    write(0,str1,33);
    while(1);
    return 0;
}