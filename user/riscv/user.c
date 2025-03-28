#include "usercall.h"

int			init_main( void ) __attribute__( ( section( ".user.init" ) ) );

#include "print.h"

int init_main()
{
    //asm volatile ("sret"); //非法指令
    //printf("用户态printf"); 现在用户程序可以直接调用printf,因为用户程序和内核一起链接，能找到printf的地址
    char *str="user program write\n";//先不管str被放到user.init段还是rodata段
    write(0,str,19);
    return 0;
}