#include "usercall.h"
#include "userlib.h"

int init_main(void) __attribute__((section(".text.user.init")));
int strlen(const char *s)
{
    int n;

    for (n = 0; s[n]; n++)
        ;
    return n;
}
void print(const char *s) { write(1, s, strlen(s)); }
void test_write();
void test_fork();
void test_gettime();
void test_brk();    
int init_main()
{
    //[[maybe_unused]]int id = getpid();
    // test_fork();
    //test_gettime();
    test_brk();
    // test_write();
    while (1)
        ;
    return 0;
}

void test_fork()
{
    int pid = fork();
    if (pid < 0)
    {
        // fork失败
        print("fork failed\n");
    }
    else if (pid == 0)
    {
        // 子进程
        print("child process\n");
        exit(1);
    }
    else
    {
        // 父进程
        print("parent process is waiting\n");
        int status;
        wait(&status);
        print("child process is over\n");
    }
}

void test_gettime()
{
    int test_ret1 = get_time();
    // volatile int i = 100000; // qemu时钟频率12500000
    // while (i > 0)
    //     i--;
    sleep(2);
    int test_ret2 = get_time();
    if (test_ret1 >= 0 && test_ret2 >= 0)
    {
        print("get_time test success\n");
    }
}

void test_write()
{
    char *str = "user program write\n";
    write(0, str, 20);
    char *str1 = "第二次调用write,来自user\n";
    write(0, str1, 33);
}

void test_brk()
{
    int64 cur_pos, alloc_pos, alloc_pos_1;

    cur_pos = sys_brk(0);
    sys_brk((void*)(cur_pos + 2*4006));

    alloc_pos = sys_brk(0);
    sys_brk((void*)(alloc_pos + 2*4006));

    alloc_pos_1 = sys_brk(0);
    alloc_pos_1 ++;
}