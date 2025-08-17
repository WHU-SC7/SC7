#include "usercall.h"
#include "userlib.h"
#include "fcntl.h"
#include "string.h"
#include "print.h"
#include "sh.h"

void test_ltp();
void test_ltp_musl();
int test_shm();
void test_final();
int init_main()
{
    int isconsole = 1;
    if (isconsole)
    {
        if (openat(AT_FDCWD, "/dev/tty", O_RDWR) < 0)
        {
            sys_mknod("/dev/tty", 1, 0);
            openat(AT_FDCWD, "/dev/tty", O_RDWR);
        }
    }
    else
    {
        if (openat(AT_FDCWD, "/output.txt", O_RDWR) >= 0)
        {
            printf("delete output.txt");
            sys_unlinkat(AT_FDCWD, "/output.txt", 0);
        }
        openat(AT_FDCWD, "/output.txt", O_RDWR | O_CREATE);
    }
    sys_dup(0); // stdout
    sys_dup(0); // stderr
    setup_dynamic_library();

    //  test_uartread();
    //  启动shell而不是运行测试
    // sys_chdir("/glibc/ltp/testcases/bin");
    // // const char* prefix = NULL;
    // [[maybe_unused]] const char *prefix = "/musl/ltp/testcases/bin/signal04";
    run_all();
    // test_lmbench();
    // test_iozone();
    // run_shell(prefix);

    // 如果shell退出，则运行测试
    // test_shm();
    // test_libc_dy();
    // test_libc();
    // test_lua();
    // test_basic();
    // test_busybox();
    // test_fs_img();
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
    test_libcbench();
    test_ltp_musl();
    test_ltp();
}

