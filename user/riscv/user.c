#include "usercall.h"
#include "userlib.h"
#include "fcntl.h"
#include "string.h"
#include "print.h"
#include "sh.h"


int test_msync(){
    printf("Testing msync system call...\n");
    
    // 分配一个内存映射区域
    void *addr = sys_mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) {
        printf("mmap failed\n");
        return -1;
    }
    
    printf("Mapped memory at %p\n", addr);
    
    // 写入一些数据
    char *data = (char *)addr;
    data[0] = 'H';
    data[1] = 'e';
    data[2] = 'l';
    data[3] = 'l';
    data[4] = 'o';
    data[5] = '\0';
    
    printf("Written data: %s\n", data);
    
    // 测试msync
    int ret = sys_msync(addr, 4096, MS_SYNC);
    if (ret == 0) {
        printf("msync(MS_SYNC) succeeded\n");
    } else {
        printf("msync(MS_SYNC) failed\n");
    }
    
    // 测试异步msync
    ret = sys_msync(addr, 4096, MS_ASYNC);
    if (ret == 0) {
        printf("msync(MS_ASYNC) succeeded\n");
    } else {
        printf("msync(MS_ASYNC) failed\n");
    }
    
    // 测试带MS_INVALIDATE的msync
    ret = sys_msync(addr, 4096, MS_SYNC | MS_INVALIDATE);
    if (ret == 0) {
        printf("msync(MS_SYNC | MS_INVALIDATE) succeeded\n");
    } else {
        printf("msync(MS_SYNC | MS_INVALIDATE) failed\n");
    }
    
    // 测试错误情况：未对齐的地址
    ret = sys_msync((void *)((uint64)addr + 1), 4096, MS_SYNC);
    if (ret == -1) {
        printf("msync with unaligned address correctly failed\n");
    } else {
        printf("msync with unaligned address unexpectedly succeeded\n");
    }
    
    // 清理
    sys_munmap(addr, 4096);
    printf("msync test completed\n");

    return 0;
}
int init_main()
{
    if (openat(AT_FDCWD, "/dev/tty", O_RDWR) < 0)
    {
        sys_mknod("/dev/tty", CONSOLE, 0);
        openat(AT_FDCWD, "/dev/tty", O_RDWR);
    }
    sys_dup(0); // stdout
    sys_dup(0); // stderr
    // setup_dynamic_library();

    // 读取字符测试 - 注释掉，避免阻塞
    //  test_uartread();
    //  启动shell而不是运行测试

    // wait(0);
    // run_all();
    sys_chdir("/glibc/ltp/testcases/bin");
    const char* prefix = "/glibc/ltp/testcases/bin/shmt09";
    // const char* prefix = "ls /proc";
    // const char* prefix = NULL;
    run_shell(prefix);
    // test_msync();

    // 如果shell退出，则运行测试
    // test_shm();
    // test_libc_dy();
    // test_libc();
    // test_lua();
    // test_basic();
    // test_busybox();
    // test_fs_img();
    // test_lmbench();
    // test_libcbench();
    // test_sh(); // glibc/ltp/testcases/bin/abort01
    shutdown();
    while (1)
        ;
    return 0;
}

void run_all()
{
    test_basic();
    test_busybox();
    test_lua();
    test_sh();
    // test_libc_all();
    test_libcbench();
    test_iozone();
}

void test_sh()
{
    int pid;
    pid = fork();
    // sys_chdir("/glibc");
    sys_chdir("/musl");
    if (pid < 0)
    {
        printf("init: fork failed\n");
        exit(1);
    }
    if (pid == 0)
    {
        char *newargv[] = {"sh", "-c", "./libctest_testcode.sh", NULL};
        // char *newargv[] = {"sh", "-c", "./lmbench_testcode.sh", NULL};
        // char *newargv[] = {"sh", "-c", "./ltp_testcode.sh", NULL};
        // char *newargv[] = {"sh", "-c","./busybox_testcode.sh", NULL};
        // char *newargv[] = {"sh", "./basic_testcode.sh", NULL};
        // char *newargv[] = {"sh", "-c","./iozone_testcode.sh", NULL};
        // char *newargv[] = {"sh", "./libcbench_testcode.sh", NULL};
        char *newenviron[] = {NULL};
        sys_execve("busybox", newargv, newenviron);
        printf("execve error.\n");
        exit(1);
    }
    wait(0);
}

/*******************************************************************************
 *                              IOZONE TEST SUITE                              *
 *******************************************************************************/
