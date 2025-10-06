#include "core.h"
#include "tlibc_print.h"

/**
 * @brief shell,现在只能接收输入
 */
void shell()
{
    LOG("欢迎使用Tlibc Shell!\n");
    LOG("这是第一个版本, 只能输入输出, 按q退出shell\n");
    LOG("不要输入方向键好吗，这个版本不支持\n");

    while(1)
    {
        // 单字符输入，简单但是不标准。每个字符读取都要陷入内核，开销大
        // char input_c;
        // __read(0,&input_c,1);
        // if(input_c == 'q')
        //     break;
        // __printf("接收到输入，字符的码值: %d\n",input_c);

        // 一次读取完整的输入，以enter输入结尾
        // 疑问，内核返回的缓冲区是否应该以enter的码值结尾。 现在SC7不会
        char buf[256];
        for(int i=0; i<256; i++)
            buf[i] = 0;
        int read_count = __read(0,buf,256);
        if(buf[0]=='q' && buf[1]==0)
            break;
        __printf("输入: %s, read返回值: %d\n",buf,read_count);
    }
}