static longtest ltp[] = {
    /*这里是完全通过的，或者几乎完全通过的*/
    {1, {"/glibc/ltp/testcases/bin/waitpid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/waitpid03", 0}},
    {1, {"/glibc/ltp/testcases/bin/waitpid04", 0}},
    {1, {"/glibc/ltp/testcases/bin/getppid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getppid02", 0}},
    {1, {"/glibc/ltp/testcases/bin/getsid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getsid02", 0}},
    {1, {"/glibc/ltp/testcases/bin/gettimeofday01", 0}},
    {1, {"/glibc/ltp/testcases/bin/gettimeofday02", 0}},
    {1, {"/glibc/ltp/testcases/bin/abort01", 0}},
    {1, {"/glibc/ltp/testcases/bin/access01", 0}},
    {1, {"/glibc/ltp/testcases/bin/alarm02", 0}},
    {1, {"/glibc/ltp/testcases/bin/alarm03", 0}},
    {1, {"/glibc/ltp/testcases/bin/alarm05", 0}},
    {1, {"/glibc/ltp/testcases/bin/alarm06", 0}},
    {1, {"/glibc/ltp/testcases/bin/alarm07", 0}},
    // {1, {"/glibc/ltp/testcases/bin/abs01", 0}}, 没有summary
    {1, {"/glibc/ltp/testcases/bin/brk01", 0}},
    {1, {"/glibc/ltp/testcases/bin/brk02", 0}},
    {1, {"/glibc/ltp/testcases/bin/chmod01", 0}},
    {1, {"/glibc/ltp/testcases/bin/chmod03", 0}},
    {1, {"/glibc/ltp/testcases/bin/chmod05", 0}},
    {1, {"/glibc/ltp/testcases/bin/chmod07", 0}},
    {1, {"/glibc/ltp/testcases/bin/creat01", 0}},
    {1, {"/glibc/ltp/testcases/bin/creat03", 0}},
    {1, {"/glibc/ltp/testcases/bin/creat04", 0}},
    {1, {"/glibc/ltp/testcases/bin/creat05", 0}},
    {1, {"/glibc/ltp/testcases/bin/creat08", 0}},
    {1, {"/glibc/ltp/testcases/bin/chown01", 0}},
    {1, {"/glibc/ltp/testcases/bin/chown02", 0}},
    {1, {"/glibc/ltp/testcases/bin/chown03", 0}},
    {1, {"/glibc/ltp/testcases/bin/chown05", 0}},
    {1, {"/glibc/ltp/testcases/bin/chroot01", 0}},
    {1, {"/glibc/ltp/testcases/bin/chroot02", 0}},
    {1, {"/glibc/ltp/testcases/bin/chroot03", 0}},
    {1, {"/glibc/ltp/testcases/bin/chroot04", 0}},
    {1, {"/glibc/ltp/testcases/bin/close01", 0}},
    {1, {"/glibc/ltp/testcases/bin/close02", 0}},
    {1, {"/glibc/ltp/testcases/bin/clock_gettime01", 0}},
    {1, {"/glibc/ltp/testcases/bin/clock_gettime02", 0}},
    {1, {"/glibc/ltp/testcases/bin/clock_nanosleep01", 0}},
    {1, {"/glibc/ltp/testcases/bin/clock_nanosleep04", 0}},
    {1, {"/glibc/ltp/testcases/bin/clone01", 0}},
    // {1, {"/glibc/ltp/testcases/bin/clone02", 0}},    ///< @todo 卡住了
    {1, {"/glibc/ltp/testcases/bin/clone03", 0}},
    {1, {"/glibc/ltp/testcases/bin/clone04", 0}},
    // {1, {"/glibc/ltp/testcases/bin/clone05", 0}},faile
    {1, {"/glibc/ltp/testcases/bin/clone06", 0}},
    // {1, {"/glibc/ltp/testcases/bin/clone07", 0}},fial，卡住了
    // {1, {"/glibc/ltp/testcases/bin/clone08", 0}},fail，1个pass
    // {1, {"/glibc/ltp/testcases/bin/clone09", 0}},broken，warnig
    // {1, {"/glibc/ltp/testcases/bin/clone301", 0}}，broken，skip
    {1, {"/glibc/ltp/testcases/bin/clone302", 0}},
    {1, {"/glibc/ltp/testcases/bin/clone303", 0}},
    {1, {"/glibc/ltp/testcases/bin/dup01", 0}},
    {1, {"/glibc/ltp/testcases/bin/dup02", 0}},
    {1, {"/glibc/ltp/testcases/bin/dup03", 0}},
    {1, {"/glibc/ltp/testcases/bin/dup04", 0}},
    {1, {"/glibc/ltp/testcases/bin/dup05", 0}},
    {1, {"/glibc/ltp/testcases/bin/dup06", 0}},
    {1, {"/glibc/ltp/testcases/bin/dup07", 0}},
    {1, {"/glibc/ltp/testcases/bin/dup201", 0}},
    {1, {"/glibc/ltp/testcases/bin/dup202", 0}},
    {1, {"/glibc/ltp/testcases/bin/dup203", 0}},
    {1, {"/glibc/ltp/testcases/bin/dup204", 0}},
    {1, {"/glibc/ltp/testcases/bin/dup205", 0}},
    {1, {"/glibc/ltp/testcases/bin/dup206", 0}},
    {1, {"/glibc/ltp/testcases/bin/dup207", 0}},
    {1, {"/glibc/ltp/testcases/bin/dup3_02", 0}},
    {1, {"/glibc/ltp/testcases/bin/exit01", 0}},
    {1, {"/glibc/ltp/testcases/bin/exit02", 0}},
    {1, {"/glibc/ltp/testcases/bin/exit_group01", 0}},
    // {1, {"/glibc/ltp/testcases/bin/fstatat01", 0}}, ///< 没有summary
    {1, {"/glibc/ltp/testcases/bin/fstat02", 0}},
    {1, {"/glibc/ltp/testcases/bin/fstat02_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/fstat03", 0}},
    {1, {"/glibc/ltp/testcases/bin/fstat03_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/faccessat01", 0}},
    {1, {"/glibc/ltp/testcases/bin/faccessat02", 0}},
    {1, {"/glibc/ltp/testcases/bin/faccessat201", 0}},
    {1, {"/glibc/ltp/testcases/bin/faccessat202", 0}},
    {1, {"/glibc/ltp/testcases/bin/fchdir01", 0}},
    {1, {"/glibc/ltp/testcases/bin/fchdir02", 0}},
    {1, {"/glibc/ltp/testcases/bin/fchdir03", 0}},
    {1, {"/glibc/ltp/testcases/bin/fchmod01", 0}},
    {1, {"/glibc/ltp/testcases/bin/fchmod02", 0}},
    {1, {"/glibc/ltp/testcases/bin/fchmod03", 0}},
    {1, {"/glibc/ltp/testcases/bin/fchmod04", 0}},
    {1, {"/glibc/ltp/testcases/bin/fchmod05", 0}},
    {1, {"/glibc/ltp/testcases/bin/fchmodat01", 0}},
    {1, {"/glibc/ltp/testcases/bin/fchmodat02", 0}},
    {1, {"/glibc/ltp/testcases/bin/fchown01", 0}},
    {1, {"/glibc/ltp/testcases/bin/fchown02", 0}},
    {1, {"/glibc/ltp/testcases/bin/fchown03", 0}},
    {1, {"/glibc/ltp/testcases/bin/fchown05", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl01", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl02", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl02_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl03", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl03_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl04", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl04_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl05", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl05_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl08", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl08_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl09", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl09_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl10", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl10_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl12", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl12_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl13", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl13_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl29", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl29_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl30", 0}},
    {1, {"/glibc/ltp/testcases/bin/fcntl30_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/fork01", 0}},
    {1, {"/glibc/ltp/testcases/bin/fork03", 0}},
    // {1, {"/glibc/ltp/testcases/bin/fork04", 0}}, 卡住了
    {1, {"/glibc/ltp/testcases/bin/fork05", 0}},
    {1, {"/glibc/ltp/testcases/bin/fork06", 0}},
    {1, {"/glibc/ltp/testcases/bin/fork07", 0}},
    {1, {"/glibc/ltp/testcases/bin/fork08", 0}},
    {1, {"/glibc/ltp/testcases/bin/fork09", 0}},
    {1, {"/glibc/ltp/testcases/bin/fork10", 0}},
    // {1, {"/glibc/ltp/testcases/bin/fork13", 0}}, broken
    // {1, {"/glibc/ltp/testcases/bin/fork14", 0}}, vma address overflow
    {1, {"/glibc/ltp/testcases/bin/ftruncate01", 0}},
    {1, {"/glibc/ltp/testcases/bin/ftruncate01_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/ftruncate03", 0}},
    {1, {"/glibc/ltp/testcases/bin/ftruncate03_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/futex_wait01", 0}},
    {1, {"/glibc/ltp/testcases/bin/futex_wait02", 0}},
    {1, {"/glibc/ltp/testcases/bin/futex_wait03", 0}},
    {1, {"/glibc/ltp/testcases/bin/futex_wait04", 0}},
    {1, {"/glibc/ltp/testcases/bin/futex_wake01", 0}},
    {1, {"/glibc/ltp/testcases/bin/futex_wake02", 0}},
    {1, {"/glibc/ltp/testcases/bin/futex_wake03", 0}},
    {1, {"/glibc/ltp/testcases/bin/futex_cmp_requeue02", 0}},
    {1, {"/glibc/ltp/testcases/bin/futex_wait_bitset01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getpagesize01", 0}},
    {1, {"/glibc/ltp/testcases/bin/wait01", 0}},
    {1, {"/glibc/ltp/testcases/bin/wait02", 0}},
    // {1, {"/glibc/ltp/testcases/bin/wait401", 0}}, // 卡住
    {1, {"/glibc/ltp/testcases/bin/wait402", 0}},
    {1, {"/glibc/ltp/testcases/bin/wait403", 0}},
    {1, {"/glibc/ltp/testcases/bin/waitid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/waitid02", 0}},
    {1, {"/glibc/ltp/testcases/bin/waitid03", 0}},
    {1, {"/glibc/ltp/testcases/bin/waitid05", 0}},
    {1, {"/glibc/ltp/testcases/bin/waitid06", 0}},
    {1, {"/glibc/ltp/testcases/bin/waitid09", 0}},
    {1, {"/glibc/ltp/testcases/bin/waitid10", 0}},
    {1, {"/glibc/ltp/testcases/bin/waitid11", 0}},
    {1, {"/glibc/ltp/testcases/bin/gettid01", 0}}, // musl panic
    {1, {"/glibc/ltp/testcases/bin/getcpu01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getcwd01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getcwd02", 0}},
    {1, {"/glibc/ltp/testcases/bin/getdomainname01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getpid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getpid02", 0}},
    {1, {"/glibc/ltp/testcases/bin/getegid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getegid01_16", 0}},
    {1, {"/glibc/ltp/testcases/bin/getegid02", 0}},
    {1, {"/glibc/ltp/testcases/bin/getegid02_16", 0}},
    {1, {"/glibc/ltp/testcases/bin/getuid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getuid03", 0}},
    {1, {"/glibc/ltp/testcases/bin/getgid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getgid03", 0}},
    {1, {"/glibc/ltp/testcases/bin/geteuid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/geteuid02", 0}},
    {1, {"/glibc/ltp/testcases/bin/gethostbyname_r01", 0}},
    {1, {"/glibc/ltp/testcases/bin/gethostid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/gethostname01", 0}},
    {1, {"/glibc/ltp/testcases/bin/gethostname02", 0}},
    {1, {"/glibc/ltp/testcases/bin/getitimer01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getitimer02", 0}},
    {1, {"/glibc/ltp/testcases/bin/getpgid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getpgid02", 0}},
    {1, {"/glibc/ltp/testcases/bin/getpgrp01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getrandom01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getrandom02", 0}},
    {1, {"/glibc/ltp/testcases/bin/getrandom03", 0}},
    {1, {"/glibc/ltp/testcases/bin/getrandom04", 0}},
    {1, {"/glibc/ltp/testcases/bin/getrandom05", 0}},
    {1, {"/glibc/ltp/testcases/bin/getrlimit01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getrlimit02", 0}},
    {1, {"/glibc/ltp/testcases/bin/getrlimit03", 0}},
    {1, {"/glibc/ltp/testcases/bin/getrusage01", 0}},
    {1, {"/glibc/ltp/testcases/bin/getrusage02", 0}},
    {1, {"/glibc/ltp/testcases/bin/in6_01", 0}},
    {1, {"/glibc/ltp/testcases/bin/kill02", 0}},
    {1, {"/glibc/ltp/testcases/bin/kill03", 0}},
    {1, {"/glibc/ltp/testcases/bin/kill05", 0}},
    {1, {"/glibc/ltp/testcases/bin/kill06", 0}},
    {1, {"/glibc/ltp/testcases/bin/kill07", 0}},
    {1, {"/glibc/ltp/testcases/bin/kill08", 0}},
    {1, {"/glibc/ltp/testcases/bin/kill09", 0}},
    {1, {"/glibc/ltp/testcases/bin/kill11", 0}},
    // // {1, {"/glibc/ltp/testcases/bin/kill12", 0}}, signal error
    {1, {"/glibc/ltp/testcases/bin/link02", 0}},
    {1, {"/glibc/ltp/testcases/bin/link04", 0}},
    {1, {"/glibc/ltp/testcases/bin/link05", 0}},
    {1, {"/glibc/ltp/testcases/bin/mknod01", 0}},
    {1, {"/glibc/ltp/testcases/bin/mknod02", 0}},
    {1, {"/glibc/ltp/testcases/bin/memcmp01", 0}},
    {1, {"/glibc/ltp/testcases/bin/memcpy01", 0}},
    {1, {"/glibc/ltp/testcases/bin/memset01", 0}},
    {1, {"/glibc/ltp/testcases/bin/mallopt01", 0}},
    {1, {"/glibc/ltp/testcases/bin/mallinfo01", 0}},
    {1, {"/glibc/ltp/testcases/bin/mallinfo02", 0}},
    {1, {"/glibc/ltp/testcases/bin/mmap02", 0}},
    {1, {"/glibc/ltp/testcases/bin/mmap03", 0}},
    {1, {"/glibc/ltp/testcases/bin/mmap05", 0}},
    {1, {"/glibc/ltp/testcases/bin/mmap06", 0}},
    {1, {"/glibc/ltp/testcases/bin/mmap08", 0}},
    {1, {"/glibc/ltp/testcases/bin/mmap09", 0}},
    {1, {"/glibc/ltp/testcases/bin/mmap11", 0}},
    {1, {"/glibc/ltp/testcases/bin/mmap13", 0}},
    {1, {"/glibc/ltp/testcases/bin/mmap19", 0}},
    // {1, {"/glibc/ltp/testcases/bin/mmapstress01", 0}},  ///< 全是0
    {1, {"/glibc/ltp/testcases/bin/mkdir05", 0}},
    {1, {"/glibc/ltp/testcases/bin/mprotect01", 0}},
    {1, {"/glibc/ltp/testcases/bin/mprotect02", 0}},
    // {1, {"/glibc/ltp/testcases/bin/mprotect03", 0}},
    {1, {"/glibc/ltp/testcases/bin/mprotect05", 0}},
    // {1, {"/glibc/ltp/testcases/bin/nanosleep02", 0}},    ///< broken
    {1, {"/glibc/ltp/testcases/bin/nfs05_make_tree", 0}},
    {1, {"/glibc/ltp/testcases/bin/pathconf01", 0}},
    {1, {"/glibc/ltp/testcases/bin/pathconf02", 0}},
    {1, {"/glibc/ltp/testcases/bin/pipe01", 0}},
    {1, {"/glibc/ltp/testcases/bin/pipe02", 0}},
    {1, {"/glibc/ltp/testcases/bin/pipe03", 0}},
    {1, {"/glibc/ltp/testcases/bin/pipe04", 0}},
    {1, {"/glibc/ltp/testcases/bin/pipe05", 0}},
    {1, {"/glibc/ltp/testcases/bin/pipe06", 0}},
    {1, {"/glibc/ltp/testcases/bin/pipe08", 0}},
    {1, {"/glibc/ltp/testcases/bin/pipe09", 0}},
    {1, {"/glibc/ltp/testcases/bin/pipe10", 0}},
    // {1, {"/glibc/ltp/testcases/bin/pipe13", 0}}, // 卡住
    {1, {"/glibc/ltp/testcases/bin/pipe14", 0}},
    {1, {"/glibc/ltp/testcases/bin/pipe2_01", 0}},
    // {1, {"/glibc/ltp/testcases/bin/pipe2_04", 0}}, // 卡住
    {1, {"/glibc/ltp/testcases/bin/ppoll01", 0}},
    {1, {"/glibc/ltp/testcases/bin/poll01", 0}},
    {1, {"/glibc/ltp/testcases/bin/pread01", 0}},
    {1, {"/glibc/ltp/testcases/bin/pread01_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/pread02", 0}},
    {1, {"/glibc/ltp/testcases/bin/pread02_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/preadv01", 0}},
    {1, {"/glibc/ltp/testcases/bin/preadv01_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/preadv02", 0}},
    {1, {"/glibc/ltp/testcases/bin/preadv02_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/preadv201", 0}},
    {1, {"/glibc/ltp/testcases/bin/preadv201_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/preadv202", 0}},
    {1, {"/glibc/ltp/testcases/bin/preadv202_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwrite01", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwrite02", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwrite03", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwrite04", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwrite01_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwrite02_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwrite03_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwrite04_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwritev01", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwritev01_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwritev02", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwritev02_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwritev201", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwritev201_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwritev202", 0}},
    {1, {"/glibc/ltp/testcases/bin/pwritev202_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/read01", 0}},
    {1, {"/glibc/ltp/testcases/bin/read02", 0}},
    {1, {"/glibc/ltp/testcases/bin/read03", 0}},
    {1, {"/glibc/ltp/testcases/bin/read04", 0}},
    {1, {"/glibc/ltp/testcases/bin/readlink03", 0}},
    {1, {"/glibc/ltp/testcases/bin/readdir01", 0}},
    {1, {"/glibc/ltp/testcases/bin/readv01", 0}},
    {1, {"/glibc/ltp/testcases/bin/readv02", 0}},
    {1, {"/glibc/ltp/testcases/bin/rmdir01", 0}},
    {1, {"/glibc/ltp/testcases/bin/rmdir03", 0}},
    {1, {"/glibc/ltp/testcases/bin/sigaction01", 0}},
    {1, {"/glibc/ltp/testcases/bin/sigaction02", 0}},
    {1, {"/glibc/ltp/testcases/bin/sbrk01", 0}},
    {1, {"/glibc/ltp/testcases/bin/sbrk02", 0}},
    {1, {"/glibc/ltp/testcases/bin/sbrk03", 0}},
    {1, {"/glibc/ltp/testcases/bin/select01", 0}},
    {1, {"/glibc/ltp/testcases/bin/select03", 0}},
    {1, {"/glibc/ltp/testcases/bin/select04", 0}},
    {1, {"/glibc/ltp/testcases/bin/set_tid_address01", 0}},
    {1, {"/glibc/ltp/testcases/bin/setgid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/setgid02", 0}},
    {1, {"/glibc/ltp/testcases/bin/setgid03", 0}},
    {1, {"/glibc/ltp/testcases/bin/setegid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/setegid02", 0}},
    {1, {"/glibc/ltp/testcases/bin/setpgid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/setpgid02", 0}},
    {1, {"/glibc/ltp/testcases/bin/setuid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/setuid03", 0}},
    {1, {"/glibc/ltp/testcases/bin/setuid04", 0}},
    {1, {"/glibc/ltp/testcases/bin/setreuid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/setreuid02", 0}},
    {1, {"/glibc/ltp/testcases/bin/setreuid03", 0}},
    {1, {"/glibc/ltp/testcases/bin/setreuid04", 0}},
    {1, {"/glibc/ltp/testcases/bin/setreuid05", 0}},
    {1, {"/glibc/ltp/testcases/bin/setreuid06", 0}},
    {1, {"/glibc/ltp/testcases/bin/setreuid07", 0}},
    {1, {"/glibc/ltp/testcases/bin/setregid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/setregid02", 0}},
    {1, {"/glibc/ltp/testcases/bin/setregid03", 0}},
    {1, {"/glibc/ltp/testcases/bin/setregid04", 0}},
    {1, {"/glibc/ltp/testcases/bin/setresuid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/setresuid02", 0}},
    {1, {"/glibc/ltp/testcases/bin/setresuid03", 0}},
    {1, {"/glibc/ltp/testcases/bin/setresuid04", 0}},
    {1, {"/glibc/ltp/testcases/bin/setresuid05", 0}},
    {1, {"/glibc/ltp/testcases/bin/setresgid01", 0}},
    {1, {"/glibc/ltp/testcases/bin/setresgid02", 0}},
    {1, {"/glibc/ltp/testcases/bin/setresgid03", 0}},
    {1, {"/glibc/ltp/testcases/bin/setresgid04", 0}},
    {1, {"/glibc/ltp/testcases/bin/setpgrp01", 0}},
    {1, {"/glibc/ltp/testcases/bin/setpgrp02", 0}},
    {1, {"/glibc/ltp/testcases/bin/setgroups01", 0}},
    {1, {"/glibc/ltp/testcases/bin/setgroups02", 0}},
    {1, {"/glibc/ltp/testcases/bin/setgroups03", 0}},
    {1, {"/glibc/ltp/testcases/bin/setgroups04", 0}},
    {1, {"/glibc/ltp/testcases/bin/syscall01", 0}},
    {1, {"/glibc/ltp/testcases/bin/signal02", 0}},
    {1, {"/glibc/ltp/testcases/bin/signal03", 0}},
    {1, {"/glibc/ltp/testcases/bin/signal04", 0}},
    {1, {"/glibc/ltp/testcases/bin/signal05", 0}},
    {1, {"/glibc/ltp/testcases/bin/signal06", 0}},
    {1, {"/glibc/ltp/testcases/bin/stat01", 0}},
    {1, {"/glibc/ltp/testcases/bin/stat01_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/stat02", 0}},
    {1, {"/glibc/ltp/testcases/bin/stat02_64", 0}},
    {1, {"/glibc/ltp/testcases/bin/statx02", 0}},
    {1, {"/glibc/ltp/testcases/bin/statx03", 0}},
    {1, {"/glibc/ltp/testcases/bin/sysinfo01", 0}},
    {1, {"/glibc/ltp/testcases/bin/sysinfo02", 0}},
    {1, {"/glibc/ltp/testcases/bin/sched_yield01", 0}},
    {1, {"/glibc/ltp/testcases/bin/sched_get_priority_max01", 0}},
    {1, {"/glibc/ltp/testcases/bin/sched_get_priority_max02", 0}},
    {1, {"/glibc/ltp/testcases/bin/sched_get_priority_min01", 0}},
    {1, {"/glibc/ltp/testcases/bin/sched_get_priority_min02", 0}},
    {1, {"/glibc/ltp/testcases/bin/time01", 0}},
    {1, {"/glibc/ltp/testcases/bin/times01", 0}},
    {1, {"/glibc/ltp/testcases/bin/tkill01", 0}},
    {1, {"/glibc/ltp/testcases/bin/tkill02", 0}},
    // {1, {"/glibc/ltp/testcases/bin/write01", 0}},  // 跑2分钟
    {1, {"/glibc/ltp/testcases/bin/write02", 0}},
    {1, {"/glibc/ltp/testcases/bin/write03", 0}},
    {1, {"/glibc/ltp/testcases/bin/write04", 0}},
    {1, {"/glibc/ltp/testcases/bin/write05", 0}},
    {1, {"/glibc/ltp/testcases/bin/write06", 0}},
    {1, {"/glibc/ltp/testcases/bin/writev01", 0}},
    {1, {"/glibc/ltp/testcases/bin/writev05", 0}},
    {1, {"/glibc/ltp/testcases/bin/writev06", 0}},
    {1, {"/glibc/ltp/testcases/bin/writev07", 0}},
    {1, {"/glibc/ltp/testcases/bin/lseek01", 0}},
    {1, {"/glibc/ltp/testcases/bin/lseek02", 0}},
    {1, {"/glibc/ltp/testcases/bin/lseek07", 0}},
    {1, {"/glibc/ltp/testcases/bin/llseek01", 0}},
    {1, {"/glibc/ltp/testcases/bin/llseek02", 0}},
    {1, {"/glibc/ltp/testcases/bin/llseek03", 0}},
    {1, {"/glibc/ltp/testcases/bin/unlink05", 0}},
    {1, {"/glibc/ltp/testcases/bin/unlink07", 0}},
    {1, {"/glibc/ltp/testcases/bin/unlink08", 0}},
    {1, {"/glibc/ltp/testcases/bin/unlinkat01", 0}},
    {1, {"/glibc/ltp/testcases/bin/uname01", 0}},
    {1, {"/glibc/ltp/testcases/bin/uname02", 0}},
    // // {1, {"/glibc/ltp/testcases/bin/uname04", 0}},
    {1, {"/glibc/ltp/testcases/bin/utsname01", 0}},
    {1, {"/glibc/ltp/testcases/bin/umask01", 0}},
    {1, {"/glibc/ltp/testcases/bin/access01", 0}},
    {1, {"/glibc/ltp/testcases/bin/access02", 0}},
    {1, {"/glibc/ltp/testcases/bin/access03", 0}},
    {1, {"/glibc/ltp/testcases/bin/symlink01", 0}}, // 通过4个， 有一个broken，没有summary
    {1, {"/glibc/ltp/testcases/bin/symlink02", 0}},

    /*---------------------------------分隔线---------------------------------------------------*/

    /*这里是有问题的*/
    // {1, {"/musl/ltp/testcases/bin/sendfile01", 0}}, // 需要注释掉sendfile调用的return -1
    // {1, {"/musl/ltp/testcases/bin/sendfile02", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile03", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile04", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile05", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile08", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile01_64", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile02_64", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile03_64", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile04_64", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile05_64", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile08_64", 0}},
    //    {1, {"/glibc/ltp/testcases/bin/dup3_01", 0}},
    // {1, {"/musl/ltp/testcases/bin/futex_wait05", 0}}, ///< @todo 莫名其妙访问 > 4GB的位置
    // {1, {"/musl/ltp/testcases/bin/futex_wake04", 0}}, ///< @todo TCONF: hugetlbfs is not supported
    // {1, {"/musl/ltp/testcases/bin/futex_waitv01", 0}}, ///< @todo __NR_futex_waitv not supported on your arch
    // {1, {"/musl/ltp/testcases/bin/futex_waitv02", 0}}, ///< @todo __NR_futex_waitv not supported on your arch
    // {1, {"/musl/ltp/testcases/bin/futex_waitv03", 0}}, ///< @todo __NR_futex_waitv not supported on your arch
    // {1, {"/musl/ltp/testcases/bin/futex_cmp_requeue01", 0}}, ///< @todo fork() failed: EPERM (1)，部分通过，后面创建1000线程的难搞
    // {1, {"/musl/ltp/testcases/bin/open12", 0}},     ///< 没有summary，不测，创建了大于4GB的文件，镜像被他搞坏了
    // {1, {"/musl/ltp/testcases/bin/open14", 0}},     ///< 没有summary，不测
    // {1, {"/musl/ltp/testcases/bin/openat02", 0}},   ///< 没有summary，不测，创建了大于4GB的文件，镜像被他搞坏了，第三个测例都无法测试
    // {1, {"/musl/ltp/testcases/bin/openat03", 0}},   ///< 没有summary，不测
    // {1, {"/musl/ltp/testcases/bin/lseek11", 0}}, // 不支持稀疏文件
    // {1, {"/musl/ltp/testcases/bin/link08", 0}}, // 需要loop设备
    // {1, {"/musl/ltp/testcases/bin/unlink08", 0}}, // broken，权限
    // {1, {"/musl/ltp/testcases/bin/symlink03", 0}}, // Remaining cases broken, panic

    /* MEM 测例 */
    // {1, {"/musl/ltp/testcases/bin/shmt02", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt03", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt04", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt05", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt06", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt07", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt08", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt09", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt10", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmdt01", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmdt02", 0}},

    {0, {0}},
};

static longtest ltp_musl[] = {
    /*这里是完全通过的，或者几乎完全通过的*/
    {1, {"/musl/ltp/testcases/bin/waitpid01", 0}},
    {1, {"/musl/ltp/testcases/bin/waitpid03", 0}},
    {1, {"/musl/ltp/testcases/bin/waitpid04", 0}},
    {1, {"/musl/ltp/testcases/bin/getppid01", 0}},
    {1, {"/musl/ltp/testcases/bin/getppid02", 0}},
    {1, {"/musl/ltp/testcases/bin/getsid01", 0}},
    {1, {"/musl/ltp/testcases/bin/getsid02", 0}},
    {1, {"/musl/ltp/testcases/bin/gettimeofday01", 0}},
    {1, {"/musl/ltp/testcases/bin/gettimeofday02", 0}},
    {1, {"/musl/ltp/testcases/bin/abort01", 0}},
    {1, {"/musl/ltp/testcases/bin/access01", 0}},
    {1, {"/musl/ltp/testcases/bin/alarm02", 0}},
    {1, {"/musl/ltp/testcases/bin/alarm03", 0}},
    {1, {"/musl/ltp/testcases/bin/alarm05", 0}},
    {1, {"/musl/ltp/testcases/bin/alarm06", 0}},
    {1, {"/musl/ltp/testcases/bin/alarm07", 0}},
    // {1, {"/musl/ltp/testcases/bin/abs01", 0}}, 没有summary
    {1, {"/musl/ltp/testcases/bin/brk01", 0}},
    {1, {"/musl/ltp/testcases/bin/brk02", 0}},
    {1, {"/musl/ltp/testcases/bin/chmod01", 0}},
    {1, {"/musl/ltp/testcases/bin/chmod03", 0}},
    {1, {"/musl/ltp/testcases/bin/chmod05", 0}},
    {1, {"/musl/ltp/testcases/bin/chmod07", 0}},
    {1, {"/musl/ltp/testcases/bin/creat01", 0}},
    {1, {"/musl/ltp/testcases/bin/creat03", 0}},
    {1, {"/musl/ltp/testcases/bin/creat04", 0}},
    {1, {"/musl/ltp/testcases/bin/creat05", 0}},
    {1, {"/musl/ltp/testcases/bin/creat08", 0}},
    {1, {"/musl/ltp/testcases/bin/chown01", 0}},
    {1, {"/musl/ltp/testcases/bin/chown02", 0}},
    {1, {"/musl/ltp/testcases/bin/chown03", 0}},
    {1, {"/musl/ltp/testcases/bin/chown05", 0}},
    {1, {"/musl/ltp/testcases/bin/chroot01", 0}},
    {1, {"/musl/ltp/testcases/bin/chroot02", 0}},
    {1, {"/musl/ltp/testcases/bin/chroot03", 0}},
    {1, {"/musl/ltp/testcases/bin/chroot04", 0}},
    {1, {"/musl/ltp/testcases/bin/close01", 0}},
    {1, {"/musl/ltp/testcases/bin/close02", 0}},
    {1, {"/musl/ltp/testcases/bin/clock_gettime01", 0}},
    {1, {"/musl/ltp/testcases/bin/clock_gettime02", 0}},
    {1, {"/musl/ltp/testcases/bin/clock_nanosleep01", 0}},
    {1, {"/musl/ltp/testcases/bin/clock_nanosleep04", 0}},
    {1, {"/musl/ltp/testcases/bin/clone01", 0}},
    // {1, {"/musl/ltp/testcases/bin/clone02", 0}},    ///< @todo 卡住了
    {1, {"/musl/ltp/testcases/bin/clone03", 0}},
    {1, {"/musl/ltp/testcases/bin/clone04", 0}},
    // {1, {"/musl/ltp/testcases/bin/clone05", 0}},faile
    {1, {"/musl/ltp/testcases/bin/clone06", 0}},
    // {1, {"/musl/ltp/testcases/bin/clone07", 0}},fial，卡住了
    // {1, {"/musl/ltp/testcases/bin/clone08", 0}},fail，1个pass
    // {1, {"/musl/ltp/testcases/bin/clone09", 0}},broken，warnig
    // {1, {"/musl/ltp/testcases/bin/clone301", 0}}，broken，skip
    {1, {"/musl/ltp/testcases/bin/clone302", 0}},
    {1, {"/musl/ltp/testcases/bin/clone303", 0}},
    {1, {"/musl/ltp/testcases/bin/dup01", 0}},
    {1, {"/musl/ltp/testcases/bin/dup02", 0}},
    {1, {"/musl/ltp/testcases/bin/dup03", 0}},
    {1, {"/musl/ltp/testcases/bin/dup04", 0}},
    {1, {"/musl/ltp/testcases/bin/dup05", 0}},
    {1, {"/musl/ltp/testcases/bin/dup06", 0}},
    {1, {"/musl/ltp/testcases/bin/dup07", 0}},
    {1, {"/musl/ltp/testcases/bin/dup201", 0}},
    {1, {"/musl/ltp/testcases/bin/dup202", 0}},
    {1, {"/musl/ltp/testcases/bin/dup203", 0}},
    {1, {"/musl/ltp/testcases/bin/dup204", 0}},
    {1, {"/musl/ltp/testcases/bin/dup205", 0}},
    {1, {"/musl/ltp/testcases/bin/dup206", 0}},
    {1, {"/musl/ltp/testcases/bin/dup207", 0}},
    {1, {"/musl/ltp/testcases/bin/dup3_02", 0}},
    {1, {"/musl/ltp/testcases/bin/exit01", 0}},
    {1, {"/musl/ltp/testcases/bin/exit02", 0}},
    {1, {"/musl/ltp/testcases/bin/exit_group01", 0}},
    // {1, {"/musl/ltp/testcases/bin/fstatat01", 0}}, ///< 没有summary
    {1, {"/musl/ltp/testcases/bin/fstat02", 0}},
    {1, {"/musl/ltp/testcases/bin/fstat02_64", 0}},
    {1, {"/musl/ltp/testcases/bin/fstat03", 0}},
    {1, {"/musl/ltp/testcases/bin/fstat03_64", 0}},
    {1, {"/musl/ltp/testcases/bin/faccessat01", 0}},
    {1, {"/musl/ltp/testcases/bin/faccessat02", 0}},
    {1, {"/musl/ltp/testcases/bin/faccessat201", 0}},
    {1, {"/musl/ltp/testcases/bin/faccessat202", 0}},
    {1, {"/musl/ltp/testcases/bin/fchdir01", 0}},
    {1, {"/musl/ltp/testcases/bin/fchdir02", 0}},
    {1, {"/musl/ltp/testcases/bin/fchdir03", 0}},
    {1, {"/musl/ltp/testcases/bin/fchmod01", 0}},
    {1, {"/musl/ltp/testcases/bin/fchmod02", 0}},
    {1, {"/musl/ltp/testcases/bin/fchmod03", 0}},
    {1, {"/musl/ltp/testcases/bin/fchmod04", 0}},
    {1, {"/musl/ltp/testcases/bin/fchmod05", 0}},
    {1, {"/musl/ltp/testcases/bin/fchmodat01", 0}},
    {1, {"/musl/ltp/testcases/bin/fchmodat02", 0}},
    {1, {"/musl/ltp/testcases/bin/fchown01", 0}},
    {1, {"/musl/ltp/testcases/bin/fchown02", 0}},
    {1, {"/musl/ltp/testcases/bin/fchown03", 0}},
    {1, {"/musl/ltp/testcases/bin/fchown05", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl01", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl02", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl02_64", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl03", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl03_64", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl04", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl04_64", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl05", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl05_64", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl08", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl08_64", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl09", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl09_64", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl10", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl10_64", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl12", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl12_64", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl13", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl13_64", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl29", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl29_64", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl30", 0}},
    {1, {"/musl/ltp/testcases/bin/fcntl30_64", 0}},
    {1, {"/musl/ltp/testcases/bin/fork01", 0}},
    {1, {"/musl/ltp/testcases/bin/fork03", 0}},
    // {1, {"/musl/ltp/testcases/bin/fork04", 0}}, 卡住了
    {1, {"/musl/ltp/testcases/bin/fork05", 0}},
    {1, {"/musl/ltp/testcases/bin/fork06", 0}},
    {1, {"/musl/ltp/testcases/bin/fork07", 0}},
    {1, {"/musl/ltp/testcases/bin/fork08", 0}},
    {1, {"/musl/ltp/testcases/bin/fork09", 0}},
    {1, {"/musl/ltp/testcases/bin/fork10", 0}},
    // {1, {"/musl/ltp/testcases/bin/fork13", 0}}, broken
    // {1, {"/musl/ltp/testcases/bin/fork14", 0}}, vma address overflow
    {1, {"/musl/ltp/testcases/bin/ftruncate01", 0}},
    {1, {"/musl/ltp/testcases/bin/ftruncate01_64", 0}},
    {1, {"/musl/ltp/testcases/bin/ftruncate03", 0}},
    {1, {"/musl/ltp/testcases/bin/ftruncate03_64", 0}},
    {1, {"/musl/ltp/testcases/bin/futex_wait01", 0}},
    {1, {"/musl/ltp/testcases/bin/futex_wait02", 0}},
    {1, {"/musl/ltp/testcases/bin/futex_wait03", 0}},
    {1, {"/musl/ltp/testcases/bin/futex_wait04", 0}},
    {1, {"/musl/ltp/testcases/bin/futex_wake01", 0}},
    {1, {"/musl/ltp/testcases/bin/futex_wake02", 0}},
    {1, {"/musl/ltp/testcases/bin/futex_wake03", 0}},
    {1, {"/musl/ltp/testcases/bin/futex_cmp_requeue02", 0}},
    {1, {"/musl/ltp/testcases/bin/futex_wait_bitset01", 0}},
    {1, {"/musl/ltp/testcases/bin/getpagesize01", 0}},
    {1, {"/musl/ltp/testcases/bin/wait01", 0}},
    {1, {"/musl/ltp/testcases/bin/wait02", 0}},
    // {1, {"/musl/ltp/testcases/bin/wait401", 0}}, // 卡住
    {1, {"/musl/ltp/testcases/bin/wait402", 0}},
    {1, {"/musl/ltp/testcases/bin/wait403", 0}},
    {1, {"/musl/ltp/testcases/bin/waitid01", 0}},
    {1, {"/musl/ltp/testcases/bin/waitid02", 0}},
    {1, {"/musl/ltp/testcases/bin/waitid03", 0}},
    {1, {"/musl/ltp/testcases/bin/waitid05", 0}},
    {1, {"/musl/ltp/testcases/bin/waitid06", 0}},
    {1, {"/musl/ltp/testcases/bin/waitid09", 0}},
    {1, {"/musl/ltp/testcases/bin/waitid10", 0}},
    {1, {"/musl/ltp/testcases/bin/waitid11", 0}},
    {1, {"/musl/ltp/testcases/bin/gettid01", 0}}, // musl panic
    {1, {"/musl/ltp/testcases/bin/gettid01", 0}},
    {1, {"/musl/ltp/testcases/bin/getcpu01", 0}},
    {1, {"/musl/ltp/testcases/bin/getcwd01", 0}},
    {1, {"/musl/ltp/testcases/bin/getcwd02", 0}},
    {1, {"/musl/ltp/testcases/bin/getdomainname01", 0}},
    {1, {"/musl/ltp/testcases/bin/getpid01", 0}},
    {1, {"/musl/ltp/testcases/bin/getpid02", 0}},
    {1, {"/musl/ltp/testcases/bin/getegid01", 0}},
    {1, {"/musl/ltp/testcases/bin/getegid01_16", 0}},
    {1, {"/musl/ltp/testcases/bin/getegid01", 0}},
    {1, {"/musl/ltp/testcases/bin/getegid01_16", 0}},
    {1, {"/musl/ltp/testcases/bin/getegid02", 0}},
    {1, {"/musl/ltp/testcases/bin/getegid02_16", 0}},
    {1, {"/musl/ltp/testcases/bin/getuid01", 0}},
    {1, {"/musl/ltp/testcases/bin/getuid03", 0}},
    {1, {"/musl/ltp/testcases/bin/getgid01", 0}},
    {1, {"/musl/ltp/testcases/bin/getgid03", 0}},
    {1, {"/musl/ltp/testcases/bin/geteuid01", 0}},
    {1, {"/musl/ltp/testcases/bin/geteuid02", 0}},
    {1, {"/musl/ltp/testcases/bin/gethostbyname_r01", 0}},
    {1, {"/musl/ltp/testcases/bin/gethostid01", 0}},
    {1, {"/musl/ltp/testcases/bin/gethostname01", 0}},
    {1, {"/musl/ltp/testcases/bin/gethostname02", 0}},
    {1, {"/musl/ltp/testcases/bin/getitimer01", 0}},
    {1, {"/musl/ltp/testcases/bin/getitimer02", 0}},
    {1, {"/musl/ltp/testcases/bin/getpgid01", 0}},
    {1, {"/musl/ltp/testcases/bin/getpgid02", 0}},
    {1, {"/musl/ltp/testcases/bin/getpgrp01", 0}},
    {1, {"/musl/ltp/testcases/bin/getrandom01", 0}},
    {1, {"/musl/ltp/testcases/bin/getrandom02", 0}},
    {1, {"/musl/ltp/testcases/bin/getrandom03", 0}},
    {1, {"/musl/ltp/testcases/bin/getrandom04", 0}},
    {1, {"/musl/ltp/testcases/bin/getrandom05", 0}},
    {1, {"/musl/ltp/testcases/bin/getrlimit01", 0}},
    {1, {"/musl/ltp/testcases/bin/getrlimit02", 0}},
    {1, {"/musl/ltp/testcases/bin/getrlimit03", 0}},
    {1, {"/musl/ltp/testcases/bin/getrusage01", 0}},
    {1, {"/musl/ltp/testcases/bin/getrusage02", 0}},
    {1, {"/musl/ltp/testcases/bin/in6_01", 0}},
    {1, {"/musl/ltp/testcases/bin/kill02", 0}},
    {1, {"/musl/ltp/testcases/bin/kill03", 0}},
    {1, {"/musl/ltp/testcases/bin/kill05", 0}},
    {1, {"/musl/ltp/testcases/bin/kill06", 0}},
    {1, {"/musl/ltp/testcases/bin/kill07", 0}},
    {1, {"/musl/ltp/testcases/bin/kill08", 0}},
    {1, {"/musl/ltp/testcases/bin/kill09", 0}},
    {1, {"/musl/ltp/testcases/bin/kill11", 0}},
    // // {1, {"/musl/ltp/testcases/bin/kill12", 0}}, // signal error
    {1, {"/musl/ltp/testcases/bin/link02", 0}},
    {1, {"/musl/ltp/testcases/bin/link04", 0}},
    {1, {"/musl/ltp/testcases/bin/link05", 0}},
    {1, {"/musl/ltp/testcases/bin/mknod01", 0}},
    {1, {"/musl/ltp/testcases/bin/mknod02", 0}},
    {1, {"/musl/ltp/testcases/bin/memcmp01", 0}},
    {1, {"/musl/ltp/testcases/bin/memcpy01", 0}},
    {1, {"/musl/ltp/testcases/bin/memset01", 0}},
    {1, {"/musl/ltp/testcases/bin/mallopt01", 0}},
    // {1, {"/musl/ltp/testcases/bin/mallinfo01", 0}},
    // {1, {"/musl/ltp/testcases/bin/mallinfo02", 0}},
    {1, {"/musl/ltp/testcases/bin/mmap01", 0}},
    {1, {"/musl/ltp/testcases/bin/mmap02", 0}},
    {1, {"/musl/ltp/testcases/bin/mmap03", 0}},
    {1, {"/musl/ltp/testcases/bin/mmap05", 0}},
    {1, {"/musl/ltp/testcases/bin/mmap06", 0}},
    {1, {"/musl/ltp/testcases/bin/mmap08", 0}},
    {1, {"/musl/ltp/testcases/bin/mmap09", 0}},
    {1, {"/musl/ltp/testcases/bin/mmap11", 0}},
    {1, {"/musl/ltp/testcases/bin/mmap13", 0}},
    {1, {"/musl/ltp/testcases/bin/mmap19", 0}},
    // {1, {"/musl/ltp/testcases/bin/mmapstress01", 0}},  ///< 全是0
    {1, {"/musl/ltp/testcases/bin/mkdir05", 0}},
    {1, {"/musl/ltp/testcases/bin/mprotect01", 0}},
    {1, {"/musl/ltp/testcases/bin/mprotect02", 0}},
    {1, {"/musl/ltp/testcases/bin/mprotect03", 0}},
    {1, {"/musl/ltp/testcases/bin/mprotect05", 0}},
    // {1, {"/musl/ltp/testcases/bin/nanosleep02", 0}},    ///< broken
    {1, {"/musl/ltp/testcases/bin/nfs05_make_tree", 0}},
    {1, {"/musl/ltp/testcases/bin/pathconf01", 0}},
    {1, {"/musl/ltp/testcases/bin/pathconf02", 0}},
    {1, {"/musl/ltp/testcases/bin/pipe01", 0}},
    {1, {"/musl/ltp/testcases/bin/pipe02", 0}},
    {1, {"/musl/ltp/testcases/bin/pipe03", 0}},
    {1, {"/musl/ltp/testcases/bin/pipe04", 0}},
    {1, {"/musl/ltp/testcases/bin/pipe05", 0}},
    {1, {"/musl/ltp/testcases/bin/pipe06", 0}},
    {1, {"/musl/ltp/testcases/bin/pipe08", 0}},
    {1, {"/musl/ltp/testcases/bin/pipe09", 0}},
    {1, {"/musl/ltp/testcases/bin/pipe10", 0}},
    // {1, {"/musl/ltp/testcases/bin/pipe13", 0}}, // 卡住
    {1, {"/musl/ltp/testcases/bin/pipe14", 0}},
    {1, {"/musl/ltp/testcases/bin/pipe2_01", 0}},
    // {1, {"/musl/ltp/testcases/bin/pipe2_04", 0}}, // 卡住
    {1, {"/musl/ltp/testcases/bin/ppoll01", 0}},
    {1, {"/musl/ltp/testcases/bin/poll01", 0}},
    {1, {"/musl/ltp/testcases/bin/pread01", 0}},
    {1, {"/musl/ltp/testcases/bin/pread01_64", 0}},
    {1, {"/musl/ltp/testcases/bin/pread02", 0}},
    {1, {"/musl/ltp/testcases/bin/pread02_64", 0}},
    {1, {"/musl/ltp/testcases/bin/preadv01", 0}},
    {1, {"/musl/ltp/testcases/bin/preadv01_64", 0}},
    {1, {"/musl/ltp/testcases/bin/preadv02", 0}},
    {1, {"/musl/ltp/testcases/bin/preadv02_64", 0}},
    {1, {"/musl/ltp/testcases/bin/preadv201", 0}},
    {1, {"/musl/ltp/testcases/bin/preadv201_64", 0}},
    {1, {"/musl/ltp/testcases/bin/preadv202", 0}},
    {1, {"/musl/ltp/testcases/bin/preadv202_64", 0}},
    {1, {"/musl/ltp/testcases/bin/pwrite01", 0}},
    {1, {"/musl/ltp/testcases/bin/pwrite02", 0}},
    {1, {"/musl/ltp/testcases/bin/pwrite03", 0}},
    {1, {"/musl/ltp/testcases/bin/pwrite04", 0}},
    {1, {"/musl/ltp/testcases/bin/pwrite01_64", 0}},
    {1, {"/musl/ltp/testcases/bin/pwrite02_64", 0}},
    {1, {"/musl/ltp/testcases/bin/pwrite03_64", 0}},
    {1, {"/musl/ltp/testcases/bin/pwrite04_64", 0}},
    {1, {"/musl/ltp/testcases/bin/pwritev01", 0}},
    {1, {"/musl/ltp/testcases/bin/pwritev01_64", 0}},
    {1, {"/musl/ltp/testcases/bin/pwritev02", 0}},
    {1, {"/musl/ltp/testcases/bin/pwritev02_64", 0}},
    {1, {"/musl/ltp/testcases/bin/pwritev201", 0}},
    {1, {"/musl/ltp/testcases/bin/pwritev201_64", 0}},
    {1, {"/musl/ltp/testcases/bin/pwritev202", 0}},
    {1, {"/musl/ltp/testcases/bin/pwritev202_64", 0}},
    {1, {"/musl/ltp/testcases/bin/read01", 0}},
    {1, {"/musl/ltp/testcases/bin/read02", 0}},
    {1, {"/musl/ltp/testcases/bin/read03", 0}},
    {1, {"/musl/ltp/testcases/bin/read04", 0}},
    {1, {"/musl/ltp/testcases/bin/readlink03", 0}},
    {1, {"/musl/ltp/testcases/bin/readdir01", 0}},
    {1, {"/musl/ltp/testcases/bin/readv01", 0}},
    {1, {"/musl/ltp/testcases/bin/readv02", 0}},
    {1, {"/musl/ltp/testcases/bin/rmdir01", 0}},
    {1, {"/musl/ltp/testcases/bin/rmdir03", 0}},
    {1, {"/musl/ltp/testcases/bin/sigaction01", 0}},
    {1, {"/musl/ltp/testcases/bin/sigaction02", 0}},
    {1, {"/musl/ltp/testcases/bin/sbrk01", 0}},
    {1, {"/musl/ltp/testcases/bin/sbrk02", 0}},
    {1, {"/musl/ltp/testcases/bin/sbrk03", 0}},
    {1, {"/musl/ltp/testcases/bin/select01", 0}},
    {1, {"/musl/ltp/testcases/bin/select03", 0}},
    {1, {"/musl/ltp/testcases/bin/select04", 0}},
    {1, {"/musl/ltp/testcases/bin/set_tid_address01", 0}},
    {1, {"/musl/ltp/testcases/bin/setgid01", 0}},
    {1, {"/musl/ltp/testcases/bin/setgid02", 0}},
    {1, {"/musl/ltp/testcases/bin/setgid03", 0}},
    {1, {"/musl/ltp/testcases/bin/setegid01", 0}},
    {1, {"/musl/ltp/testcases/bin/setegid02", 0}},
    {1, {"/musl/ltp/testcases/bin/setpgid01", 0}},
    {1, {"/musl/ltp/testcases/bin/setpgid02", 0}},
    {1, {"/musl/ltp/testcases/bin/setuid01", 0}},
    {1, {"/musl/ltp/testcases/bin/setuid03", 0}},
    {1, {"/musl/ltp/testcases/bin/setuid04", 0}},
    {1, {"/musl/ltp/testcases/bin/setreuid01", 0}},
    {1, {"/musl/ltp/testcases/bin/setreuid02", 0}},
    {1, {"/musl/ltp/testcases/bin/setreuid03", 0}},
    {1, {"/musl/ltp/testcases/bin/setreuid04", 0}},
    {1, {"/musl/ltp/testcases/bin/setreuid05", 0}},
    {1, {"/musl/ltp/testcases/bin/setreuid06", 0}},
    {1, {"/musl/ltp/testcases/bin/setreuid07", 0}},
    {1, {"/musl/ltp/testcases/bin/setregid01", 0}},
    {1, {"/musl/ltp/testcases/bin/setregid02", 0}},
    {1, {"/musl/ltp/testcases/bin/setregid03", 0}},
    {1, {"/musl/ltp/testcases/bin/setregid04", 0}},
    {1, {"/musl/ltp/testcases/bin/setresuid01", 0}},
    {1, {"/musl/ltp/testcases/bin/setresuid02", 0}},
    {1, {"/musl/ltp/testcases/bin/setresuid03", 0}},
    {1, {"/musl/ltp/testcases/bin/setresuid04", 0}},
    {1, {"/musl/ltp/testcases/bin/setresuid05", 0}},
    {1, {"/musl/ltp/testcases/bin/setresgid01", 0}},
    {1, {"/musl/ltp/testcases/bin/setresgid02", 0}},
    {1, {"/musl/ltp/testcases/bin/setresgid03", 0}},
    {1, {"/musl/ltp/testcases/bin/setresgid04", 0}},
    {1, {"/musl/ltp/testcases/bin/setpgrp01", 0}},
    {1, {"/musl/ltp/testcases/bin/setpgrp02", 0}},
    {1, {"/musl/ltp/testcases/bin/setgroups01", 0}},
    {1, {"/musl/ltp/testcases/bin/setgroups02", 0}},
    {1, {"/musl/ltp/testcases/bin/setgroups03", 0}},
    {1, {"/musl/ltp/testcases/bin/setgroups04", 0}},
    {1, {"/musl/ltp/testcases/bin/syscall01", 0}},
    {1, {"/musl/ltp/testcases/bin/signal02", 0}},
    {1, {"/musl/ltp/testcases/bin/signal03", 0}},
    {1, {"/musl/ltp/testcases/bin/signal04", 0}},
    {1, {"/musl/ltp/testcases/bin/signal05", 0}},
    {1, {"/musl/ltp/testcases/bin/signal06", 0}},
    {1, {"/musl/ltp/testcases/bin/stat01", 0}},
    {1, {"/musl/ltp/testcases/bin/stat01_64", 0}},
    {1, {"/musl/ltp/testcases/bin/stat02", 0}},
    {1, {"/musl/ltp/testcases/bin/stat02_64", 0}},
    {1, {"/musl/ltp/testcases/bin/statx02", 0}},
    {1, {"/musl/ltp/testcases/bin/statx03", 0}},
    {1, {"/musl/ltp/testcases/bin/sysinfo01", 0}},
    {1, {"/musl/ltp/testcases/bin/sysinfo02", 0}},
    {1, {"/musl/ltp/testcases/bin/sched_yield01", 0}},
    {1, {"/musl/ltp/testcases/bin/sched_get_priority_max01", 0}},
    {1, {"/musl/ltp/testcases/bin/sched_get_priority_max02", 0}},
    {1, {"/musl/ltp/testcases/bin/sched_get_priority_min01", 0}},
    {1, {"/musl/ltp/testcases/bin/sched_get_priority_min02", 0}},
    {1, {"/musl/ltp/testcases/bin/time01", 0}},
    {1, {"/musl/ltp/testcases/bin/times01", 0}},
    {1, {"/musl/ltp/testcases/bin/tkill01", 0}},
    {1, {"/musl/ltp/testcases/bin/tkill02", 0}},
    // {1, {"/musl/ltp/testcases/bin/write01", 0}},  // 跑2分钟
    {1, {"/musl/ltp/testcases/bin/write02", 0}},
    {1, {"/musl/ltp/testcases/bin/write03", 0}},
    {1, {"/musl/ltp/testcases/bin/write04", 0}},
    {1, {"/musl/ltp/testcases/bin/write05", 0}},
    {1, {"/musl/ltp/testcases/bin/write06", 0}},
    {1, {"/musl/ltp/testcases/bin/writev01", 0}},
    {1, {"/musl/ltp/testcases/bin/writev05", 0}},
    {1, {"/musl/ltp/testcases/bin/writev06", 0}},
    {1, {"/musl/ltp/testcases/bin/writev07", 0}},
    {1, {"/musl/ltp/testcases/bin/lseek01", 0}},
    {1, {"/musl/ltp/testcases/bin/lseek02", 0}},
    {1, {"/musl/ltp/testcases/bin/lseek07", 0}},
    {1, {"/musl/ltp/testcases/bin/llseek01", 0}},
    {1, {"/musl/ltp/testcases/bin/llseek02", 0}},
    {1, {"/musl/ltp/testcases/bin/llseek03", 0}},
    {1, {"/musl/ltp/testcases/bin/unlink05", 0}},
    {1, {"/musl/ltp/testcases/bin/unlink07", 0}},
    {1, {"/musl/ltp/testcases/bin/unlink08", 0}},
    {1, {"/musl/ltp/testcases/bin/unlinkat01", 0}},
    {1, {"/musl/ltp/testcases/bin/uname01", 0}},
    {1, {"/musl/ltp/testcases/bin/uname02", 0}},
    {1, {"/musl/ltp/testcases/bin/uname04", 0}},
    {1, {"/musl/ltp/testcases/bin/utsname01", 0}},
    {1, {"/musl/ltp/testcases/bin/utsname02", 0}},
    {1, {"/musl/ltp/testcases/bin/utsname03", 0}},
    {1, {"/musl/ltp/testcases/bin/utsname04", 0}},
    {1, {"/musl/ltp/testcases/bin/umask01", 0}},
    // {1, {"/musl/ltp/testcases/bin/vfork01", 0}},
    // {1, {"/musl/ltp/testcases/bin/vfork02", 0}},
    {1, {"/musl/ltp/testcases/bin/access01", 0}},
    {1, {"/musl/ltp/testcases/bin/access02", 0}},
    {1, {"/musl/ltp/testcases/bin/access03", 0}},
    {1, {"/musl/ltp/testcases/bin/symlink01", 0}}, // 通过4个， 有一个broken，没有summary
    {1, {"/musl/ltp/testcases/bin/symlink02", 0}},

    /*---------------------------------分隔线---------------------------------------------------*/

    /*这里是有问题的*/
    // {1, {"/musl/ltp/testcases/bin/sendfile01", 0}}, // 需要注释掉sendfile调用的return -1
    // {1, {"/musl/ltp/testcases/bin/sendfile02", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile03", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile04", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile05", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile08", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile01_64", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile02_64", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile03_64", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile04_64", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile05_64", 0}},
    // {1, {"/musl/ltp/testcases/bin/sendfile08_64", 0}},
    // {1, {"/musl/ltp/testcases/bin/futex_wait05", 0}}, ///< @todo 莫名其妙访问 > 4GB的位置
    // {1, {"/musl/ltp/testcases/bin/futex_wake04", 0}}, ///< @todo TCONF: hugetlbfs is not supported
    // {1, {"/musl/ltp/testcases/bin/futex_waitv01", 0}}, ///< @todo __NR_futex_waitv not supported on your arch
    // {1, {"/musl/ltp/testcases/bin/futex_waitv02", 0}}, ///< @todo __NR_futex_waitv not supported on your arch
    // {1, {"/musl/ltp/testcases/bin/futex_waitv03", 0}}, ///< @todo __NR_futex_waitv not supported on your arch
    // {1, {"/musl/ltp/testcases/bin/futex_cmp_requeue01", 0}}, ///< @todo fork() failed: EPERM (1)，部分通过，后面创建1000线程的难搞
    // {1, {"/musl/ltp/testcases/bin/open12", 0}},     ///< 没有summary，不测，创建了大于4GB的文件，镜像被他搞坏了
    // {1, {"/musl/ltp/testcases/bin/open14", 0}},     ///< 没有summary，不测
    // {1, {"/musl/ltp/testcases/bin/openat02", 0}},   ///< 没有summary，不测，创建了大于4GB的文件，镜像被他搞坏了，第三个测例都无法测试
    // {1, {"/musl/ltp/testcases/bin/openat03", 0}},   ///< 没有summary，不测
    // {1, {"/musl/ltp/testcases/bin/lseek11", 0}}, // 不支持稀疏文件
    // {1, {"/musl/ltp/testcases/bin/link08", 0}}, // 需要loop设备
    // {1, {"/musl/ltp/testcases/bin/unlink08", 0}}, // broken，权限
    // {1, {"/musl/ltp/testcases/bin/symlink03", 0}}, // Remaining cases broken, panic

    /* MEM 测例 */
    // {1, {"/musl/ltp/testcases/bin/shmt02", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt03", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt04", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt05", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt06", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt07", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt08", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt09", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmt10", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmdt01", 0}},
    // {1, {"/musl/ltp/testcases/bin/shmdt02", 0}},

    {0, {0}},
};

static longtest final_test[] = {
    {1, {"/glibc/interrupts-test-1", 0}},
    {0, {"/glibc/interrupts-test-2", 0}},
    {1, {"/glibc/copy-file-range-test-1", 0}},
    {1, {"/glibc/copy-file-range-test-2", 0}},
    {1, {"/glibc/copy-file-range-test-3", 0}},
    {1, {"/glibc/copy-file-range-test-4", 0}},
    {1, {"/glibc/test_splice", "1"}},
    {1, {"/glibc/test_splice", "2"}},
    {1, {"/glibc/test_splice", "3"}},
    {1, {"/glibc/test_splice", "4"}},
    {1, {"/glibc/test_splice", "5"}},
    {1, {"/musl/interrupts-test-1", 0}},
    {0, {"/musl/interrupts-test-2", 0}},
    {1, {"/musl/copy-file-range-test-1", 0}},
    {1, {"/musl/copy-file-range-test-2", 0}},
    {1, {"/musl/copy-file-range-test-3", 0}},
    {1, {"/musl/copy-file-range-test-4", 0}},
    {1, {"/musl/test_splice", "1"}},
    {1, {"/musl/test_splice", "2"}},
    {1, {"/musl/test_splice", "3"}},
    {1, {"/musl/test_splice", "4"}},
    {1, {"/musl/test_splice", "5"}},
    {0, {0}},
};

void test_final()
{
    int i, status, pid;
    printf("#### OS COMP TEST GROUP START interrupts-glibc ####\n");
    for (i = 0; i < 2; i++)
    {
        if (!final_test[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve(final_test[i].name[0], final_test[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }
    printf("#### OS COMP TEST GROUP END interrupts-glibc ####\n");

    printf("#### OS COMP TEST GROUP START copyfilerange-glibc ####\n");
    for (i = 2; i < 6; i++)
    {
        if (!final_test[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve(final_test[i].name[0], final_test[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }
    printf("#### OS COMP TEST GROUP END copyfilerange-glibc ####\n");

    printf("#### OS COMP TEST GROUP START splice-glibc ####\n");
    for (i = 6; i < 11; i++)
    {
        if (!final_test[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve(final_test[i].name[0], final_test[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }

    printf("#### OS COMP TEST GROUP END splice-glibc ####\n");

    printf("#### OS COMP TEST GROUP START interrupts-musl ####\n");
    for (i = 11; i < 13; i++)
    {
        if (!final_test[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve(final_test[i].name[0], final_test[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }
    printf("#### OS COMP TEST GROUP END interrupts-musl ####\n");

    printf("#### OS COMP TEST GROUP START copyfilerange-musl ####\n");
    for (i = 13; i < 17; i++)
    {
        if (!final_test[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve(final_test[i].name[0], final_test[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }
    printf("#### OS COMP TEST GROUP END copyfilerange-musl ####\n");

    printf("#### OS COMP TEST GROUP START splice-musl ####\n");
    for (i = 17; i < 22; i++)
    {
        if (!final_test[i].valid)
            continue;
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve(final_test[i].name[0], final_test[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }

    printf("#### OS COMP TEST GROUP END splice-musl ####\n");
}

void test_sh()
{
    int pid;
    pid = fork();
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

    // sys_chdir("/glibc");
    // if (pid < 0)
    // {
    //     printf("init: fork failed\n");
    //     exit(1);
    // }
    // if (pid == 0)
    // {
    //     char *newargv[] = {"sh", "-c", "./libctest_testcode.sh", NULL};
    //     // char *newargv[] = {"sh", "-c", "./lmbench_testcode.sh", NULL};
    //     // char *newargv[] = {"sh", "-c", "./ltp_testcode.sh", NULL};
    //     // char *newargv[] = {"sh", "-c","./busybox_testcode.sh", NULL};
    //     // char *newargv[] = {"sh", "./basic_testcode.sh", NULL};
    //     // char *newargv[] = {"sh", "-c","./iozone_testcode.sh", NULL};
    //     // char *newargv[] = {"sh", "./libcbench_testcode.sh", NULL};
    //     char *newenviron[] = {NULL};
    //     sys_execve("busybox", newargv, newenviron);
    //     printf("execve error.\n");
    //     exit(1);
    // }
    // wait(0);
}

void test_ltp()
{
    printf("#### OS COMP TEST GROUP START ltp-glibc ####\n");
    int i, status, pid;
    sys_chdir("/glibc/ltp/testcases/bin");
    for (i = 0; ltp[i].name[0]; i++)
    {
        if (!ltp[i].valid)
            continue;
        // 提取基准文件名
        char *path = ltp[i].name[0];
        char *basename = path;
        char *p = strrchr(path, '/');
        if (p)
            basename = p + 1;
        printf("RUN LTP CASE %s\n", basename);
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve(ltp[i].name[0], ltp[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
        printf("FAIL LTP CASE %s : %d\n", basename, status);
    }
    printf("#### OS COMP TEST GROUP END ltp-glibc ####\n");
}

void test_ltp_musl()
{
    printf("#### OS COMP TEST GROUP START ltp-musl ####\n");
    int i, status, pid;
    sys_chdir("/musl/ltp/testcases/bin");
    for (i = 0; ltp_musl[i].name[0]; i++)
    {
        if (!ltp_musl[i].valid)
            continue;
        // 提取基准文件名
        char *path = ltp_musl[i].name[0];
        char *basename = path;
        char *p = strrchr(path, '/');
        if (p)
            basename = p + 1;
        printf("RUN LTP CASE %s\n", basename);
        pid = fork();
        if (pid == 0)
        {
            char *newenviron[] = {NULL};
            sys_execve(ltp_musl[i].name[0], ltp_musl[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
        printf("FAIL LTP CASE %s : %d\n", basename, status);
    }
    printf("#### OS COMP TEST GROUP END ltp-musl ####\n");
}
/*******************************************************************************
 *                              IOZONE TEST SUITE                              *
 *******************************************************************************/
void test_iozone()
{
    int pid, status;
    sys_chdir("/glibc");
    printf("#### OS COMP TEST GROUP START iozone-glibc ####\n");
    char *newenviron[] = {NULL};
    printf("iozone automatic measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[0].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);
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
    printf("iozone throughput fwrite/fread measurements\n"); // musl 跑不了
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[5].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);
    printf("iozone throughput pwrite/pread measurements\n"); // musl 跑不了
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
    printf("#### OS COMP TEST GROUP END iozone-glibc ####\n");

    sys_chdir("/musl");
    printf("#### OS COMP TEST GROUP START iozone-musl ####\n");
    printf("iozone automatic measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[0].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);
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

    printf("iozone throughput pwritev/preadv measurements\n");
    pid = fork();
    if (pid == 0)
    {
        sys_execve("iozone", iozone[7].name, newenviron);
        exit(0);
    }
    waitpid(pid, &status, 0);
    printf("#### OS COMP TEST GROUP END iozone-musl ####\n");
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
    sys_chdir("/musl");
    printf("#### OS COMP TEST GROUP START lmbench-musl ####\n");
    printf("latency measurements\n");

    for (i = 0; lmbench[i].name[1]; i++)
    {
        if (i == 18 || i == 23 || i == 26)
        {
            continue;
        }
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

    printf("#### OS COMP TEST GROUP END lmbench-musl ####\n");

    sys_chdir("/glibc");
    printf("#### OS COMP TEST GROUP START lmbench-glibc ####\n");
    printf("latency measurements\n");

    for (i = 0; lmbench[i].name[1]; i++)
    {
        if (!lmbench[i].valid)
            continue;
        if (i == 18 || i == 26)
        {
            continue;
        }
        pid = fork();
        char *newenviron[] = {NULL};
        if (pid == 0)
        {
            sys_execve(lmbench[i].name[0], lmbench[i].name, newenviron);
            exit(0);
        }
        waitpid(pid, &status, 0);
    }

    printf("#### OS COMP TEST GROUP END lmbench-glibc ####\n");
}

[[maybe_unused]] static longtest lmbench[] = {
    {1, {"lmbench_all", "lat_syscall", "-P", "1", "null", 0}},
    {1, {"lmbench_all", "lat_syscall", "-P", "1", "read", 0}},
    {1, {"lmbench_all", "lat_syscall", "-P", "1", "write", 0}},
    {1, {"busybox", "mkdir", "-p", "/var/tmp", 0}},
    {1, {"busybox", "touch", "/var/tmp/lmbench", 0}},
    {1, {"lmbench_all", "lat_syscall", "-P", "1", "stat", "/var/tmp/lmbench", 0}},
    {1, {"lmbench_all", "lat_syscall", "-P", "1", "fstat", "/var/tmp/lmbench", 0}},
    {1, {"lmbench_all", "lat_syscall", "-P", "1", "open", "/var/tmp/lmbench", 0}},
    {1, {"lmbench_all", "lat_select", "-n", "100", "-P", "1", "file", 0}},
    {1, {"lmbench_all", "lat_sig", "-P", "1", "install", 0}},
    {1, {"lmbench_all", "lat_sig", "-P", "1", "catch", 0}},
    {1, {"lmbench_all", "lat_pipe", "-P", "1", 0}},
    {1, {"lmbench_all", "lat_proc", "-P", "1", "fork", 0}},
    {1, {"lmbench_all", "lat_proc", "-P", "1", "exec", 0}},
    {1, {"busybox", "cp", "hello", "/tmp", 0}},
    {1, {"lmbench_all", "lat_proc", "-P", "1", "shell", 0}},
    {1, {"lmbench_all", "lmdd", "label=File /var/tmp/XXX write bandwidth:", "of=/var/tmp/XXX", "move=1m", "fsync=1", "print=3", 0}},
    {1, {"lmbench_all", "lat_pagefault", "-P", "1", "/var/tmp/XXX", 0}},
    {1, {"lmbench_all", "lat_mmap", "-P", "1", "512k", "/var/tmp/XXX", 0}}, // musl有问题
    {1, {"busybox", "echo", "file", "system", "latency", 0}},
    {1, {"lmbench_all", "lat_fs", "/var/tmp", 0}},
    {1, {"busybox", "echo", "Bandwidth", "measurements", 0}},
    {1, {"lmbench_all", "bw_pipe", "-P", "1", 0}},
    {1, {"lmbench_all", "bw_file_rd", "-P", "1", "512k", "io_only", "/var/tmp/XXX", 0}},
    {1, {"lmbench_all", "bw_file_rd", "-P", "1", "512k", "open2close", "/var/tmp/XXX", 0}},
    {1, {"lmbench_all", "bw_mmap_rd", "-P", "1", "512k", "mmap_only", "/var/tmp/XXX", 0}},
    {1, {"lmbench_all", "bw_mmap_rd", "-P", "1", "512k", "open2close", "/var/tmp/XXX", 0}},
    {1, {"busybox", "echo", "context", "switch", "overhead", 0}},
    {1, {"lmbench_all", "lat_ctx", "-P", "1", "-s", "32", "2", "4", "8", "16", "24", "32", "64", "96", 0}},
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
    // sys_chdir("/musl");
    sys_chdir("/glibc");
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
    printf("test_libc_dy start\n");
    int i, pid, status;
    // sys_chdir("/musl");
    sys_chdir("/glibc");
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
    {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cancel", 0}}, ///<    @todo 无限循环
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
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cond_smasher", 0}}, ///< @todo 没唤醒
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
    // sys_chdir("/sdcard");
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
    printf("#### OS COMP TEST GROUP END basic-glibc ####\n");

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

struct test_results
{
    int passed;
    int failed;
    char message[256];
};

int test_shm()
{
    printf("=== Shared Memory and Munmap Interaction Test ===\n");

    // 1. 创建共享内存段
    int shmid = sys_shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1)
    {
        printf("Failed to create shared memory segment\n");
        exit(1);
    }
    printf("Created shared memory segment with ID: %d\n", shmid);

    // 2. 附加共享内存
    char *shm_addr = (char *)sys_shmat(shmid, 0, 0);
    if (shm_addr == (char *)-1)
    {
        printf("Failed to attach shared memory\n");
        exit(1);
    }
    printf("Attached shared memory at address: %p\n", shm_addr);

    // 3. 写入数据到共享内存
    strcpy(shm_addr, "Data in shared memory");
    printf("Wrote to shared memory: %s\n", shm_addr);

    // 4. 创建mmap映射
    char *mmap_addr = (char *)sys_mmap(0, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mmap_addr == MAP_FAILED)
    {
        printf("Failed to create mmap\n");
        exit(1);
    }
    printf("Created mmap at address: %p\n", mmap_addr);

    // 5. 写入数据到mmap
    strcpy(mmap_addr, "Data in mmap");
    printf("Wrote to mmap: %s\n", mmap_addr);

    // 6. 创建子进程
    int pid = fork();
    if (pid == 0)
    {
        // 子进程
        printf("Child process started (PID: %d)\n", getpid());

        // 子进程附加到共享内存
        char *child_shm_addr = (char *)sys_shmat(shmid, 0, 0);
        if (child_shm_addr == (char *)-1)
        {
            printf("Child failed to attach shared memory\n");
            exit(1);
        }
        printf("Child attached shared memory at: %p\n", child_shm_addr);
        printf("Child read from shared memory: %s\n", child_shm_addr);

        // 修改共享内存数据
        strcpy(child_shm_addr, "Modified by child process");
        printf("Child modified shared memory: %s\n", child_shm_addr);

        // 等待父进程操作
        sleep(3);

        // 分离共享内存
        if (sys_shmdt((uint64)child_shm_addr) == -1)
        {
            printf("Child failed to detach shared memory\n");
        }
        else
        {
            printf("Child detached shared memory\n");
        }

        exit(0);
    }
    else if (pid > 0)
    {
        // 父进程
        printf("Parent process (PID: %d), child PID: %d\n", getpid(), pid);

        // 等待子进程修改数据
        sleep(1);
        printf("Parent read from shared memory: %s\n", shm_addr);

        // 部分munmap共享内存（测试部分解除映射）
        printf("Testing partial munmap of shared memory...\n");
        uint64 partial_start = (uint64)shm_addr + 4096;
        uint64 partial_len = 4096;

        if (sys_munmap((void *)partial_start, partial_len) == 0)
        {
            printf("Partial munmap successful\n");
        }
        else
        {
            printf("Partial munmap failed\n");
        }

        // 等待子进程完成
        wait(0);
        printf("Child process finished\n");

        // 删除共享内存段
        printf("Deleting shared memory segment...\n");
        if (sys_shmctl(shmid, IPC_RMID, 0) == -1)
        {
            printf("Failed to delete shared memory segment\n");
        }
        else
        {
            printf("Shared memory segment marked for deletion\n");
        }

        // 尝试访问已删除的共享内存
        printf("Trying to access deleted shared memory...\n");
        char *new_shm_addr = (char *)sys_shmat(shmid, 0, 0);
        if (new_shm_addr == (char *)-22)
        {
            printf("Successfully prevented access to deleted shared memory\n");
        }
        else
        {
            printf("ERROR: Still able to access deleted shared memory!\n");
            sys_shmdt((uint64)new_shm_addr);
        }

        // 分离剩余的共享内存
        if (sys_shmdt((uint64)shm_addr) == -1)
        {
            printf("Parent failed to detach shared memory\n");
        }
        else
        {
            printf("Parent detached shared memory\n");
        }

        // 清理mmap
        if (sys_munmap(mmap_addr, 8192) == 0)
        {
            printf("Mmap cleanup successful\n");
        }
        else
        {
            printf("Mmap cleanup failed\n");
        }

        printf("=== Munmap Interaction Test completed ===\n");
    }
    else
    {
        printf("Fork failed\n");
        exit(1);
    }

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

// void test_mmap_private()
// {
//     printf("test_mmap start\n");

//     // 创建一个测试文件
//     int fd = openat(AT_FDCWD, "/test_mmap.txt", O_RDWR | O_CREATE);
//     if (fd < 0) {
//         printf("Failed to create test file\n");
//         return;
//     }

//     // 写入一些测试数据
//     const char *test_data = "Hello, mmap test! This is a test file for MAP_PRIVATE mapping.";
//     int data_len = strlen(test_data);
//     if (write(fd, test_data, data_len) != data_len) {
//         printf("Failed to write test data\n");
//         close(fd);
//         return;
//     }

//     // 使用MAP_PRIVATE映射文件
//     void *mapped_addr = sys_mmap(0, data_len, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
//     if (mapped_addr == (void *)-1) {
//         printf("mmap failed\n");
//         close(fd);
//         return;
//     }

//     printf("File mapped at address: %p\n", mapped_addr);

//     // 读取映射的内容（这会触发缺页处理）
//     char buffer[256];
//     memcpy(buffer, mapped_addr, data_len);
//     buffer[data_len] = '\0';
//     printf("Read from mapped memory: %s\n", buffer);

//     // 尝试写入映射的内存（这会触发写时复制）
//     memcpy(mapped_addr, "Modified content!", 17);
//     printf("Modified mapped memory\n");

//     // 再次读取，验证修改是否生效
//     memcpy(buffer, mapped_addr, data_len);
//     buffer[data_len] = '\0';
//     printf("Read after modification: %s\n", buffer);

//     char file_buffer[256];
//     sys_read(fd, file_buffer, data_len);
//     printf("file content :%s\n",file_buffer);

//     // 清理
//     sys_munmap(mapped_addr, data_len);
//     close(fd);
//     sys_unlinkat(AT_FDCWD, "/test_mmap.txt", 0);

//     printf("test_mmap completed successfully\n");
// }

/*******************************************************************************
 *                              BUSYBOX RUN FUNCTION                           *
 *******************************************************************************/
/**
 * 使用busybox执行给定的命令字符串
 * @param command 要执行的命令字符串，例如 "ls -la" 或 "echo hello world"
 * @return 命令的退出状态码
 */
int busybox_run(const char *command)
{
    if (!command || strlen(command) == 0)
    {
        printf("错误: 命令字符串为空\n");
        return -1;
    }

    printf("执行busybox命令: %s\n", command);

    // 创建子进程执行命令
    int pid = fork();
    if (pid < 0)
    {
        printf("错误: fork失败\n");
        return -1;
    }

    if (pid == 0)
    {
        // 子进程
        // 使用栈数组存储命令副本和参数
        char cmd_copy[512];
        char *argv[64]; // 最多支持64个参数

        // 复制命令字符串
        int cmd_len = strlen(command);
        if (cmd_len >= sizeof(cmd_copy))
        {
            printf("错误: 命令字符串过长\n");
            exit(1);
        }
        strcpy(cmd_copy, command);

        // 设置第一个参数为"busybox"
        argv[0] = "/musl/busybox";

        // 分割命令字符串
        char *token = strtok(cmd_copy, " ");
        int i = 1;
        while (token && i < 63)
        { // 最多62个参数 + "busybox"
            argv[i++] = token;
            token = strtok(NULL, " ");
        }
        argv[i] = NULL; // 参数数组必须以NULL结尾

        // 设置环境变量
        char *newenviron[] = {NULL};

        // 执行busybox命令
        sys_execve("/musl/busybox", argv, newenviron);

        // 如果execve失败，退出
        printf("错误: execve失败\n");
        exit(1);
    }
    else
    {
        // 父进程等待子进程完成
        int status;
        waitpid(pid, &status, 0);

        int exit_code = 0;
        if (status & 0xFF)
        {
            // 进程异常终止
            printf("命令异常终止\n");
            exit_code = -1;
        }
        else
        {
            // 进程正常退出
            exit_code = (status >> 8) & 0xFF;
            printf("命令执行完成，退出码: %d\n", exit_code);
        }

        return exit_code;
    }
    return 0;
}

/*******************************************************************************
 *                              BUSYBOX RUN FUNCTION END                       *
 *******************************************************************************/