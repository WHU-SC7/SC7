// 提供用户程序使用的函数，如printf
//
//

#ifndef _USERLIB_H_
#define _USERLIB_H_
#include "def.h"
#include "string.h"
#include "print.h"
#include "usercall.h"

#define WEXITSTATUS(s) (((s) & 0xff00) >> 8)

// 共享内存相关常量
#define SHM_SIZE 4096
#define TEST_DATA "Hello from shared memory!"

// 共享内存权限标志
#define SHM_RDONLY 010000
#define SHM_RND 020000
#define SHM_REMAP 040000
#define SHM_EXEC 0100000

// IPC相关常量
#define IPC_PRIVATE 0
#define IPC_CREAT 01000
#define IPC_EXCL 02000
#define IPC_NOWAIT 04000
#define IPC_RMID 0
#define IPC_SET 1
#define IPC_STAT 2

// 权限位
#define S_IRUSR 0000400
#define S_IWUSR 0000200
#define S_IXUSR 0000100
#define S_IRGRP 0000040
#define S_IWGRP 0000020
#define S_IXGRP 0000010
#define S_IROTH 0000004
#define S_IWOTH 0000002
#define S_IXOTH 0000001

extern int get_time(void)
{
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
    timeval_t tv = {.sec = time, .usec = 0};
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

int mkdir(const char *path, uint16 mode)
{
    return sys_mkdirat(AT_FDCWD, path, mode);
}

int kill(int pid, int sig)
{
    return sys_kill(pid, sig);
}

int init_main(void) __attribute__((section(".text.user.init")));
static char *basic_name[] = {
    "brk",
    "chdir",
    "close",
    "dup",
    "dup2",
    "execve",
    "exit",
    "fork",
    "fstat",
    "getcwd",
    "getdents",
    "getpid",
    "mmap",
    "getppid",
    "gettimeofday",
    "mount",
    "umount",
    "munmap",
    "openat",
    "open",
    "pipe",
    "read",
    "sleep",
    "test_echo",
    "times",
    "clone",
    "uname",
    "wait",
    "waitpid",
    "write",
    "yield",
    "mkdir_",
    "unlink",
};
static longtest busybox[];
static char *busybox_cmd[];
static longtest lua[];
static longtest iozone[];
static longtest lmbench[];
static longtest libctest[];
static longtest libctest_dy[];

void test_basic();
void test_busybox();
void test_fs_img();
void test_libc();
void test_libc_dy();
void test_libcbench();
void test_sh();
void test_iozone();
void test_lua();
void test_lmbench();
void test_libc_all();
void run_all();
void exe(char *path);
int test_signal();
int test_shm();
int test_pselect6_signal(void);

static longtest busybox_setup_dynamic_library[] = {
    {1, {"busybox", "cp", "/glibc/lib/libc.so.6", "/usr/lib/libc.so.6", 0}},
    {1, {"busybox", "cp", "/glibc/lib/libm.so.6", "/usr/lib/libm.so.6", 0}},
    // {1, {"busybox", "cp", "/glibc/ltp/testcases/bin/open12_child", "/bin/open12_child", 0}},
    // {0, {"busybox", "cp", "/glibc/lib/ld-linux-riscv64-lp64d.so.1", "/usr/lib/ld-linux-riscv64-lp64d.so.1", 0}},
    {0, {0}},
};

// loongarch glibc未必需要这个
void setup_dynamic_library()
{
    int i, pid, status;

    for (i = 0; busybox_setup_dynamic_library[i].name[1]; i++)
    {
        if (!busybox_setup_dynamic_library[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve("/musl/busybox", busybox_setup_dynamic_library[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }
}

#endif