void test_iozone()
{
    // setup_dynamic_library();
    int pid, status;
    // sys_chdir("/glibc");
    sys_chdir("/musl");
    printf("run iozone_testcode.sh\n");
    char *newenviron[] = {NULL};
    // printf("iozone automatic measurements\n");
    // pid = fork();
    // if (pid == 0)
    // {
    //     sys_execve("iozone", iozone[0].name, newenviron);
    //     exit(0);
    // }
    // waitpid(pid, &status, 0);

    printf("iozone throughput write/read measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[1].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);

    printf("iozone throughput random-read measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[2].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);

    printf("iozone throughput read-backwards measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[3].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);

    printf("iozone throughput stride-read measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[4].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);

    printf("iozone throughput fwrite/fread measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[5].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);

    printf("iozone throughput pwrite/pread measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[6].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);

    printf("iozone throughput pwritev/preadv measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[7].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);
}

static longtest iozone[] = {
    {1, {"iozone", "-a", "-r", "1k", "-s", "4m", 0}},
    {1, {"iozone", "-t", "4", "-i", "0", "-i", "1", "-r", "1k", "-s", "1m", 0}},
    {1, {"iozone", "-t", "4", "-i", "0", "-i", "2", "-r", "1k", "-s", "1m", 0}},
    {1, {"iozone", "-t", "4", "-i", "0", "-i", "3", "-r", "1k", "-s", "1m", 0}},
    {1, {"iozone", "-t", "4", "-i", "0", "-i", "5", "-r", "1k", "-s", "1m", 0}},
    {1, {"iozone", "-t", "4", "-i", "6", "-i", "7", "-r", "1k", "-s", "1m", 0}},
    {1,
     {"iozone", "-t", "4", "-i", "9", "-i", "10", "-r", "1k", "-s", "1m", 0}},
    {1,
     {"iozone", "-t", "4", "-i", "11", "-i", "12", "-r", "1k", "-s", "1m", 0}},
    {0, {0, 0}} // 数组结束标志，必须保留
};
/*******************************************************************************
 *                              IOZONE TEST SUITE END                          *
 *******************************************************************************/

/*******************************************************************************
 *                              LM BENCH TEST SUITE                            *
 *******************************************************************************/
void test_lmbench()
{
    int pid, status, i;
    // sys_chdir("/musl");
    sys_chdir("/glibc");

    printf("run lmbench_testcode.sh\n");
    printf("latency measurements\n");

    for (i = 0; lmbench[i].name[1]; i++)
    {
        if (!lmbench[i].valid)
            continue;
        pid = fork();
        char *newenviron[] = {NULL};
        if (pid == 0)
        {
            sys_execve(lmbench[i].name[0], lmbench[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }
}

static longtest lmbench[] = {
    {1, {"lmbench_all", "lat_syscall", "-P", "1", "null", 0}},
    {1, {"lmbench_all", "lat_syscall", "-P", "1", "read", 0}},
    {1, {"lmbench_all", "lat_syscall", "-P", "1", "write", 0}},
    {1, {"busybox", "mkdir", "-p", "/var/tmp", 0}},
    {1, {"busybox", "touch", "/var/tmp/lmbench", 0}},
    {1,
     {"lmbench_all", "lat_syscall", "-P", "1", "stat", "/var/tmp/lmbench", 0}},
    {1,
     {"lmbench_all", "lat_syscall", "-P", "1", "fstat", "/var/tmp/lmbench", 0}},
    {1,
     {"lmbench_all", "lat_syscall", "-P", "1", "open", "/var/tmp/lmbench", 0}},
    {1, {"lmbench_all", "lat_select", "-n", "100", "-P", "1", "file", 0}},
    {1, {"lmbench_all", "lat_sig", "-P", "1", "install", 0}},
    {1, {"lmbench_all", "lat_sig", "-P", "1", "catch", 0}},
    {1, {"lmbench_all", "lat_pipe", "-P", "1", 0}},
    {1, {"lmbench_all", "lat_proc", "-P", "1", "fork", 0}},
    {1, {"lmbench_all", "lat_proc", "-P", "1", "exec", 0}},
    {1, {"busybox", "cp", "hello", "/tmp", 0}},
    {1, {"lmbench_all", "lat_proc", "-P", "1", "shell", 0}},
    {1,
     {"lmbench_all", "lmdd", "label=File /var/tmp/XXX write bandwidth:",
      "of=/var/tmp/XXX", "move=1m", "fsync=1", "print=3", 0}},
    {1, {"lmbench_all", "lat_pagefault", "-P", "1", "/var/tmp/XXX", 0}},
    {1, {"lmbench_all", "lat_mmap", "-P", "1", "512k", "/var/tmp/XXX", 0}},
    {1, {"busybox", "echo", "file", "system", "latency", 0}},
    {1, {"lmbench_all", "lat_fs", "/var/tmp", 0}},
    {1, {"busybox", "echo", "Bandwidth", "measurements", 0}},
    {1, {"lmbench_all", "bw_pipe", "-P", "1", 0}},
    {1,
     {"lmbench_all", "bw_file_rd", "-P", "1", "512k", "io_only", "/var/tmp/XXX",
      0}},
    {1,
     {"lmbench_all", "bw_file_rd", "-P", "1", "512k", "open2close",
      "/var/tmp/XXX", 0}},
    {1,
     {"lmbench_all", "bw_mmap_rd", "-P", "1", "512k", "mmap_only",
      "/var/tmp/XXX", 0}},
    {1,
     {"lmbench_all", "bw_mmap_rd", "-P", "1", "512k", "open2close",
      "/var/tmp/XXX", 0}},
    {1, {"busybox", "echo", "context", "switch", "overhead", 0}},
    {1,
     {"lmbench_all", "lat_ctx", "-P", "1", "-s", "32", "2", "4", "8", "16",
      "24", "32", "64", "96", 0}},
    {0, {0, 0}},
};
/*******************************************************************************
 *                              LM BENCH TEST SUITE END                        *
 *******************************************************************************/

/*******************************************************************************
 *                              LIBC BENCH TEST SUITE                          *
 *******************************************************************************/
void test_libcbench()
{
    int pid;
    printf("#### OS COMP TEST GROUP START libcbench-glibc ####\n");
    pid = fork();
    sys_chdir("/musl");
    // sys_chdir("/glibc");
    if (pid < 0)
    {
        printf("init: fork failed\n");
        exit(1);
    }
    if (pid == 0)
    {
        // char *newargv[] = {"sh", "-c", "./run-static.sh", NULL};
        char *newargv[] = {NULL};
        // char *newargv[] = {"sh", "-c","./libctest_testcode.sh", NULL};
        char *newenviron[] = {NULL};
        sys_execve("./libc-bench", newargv, newenviron);
        printf("execve error.\n");
        exit(1);
    }
    wait(0);
    printf("#### OS COMP TEST GROUP END libcbench-glibc ####\n");

    printf("#### OS COMP TEST GROUP START libcbench-musl ####\n");
    pid = fork();
    // sys_chdir("/musl");
    sys_chdir("/musl");
    if (pid < 0)
    {
        printf("init: fork failed\n");
        exit(1);
    }
    if (pid == 0)
    {
        // char *newargv[] = {"sh", "-c", "./run-static.sh", NULL};
        char *newargv[] = {NULL};
        // char *newargv[] = {"sh", "-c","./libctest_testcode.sh", NULL};
        char *newenviron[] = {NULL};
        sys_execve("./libc-bench", newargv, newenviron);
        printf("execve error.\n");
        exit(1);
    }
    wait(0);
    printf("#### OS COMP TEST GROUP END libcbench-musl ####\n");
}
/*******************************************************************************
 *                              LIBC BENCH TEST SUITE END                      *
 *******************************************************************************/

/*******************************************************************************
 *                              LIBC TEST SUITE                                *
 *******************************************************************************/
void test_libc_all()
{
    int i, pid, status;
    sys_chdir("/musl");
    printf("#### OS COMP TEST GROUP START libctest-musl ####\n");
    // for (i = 0; libctest[i].name[1]; i++)
    // {
    //     if (!libctest[i].valid)
    //         continue;
    //     pid = fork();
    //     if (pid == 0)
    //     {
    //         char *newenviron[] = {NULL};
    //         sys_execve("./runtest.exe", libctest[i].name, newenviron);
    //         exit(0);
    //     }
    //     waitpid(pid, &status, 0);
    // }
    for (i = 0; libctest_dy[i].name[1]; i++)
    {
        if (!libctest_dy[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve("./runtest.exe", libctest_dy[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }
    printf("#### OS COMP TEST GROUP END libctest-musl ####\n");
}
void test_libc()
{
    printf("#### OS COMP TEST GROUP START libctest-glibc ####\n");
    int i, pid, status;
    // sys_chdir("/glibc");
    sys_chdir("/musl");
    for (i = 0; libctest[i].name[1]; i++)
    {
        if (!libctest[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve("./runtest.exe", libctest[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }
}

void test_libc_dy()
{
    int i, pid, status;
    sys_chdir("/musl");
    // sys_chdir("/glibc");
    for (i = 0; libctest_dy[i].name[1]; i++)
    {
        if (!libctest_dy[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve("./runtest.exe", libctest_dy[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }
}

static longtest libctest[] = {
    {1, {"./runtest.exe", "-w", "entry-static.exe", "argv", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "basename", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "clocale_mbfuncs", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "clock_gettime", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "crypt", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "dirname", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "env", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fdopen", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fnmatch", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fscanf", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fwscanf", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "iconv_open", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "inet_pton", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "mbc", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "memstream", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cancel_points", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cancel", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cond", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_tsd", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "qsort", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "random", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "search_hsearch", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "search_insque", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "search_lsearch", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "search_tsearch", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "setjmp", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "snprintf", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "socket", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "sscanf", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "sscanf_long", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "stat", 0}}, ///< 打开tmp文件失1是合理的，因为已经删除了
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strftime", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_memcpy", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_memmem", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_memset", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_strchr", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_strcspn", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_strstr", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strptime", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strtod", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strtod_simple", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strtof", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strtol", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strtold", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "swprintf", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "tgmath", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "time", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "tls_align", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "udiv", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "ungetc", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "utime", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "wcsstr", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "wcstol", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pleval", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "daemon_failure", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "dn_expand_empty", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "dn_expand_ptr_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fflush_exit", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fgets_eof", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fgetwc_buffering", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fpclassify_invalid_ld80", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "ftello_unflushed_append", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "getpwnam_r_crash", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "getpwnam_r_errno", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "iconv_roundtrips", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "inet_ntop_v4mapped", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "inet_pton_empty_last_field", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "iswspace_null", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "lrand48_signextend", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "lseek_large", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "malloc_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "mbsrtowcs_overflow", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "memmem_oob_read", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "memmem_oob", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "mkdtemp_failure", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "mkstemp_failure", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "printf_1e9_oob", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "printf_fmt_g_round", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "printf_fmt_g_zeros", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "printf_fmt_n", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_robust_detach", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cancel_sem_wait", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cond_smasher", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_condattr_setclock", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cond_smasher", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_condattr_setclock", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_exit_cancel", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_once_deadlock", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_rwlock_ebusy", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "putenv_doublefree", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regex_backref_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regex_bracket_icase", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regex_ere_backref", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regex_escaped_high_byte", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regex_negated_range", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regexec_nosub", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "rewind_clear_error", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "rlimit_open_files", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "scanf_bytes_consumed", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "scanf_match_literal_eof", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "scanf_nullbyte_char", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "setvbuf_unget", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "sigprocmask_internal", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "sscanf_eof", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "statvfs", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strverscmp", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "syscall_sign_extend", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "uselocale_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "wcsncpy_read_overflow", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "wcsstr_false_negative", 0}},
    {0, {0, 0}}, // 数组结束标志，必须保留
};

static longtest libctest_dy[] = {
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "argv", 0}}, //< 从argv开始到sem_init之间，没有写注释的都是glibc dynamic可以通过的。
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "basename", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "clocale_mbfuncs", 0}}, //< 这个glibc dynamic有问题，输出很多assert failed
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "clock_gettime", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "crypt", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "dirname", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "dlopen", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "env", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fdopen", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fnmatch", 0}}, //< 不行，有assert failed
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fscanf", 0}},  //< 不行，有assert failed
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fwscanf", 0}}, //< 不行，有assert failed
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "iconv_open", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "inet_pton", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "mbc", 0}}, //< 不行. src/functional/mbc.c:44: cannot set UTF-8 locale for test (codeset=ANSI_X3.4-1968)
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "memstream", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_cancel_points", 0}}, //< pthread应该本来就跑不了
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_cancel", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_cond", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_tsd", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "qsort", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "random", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "search_hsearch", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "search_insque", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "search_lsearch", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "search_tsearch", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sem_init", 0}}, //< 只测试到这里，这个不行，报错panic:[syscall.c:1728] Futex type not support!
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sem_init", 0}}, //< 只测试到这里，这个不行，报错panic:[syscall.c:1728] Futex type not support!
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "setjmp", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "snprintf", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "socket", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "socket", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sscanf", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sscanf_long", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "stat", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strftime", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_memcpy", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_memmem", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_memset", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_strchr", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_strcspn", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_strstr", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strptime", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strtod", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strtod_simple", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strtof", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strtol", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strtold", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "swprintf", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "tgmath", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "time", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "tls_init", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "tls_local_exec", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "udiv", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "ungetc", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "utime", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "wcsstr", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "wcstol", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "daemon_failure", 0}}, ///< @todo remap
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "daemon_failure", 0}}, ///< @todo remap
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "dn_expand_empty", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "dn_expand_ptr_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fflush_exit", 0}}, ///< @todo remap
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fgets_eof", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fgetwc_buffering", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fpclassify_invalid_ld80", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "ftello_unflushed_append", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "getpwnam_r_crash", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "getpwnam_r_errno", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "iconv_roundtrips", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "inet_ntop_v4mapped", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "inet_pton_empty_last_field", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "iswspace_null", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "lrand48_signextend", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "lseek_large", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "malloc_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "mbsrtowcs_overflow", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "memmem_oob_read", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "memmem_oob", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "mkdtemp_failure", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "mkstemp_failure", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "printf_1e9_oob", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "printf_fmt_g_round", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "printf_fmt_g_zeros", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "printf_fmt_n", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_robust_detach", 0}}, //< kerneltrap
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_cond_smasher", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_condattr_setclock", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_cond_smasher", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_condattr_setclock", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_exit_cancel", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_once_deadlock", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_rwlock_ebusy", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "putenv_doublefree", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regex_backref_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regex_bracket_icase", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regex_ere_backref", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regex_escaped_high_byte", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regex_negated_range", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regexec_nosub", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "rewind_clear_error", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "rlimit_open_files", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "scanf_bytes_consumed", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "scanf_match_literal_eof", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "scanf_nullbyte_char", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "setvbuf_unget", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sigprocmask_internal", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sscanf_eof", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "statvfs", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strverscmp", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "tls_get_new_dtv", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "syscall_sign_extend", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "uselocale_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "wcsncpy_read_overflow", 0}},
    {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "wcsstr_false_negative", 0}},
    {0, {0, 0}}, // 数组结束标志，必须保留
};
/*******************************************************************************
 *                              LIBC TEST SUITE END                            *
 *******************************************************************************/

/*******************************************************************************
 *                              LUA TEST SUITE                                 *
 *******************************************************************************/
void test_lua()
{
    printf("#### OS COMP TEST GROUP START lua-glibc ####\n");
    int i, status, pid;
    sys_chdir("/glibc");
    for (i = 0; lua[i].name[1]; i++)
    {
        if (!lua[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve("lua", lua[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
        if (status == 0)
        {
            printf("testcase lua %s success\n", lua[i].name[1]);
        }
        else
        {
            printf("testcase lua %s success\n", lua[i].name[1]);
        }
    }
    printf("#### OS COMP TEST GROUP END lua-glibc ####\n");

    printf("#### OS COMP TEST GROUP START lua-musl ####\n");
    sys_chdir("/musl");
    for (i = 0; lua[i].name[1]; i++)
    {
        if (!lua[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve("lua", lua[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
        if (status == 0)
        {
            printf("testcase lua %s success\n", lua[i].name[1]);
        }
        else
        {
            printf("testcase lua %s success\n", lua[i].name[1]);
        }
    }
    printf("#### OS COMP TEST GROUP END lua-musl ####");
}
static longtest lua[] = {
    {1, {"./lua", "date.lua", 0}},
    {1, {"./lua", "file_io.lua", 0}},
    {1, {"./lua", "max_min.lua", 0}},
    {1, {"./lua", "random.lua", 0}},
    {1, {"./lua", "remove.lua", 0}},
    {1, {"./lua", "round_num.lua", 0}},
    {1, {"./lua", "sin30.lua", 0}},
    {1, {"./lua", "sort.lua", 0}},
    {1, {"./lua", "strings.lua", 0}},
    {0, {0}},

};
/*******************************************************************************
 *                              LUA TEST SUITE END                             *
 *******************************************************************************/

/*******************************************************************************
 *                              BUSYBOX TEST SUITE                             *
 *******************************************************************************/
void test_busybox()
{
    printf("#### OS COMP TEST GROUP START busybox-glibc ####\n");
    int pid, status;
    // sys_chdir("/musl");
    sys_chdir("/glibc");
    // sys_chdir("/sdcard");
    int i;
    for (i = 0; busybox[i].name[1]; i++)
    {
        if (!busybox[i].valid)
            continue;
        pid = fork();
        if (pid < 0)
        {
            printf("init: fork failed\n");
            exit(1);
        }
        if (pid == 0)
        {
            // char *newargv[] = {"busybox","sh", "-c","exec busybox pmap $$", 0};
            char *newenviron[] = {NULL};
            // sys_execve("busybox",newargv, newenviron);
            sys_execve("busybox", busybox[i].name, newenviron);
            printf("execve error.\n");
            exit(1);
        }
        waitpid(pid, &status, 0);
        if (status == 0)
            printf("testcase busybox %s success\n", busybox_cmd[i]);
        else
            printf("testcase busybox %s failed\n", busybox_cmd[i]);
    }
    printf("#### OS COMP TEST GROUP END busybox-glibc ####\n");

    printf("#### OS COMP TEST GROUP START busybox-musl ####\n");
    sys_chdir("/musl");
    // sys_chdir("/glibc");
     sys_chdir("/sdcard");
    for (i = 0; busybox[i].name[1]; i++)
    {
        if (!busybox[i].valid)
            continue;
        pid = fork();
        if (pid < 0)
        {
            printf("init: fork failed\n");
            exit(1);
        }
        if (pid == 0)
        {
            // char *newargv[] = {"busybox","sh", "-c","exec busybox pmap $$", 0};
            char *newenviron[] = {NULL};
            // sys_execve("busybox",newargv, newenviron);
            sys_execve("busybox", busybox[i].name, newenviron);
            printf("execve error.\n");
            exit(1);
        }
        waitpid(pid, &status, 0);
        if (status == 0)
            printf("testcase busybox %s success\n", busybox_cmd[i]);
        else
            printf("testcase busybox %s failed\n", busybox_cmd[i]);
    }
    printf("#### OS COMP TEST GROUP END busybox-musl ####\n");
}

static longtest busybox[] = {
    {1, {"busybox", "echo", "#### independent command test", 0}},
    {1, {"busybox", "ash", "-c", "exit", 0}},
    {1, {"busybox", "sh", "-c", "exit", 0}},
    {1, {"busybox", "basename", "/aaa/bbb", 0}},
    {1, {"busybox", "cal", 0}},
    {1, {"busybox", "clear", 0}},
    {1, {"busybox", "date", 0}},
    {1, {"busybox", "df", 0}},
    {1, {"busybox", "dirname", "/aaa/bbb", 0}},
    {1, {"busybox", "dmesg", 0}},
    {1, {"busybox", "du", "-d", "1", "/proc", 0}}, //< glibc跑这个有点慢,具体来说是输出第七行的6       ./ltp/testscripts之后慢
    {1, {"busybox", "expr", "1", "+", "1", 0}},
    {1, {"busybox", "false", 0}},
    {1, {"busybox", "true", 0}},
    {1, {"busybox", "which", "ls", 0}},
    {1, {"busybox", "uname", 0}},
    {1, {"busybox", "uptime", 0}}, //< [glibc] syscall 62  还要 syscall 103
    {1, {"busybox", "printf", "abc\n", 0}},
    {1, {"busybox", "ps", 0}},
    {1, {"busybox", "pwd", 0}},
    {1, {"busybox", "free", 0}},
    {1, {"busybox", "hwclock", 0}},
    {1, {"busybox", "kill", "10", 0}},
    {1, {"busybox", "ls", 0}},
    {1, {"busybox", "sleep", "1", 0}}, //< [glibc] syscall 115
    {1, {"busybox", "echo", "#### file opration test", 0}},
    {1, {"busybox", "touch", "test.txt", 0}},
    {1, {"busybox", "echo", "hello world", ">", "test.txt", 0}},
    {1, {"busybox", "cat", "test.txt", 0}}, //<完成 [glibc] syscall 71  //< [musl] syscall 71
    {1, {"busybox", "cut", "-c", "3", "test.txt", 0}},
    {1, {"busybox", "od", "test.txt", 0}}, //< 能过[musl] syscall 65
    {1, {"busybox", "head", "test.txt", 0}},
    {1, {"busybox", "tail", "test.txt", 0}},          //< 能过[glibc] syscall 62 //< [musl] syscall 62
    {1, {"busybox", "hexdump", "-C", "test.txt", 0}}, //< 能过[musl] syscall 65
    {1, {"busybox", "md5sum", "test.txt", 0}},
    {1, {"busybox", "echo", "ccccccc", ">>", "test.txt", 0}},
    {1, {"busybox", "echo", "bbbbbbb", ">>", "test.txt", 0}},
    {1, {"busybox", "echo", "aaaaaaa", ">>", "test.txt", 0}},
    {1, {"busybox", "echo", "2222222", ">>", "test.txt", 0}},
    {1, {"busybox", "echo", "1111111", ">>", "test.txt", 0}},
    {1, {"busybox", "echo", "bbbbbbb", ">>", "test.txt", 0}},
    {1, {"busybox", "sort", "test.txt", "|", "./busybox", "uniq", 0}},
    {1, {"busybox", "stat", "test.txt", 0}},
    {1, {"busybox", "strings", "test.txt", 0}},
    {1, {"busybox", "wc", "test.txt", 0}},
    {1, {"busybox", "[", "-f", "test.txt", "]", 0}},
    {1, {"busybox", "more", "test.txt", 0}}, //< 完成 [glibc] syscall 71     //< [musl] syscall 71
    {1, {"busybox", "rm", "test.txt", 0}},
    {1, {"busybox", "mkdir", "test_dir", 0}},
    {1, {"busybox", "mv", "test_dir", "test", 0}}, //<能过 [glibc] syscall 276      //< [musl] syscall 276
    {1, {"busybox", "rmdir", "test", 0}},
    {1, {"busybox", "grep", "hello", "busybox_cmd.txt", 0}},
    {1, {"busybox", "cp", "busybox_cmd.txt", "busybox_cmd.bak", 0}}, //< 应该都完成了[glibc] syscall 71     //< [musl] syscall 71
    {1, {"busybox", "rm", "busybox_cmd.bak", 0}},
    {1, {"busybox", "find", ".", "-maxdepth", "1", "-name", "busybox_cmd.txt", 0}}, //< [glibc] syscall 98     //< [musl] 虽然没有问题，但是找的真久啊，是整个磁盘扫了一遍吗
    {0, {0, 0}},
};

static char *busybox_cmd[] = {
    "echo \"#### independent command test\"",
    "ash -c exit",
    "sh -c exit",
    "basename /aaa/bbb",
    "cal",
    "clear",
    "date",
    "df",
    "dirname /aaa/bbb",
    "dmesg",
    "du",
    "expr 1 + 1",
    "false",
    "true",
    "which ls",
    "uname",
    "uptime",
    "printf",
    "ps",
    "pwd",
    "free",
    "hwclock",
    "kill 10",
    "ls",
    "sleep 1",
    "echo \"#### file opration test\"",
    "touch test.txt",
    "echo \"hello world\" > test.txt",
    "cat test.txt",
    "cut -c 3 test.txt",
    "od test.txt",
    "head test.txt",
    "tail test.txt",
    "hexdump -C test.txt",
    "md5sum test.txt",
    "echo \"ccccccc\" >> test.txt",
    "echo \"bbbbbbb\" >> test.txt",
    "echo \"aaaaaaa\" >> test.txt",
    "echo \"2222222\" >> test.txt",
    "echo \"1111111\" >> test.txt",
    "echo \"bbbbbbb\" >> test.txt",
    "sort test.txt | ./busybox uniq",
    "stat test.txt",
    "strings test.txt",
    "wc test.txt",
    "[ -f test.txt ]",
    "more test.txt",
    "rm test.txt",
    "mkdir test_dir",
    "mv test_dir test",
    "rmdir test",
    "grep hello busybox_cmd.txt",
    "cp busybox_cmd.txt busybox_cmd.bak",
    "rm busybox_cmd.bak",
    "find -name \"busybox_cmd.txt\"",
    NULL // Terminating NULL pointer (common convention for string arrays)
};

/*******************************************************************************
 *                              BUSYBOX TEST SUITE END                         *
 *******************************************************************************/

/*******************************************************************************
 *                              BASIC TEST SUITE                               *
 *******************************************************************************/

void test_basic()
{
    printf("#### OS COMP TEST GROUP START basic-glibc ####\n");
    int basic_testcases = sizeof(basic_name) / sizeof(basic_name[0]);
    int pid;
    sys_chdir("/glibc/basic");
    // char *newenviron[] = {NULL};

    // // 分组执行测例 (每组最多4个)
    // for (int i = 0; i < basic_testcases; i += 2)
    // {
    //     int group_size = (basic_testcases - i) < 2 ? (basic_testcases - i) : 2;
    //     int pids[4]; // 当前组的子进程PID

    //     // 创建当前组的子进程
    //     for (int j = 0; j < group_size; j++)
    //     {
    //         pid = fork();
    //         if (pid < 0)
    //         { /* 错误处理 */
    //         }
    //         if (pid == 0)
    //         {
    //             char *argc[] = {basic_name[i + j], NULL};
    //             sys_execve(basic_name[i + j], argc, newenviron);
    //             exit(1); // exe失败时退出
    //         }
    //         pids[j] = pid; // 记录当前组子进程PID
    //     }

    //     // 等待当前组所有子进程结束
    //     for (int j = 0; j < group_size; j++)
    //     {
    //         waitpid(pids[j], NULL, 0);
    //     }
    // }
    // printf("#### OS COMP TEST GROUP END basic-glibc ####\n");

    printf("#### OS COMP TEST GROUP START basic-musl ####\n");
    sys_chdir("/musl/basic");
    for (int i = 0; i < basic_testcases; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf("init: fork failed\n");
            exit(1);
        }
        if (pid == 0)
        {
            exe(basic_name[i]);
            exit(1);
        }
        wait(0);
    }
    printf("#### OS COMP TEST GROUP END basic-musl ####\n");
}

/*******************************************************************************
 *                              BASIC TEST SUITE END                           *
 *******************************************************************************/

/*******************************************************************************
 *                              OTHER TEST SUITE                               *
 *******************************************************************************/
// SIGCHLD信号处理函数
void sigchld_handler(int sig)
{
    printf("收到SIGCHLD信号，子进程已退出\n");
    // 等待子进程，避免僵尸进程
    int status;
    wait(&status);
    printf("子进程退出状态: %d\n", WEXITSTATUS(status));
}

int test_signal()
{
    printf("测试SIGCHLD信号处理\n");

    // 设置SIGCHLD信号处理函数
    struct sigaction sa;
    sa.__sigaction_handler.sa_handler = sigchld_handler;
    sa.sa_flags = 0;

    if (sys_sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        printf("设置SIGCHLD信号处理失败\n");
        return 1;
    }

    printf("SIGCHLD信号处理函数已设置\n");

    // 创建子进程
    int pid = fork();
    if (pid == 0)
    {
        // 子进程
        printf("子进程开始运行，PID: %d\n", getpid());
        sleep(2); // 子进程睡眠2秒
        printf("子进程即将退出\n");
        exit(42); // 子进程退出，状态码为42
    }
    else if (pid > 0)
    {
        // 父进程
        printf("父进程等待子进程退出，子进程PID: %d\n", pid);

        // 父进程继续运行，等待SIGCHLD信号
        while (1)
        {
            sleep(1);
            printf("父进程继续运行...\n");
        }
    }
    else
    {
        printf("fork失败\n");
        return 1;
    }

    return 0;
}

// 定义 IPC_PRIVATE (内核中为0)

// 共享内存大小
#define SHM_SIZE 4096
#define IPC_CREAT 0x200 // flag，如果不存在则创建共享内存段。
struct test_results {
    int passed;
    int failed;
    char message[256];
};
#define TEST_DATA "Hello from parent process!"

int test_shm()
{
    int shmid;
    char *shm_ptr;
    pid_t pid;
    struct test_results *results;

    printf("=== Shared Memory Sync Test ===\n");

    // 1. 创建共享内存
    shmid = sys_shmget(0, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) {
        printf("shmget failed\n");
        exit(1);
    }
    printf("shmget success: shmid = %d\n", shmid);

    // 2. 附加共享内存
    shm_ptr = (char *)sys_shmat(shmid, 0, 0);
    if (shm_ptr == (void *)-1) {
        printf("shmat failed\n");
        exit(1);
    }
    printf("shmat success: attached at %p\n", shm_ptr);

    // 3. 初始化测试结果结构
    results = (struct test_results *)shm_ptr;
    results->passed = 0;
    results->failed = 0;
    strcpy(results->message, TEST_DATA);
    
    printf("Initial data: passed=%d, failed=%d, message='%s'\n", 
           results->passed, results->failed, results->message);

    // 4. 创建子进程
    pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        // 子进程
        printf("\n[Child Process] Reading shared memory...\n");
        printf("Child sees: passed=%d, failed=%d, message='%s'\n", 
               results->passed, results->failed, results->message);

        // 子进程更新共享内存
        results->passed = 1;
        strcpy(results->message, "Modified by child process!");
        
        
        printf("Child updated: passed=%d, failed=%d, message='%s'\n", 
               results->passed, results->failed, results->message);
        
        exit(0);
    } else {
        // 父进程
        printf("\n[Parent Process] Waiting for child...\n");
        wait(NULL);
        
        printf("\n[Parent Process] After child modification:\n");
        printf("Parent sees: passed=%d, failed=%d, message='%s'\n", 
               results->passed, results->failed, results->message);
        
        // 验证结果
        if (results->passed == 1 && strcmp(results->message, "Modified by child process!") == 0) {
            printf("✓ Test PASSED: Shared memory synchronization works correctly!\n");
        } else {
            printf("✗ Test FAILED: Shared memory synchronization failed!\n");
            printf("Expected: passed=1, message='Modified by child process!'\n");
            printf("Actual: passed=%d, message='%s'\n", results->passed, results->message);
        }
    }

    printf("\n=== Test completed ===\n");
    return 0;
}

void exe(char *path)
{
    // printf("开始执行测例\n");
    int pid = fork();
    if (pid < 0)
    {
        printf("fork failed\n");
    }
    else if (pid == 0)
    {
        // 子进程
        char *newargv[] = {path, "/dev/sda2", "./mnt", NULL};
        char *newenviron[] = {NULL};
        sys_execve(path, newargv, newenviron);
        printf("execve error.\n");
        exit(1);
    }
    else
    {
        int status;
        wait(&status);
        // printf("测例执行成功\n");
    }
}

// SIGUSR1信号处理函数
void sigusr1_handler(int sig)
{
    printf("收到SIGUSR1信号，信号编号: %d\n", sig);
}

// 测试pselect6_time32信号处理功能
int test_pselect6_signal()
{
    printf("测试pselect6_time32信号处理功能\n");

    // 设置SIGUSR1信号处理函数
    struct sigaction sa;
    sa.__sigaction_handler.sa_handler = sigusr1_handler; // 使用专门的SIGUSR1处理函数
    sa.sa_flags = 0;

    if (sys_sigaction(SIGUSR1, &sa, NULL) == -1)
    {
        printf("设置SIGUSR1信号处理失败\n");
        return 1;
    }

    printf("SIGUSR1信号处理函数已设置\n");

    // 创建子进程来发送信号
    int pid = fork();
    if (pid == 0)
    {
        // 子进程
        printf("子进程开始运行，PID: %d\n", getpid());
        sleep(2); // 子进程睡眠2秒
        printf("子进程发送SIGUSR1信号给父进程\n");
        kill(getppid(), SIGUSR1); // 发送信号给父进程
        exit(0);
    }
    else if (pid > 0)
    {
        // 父进程
        printf("父进程开始pselect6_time32等待，子进程PID: %d\n", pid);

        // 设置信号掩码，不阻塞SIGUSR1（允许信号中断）
        __sigset_t sigmask;
        sigmask.__val[0] = 0; // 清空掩码，允许所有信号

        // 调用pselect6_time32，应该被信号中断
        int ret = sys_pselect6_time32(0, 0, 0, 0, 0, (uint64)&sigmask);
        printf("pselect6_time32返回: %d (期望: -4, EINTR)\n", ret);

        if (ret == -4)
        {
            printf("✓ pselect6_time32信号处理测试通过\n");
        }
        else
        {
            printf("✗ pselect6_time32信号处理测试失败\n");
        }

        // 等待子进程
        wait(0);
    }
    else
    {
        printf("fork失败\n");
        return 1;
    }

    return 0;
}

void test_fs_img()
{
    int pid;
    pid = fork();
    // sys_chdir("/glibc");
    // sys_chdir("/sdcard");
    if (pid < 0)
    {
        printf("init: fork failed\n");
        exit(1);
    }
    if (pid == 0)
    {
        char *newargv[] = {"sh", "-c", "exec busybox pmap $$", NULL};
        char *newenviron[] = {NULL};
        sys_execve("busybox_unstripped_musl", newargv, newenviron);
        printf("execve error.\n");
        exit(1);
    }
    wait(0);
}
/*******************************************************************************
 *                              OTHER TEST SUITE END                           *
 *******************************************************************************/
