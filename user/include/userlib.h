//提供用户程序使用的函数，如printf
//
//

#ifndef _USERLIB_H_
#define _USERLIB_H_
#include "def.h"
#include "usercall.h"

#define WEXITSTATUS(s) (((s) & 0xff00) >> 8)

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
}


int sleep(unsigned long long time)
{
    timeval_t  tv = {.sec = time, .usec = 0};
    if (sys_nanosleep(&tv, &tv))
        return tv.sec;
    return 0;
}

int wait(int *code)
{
    return waitpid((int)-1, code, 0);
}

int open(const char *path, int flags) 
{
    return sys_openat(AT_FDCWD, path, flags, O_RDWR);
}

int openat(int dirfd, const char *path, int flags)
{
    return sys_openat(dirfd, path, flags, 0600);
}

#endif