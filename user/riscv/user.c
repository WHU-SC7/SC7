#include "usercall.h"

int			init_main( void ) __attribute__( ( section( ".user.init" ) ) );

#include "print.h"

int init_main()
{
    asm volatile ("sret");
    printf("用户态printf");
    char *str="user program write";//先不管str被放到user.init段还是rodata段
    write(0,str,19);
    return 0;
}