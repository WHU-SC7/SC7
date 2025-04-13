#include "usercall.h"

int			init_main( void ) __attribute__( ( section( ".text.user.init" ) ) );

#include "print.h"
void test_write();
void test_fork();
int init_main()
{
    //[[maybe_unused]]int id = getpid();
    test_fork();
    test_write();
    while(1);
    return 0;
}

void test_fork(){
    char * str;
    int pid = fork();
    if (pid < 0) {
        // fork失败
        str="fork failed\n";
        write(0,str,12);
    } else if (pid == 0) {
        // 子进程
        str="child process\n";
        write(0,str,14);
        exit(1);
    } else {
        // 父进程
        str="parent process is waiting\n";
        write(0,str,26);
        wait(pid); // 父进程等待子进程结束
        str="child process is over\n";
        write(0,str,22);
    }

}

void test_write()
{
    char *str="user program write\n";
    write(0,str,19);
    char *str1="第二次调用write,来自user\n";
    write(0,str1,33);
}