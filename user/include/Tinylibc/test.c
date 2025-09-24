#include "core.h"

//还没想好test.c怎么做

//主测试函数
void tlibc_test()
{
    char *str = "Hello, tlibc_test!\n";
    Tinylibc_write(1,str,18);
    while(1);
}