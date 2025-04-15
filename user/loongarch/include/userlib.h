//提供用户程序使用的函数，如printf
#ifndef _USERLIB_H_
#define _USERLIB_H_
#include "def.h"
#include "usercall.h"


extern int get_time(void){
    timeval_t time;
    int err = sys_get_time(&time, 0);
    if (err == 0)
    {
        return ((time.sec & 0xffff) * 1000 + time.usec / 1000);
    }
    else
    {
        return -1;
    }
};
#endif