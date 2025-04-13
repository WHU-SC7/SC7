#include "usercall.h"

int			init_main( void ) __attribute__( ( section( ".text.user.init" ) ) );


int init_main()
{
    //while(1) ;

    char *str="user program write\n";//先不管str被放到user.init段还是rodata段
    write(0,str,19);
    char *str1="第二次调用write,来自user\n";
    write(0,str1,33);
    write(0,str1,33);
    write(0,str1,33);
    while(1) ;
    return 0;
}