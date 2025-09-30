#include "core.h"

//还没想好test.c怎么做

//主测试函数
void tlibc_test()
{
    char *str = "Hello, tlibc_test!\n";
    __write(1,str,20);

    /*printf测试*/
    // print_int(2314);
    __printf("hello!print, number is: %d,next: %d,%d,%d,%d,%d,%d,%d,%d\n",11111,222,3333333,444,5,6,7,114514,1919810);
    __printf("测试!,%d,%d,%d,%d,%d\n",12414,535,3257,73744,1453);
    // __printf("\7\0",1,2,3,4,5,6,7,8,9,10);
    while(1);
}