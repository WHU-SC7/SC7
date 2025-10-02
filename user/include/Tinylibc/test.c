#include "core.h"
#include "tlibc.h"

//还没想好test.c怎么做

//主测试函数
void tlibc_test()
{
    char *str = "Hello, tlibc_test!\n";
    __write(1,str,20);

    //read测试，从终端读取
    char buf[64];
    for(int i=0;i<64;i++)
    {
        buf[i]=0;
    }
    __read(0,buf,1); //读取一个字符，然后输出
    __printf("输出: %s\n",buf);

    //文件openat和read测试
    //如何创建文件？
        //必须设置读写位
        //必须有O_CREAT，可选O_TRUNC
        //文件权限先默认为0644
    unsigned long open_ret = __openat(AT_FDCWD,"/readfile",O_RDWR,0644);
    __printf("openat返回值: %d\n",open_ret);
    for(int i=0;i<64;i++)
    {
        buf[i]=0;
    }
    __read(open_ret,buf,10);
    __printf("openfile文件内容: %s\n",buf);

    //creat测试
    open_ret = __creat("/createfile",0644);
    __printf("openat返回值: %d\n",open_ret);
    char *write_string = "6123";
    __write(open_ret,write_string,4);

    /*printf测试*/
    // print_int(2314);
    __printf("hello!print, number is: %d,next: %d,%d,%d,%d,%d,%d,%d,%d\n",11111,222,3333333,444,5,6,7,114514,1919810);
    __printf("测试!,%d,%d,%d,%d,%d\n",12414,535,3257,73744,1453);
    // __printf("\7\0",1,2,3,4,5,6,7,8,9,10);

    //shutdown
    shutdow();
    while(1);
}