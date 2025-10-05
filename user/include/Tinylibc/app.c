#include "core.h"
#include "tlibc_print.h"

/**
 * @brief shell,现在只能接收输入
 */
void shell()
{
    __printf("欢迎使用Tlibc Shell!\n");
    __printf("这是第一个版本, 只能输入输出, 按q退出shell\n");
    // char buf[64];
    char input_c;
    while(1)
    {
        __read(0,&input_c,1);
        if(input_c == 'q')
            break;
        __printf("接收到输入，字符的码值: %d\n",input_c);
    }
}