#include "def.h"
#include "syscall.h"
#include "syscall_num.h"

//高级库函数
void fopen();
void fwrite();
void fread();
void fclose();

//系统调用包装
// void write(int fd, const void *buf, int len);
// open
// close

//test
// void Tinylibc_write(int fd, const void *buf, int len);
void Tinylibc_write(int fd, const void *buf, int len)
{
    syscall(SYS_write,fd,buf,len);
}