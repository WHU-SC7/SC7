#include "usercall.h"
#include "userlib.h"
#include "fcntl.h"

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
void test_times();
void test_uname();
void test_waitpid();
void test_execve();
void test_wait(void);
int init_main()
{
    if(openat(AT_FDCWD, "console", O_RDWR) < 0)
    {
        sys_mknod("console", CONSOLE, 0);
        openat(AT_FDCWD, "console", O_RDWR);
    }
    sys_dup(0);  // stdout
    sys_dup(0);  // stderr

    //[[maybe_unused]]int id = getpid();
    // test_fork();
    test_execve();
    //test_wait();
    // test_gettime();
    //  test_brk();
    //  test_times();
    //  test_waitpid();
    //  test_uname();
    //  test_write();
    while (1)
        ;
    return 0;
}

void test_execve()
{
    int pid = fork();
    if (pid < 0)
    {
        print("fork failed\n");
    }
    else if (pid == 0)
    {
        // 子进程
        char *newargv[] = {"/glibc/basic/dup2", NULL};
        char *newenviron[] = {NULL};
        sys_execve("/glibc/basic/dup2", newargv, newenviron);
        print("execve error.\n");
        exit(1);
    }
    else
    {
        int status;
        wait(&status);
        print("child process is over\n");
    }
}

void test_write(){
	const char *str = "Hello operating system contest.\n";
	int str_len = strlen(str);
	int reallylen = write(1, str, str_len);
    if (reallylen != str_len)
    {
        print("write error.\n");
    }
    else
    {
        print("write success.\n");
    }
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
        pid_t ppid = getppid();
        if (ppid > 0)
            print("getppid success. ppid");
        else
            print("  getppid error.\n");
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

int i = 1000;
void test_waitpid(void)
{
    int cpid, wstatus;
    cpid = fork();
    if (cpid != -1)
    {
        print("fork test Success!\n");
    };
    if (cpid == 0)
    {
        while (i--)
            ;
        sys_sched_yield();
        print("This is child process\n");
        exit(3);
    }
    else
    {
        pid_t ret = waitpid(cpid, &wstatus, 0);
        if (ret != -1)
        {
            print("waitpid test Success!\n");
        }
        else
            print("waitpid error.\n");
    }
}
void test_wait(void){
    int cpid, wstatus;
    cpid = fork();
    if(cpid == 0){
	print("This is child process\n");
        exit(0);
    }else{
	pid_t ret = wait(&wstatus);
	if(ret == cpid)
	    print("wait child success.\n");
	else
	    print("wait child error.\n");
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

// void test_write()
// {
//     char *str = "user program write\n";
//     write(0, str, 20);
//     char *str1 = "第二次调用write,来自user\n";
//     write(0, str1, 33);
// }

void test_brk()
{
    int64 cur_pos, alloc_pos, alloc_pos_1;

    cur_pos = sys_brk(0);
    sys_brk((void *)(cur_pos + 2 * 4006));

    alloc_pos = sys_brk(0);
    sys_brk((void *)(alloc_pos + 2 * 4006));

    alloc_pos_1 = sys_brk(0);
    alloc_pos_1++;
}
struct tms mytimes;
void test_times()
{

    for (int i = 0; i < 1000000; i++)
    {
    }
    uint64 test_ret = sys_times(&mytimes);
    mytimes.tms_cstime++;
    if (test_ret == 0)
    {
        print("test_times Success!");
    }
    else
    {
        print("test_times Failed!");
    }
}

struct utsname un;
void test_uname()
{
    int test_ret = sys_uname(&un);

    if (test_ret >= 0)
    {
        print("test_uname Success!");
    }
    else
    {
        print("test_uname Failed!");
    }
}