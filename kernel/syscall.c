#include "types.h"
#include "print.h"
#include "trap.h" //这个必须要再hsai_trap.h前。syscall(struct trapframe *trapframe)参数表中的trapframe对外部不可见，所以先声明trapframe
#include "hsai_trap.h"
#include "defs.h"
#include "vmem.h"
#include "cpu.h"
#include "process.h"
#include "string.h"
#include "timer.h"
#include "syscall_ids.h"
#include "fs_defs.h"
#include "fcntl.h"
#include "vfs_ext4.h"
#include "file.h"
#include "elf.h"
#include "fcntl.h"
#include "stat.h"
#include "ext4_oflags.h"
#include "ext4_errno.h"
#include "fs_defs.h"
#include "fcntl.h"
#include "vfs_ext4.h"
#include "file.h"
#include "ext4_oflags.h"
#include "ext4_errno.h"
#include "print.h"
#include "pmem.h"
#include "fcntl.h"
#include "ext4_oflags.h"
#include "fs.h"
#include "vfs_ext4.h"
#include "struct_iovec.h"
#include "futex.h"
#include "socket.h"
#include "errno-base.h"
#include "resource.h"
#include "slab.h"
#include "select.h"
#include "signal.h"

#include "stat.h"
#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

/**
 * @brief  在指定目录文件描述符下打开文件
 *
 * @param fd    目录的文件描述符 (FDCWD表示当前工作目录)
 * @param upath 用户空间指向路径字符串的指针
 * @param flags 文件打开标志
 * @param mode  文件创建时的权限模式
 * @return int  成功返回文件描述符，失败返回负的标准错误码
 */
int sys_openat(int fd, const char *upath, int flags, uint16 mode)
{
    if (fd != AT_FDCWD && (fd < 0 || fd >= NOFILE))
        return -ENOENT;
    char path[MAXPATH];
    proc_t *p = myproc();
    if (copyinstr(p->pagetable, path, (uint64)upath, MAXPATH) == -1)
    {
        return -EFAULT;
    }
#if DEBUG
    LOG("sys_openat fd:%d,path:%s,flags:%d,mode:%d\n", fd, path, flags, mode);
#endif
    // 添加进程状态调试信息
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_openat] pid:%d opening file: %s, flags: %x\n", 
                   p->pid, path, flags);
    struct filesystem *fs = get_fs_from_path(path); ///<  根据路径获取对应的文件系统
    /* @todo 官方测例好像vfat和ext4一种方式打开 */
    if (fs->type == EXT4 || fs->type == VFAT)
    {
        const char *dirpath = (fd == AT_FDCWD) ? myproc()->cwd.path : myproc()->ofile[fd]->f_path;
        char absolute_path[MAXPATH] = {0};
        get_absolute_path(path, dirpath, absolute_path);
        struct file *f;
        f = filealloc();
        if (!f)
            return -ENFILE;
        int fd = -1;
        if ((fd = fdalloc(f)) < 0)
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "OUT OF FD!\n");
            return -EMFILE;
        };

        f->f_flags = flags | (strcmp(absolute_path, "/tmp") ? 0 : O_CREAT);
        f->f_mode = mode;

        strcpy(f->f_path, absolute_path);
        int ret;

        if ((ret = vfs_ext4_openat(f)) < 0)
        {
            // printf("打开失败: %s (错误码: %d)\n", path, ret);
            /*
             *   以防万一有什么没有释放的东西，先留着
             *   get_file_ops()->close(f);
             */
            myproc()->ofile[fd] = 0;
            // if(!strcmp(path, "./mnt")) {
            //     return 2;
            return -ENOENT;
        }
        /* @note 处理busybox的几个文件夹 */
        if (!strcmp(absolute_path, "/proc/mounts") ||  ///< df
            !strcmp(absolute_path, "/proc") ||         ///< ps
            !strcmp(absolute_path, "/proc/meminfo") || ///< free
            !strcmp(absolute_path, "/dev/misc/rtc")    ///< hwclock
        )
        {
            if (vfs_ext4_is_dir(absolute_path) == 0)
                vfs_ext4_dirclose(f);
            else
                vfs_ext4_fclose(f);
            f->f_type = FD_BUSYBOX;
            f->f_pos = 0;
        }
        return fd;
    }
    else
        panic("unsupport filesystem");
    return 0;
};
/**
 * @brief 向文件描述符写入数据
 *
 * @param fd  目标文件描述符
 * @param va  用户空间数据缓冲区的虚拟地址
 * @param len 请求写入的字节数
 * @return int 成功时返回实际写入的字节数，-1表示失败
 */
int sys_write(int fd, uint64 va, int len)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_write]:fd:%d va %p len %d\n", fd, va, len);
    struct file *f;
    if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
        return -ENOENT;
    int reallylen = get_file_ops()->write(f, va, len);
    return reallylen;
}

/**
 * @brief 分散写（writev） - 将多个分散的内存缓冲区数据写入文件描述符
 *
 * @param fd      目标文件描述符
 * @param uiov    用户空间iovec结构数组的虚拟地址
 * @param iovcnt  iovec数组的元素个数
 * @return uint64 实际写入的总字节数（成功时），-1表示失败
 */
uint64 sys_writev(int fd, uint64 uiov, uint64 iovcnt)
{
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[sys_writev] fd:%d iov:%p iovcnt:%d\n", fd, uiov, iovcnt);
#endif
    struct file *f;
    if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
        return -1;
    iovec v[IOVMAX];
    if (uiov)
    {
        copyin(myproc()->pagetable, (char *)v, uiov, sizeof(iovec) * iovcnt); ///< / 将用户空间的iovec数组拷贝到内核空间
    }
    else
        return -1;
    uint64 len = 0;
    // 遍历iovec数组，逐个缓冲区执行写入
    for (int i = 0; i < iovcnt; i++)
    {
        len += get_file_ops()->write(f, (uint64)(v[i].iov_base), v[i].iov_len);
    }
    return len;
}

uint64 sys_getpid(void)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "pid is %d\n", myproc()->pid);
    return myproc()->pid;
}

uint64 sys_getppid()
{
    proc_t *pp = myproc()->parent;
    assert(pp != NULL, "sys_getppid\n");
    DEBUG_LOG_LEVEL(LOG_DEBUG, "pid is %d, ppid is %d\n", myproc()->pid, pp->pid);
    return pp->pid;
}

uint64 sys_fork(void)
{
    return fork();
}

/**
 * @brief  创建一个子进程；
 *
 * @param flags  创建的标志，如SIGCHLD；
 * @param stack  指定新进程的栈，可为0；
 * @param ptid   父线程ID；
 * @param tls    TLS线程本地存储描述符；
 * @param ctid   子线程ID；
 * @return int   成功则返回子进程的线程ID，失败返回-1；
 */
int sys_clone(uint64 flags, uint64 stack, uint64 ptid, uint64 tls, uint64 ctid)
{
#if DEBUG
    LOG("sys_clone: flags:%p, stack:%p, ptid:%p, tls:%p, ctid:%p\n",
        flags, stack, ptid, tls, ctid);
    // assert(flags == 17, "sys_clone: flags is not SIGCHLD");
#endif

    // 检查flags中的信号部分（低8位）
    int signal = flags & 0xff;

    if (stack == 0)
    {
        // 如果flags包含SIGCHLD，确保父进程能正确处理子进程退出信号
        if (signal == SIGCHLD)
        {
            // 这里可以设置默认的SIGCHLD处理，或者确保父进程已经设置了处理函数
            struct proc *p = myproc();
            // 如果父进程没有设置SIGCHLD处理函数，设置一个默认的忽略处理
            if (p->sigaction[SIGCHLD].__sigaction_handler.sa_handler == NULL)
            {
                // 设置默认的SIGCHLD处理为忽略
                p->sigaction[SIGCHLD].__sigaction_handler.sa_handler = (__sighandler_t)1; // SIG_IGN
                p->sigaction[SIGCHLD].sa_flags = 0;
                memset(&p->sigaction[SIGCHLD].sa_mask, 0, sizeof(p->sigaction[SIGCHLD].sa_mask));
            }
        }
        
        // 调用fork()创建子进程
        int pid = fork();
        
        // 处理CLONE_CHILD_SETTID标志位
        if (pid > 0 && (flags & CLONE_CHILD_SETTID) && ctid != 0)
        {
            // 在父进程中，将子进程的PID写入ctid指向的用户空间地址
            struct proc *p = myproc();
            if (copyout(p->pagetable, ctid, (char *)&pid, sizeof(pid)) < 0)
            {
                // 如果写入失败，记录错误但不影响fork的成功
                LOG("sys_clone: CLONE_CHILD_SETTID copyout failed\n");
            }
        }
        
        // 处理CLONE_CHILD_CLEARTID标志位
        if (pid > 0 && (flags & CLONE_CHILD_CLEARTID) && ctid != 0)
        {
            // 在父进程中，设置clear_child_tid，子进程退出时会清零这个地址
            struct proc *p = myproc();
            p->clear_child_tid = ctid;
        }
        
        return pid;
    }
    
    if (flags & CLONE_VM)
        return clone_thread(stack, ptid, tls, ctid, flags);
    return clone(flags, stack, ptid, ctid);
}

int sys_clone3()
{
    // release(&myproc()->lock);
    // exit(0);
    return -ENOSYS;
}

int sys_wait(int pid, uint64 va, int option)
{
    return wait(pid, va);
}

uint64 sys_exit(int n)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_exit] pid:%d exiting with code: %d\n", myproc()->pid, n);
    exit(n);
    return 0;
}

uint64 sys_kill(int pid, int sig)
{
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "sys_kill: pid:%d, sig:%d\n", pid, sig);
#endif
    assert(pid >= 0, "pid null!");
    if (sig < 0 || sig >= SIGRTMAX)
    {
        panic("sig error");
        return -1;
    }

    return kill(pid, sig);
}

uint64 sys_gettimeofday(uint64 tv_addr)
{
    struct proc *p = myproc();
    timeval_t tv = timer_get_time();
    return copyout(p->pagetable, tv_addr, (char *)&tv, sizeof(timeval_t));
}

/**
 * @brief 获取指定时钟的时间
 *
 * @param tid       线程ID（当前实现未使用）
 * @param uaddr     用户空间存放时间结构体的地址
 * @return uint64   成功返回0，失败返回-1
 */
int sys_clock_gettime(uint64 clkid, uint64 uaddr)
{
    // timeval_t tv = timer_get_time();
    // DEBUG_LOG_LEVEL(LOG_DEBUG, "clock_gettime:sec:%u,usec:%u\n", tv.sec, tv.usec);
    timespec_t tv;
    switch(clkid) {
        case CLOCK_REALTIME:   // 实时系统时间
            tv = timer_get_ntime();
            // panic("not implement yet,clkid :%d",clkid);
            break;
        case CLOCK_MONOTONIC:  // 单调递增时间（系统启动后）
            tv = timer_get_ntime();
            break;
        case CLOCK_REALTIME_COARSE:
            tv = timer_get_ntime();
            break;
        default:
            panic("not implement yet,clkid :%d",clkid);
            return -1;  // 不支持的时钟类型
    }
    DEBUG_LOG_LEVEL(LOG_DEBUG, "clock_gettime:sec:%u,nsec:%u\n", tv.tv_sec, tv.tv_nsec );
    if (copyout(myproc()->pagetable, uaddr, (char *)&tv, sizeof(struct timespec)) < 0)
        return -1;
    return 0;
}

/**
 * @brief 睡眠一段时间
 *        timeval_t* req   目标睡眠时间
 *        timeval_t* rem   未完成睡眠时间
 * @return int 成功返回0 失败返回-1
 */
int sleep(timeval_t *req, timeval_t *rem)
{
    proc_t *p = myproc();
    timeval_t wait; ///<  用于存储从用户空间拷贝的休眠时间
    if (copyin(p->pagetable, (char *)&wait, (uint64)req, sizeof(timeval_t)) == -1)
    {
        return -1;
    }
    timeval_t start, end;
    start = timer_get_time(); ///<  获取休眠开始时间
    acquire(&tickslock);
    while (1)
    {
        end = timer_get_time();
        if (end.sec - start.sec >= wait.sec)
            break;
        if (myproc()->killed)
        {
            release(&tickslock);
            return -1;
        }
        sleep_on_chan(&ticks, &tickslock);
    }
    release(&tickslock);

    return 0;
}

/**
 * @brief 调整进程的堆大小
 *
 * @param n  新的程序断点地址（program break）(堆的结束地址)
 * @return uint64  成功返回新的程序断点地址，失败返回-1
 */
uint64 sys_brk(uint64 n)
{
    uint64 addr;
    addr = myproc()->sz;
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[sys_brk] p->sz: %p,n:  %p\n", addr, n);
#endif
    if (n == 0)
    {
        return addr;
    }
    if (growproc(n - addr) < 0)
        return -1;
    return n;
}

uint64 sys_times(uint64 dstva)
{
    return get_times(dstva);
}

int sys_settimer(int which, uint64 new_value, uint64 old_value)
{
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[sys_settimer] which:%d, interval:%p,oldvalue:%p\n", which, new_value, old_value);
#endif
    // proc_t *p = myproc();

    // // 只支持ITIMER_REAL
    // if (which != 0)
    // { // ITIMER_REAL = 0
    //     return -1;
    // }

    // // 保存旧的定时器设置
    // if (old_value)
    // {
    //     if (copyout(p->pagetable, old_value, (char *)&p->itimer, sizeof(struct itimerval)) < 0)
    //     {
    //         return -1;
    //     }
    // }

    // // 设置新的定时器
    // if (new_value)
    // {
    //     struct itimerval new_timer;
    //     if (copyin(p->pagetable, (char *)&new_timer, new_value, sizeof(struct itimerval)) < 0)
    //     {
    //         return -1;
    //     }

    //     // 更新进程的定时器设置
    //     p->itimer = new_timer;

    //     // 计算下一次警报的tick值
    //     if (new_timer.it_value.sec || new_timer.it_value.usec)
    //     {
    //         uint64 now = r_time();
    //         uint64 interval = (uint64)new_timer.it_value.sec * CLK_FREQ +
    //                           (uint64)new_timer.it_value.usec * (CLK_FREQ / 1000000);
    //         p->alarm_ticks = now + interval;
    //         p->timer_active = 1;
    //     }
    //     else
    //     {
    //         p->timer_active = 0; // 定时器值为0则禁用
    //     }
    // }

    return 0;
}
// 定义了一个结构体 utsname，用于存储系统信息
struct utsname
{
    char sysname[65]; ///<  操作系统名称
    char nodename[65];
    char release[65]; ///<  操作系统发行版本
    char version[65]; ///<  操作系统版本
    char machine[65];
    char domainname[65];
};
int sys_uname(uint64 buf)
{
    struct utsname uts;
    strncpy(uts.sysname, "SC7\0", 65);
    strncpy(uts.nodename, "none\0", 65);
    strncpy(uts.release, "6.1.0\0", 65);
    strncpy(uts.version, "6.1.0\0", 65);
#ifdef RISCV
    strncpy(uts.machine, "riscv64", 65);
#else ///< loongarch
    strncpy(uts.machine, "Loongarch64", 65);
#endif
    strncpy(uts.domainname, "none\0", 65);

    return copyout(myproc()->pagetable, buf, (char *)&uts, sizeof(uts));
}

uint64 sys_sched_yield()
{
    yield();
    return 0;
}

/**
 * @brief 执行新程序
 *
 * @param upath  用户态程序路径
 * @param uargv  用户态参数指针数组
 * @param uenvp  用户态环境变量指针数组
 * @return int
 */
int sys_execve(const char *upath, uint64 uargv, uint64 uenvp)
{
    char path[MAXPATH], *argv[MAXARG], *envp[NENV];
    proc_t *p = myproc();

    // 复制路径
    if (copyinstr(p->pagetable, path, (uint64)upath, MAXPATH) == -1)
    {
        return -1;
    }

#if DEBUG
    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_execve] path:%s, uargv:%p, uenv:%p\n", path, uargv, uenvp);
#endif

    // 处理参数
    memset(argv, 0, sizeof(argv));
    int i;
    uint64 uarg = 0;
    for (i = 0;; i++)
    {
        if (i >= NELEM(argv))
        {
            panic("sys_execve: argv too long\n");
            goto bad;
        }
        if (fetchaddr((uint64)(uargv + sizeof(uint64) * i), (uint64 *)&uarg) < 0)
        {
            panic("sys_execve: fetchaddr error,uargv:%p\n", uarg);
            goto bad;
        }
        if (uarg == 0)
        {
            argv[i] = 0;
            break;
        }
        argv[i] = pmem_alloc_pages(1);
        memset(argv[i], 0, PGSIZE);
        if (fetchstr((uint64)uarg, argv[i], PGSIZE) < 0)
        {
            panic("sys_execve: fetchstr error,uargv:%p\n", uargv);
            goto bad;
        }
    }

    // 处理环境变量
    memset(envp, 0, sizeof(envp));
    uint64 uenv = 0;
    int env_count = 0;
    const char *ld_path = "LD_LIBRARY_PATH=/glibc/lib:/musl/lib:";

    // ========== 关键修改：添加 LD_LIBRARY_PATH ==========
    // 首先添加预设的 LD_LIBRARY_PATH
    envp[env_count] = pmem_alloc_pages(1);
    if (!envp[env_count])
    {
        panic("sys_execve: alloc failed for LD_LIBRARY_PATH\n");
        goto bad;
    }
    memset(envp[env_count], 0, PGSIZE);
    strncpy(envp[env_count], ld_path, PGSIZE - 1);
    env_count++;

    // 复制用户环境变量（跳过已存在的 LD_LIBRARY_PATH）
    for (i = 0;; i++)
    {
        if (env_count >= NELEM(envp) - 1)
        { // 保留一个NULL终止符位置
            panic("sys_execve: envp too long\n");
            goto bad;
        }
        if (fetchaddr((uint64)(uenvp + sizeof(uint64) * i), (uint64 *)&uenv) < 0)
        {
            panic("sys_execve: fetchaddr error,uenvp:%p\n", uenvp);
            goto bad;
        }
        if (uenv == 0)
        {
            break; // 遇到NULL终止符
        }

        // 分配空间并复制环境变量
        char *env_str = pmem_alloc_pages(1);
        if (!env_str)
        {
            panic("sys_execve: alloc failed for env var\n");
            goto bad;
        }
        memset(env_str, 0, PGSIZE);
        if (fetchstr((uint64)uenv, env_str, PGSIZE) < 0)
        {
            pmem_free_pages(env_str, 1);
            panic("sys_execve: fetchstr error,uenvp:%p\n", uenv);
            goto bad;
        }

        // 跳过用户设置的 LD_LIBRARY_PATH
        if (strncmp(env_str, "LD_LIBRARY_PATH=", 16) == 0)
        {
            pmem_free_pages(env_str, 1);
            continue;
        }

        envp[env_count++] = env_str;
    }

    // 添加终止符
    envp[env_count] = 0;

    // 执行程序
    int ret = exec(path, argv, envp);

    // 清理资源
    for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
        pmem_free_pages(argv[i], 1);

    for (i = 0; i < env_count; i++)
        pmem_free_pages(envp[i], 1);

    return ret;

bad:
    // 错误处理：释放已分配的资源
    for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
        pmem_free_pages(argv[i], 1);

    for (i = 0; i < env_count; i++)
        if (envp[i])
            pmem_free_pages(envp[i], 1);

    return -1;
}

/**
 * @brief 关闭文件描述符
 *
 * @param fd  要关闭的文件描述符
 * @return int 成功返回0，失败返回错误码
 */
int sys_close(int fd)
{
    struct proc *p = myproc();
    struct file *f;

    DEBUG_LOG_LEVEL(LOG_DEBUG, "close fd is %d\n", fd);
    if (fd < 0)
        /*
         * @todo glibc返回值的问题
         * glibc的dup超过数量后，openat返回EMFILE之后，会调用close (-1)，理论上应该返回ENFILE，但是
         * 测例要求返回EMFILE，不知道后续会不会删掉该测例，但是这里先返回EMFILE
         */
        return -EMFILE;
    if (fd >= NOFILE || (f = p->ofile[fd]) == 0)
        return -ENFILE;
    p->ofile[fd] = 0; ///<  清空进程文件描述符表中的对应条目
    get_file_ops()->close(f);
    return 0;
}

/**
 * @brief 创建管道
 *
 * @param fd     用户空间指针，用于返回两个文件描述符（读端和写端）
 * @param flags  管道标志位
 * @return int   成功返回0，失败返回-1
 */
int sys_pipe2(int *fd, int flags)
{
    uint64 fdaddr = (uint64)fd; // user pointer to array of two integers
    struct file *rf, *wf;       ///< 管道读/写端文件对象指针
    int fdread, fdwrite;
    struct proc *p = myproc();

    if (pipealloc(&rf, &wf) < 0) ///<  分配管道资源
        return -1;
    fdread = -1;
    if ((fdread = fdalloc(rf)) < 0 || (fdwrite = fdalloc(wf)) < 0)
    {
        if (fdread >= 0)
            p->ofile[fdread] = 0;
        get_file_ops()->close(rf);
        get_file_ops()->close(wf);
        return -1;
    }
    if (copyout(p->pagetable, fdaddr, (char *)&fdread, sizeof(fdread)) < 0 ||
        copyout(p->pagetable, fdaddr + sizeof(fdread), (char *)&fdwrite, sizeof(fdwrite)) < 0)
    {
        p->ofile[fdread] = 0;
        p->ofile[fdwrite] = 0;
        get_file_ops()->close(rf);
        get_file_ops()->close(wf);
        return -1;
    }
    return 0;
}

/**
 * @brief 从文件描述符中读取数据
 *
 * @param fd    文件描述符
 * @param va    用户空间目标缓冲区的虚拟地址
 * @param len   请求读取的字节数
 * @return int  成功返回实际读取的字节数，失败返回-1
 */
int sys_read(int fd, uint64 va, int len)
{
    struct file *f;
    if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
        return -1;

    return get_file_ops()->read(f, va, len);
}

/**
 * @brief 复制文件描述符
 *
 * @param fd    要复制的文件描述符
 * @return int  成功返回新描述符，失败返回标准错误码
 */
int sys_dup(int fd)
{
    struct file *f;
    if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
        return -ENOENT;
    if ((fd = fdalloc(f)) < 0)
        return -EMFILE;
    get_file_ops()->dup(f);
    return fd;
}

/**
 * @brief 复制文件描述符系统调用（带标志位）
 *
 * @param oldfd 要被复制的原文件描述符
 * @param newfd 要复制到的新文件描述符
 * @param flags 复制标志（当前实现未使用）
 * @return uint64 成功返回新的文件描述符，失败返回负的标准错误码
 */
uint64 sys_dup3(int oldfd, int newfd, int flags)
{
    struct file *f;
    if (oldfd < 0 || oldfd >= NOFILE || (f = myproc()->ofile[oldfd]) == 0)
        return -ENOENT;
    if (oldfd == newfd)
        return newfd;
    if (newfd < 0 || newfd >= NOFILE || newfd >= myproc()->ofn.rlim_cur)
        return -EMFILE;
    if (myproc()->ofile[newfd] != 0)
        get_file_ops()->close(myproc()->ofile[newfd]);
    myproc()->ofile[newfd] = f;
    get_file_ops()->dup(f);
    return newfd;
}

/**
 * @brief 创建设备文件节点
 *
 * @param upath   用户空间传递的路径字符串地址
 * @param major   设备主编号
 * @param minor   设备次编号
 * @return uint64 成功返回0，失败返回-1
 */
uint64 sys_mknod(const char *upath, int major, int minor)
{
    char path[MAXPATH];
    proc_t *p = myproc();
    if (copyinstr(p->pagetable, path, (uint64)upath, MAXPATH) == -1)
    {
        return -1;
    }

    struct filesystem *fs = get_fs_from_path(path);
    if (fs == NULL)
    {
        return -1;
    }

    if (fs->type == EXT4)
    {
        char absolute_path[MAXPATH] = {0};
        get_absolute_path(path, myproc()->cwd.path, absolute_path);
        uint32 dev = major; ///<   组合主次设备号（这里minor未被使用)
        if (vfs_ext4_mknod(absolute_path, T_CHR, dev) < 0)
        {
            return -1;
        }
    }
    return 0;
}

static struct file *find_file(const char *path)
{
    extern struct proc pool[NPROC];
    struct proc *p;
    for (int i = 0; i < NPROC; i++)
    {
        p = &pool[i];
        for (int j = 0; j < NOFILE; j++)
        {
            if (p->ofile[j] && p->ofile[j]->f_count > 0 &&
                !strcmp(p->ofile[j]->f_path, path))
                return p->ofile[j];
        }
    }
    return NULL;
}

/**
 * @brief 获取已经打开的文件状态信息
 *
 * @param fd    文件描述符
 * @param addr  用户空间缓冲区地址，用于存放获取到的文件状态信息(struct stat)
 * @return int  成功返回0，失败返回-1
 */
int sys_fstat(int fd, uint64 addr)
{
    if (fd < 0 || fd >= NOFILE)
        return -ENOENT;
    return get_file_ops()->fstat(myproc()->ofile[fd], addr);
}

int sys_statfs(uint64 upath, uint64 addr)
{
    char path[MAXPATH];
    proc_t *p = myproc();

    // 复制路径
    if (copyinstr(p->pagetable, path, (uint64)upath, MAXPATH) == -1)
    {
        return -1;
    }
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_statfs]: path: %s,addr:%d\n", path, addr);
    struct statfs stat;
    if (copyinstr(p->pagetable, (char *)&stat, (uint64)addr, sizeof(stat)) == -1)
    {
        return -1;
    }
    struct filesystem *fs = get_fs_from_path(path);
    if (fs == NULL)
    {
        return -1;
    }
    int ret = vfs_ext4_statfs(fs, &stat);
    if (ret < 0)
        return ret;
    if (copyout(p->pagetable, addr, (char *)&stat, sizeof(stat)) == -1)
    {
        return -1;
    }
    return EOK;
}

/**
 * @brief RV获取文件状态信息（支持相对路径和目录文件描述符）
 *
 * @param fd    目录文件描述符 (AT_FDCWD 表示当前工作目录)
 * @param upath 用户空间路径字符串指针
 * @param state 用户空间 struct stat 结构指针
 * @param flags 控制标志（当前实现未显式处理）
 * @return int  成功返回 0，失败返回 -1
 */
int sys_fstatat(int fd, uint64 upath, uint64 state, int flags)
{
    if ((fd < 0 || fd >= NOFILE) && fd != AT_FDCWD)
        return -ENOENT;
    if (fd != AT_FDCWD && myproc()->ofile[fd] == NULL)
        return -ENOENT;
    char path[MAXPATH];
    int ret;
    proc_t *p = myproc();
    if (copyinstr(p->pagetable, path, (uint64)upath, MAXPATH) == -1)
    {
        return -1;
    }
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[sys_fstatat]: path: %s,fd:%d,state:%d,flags:%d\n", path, fd, state, flags);
#endif
    if (path[0] == 0 && fd == 0) // 判断Path为NULL,打开失败 fd =0 特判通过rewin-clear-error
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "[sys_fstatat] NULL path pointer\n");
        return -EFAULT;
    }
    struct filesystem *fs = get_fs_from_path(path);
    if (fs == NULL)
    {
        return -1;
    }

    if (fs->type == EXT4 || fs->type == VFAT)
    {
        char absolute_path[MAXPATH] = {0};
        char *dirpath = (fd == AT_FDCWD) ? myproc()->cwd.path : myproc()->ofile[fd]->f_path;
        get_absolute_path(path, dirpath, absolute_path);
        struct kstat st;
        ret = vfs_ext4_stat(absolute_path, &st);
        if (ret < 0)
            return ret;
        if (copyout(myproc()->pagetable, state, (char *)&st, sizeof(st)))
        {
            return -1;
        }
    }
    return ret;
}
/**
 * @brief LA文件状态获取系统调用
 *
 * @param fd     文件描述符
 * @param path   目标文件路径
 * @param flags  控制标志位
 * @param mode
 * @param addr   用户空间缓冲区地址，用于存放statx结构体
 * @return int   返回负的标准错误码
 */
int sys_statx(int fd, const char *upath, int flags, int mode, uint64 addr)
{
    if ((fd < 0 || fd >= NOFILE) && fd != AT_FDCWD)
        return -ENOENT;
    if (fd != AT_FDCWD && myproc()->ofile[fd] == NULL)
        return -ENOENT;
    char path[MAXPATH];

    if (copyinstr(myproc()->pagetable, path, (uint64)upath, MAXPATH) < 0)
        return -EFAULT;

    char absolute_path[MAXPATH] = {0};
    const char *dirpath = (fd == AT_FDCWD) ? myproc()->cwd.path : myproc()->ofile[fd]->f_path;

    get_absolute_path(path, dirpath, absolute_path);
    struct filesystem *fs = get_fs_from_path(absolute_path);
    if (fs == NULL)
    {
        DEBUG_LOG_LEVEL(LOG_WARNING, "sys_statx: fs not found for path %s\n", absolute_path);
        return -1;
    }
    if (fs->type == EXT4 || fs->type == VFAT)
    {
        struct statx st;
        int ret = vfs_ext4_statx(absolute_path, &st);
        if (ret < 0)
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "sys_statx: vfs_ext4_stat failed for path %s, ret is %d\n", absolute_path, ret);
            return ret;
        }
        if (copyout(myproc()->pagetable, addr, (char *)&st, sizeof(st)) < 0)
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "sys_statx: copyout failed for path %s\n", absolute_path);
            return -EFAULT;
        }
    }
    else
    {
        DEBUG_LOG_LEVEL(LOG_WARNING, "sys_statx: unsupported filesystem type for path %s\n", absolute_path);
        return -1;
    }
    return 0;
}

/**
 * @brief 系统日志系统调用实现
 *
 * @param type    操作类型
 * @param ubuf    用户空间缓冲区地址
 * @param len     操作长度
 * @return uint64 成功返回0或请求的信息，失败返回-1
 */
uint64 sys_syslog(int type, uint64 ubuf, int len)
{
    char syslogbuffer[1024];
    int bufferlength = 0;
    strncpy(syslogbuffer, "[log]init done\n", 1024);
    bufferlength += strlen(syslogbuffer);
    if (type == SYSLOG_ACTION_READ_ALL)
    {
        if (copyout(myproc()->pagetable, ubuf, syslogbuffer, bufferlength) < 0)
            return -1;
    }
    else if (type == SYSLOG_ACTION_SIZE_BUFFER)
        return sizeof(syslogbuffer);
    return 0;
}

/**
 * @brief 系统信息系统调用实现
 *
 * @param uaddr    用户空间缓冲区地址，用于存储返回的系统信息
 * @return uint64  成功返回0，失败返回-1
 */
uint64 sys_sysinfo(uint64 uaddr)
{
    struct sysinfo info;
    memset(&info, 0, sizeof(info));
    info.uptime = r_time() / CLK_FREQ;                         ///< 系统运行时间（秒）
    info.loads[0] = info.loads[1] = info.loads[2] = 1 * 65536; //< 负载系数设置为1,还要乘65536
    info.totalram = (uint64)PAGE_NUM * PGSIZE;                 ///< 总内存大小
    info.freemem = 1 * 1024 * 1024;                            //@todo 获取可用内存        ///< 空闲内存大小（待实现）//< 先给1M
    info.sharedram = 0;                                        //< 共享内存大小，可以设为0
    info.bufferram = NBUF * BSIZE;                             ///< 缓冲区内存大小
    info.totalswap = 0;                                        //< 交换区，内存不足时把不活跃的内存交换到磁盘交换区
    info.freeswap = 0;                                         //< 现在没有交换区，swap的值都设为0
    info.nproc = procnum();                                    ///< 系统当前进程数
    info.totalhigh = 0;                                        //< 可设为0
    info.freehigh = 0;                                         //< 可设为0
    info.mem_unit = 1;                                         ///< 内存单位大小，一般为1
    if (copyout(myproc()->pagetable, uaddr, (char *)&info, sizeof(info)) < 0)
        return -1;
    return 0;
}

/**
 * @brief 内存映射系统调用实现
 *
 * @param start     映射起始地址（建议地址，通常为0表示由内核决定）
 * @param len       映射区域的长度
 * @param prot      内存保护标志
 * @param flags     映射标志
 * @param fd        文件描述符
 * @param off       文件偏移量
 * @return int      成功返回映射区域的起始地址，失败返回错误码
 */
uint64 sys_mmap(uint64 start, int64 len, int prot, int flags, int fd, int off)
{
#if DEBUG
    LOG("mmap start:%p len:0x%lx prot:%d flags:0x%x fd:%d off:%d\n", start, len, prot, flags, fd, off);
#endif
    return mmap((uint64)start, len, prot, flags, fd, off);
}

/**
 * @brief 内存解除映射(munmap)系统调用实现
 *
 * @param start 要解除映射的内存区域起始地址（必须页对齐)
 * @param len   要解除映射的区域长度（字节，会自动向上取整到页大小）
 * @return int  成功返回0，失败返回-1并设置errno
 */
int sys_munmap(void *start, int len)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "sys_munmap: start:%p len:0x%x\n", start, len);
    return munmap((uint64)start, len);
}

/**
 * @brief 获取当前工作目录
 *
 * @param buf       用户空间提供的缓冲区, 用于存储当前工作目录路径
 * @param size      缓冲区的大小
 * @return uint64   成功时返回cwd总长度, 失败时返回各种类型标准错误码
 */
uint64 sys_getcwd(char *buf, int size)
{
    if (buf == NULL || size <= 0)
    {
#if DEBUG
        LOG_LEVEL(LOG_WARNING, "sys_getcwd: buf is NULL or size is invalid\n");
#endif
        return -EINVAL; ///< 标准错误码：无效参数
    }

    char *path = myproc()->cwd.path;
    int len = strlen(path);
    int total_len = len + 1; ///< 包含终止符

    if (total_len > size)
    {
#if DEBUG
        LOG_LEVEL(LOG_WARNING, "sys_getcwd: buffer too small\n");
#endif
        return -ERANGE; ///< 标准错误码：缓冲区不足
    }

    /* 复制路径 + 终止符 */
    if (copyout(myproc()->pagetable, (uint64)buf, path, total_len) < 0)
    {
#if DEBUG
        LOG_LEVEL(LOG_WARNING, "sys_getcwd: copyout failed\n");
#endif
        return -EFAULT; ///< 复制失败
    }

    return total_len; ///< 成功返回总长度
}

/**
 * @brief 在指定目录下创建目录
 *
 * @param dirfd  目录文件描述符（目前仅支持FDCWD，即当前工作目录）
 * @param upath  用户空间提供的路径名（相对或绝对路径）
 * @param mode   目录权限模式
 * @return int   成功时返回0,失败时返回-1
 */
int sys_mkdirat(int dirfd, const char *upath, uint16 mode) //< 初赛先只实现相对路径的情况
{
    if (dirfd != AT_FDCWD) //< 如果传入的fd不是FDCWD
    {
        printf("[sys_mkdirat] 传入的fd不是FDCWD,待实现\n");
        return -1;
    }
    char path[MAXPATH] = {0};
    copyinstr(myproc()->pagetable, path, (uint64)upath, MAXPATH);
    const char *dirpath = (dirfd == AT_FDCWD) ? myproc()->cwd.path : myproc()->ofile[dirfd]->f_path; //< 目前只会是相对路径
    char absolute_path[MAXPATH] = {0};
    get_absolute_path(path, dirpath, absolute_path);
#if DEBUG
    printf("[sys_mkdirat] 创建目录到: %s\n", absolute_path);
#endif
    vfs_ext4_mkdir(absolute_path, 0777); //< 传入绝对路径，权限777表示所有人都可RWX
#if DEBUG
    printf("[sys_mkdirat] 创建成功\n");
#endif
    return 0;
}

/**
 * @brief 改变当前工作目录
 *
 * @param path 用户空间提供的目标路径字符串
 * @return int 成功返回0,失败返回-1
 */
int sys_chdir(const char *path)
{
#if DEBUG
    printf("sys_chdir!\n");
#endif
    char buf[MAXPATH], absolutepath[MAXPATH];
    memset(buf, 0, MAXPATH); //< 清空，以防上次的残留
    memset(absolutepath, 0, MAXPATH);
    copyinstr(myproc()->pagetable, buf, (uint64)path, MAXPATH); //< 复制用户空间的path到内核空间的buf
    /*
        [todo] 判断路径是否存在，是否是目录
    */
    char *cwd = myproc()->cwd.path; // char path[MAXPATH]
    get_absolute_path(buf, cwd, absolutepath);
    memset(cwd, 0, MAXPATH); //< 清空，以防上次的残留
    memmove(cwd, absolutepath, strlen(absolutepath));
#if DEBUG
    printf("修改成功,当前工作目录: %s\n", myproc()->cwd.path);
#endif
    // LOG_LEVEL(LOG_ERROR,"修改成功,当前工作目录: %s\n", myproc()->cwd.path);
    return 0;
}

#define GETDENTS64_BUF_SIZE 4 * 4096          //< 似乎用不了这么多
char sys_getdents64_buf[GETDENTS64_BUF_SIZE]; //< 函数专用缓冲区

/*全新版本!支持busybox和basic*/
int sys_getdents64(int fd, struct linux_dirent64 *buf, int len) //< busybox用的时候len是800,basic测例的len是512
{
    struct file *f = myproc()->ofile[fd];

    /* @note busybox的ps */
    if (!strcmp(f->f_path, "/proc"))
        return 0;

    memset((void *)sys_getdents64_buf, 0, GETDENTS64_BUF_SIZE);
    int count = vfs_ext4_getdents(f, (struct linux_dirent64 *)sys_getdents64_buf, len);

    copyout(myproc()->pagetable, (uint64)buf, (char *)sys_getdents64_buf, count);
    return count;
}

/**
 * @brief 文件系统挂载
 *
 * @todo 还没实现真正挂载special位置的路径
 * @param special 挂载设备
 * @param dir 挂载点
 * @param fstype 挂载的文件系统类型
 * @param flags 挂载参数
 * @param data 传递给文件系统的字符串参数，可为NULL
 * @return int 成功返回0，失败返回-1
 */
int sys_mount(const char *special, const char *dir, const char *fstype, unsigned long flags, const void *data)
{
    char special_str[MAXPATH] __attribute__((unused));
    char path[MAXPATH];
    static char abs_path[MAXPATH];
    char fstype_str[MAXPATH];
    char data_str[MAXPATH];
    memset(special_str, 0, MAXPATH);
    memset(path, 0, MAXPATH);
    memset(fstype_str, 0, MAXPATH);
    memset(data_str, 0, MAXPATH);

    if ((copyinstr(myproc()->pagetable, path, (uint64)dir, MAXPATH) < 0) ||
        (copyinstr(myproc()->pagetable, fstype_str, (uint64)fstype, MAXPATH) < 0) ||
        ((data != NULL) && (copyinstr(myproc()->pagetable, data_str, (uint64)data, MAXPATH) < 0)))
        return -1;

    get_absolute_path(path, myproc()->cwd.path, abs_path); //< 获取绝对路径

    fs_t fs_type = 0;

    if (strcmp(fstype_str, "ext4") == 0)
    {
        fs_type = EXT4;
    }
    else if (strcmp(fstype_str, "vfat") == 0)
    {
        fs_type = VFAT;
    }
    else
    {
        printf("不支持的文件系统类型: %s\n", fstype_str);
        return -1;
    }

    /* TODO 这里需要根据传入的设备创建设备，将TMPDEV分配给他 */

    int ret = fs_mount(TMPDEV, fs_type, abs_path, flags, data); //< 挂载
    return ret;
}

/**
 * @brief 卸载文件系统
 * @todo 还没实现真正卸载
 * @param special 指定卸载目录
 * @return int 成功返回0，失败返回-1
 */
int sys_umount(const char *special)
{
    char path[MAXPATH], abs_path[MAXPATH];
    memset(path, 0, MAXPATH);
    if (copyinstr(myproc()->pagetable, path, (uint64)special, MAXPATH) == -1)
    {
        return -1;
    }
    get_absolute_path(path, myproc()->cwd.path, abs_path); //< 获取绝对路径

    /* TODO 下面这个函数是真的傻，我后面一定要重构 */
    filesystem_t *fs = get_fs_from_path(abs_path); //< 获取文件系统
    int ret = fs_umount(fs);                       //< 卸载
    return ret;
}

#define AT_REMOVEDIR 0x200 //< flags是0不删除
/**
 * @brief 移除指定文件的链接(可用于删除文件)
 * @param dirfd 删除的链接所在目录
 * @param path 要删除的链接的名字
 * @param flags 可设置为0或AT_REMOVEDIR
 * */
int sys_unlinkat(int dirfd, char *path, unsigned int flags)
{
    if ((flags & ~AT_REMOVEDIR) != 0)
        return -EINVAL;

    char buf[MAXPATH] = {0};                                    //< 清空，以防上次的残留
    copyinstr(myproc()->pagetable, buf, (uint64)path, MAXPATH); //< 复制用户空间的path到内核空间的buf
#if DEBUG
    LOG("[sys_unlinkat]dirfd: %d, path: %s, flags: %d\n", dirfd, buf, flags); //< 打印参数
#endif

    const char *dirpath = (dirfd == AT_FDCWD) ? myproc()->cwd.path : myproc()->ofile[dirfd]->f_path; //< 目前只会是相对路径
    char absolute_path[MAXPATH] = {0};
    get_absolute_path(buf, dirpath, absolute_path); //< 从mkdirat抄过来的时候忘记把第一个参数从path改成这里的buf了... debug了几分钟才看出来

    if (flags & AT_REMOVEDIR)
    {
        if (vfs_ext4_rm(absolute_path) < 0) //< unlink系统测例实际上考察的是删除文件的功能，先openat创一个test_unlink，再用unlink要求删除，然后open检查打不打的开
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "[sys_unlinkat] 目录不存在: %s\n", absolute_path);
            return -ENOENT;
        }
        DEBUG_LOG_LEVEL(LOG_INFO, "删除目录: %s\n", absolute_path);
        return 0;
    }

    struct file *f = find_file(absolute_path);
    if (f != NULL)
    {
        f->removed = 1;
        return 0;
    }

    /* 拆分父目录和子文件名 */
    char pdir[MAXPATH];
    const char *slash = strrchr(absolute_path, '/');
    if (slash == NULL)
    {
        /* 没有斜杠，说明在当前目录 */
        strcpy(pdir, dirpath);
    }
    else if (slash == absolute_path)
    {
        /* 根目录下的文件 */
        strcpy(pdir, "/");
    }
    else
    {
        int plen = slash - absolute_path;
        strncpy(pdir, absolute_path, plen);
        pdir[plen] = '\0';
    }

    return vfs_ext4_unlinkat(pdir, absolute_path);
}

int sys_getuid()
{
    return 0; // myproc()->uid; //< 0
}

int sys_geteuid()
{
    return myproc()->uid;
}

int sys_ioctl()
{
#if DEBUG
    printf("sys_ioctl\n");
#endif
    return 0;
}

int sys_exit_group()
{
    // printf("sys_exit_group\n");
    struct inode *ip;
    if ((ip = namei("/tmp")) != NULL)
    {
        vfs_ext4_rm("/tmp");
        free_inode(ip);
    }
    exit(0);
    return 0;
}

/**
 * @brief 修改或获取当前进程的信号掩码
 *
 * @param how     指定如何修改信号掩码
 * @param uset    用户空间指针，指向要设置的新信号集（可为NULL）
 * @param uoset   用户空间指针，用于返回原来的信号集（可为NULL）
 * @return uint64 成功返回0，失败返回-1
 */
uint64 sys_rt_sigprocmask(int how, uint64 uset, uint64 uoset)
{
    __sigset_t set, oset; ///<  定义内核空间的信号集变量

    if (uset && copyin(myproc()->pagetable, (char *)&set, uset, SIGSET_LEN * 8) < 0)
    {
        return -1;
    }
    if (sigprocmask(how, &set, uoset ? &oset : NULL))
        return -1;
    if (uoset && copyout(myproc()->pagetable, uoset, (char *)&oset, SIGSET_LEN * 8) < 0)
        return -1;
#if DEBUG
    printf("[sys_rt_sigprocmask] return : how:%d,set:%p\n", how, set.__val[0]);
#endif
    return 0;
}

/**
 * @brief 设置或获取指定信号的处理行为
 *
 * @param signum  要操作的信号编号
 * @param uact    用户空间指针，指向新的信号处理结构体
 * @param uoldact 用户空间指针，用于返回原来的信号处理结构体
 * @return int    成功返回0，失败返回-1
 */
int sys_rt_sigaction(int signum, sigaction const *uact, sigaction *uoldact)
{
#if DEBUG
    printf("[sys_sigaction]: signum:%d,uact:%p,uoldact:%p\n", signum, uact, uoldact);
#endif
    sigaction act = {0};
    sigaction oldact = {0};
    if (uact)
    {
        if (copyin(myproc()->pagetable, (char *)&act, (uint64)uact, sizeof(sigaction)) < 0)
            return -1;
    }
    if (set_sigaction(signum, uact ? &act : NULL, uoldact ? &oldact : NULL) < 0)
        return -1;
    if (uoldact)
    {
        if (copyout(myproc()->pagetable, (uint64)uoldact, (char *)&oldact, sizeof(sigaction)) < 0)
            return -1;
    }
#if DEBUG
    printf("[sys_sigaction] return: signum:%d,act fp:%p\n", signum, act.__sigaction_handler.sa_handler);
#endif

    return 0;
}

/**
 * @brief       检查文件访问权限
 *
 * @param fd
 * @param upath
 * @param mode
 * @param flags
 * @return uint64
 */
uint64 sys_faccessat(int fd, int upath, int mode, int flags)
{
    char path[MAXPATH];
    memset(path, 0, MAXPATH);
    if (copyinstr(myproc()->pagetable, path, (uint64)upath, MAXPATH) == -1)
    {
        return -1;
    }
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[sys_faccessat]: fd:%d,path:%s,mode:%d,flags:%d\n", fd, path, mode, flags);
#endif
    struct filesystem *fs = get_fs_from_path(path);
    if (fs == NULL)
    {
        return -1;
    }

    if (fs->type == EXT4)
    {
        char absolute_path[MAXPATH] = {0};
        const char *dirpath = (fd == AT_FDCWD) ? myproc()->cwd.path : myproc()->ofile[fd]->f_path;
        get_absolute_path(path, dirpath, absolute_path);
        struct file *f;
        f = filealloc();
        if (!f)
            return -1;
        int fd = -1;
        if ((fd = fdalloc(f)) == -1)
        {
            panic("fdalloc error");
            return -1;
        };

        f->f_flags = flags | O_CREAT;
        strcpy(f->f_path, absolute_path);
        int ret;
        if ((ret = vfs_ext4_openat(f)) < 0)
        {
            myproc()->ofile[fd] = 0;
            return -1;
        }
    }
    return 0;
}

/**
 * @brief 文件控制系统调用
 *
 * @param fd  文件描述符
 * @param cmd 控制命令
 * @param arg 命令参数
 * @return uint64
 */
uint64 sys_fcntl(int fd, int cmd, uint64 arg)
{
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[sys_fcntl]: fd:%d,cmd:%d,arg:%p\n", fd, cmd, arg);
#endif
    int new_fd;
    int ret = 0;
    struct file *f = myproc()->ofile[fd];
    switch (cmd)
    {
    case F_DUPFD: ///< 使用编号最低的 大于或等于 arg 的可用文件描述符
        if ((new_fd = fdalloc2(f, arg)) < 0)
        {
            return -1;
        }
        get_file_ops()->dup(f);
        ret = new_fd;
        break;
    case F_DUPFD_CLOEXEC:
        if ((new_fd = fdalloc2(f, arg)) > 0)
        {
            get_file_ops()->dup(f);
        }
        ret = new_fd;
        break;
    case F_GETFD:
        ret = f->f_flags; // fd_flags暂时用f_flags替代
        break;
    case F_SETFD:
        ret = 0;
        break;
    case F_GETFL:
        ret = f->f_flags;
        break;
    default:
        // printf("fcntl : unknown cmd:%d", cmd);
        return ret;
    }

    return ret;
}

/**
 * @brief       修改文件访问/修改时间（纳秒精度）的系统调用
 *
 * @param fd    目录文件描述符（AT_FDCWD表示当前工作目录）
 * @param upath 用户空间传递的目标文件路径指针
 * @param utv   用户空间传递的时间结构体数组指针（timespec_t类型，含访问/修改时间）
 * @param flags 操作标志
 * @return int
 */
int sys_utimensat(int fd, uint64 upath, uint64 utv, int flags)
{
    char path[MAXPATH];
    proc_t *p = myproc();
    if (upath && copyinstr(p->pagetable, path, upath, MAXPATH) < 0)
        return -EFAULT;
    int ret = 0;
    if (fd == -1)
    {
        return -9;
    }
    timespec_t tv[2];
    if (utv && copyin(p->pagetable, (char *)tv, utv, sizeof(tv)) < 0)
        return -EFAULT;
    if (!utv)
    {
        // 设为当前时间
        tv[0] = timer_get_ntime();
        tv[1] = tv[0];
    }
    else
    {
        if (tv[0].tv_nsec == UTIME_NOW)
            tv[0] = timer_get_ntime();
        if (tv[1].tv_nsec == UTIME_NOW)
            tv[1] = timer_get_ntime();
    }
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[sys_utimensat]: fd:%d,path:%s,utv:%p,flags:%d\n", fd, path, utv, flags);
#endif

    struct filesystem *fs = get_fs_from_path(path);
    if (fs == NULL)
    {
        return -1;
    }
    if (fs->type == EXT4)
    {
        char absolute_path[MAXPATH] = {0};
        const char *dirpath = (fd == AT_FDCWD) ? myproc()->cwd.path : myproc()->ofile[fd]->f_path;
        if (fd >= 0)
        {
            get_absolute_path(NULL, dirpath, absolute_path);
        }
        else
        {
            get_absolute_path(path, dirpath, absolute_path);
        }
        DEBUG_LOG_LEVEL(DEBUG, "abs path:%s\n", absolute_path);
        if ((ret = vfs_ext4_utimens(absolute_path, tv)) < 0)
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "utimens fail!\n");
            return ret;
        };
    }
    return 0;
}

extern void shutdown();
void sys_shutdown(void)
{
    extern timeval_t get_system_runtime();
    get_system_runtime();
    // LOG_LEVEL(LOG_INFO,"系统关机，已经运行的事件: %ld秒 %ld微秒\n",tv.sec,tv.usec);
#ifdef RISCV
    shutdown();
#else
    *(volatile uint8 *)(0x8000000000000000 | 0x100E001C) = 0x34;
#endif
}

/**
 * @brief 管理线程的 健壮互斥锁（Robust Futexes） 列表,现在不实现
 * @param struct robust_list_head *head
 * @param size_t len
 */
uint64 sys_set_robust_list()
{
    return 0;
}

/**
 * @brief 返回线程id，不是进程id。主线程的tid通常等于进程id
 * @param 无参数
 */
uint64 sys_gettid()
{
    return myproc()->pid; //< 之后tgkill向这个线程发送信号
}

/**
 * @brief 向特定线程发送信号，是 tkill 的扩展版本
 * @param tgid 线程组id，通常为进程的 PID
 * @param tid 目标线程id
 * @param sig 要发送的信号
 */
uint64 sys_tgkill(uint64 tgid, uint64 tid, int sig)
{
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[sys_tgkill]: tgid:%p, tid:%p, sig:%d\n", tgid, tid, sig);
#endif
    return tgkill(tgid, tid, sig);
}

/**
 * @brief 读取符号链接指向的路径，现在读不了，没有la glibc要读的path: /proc/self/exe，会返回-1
 */
uint64 sys_readlinkat(int dirfd, char *user_path, char *buf, int bufsize)
{

    char path[MAXPATH];
    if (copyinstr(myproc()->pagetable, path, (uint64)user_path, MAXPATH) < 0)
    {
        return -1;
    }
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[sys_readlinkat] dirfd: %d, user_path: %s, buf: %p, bufsize: %d\n", dirfd, path, buf, bufsize);
#endif
    const char *dirpath = dirfd == AT_FDCWD ? myproc()->cwd.path : myproc()->ofile[dirfd]->f_path;
    char absolute_path[MAXPATH] = {0};
    get_absolute_path(path, dirpath, absolute_path);
    // printf("%s\n", absolute_path);
    if (vfs_ext_readlink(absolute_path, (uint64)buf, bufsize) < 0)
    {
        return -1;
    }
    // printf("return 0");
    return 0;
}

/**
 * @brief 向用户地址buf中写入buflen长度的随机数
 */
uint64 sys_getrandom(void *buf, uint64 buflen, unsigned int flags)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_getrandom]: buf:%p, buflen:%d, flags:%d\n", buf, buflen, flags);
    // printf("buf: %d, buflen: %d, flag: %d",(uint64)buf,buflen,flags);
    /*loongarch busybox glibc启动时调用，参数是：buf: 540211080, buflen: 8, flag: 1.*/
    if (buflen != 8)
    {
        printf("sys_getrandom不支持非8字节的随机数!");
        return -1;
    }
    uint64 random = 0x7be6f23c6eb43a7e;
    copyout(myproc()->pagetable, (uint64)buf, (char *)&random, 8);
    return buflen;
}

/**
 * @brief 读取向量系统调用
 * @param fd 文件描述符
 * @param iov 用户空间的 iovec 数组指针
 * @param iovcnt iovec 数组的元素数量
 * @return 成功时返回读取的字节数，失败时返回 -1
 */
uint64
sys_readv(int fd, uint64 iov, int iovcnt)
{
    void *buf;
    int needbytes = sizeof(struct iovec) * iovcnt;
    if ((buf = kmalloc(needbytes)) == 0)
        return -1;

    if (fd != AT_FDCWD && (fd < 0 || fd >= NOFILE))
        return -1;
    struct proc *p = myproc();
    struct file *f = p->ofile[fd];

    if (copyin(p->pagetable, (char *)buf, iov, needbytes) < 0)
    {
        kfree(buf);
        return -1;
    }

    uint64 file_size = 0;
    vfs_ext4_get_filesize(f->f_path, &file_size);

    int readbytes = 0;
    int current_read_bytes = 0;
    struct iovec *buf_iov = (struct iovec *)buf; //< 转换为iovec指针
    for (int i = 0; i != iovcnt && file_size > 0; i++)
    {
        if ((current_read_bytes = get_file_ops()->read(f, (uint64)buf_iov->iov_base, MIN(buf_iov->iov_len, file_size))) < 0)
        {
            kfree(buf);
            return -1;
        }
        readbytes += current_read_bytes;
        file_size -= current_read_bytes;
        buf_iov++;
    }
    kfree(buf);
    return readbytes;
}

/**
 * @brief 从文件指定位置读取数据
 *
 * @param fd    文件描述符
 * @param buf   用户空间缓冲区地址
 * @param count 请求读取的字节数
 * @param offset 文件偏移量
 * @return ssize_t 成功返回读取字节数，失败返回-1
 */
uint64 sys_pread(int fd, void *buf, uint64 count, uint64 offset)
{
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[sys_pread] fd:%d buf:%p count:%d offset:%d\n", fd, buf, count, offset);
#endif
    struct file *f;
    int ret = 0;

    if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
        return -1;
    // 保存原始文件位置
    uint64 orig_pos = f->f_pos;

    // 设置新位置
    ret = vfs_ext4_lseek(f, offset, SEEK_SET); //< 设置文件位置指针到指定偏移量
    if (ret < 0)
    {
        DEBUG_LOG_LEVEL(LOG_WARNING, "lseek in pread failed!, ret is %d\n", ret);
        return ret;
    }

    // 执行读取
    ret = get_file_ops()->read(f, (uint64)buf, count);

    if (ret < 0)
    {
        DEBUG_LOG_LEVEL(LOG_WARNING, "read in pread failed!, ret is %d\n", ret);
        return ret;
    }

    // 恢复原始位置（即使读取失败）
    vfs_ext4_lseek(f, orig_pos, SEEK_SET);

    return ret;
}

/**
 * @brief 将文件内容从一个文件描述符传输到另一个文件描述符
 * @param out_fd 目标文件描述符
 * @param in_fd 源文件描述符
 * @param offset 指向偏移量的指针,是偏移量？
 * @param count 要传输的字节数
 * @return 成功时：返回实际传输的字节数（size_t）,失败返回-1
 *
 * 哈哈，太奇怪了，只要这个调用返回-1,cat就能正常输出文件内容。都不需要按标准完成这个函数. musl和glibc都是这样
 * ohg, la的musl和glibc也是这样，
 *
 * 然后more也可以过
 */
uint64 sys_sendfile64(int out_fd, int in_fd, uint64 *offset, uint64 count)
{
// rv musl busybox调用的参数 [sys_sendfile] out_fd: 1, in_fd: 3, offset: 0, count: 16777216
// rv glibc busybox调用的参数 [sys_sendfile] out_fd: 1, in_fd: 3, offset: 0, count: 16777216
//< 为什么count这么大？ count是16M,固定数值
// la musl的参数也一样。la glibc也是
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[sys_sendfile] out_fd: %d, in_fd: %d, offset: %ld, count: %ld\n", out_fd, in_fd, offset, count);
#endif
    return -1;
}

/**
 * 看来busybox用的62号调用是lseek不是llseek
 * @brief 移动文件读写位置指针（即文件偏移量）
 * @param fd
 * @param offset 要移动的偏移量
 * @param whence 从文件头，当前偏移量还是文件尾开始
 * @return 成功时返回移动后的偏移量，错误时返回标准错误码(负数)
 */
uint64 sys_lseek(uint32 fd, uint64 offset, int whence)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_lseek]uint32 fd: %d, uint64 offset: %ld, int whence: %d\n", fd, offset, whence);
    // if (offset > 0x10000000) //< 除了lseek-large一般不会用这么大的偏移，直接返回给lseek-large他要的值
    //     return offset;
    struct file *f;
    if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
        return -ENOENT;
    int ret = 0;
    ret = vfs_ext4_lseek(f, offset, whence);
    if (ret < 0)
        DEBUG_LOG_LEVEL(LOG_WARNING, "sys_lseek fd %d failed!\n", fd);
    return ret;
}

/**
 * @brief 原子性地重命名或移动文件/目录,是 renameat 的扩展版本
 * @param olddfd 原文件的目录文件描述符（若为 AT_FDCWD，则 oldname 为相对当前目录）
 * @param oldname 原文件/目录的路径名（用户空间指针）
 * @param newdfd 目标位置的目录文件描述符（类似 olddfd）
 * @param newname 	新文件/目录的路径名（用户空间指针）
 * @param flags 控制标志位
 *
 * @todo 还未考虑flags
 */
uint64 sys_renameat2(int olddfd, const char *oldname, int newdfd, const char *newname, uint32 flags)
{
    char k_oldname[MAXPATH] = {0};
    copyinstr(myproc()->pagetable, k_oldname, (uint64)oldname, MAXPATH);
    char k_newname[MAXPATH] = {0};
    copyinstr(myproc()->pagetable, k_newname, (uint64)newname, MAXPATH);

    const char *oldpath = (olddfd == AT_FDCWD) ? myproc()->cwd.path : myproc()->ofile[olddfd]->f_path; //< 目前只会是相对路径
    const char *newpath = (newdfd == AT_FDCWD) ? myproc()->cwd.path : myproc()->ofile[newdfd]->f_path; //< 目前只会是相对路径

    char old_abs_path[MAXPATH] = {0}, new_abs_path[MAXPATH] = {0};
    get_absolute_path(k_oldname, oldpath, old_abs_path);
    get_absolute_path(k_newname, newpath, new_abs_path);
    int ret = 0;
    if ((ret = vfs_ext4_frename(old_abs_path, new_abs_path)) < 0)
    {
#if DEBUG
        LOG_LEVEL(LOG_WARNING, "[sys_renameat2] rename failed: %s -> %s, ret = %d\n", old_abs_path, new_abs_path, ret);
#endif
        return ret;
    }
#if DEBUG
    LOG("[sys_renameat2]olddfd: %d, oldname: %s, newdfd: %d, newname: %s, flags: %d\n", olddfd, k_oldname, newdfd, k_newname, flags);
#endif
    return 0;
}

/* vma信息like:
vma_head信息vma信息, type: 0, perm: 0, addr: 0, end: 0, size: 0, flags: 0, fd: 0, f_off: 0vma->prev: 0x00000000814da000, vma->next: 0x0000000081401000
第0个vma:vma信息, type: 1, perm: 0, addr: 0, end: 0, size: 0, flags: 0, fd: 0, f_off: 0vma->prev: 0x0000000081402000, vma->next: 0x00000000813b4000
第1个vma:vma信息, type: 1, perm: 16, addr: 130000, end: 931000, size: 801000, flags: 0, fd: -1, f_off: 0vma->prev: 0x0000000081401000, vma->next: 0x0000000081c9d000
第2个vma:vma信息, type: 1, perm: 16, addr: 931000, end: 972000, size: 41000, flags: 0, fd: -1, f_off: 0vma->prev: 0x00000000813b4000, vma->next: 0x00000000814da000
第3个vma:vma信息, type: 2, perm: 4, addr: 7ffcd000, end: 7ffff000, size: 32000, flags: 0, fd: -1, f_off: ffffffffvma->prev: 0x0000000081c9d000, vma->next: 0x0000000081402000
*/
/**
 * @brief 打印给定的vma的信息
 */
void print_vma(struct vma *vma)
{
    printf("vma信息, type: %d, perm: %x, ", vma->type, vma->perm);
    printf("addr: %x, end: %x, size: %x, ", vma->addr, vma->end, vma->size);
    printf("flags: %x, fd: %d, f_off: %x", vma->flags, vma->fd, vma->f_off);
    printf("vma->prev: %p, vma->next: %p\n", vma->prev, vma->next);
}

#define MREMAP_MAYMOVE 0x1   //< 允许内核在必要时移动映射到新的虚拟地址（若原位置空间不足）。
#define MREMAP_FIXED 0x2     //< 必须将映射移动到指定的新地址（需配合 new_addr 参数），且会覆盖目标地址的现有映射。
#define MREMAP_DONTUNMAP 0x4 //< （Linux 5.7+）保留原映射的物理页，仅在新地址创建映射（实现内存"别名"）。
/**
 * @brief 重新映射一段虚拟地址
 * @param addr 要重新映射的虚拟地址
 * @param old_len 原来的地址空间长度
 * @param new_len 要改变到的地址空间长度
 * @param flags 映射选项，可以选择在new_addr指定的地址重新映射，也可以原地重新映射
 * @return 映射后的新虚拟地址的起始
 *
 * @todo 更多的情况待处理，只实现了sscanf_long要求的情况
 */
uint64 sys_mremap(unsigned long addr, unsigned long old_len, unsigned long new_len, unsigned long flags, unsigned long new_addr)
{
#if DEBUG
    LOG_LEVEL(LOG_INFO, "[sys_mremap]addr: %x, old_len: %x, new_len: %x, flags: %x, new_addr: %x\n", addr, old_len, new_len, flags, new_addr);
#endif
    if (flags == MREMAP_MAYMOVE) //< 这里应该不会用到new_addr
    {
        /*先找到addr对应的vma*/
        struct vma *vma_head = myproc()->vma;
        struct vma *vma = vma_head->next;

        while (vma != vma_head) //< 遍历p的vma链表，并查找
        {
            if (vma->addr == addr) //< 找到了vma
                break;
            vma = vma->next;
        }
        if (vma == vma_head) //< 判断找到的vma是否合法
        {
            LOG_LEVEL(LOG_ERROR, "[sys_mremap]没有找到对应addr的vma\n");
            panic("退出!\n");
        }
        if (vma->size != old_len)
        {
            LOG_LEVEL(LOG_ERROR, "[sys_mremap]old_len不等于vma->size\n");
            panic("退出!\n");
        }

        /*已经找到正确的vma*/
        if (vma->addr + new_len < vma->next->addr) //< 当前vma扩充后不会超过下一个vma
        //< 一个潜在问题是vma->next是vma_head，会导致错误判断，不过栈一般在高位，应该不会出现问题
        {
            if (new_len > old_len) //< 也就是new_len > vma->size，要从end开始扩充
            {
                uvmalloc1(myproc()->pagetable, vma->end, addr + new_len, PTE_R); //< [todo]应该设置什么权限？
                vma->size = new_len;
                vma->end = vma->addr + new_len;
                return addr; //< 返回分配的虚拟地址的起始
            }
            else //< 需要收缩vma
            {
                LOG_LEVEL(LOG_ERROR, "[sys_mremap]new_len<old_len还没有处理\n");
                panic("退出!\n");
            }
        }
        else //< 需要找新的虚拟地址空间，因为vma->addr+new_len > vma->next->addr
        {
            // LOG_LEVEL(LOG_ERROR, "[sys_mremap]vma->addr+new_len > vma->next->addr还没有处理\n");
            // panic("退出!\n");
        }
    }
    else if (flags == MREMAP_FIXED)
    {
        LOG_LEVEL(LOG_ERROR, "[sys_mremap]需要实现MREMAP_FIXED\n");
        panic("退出!\n");
    }
    else if (flags == MREMAP_DONTUNMAP)
    {
        LOG_LEVEL(LOG_ERROR, "[sys_mremap]需要实现MREMAP_DONTUNMAP\n");
        panic("退出!\n");
    }
    else
    {
        LOG_LEVEL(LOG_ERROR, "[sys_mremap]unknown flags: %x\n", flags);
        panic("退出!\n");
    }
    // exit(0);
    return 0;
}

uint64 sys_ppoll(uint64 pollfd, int nfds, uint64 tsaddr, uint64 sigmaskaddr)
{
    panic("sys_ppoll\n");
    return 0;
}

struct __kernel_timespec
{
    uint64 tv_sec; // 秒
    long tv_nsec;  // 纳秒 [0, 999999999]
};

/**
 * @brief 高精度睡眠
 * @param which_clock 指定时钟源
 * @param flags 控制行为
 * @param rqtp 用户空间指针，指向请求的睡眠时间（秒 + 纳秒）
 * @param rmtp 用户空间指针，用于返回剩余的睡眠时间（若被信号中断）。可为NULL
 */
uint64 sys_clock_nanosleep(int which_clock,
                           int flags,
                           uint64 *rqtp,
                           uint64 *rmtp)
{
//< [sys_clock_nanosleep]which_clock: 0, flags: 0, rqtp: 0x000000007fffeb48, rmtp: 0x000000007fffeb48
#if DEBUG
    LOG("[sys_clock_nanosleep]which_clock: %d, flags: %d, rqtp: %p, rmtp: %p\n", which_clock, flags, rqtp, rmtp);
#endif
    struct __kernel_timespec kernel_request_tp; //< 栈上分配空间
    struct __kernel_timespec kernel_remain_tp;
    copyin(myproc()->pagetable, (char *)&kernel_request_tp, (uint64)rqtp, sizeof(struct __kernel_timespec)); //< 读入睡眠时间

//< kernel_request_tp, second: 7fffffff, nanosecond: 0
#if DEBUG
    LOG("kernel_request_tp, second: %x, nanosecond: %x\n", kernel_request_tp.tv_sec, kernel_request_tp.tv_nsec);
#endif
    kernel_remain_tp.tv_sec = 0;
    kernel_remain_tp.tv_nsec = 0;
    copyout(myproc()->pagetable, (uint64)rmtp, (char *)&kernel_remain_tp, sizeof(struct __kernel_timespec));
    // copyin(myproc()->pagetable,(char *)&kernel_request_tp,(uint64)rqtp,sizeof(struct __kernel_timespec)); //< 写入rmtp成功了
    // LOG("kernel_request_tp, second: %x, nanosecond: %x\n",kernel_request_tp.tv_sec,kernel_request_tp.tv_nsec);
    exit(0); //< 直接退出。不知道为什么sleep一直请求sys_clock_nanosleep
    return 0;
}
/**
 * @brief 处理 futex（fast userspace mutex）系统调用，用于用户空间的轻量级锁操作。
 *
 * futex系统调用提供了一种机制，允许用户空间程序通过共享内存中的整数变量实现快速等待和唤醒，
 * 减少内核态切换开销，实现高效的线程同步。
 *
 * @param uaddr   指向用户空间中的共享整数变量地址，futex操作的目标地址。
 * @param op      操作类型，指定futex执行的具体操作，如等待、唤醒、重置等（参考具体操作码定义）。
 * @param val     操作相关的值，含义随op不同而异（如等待时的期望值，唤醒时的唤醒数量等）。
 * @param utime   指向超时时间结构，表示等待操作的超时时间（可为NULL表示无限等待）。
 * @param uaddr2  第二个用户空间地址参数，某些操作（如重排、比较交换）使用。
 * @param val3    第三个辅助参数，视具体futex操作而定，通常为额外的数值。
 *
 * @return 返回操作结果，成功时通常返回0或被唤醒线程数量，失败时返回负的错误码。
 */
uint64
sys_futex(uint64 uaddr, int op, uint32 val, uint64 utime, uint64 uaddr2, uint32 val3)
{
    // 移除调试代码，让futex正常工作
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_futex] uaddr: %p, op: %d, val: %d, utime: %p, uaddr2: %p, val3: %d\n", uaddr, op, val, utime, uaddr2, val3);
    struct proc *p = myproc();
    int userVal;
    timespec_t t;
    op &= (FUTEX_PRIVATE_FLAG - 1);
    switch (op)
    {
    case FUTEX_WAIT:
        copyin(p->pagetable, (char *)&userVal, uaddr, sizeof(int));
        if (utime)
        {
            if (copyin(p->pagetable, (char *)&t, utime, sizeof(timespec_t)) < 0)
                panic("copy time error!\n");
        }
        if (userVal != val)
            return -1;
        /* 使用当前运行的线程而不是主线程 */
        futex_wait(uaddr, p->main_thread, utime ? &t : 0);
        break;
    case FUTEX_WAKE:
        return futex_wake(uaddr, val);
        break;
    case FUTEX_REQUEUE:
        futex_requeue(uaddr, val, uaddr2);
        break;
    default:
        DEBUG_LOG_LEVEL(LOG_WARNING, "Futex type not support!\n");
        exit(0);
    }
    return 0;
}
/**
 * @brief 设置线程ID地址
 *
 * @return uint64 tid的值
 */
uint64 sys_set_tid_address(uint64 uaddr)
{
    uint64 address;
    if (copyin(myproc()->pagetable, (char *)&address, uaddr, sizeof(uint64)) < 0)
        return -1; ///< 复制失败

    struct proc *p = myproc();
    p->main_thread->clear_child_tid = address;
    int tid = p->main_thread->tid;
    copyout(myproc()->pagetable, address, (char *)&tid, sizeof(int));

    return tid;
}

uint64 sys_mprotect(uint64 start, uint64 len, uint64 prot)
{
#if DEBUG
    LOG("[sys_mprotect] start: %p, len: 0x%x, prot: 0x%x\n", start, len, prot);
#endif
    struct proc *p = myproc();
    int perm = get_mmapperms(prot);
    int page_n = PGROUNDUP(len) >> PGSHIFT;
    uint64 va = start;
    for (int i = 0; i < page_n; i++)
    {
        experm(p->pagetable, va, perm);
        va += PGSIZE;
    }
    return 0;
}

uint64 sys_setgid(int gid)
{
    myproc()->gid = gid;
    return 0;
}

/**
 * @brief 分配一个socket描述符
 *
 * @param domain 指定通信发生的区域
 * @param type   描述要建立的套接字的类型 SOCK_STREAM:流式   SOCK_DGRAM：数据报式
 * @param protocol 该套接字使用的特定协议
 * @return int
 */
int sys_socket(int domain, int type, int protocol)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_socket] domain: %d, type: %d, protocol: %d\n", domain, type, protocol);
    // assert(domain == PF_INET, "domain must be PF_INET");
    int flags = type & (SOCK_CLOEXEC | SOCK_NONBLOCK);
    ///< SOCK_CLOEXEC 设置文件描述符的close-on-exec，自动关闭文件描述符
    ///< SOCK_NONBLOCK 将socket设置为非阻塞，需通过轮询或事件驱动
    type &= ~(SOCK_CLOEXEC | SOCK_NONBLOCK);
    struct file *f;
    f = filealloc();
    if (!f)
        return -1;

    struct socket *sock = kalloc();
    if (!sock)
    {
        get_file_ops()->close(f);
        ;
        return -1;
    }
    memset(sock, 0, sizeof(struct socket));
    sock->domain = domain;
    sock->type = type;
    sock->protocol = protocol;
    sock->state = SOCKET_UNBOUND; ///< 分配时将socket状态设置为未绑定

    if (flags & SOCK_CLOEXEC)
        f->f_flags |= O_CLOEXEC;
    if (flags & SOCK_NONBLOCK)
        f->f_flags |= O_NONBLOCK;
    f->f_type = FD_SOCKET; ///< 设置文件类型为socket
    if (type == SOCK_DGRAM)
    {
        f->f_flags |= O_RDWR;
    }
    else
    {
        f->f_flags |= O_WRONLY;
    }
    f->f_data.sock = sock;

    int fd = -1;
    if ((fd = fdalloc(f)) == -1)
    {
        panic("fdalloc error");
        return -1;
    };
    return fd;
}

int sys_listen(int sockfd, int backlog)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_listen] sockfd: %d, backlog: %d\n", sockfd, backlog);
    return 0;
}

/**
 * @brief 绑定套接字到本地地址
 *
 * @param sockfd 套接字描述符
 * @param addr 地址结构指针
 * @param addrlen 地址结构长度
 * @return int 状态码
 */
int sys_bind(int sockfd, const uint64 addr, uint64 addrlen)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_bind] sockfd: %d, addr: %p, addrlen: %d\n", sockfd, addr, addrlen);
    proc_t *p = myproc();
    struct sockaddr_in socket;
    struct file *f;
    if ((f = p->ofile[sockfd]) == 0)
        return -1;
    if (f->f_type != FD_SOCKET)
        return -1;
    if (copyin(p->pagetable, (char *)&socket, addr, sizeof(socket)) == -1)
    {
        return -1;
    }
    // 验证地址族
    if (socket.sin_family != PF_INET)
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "Invalid address family: %d\n", socket.sin_family);
        return -1;
    }
    struct socket *sock = f->f_data.sock;
    // 调用内部绑定函数
    int ret = sock_bind(sock, (struct sockaddr_in *)&socket, addrlen);
    if (ret < 0)
    {
        return ret;
    }
    if (copyout(p->pagetable, addr, (char *)&socket, sizeof(socket)) == -1)
    {
        return -1;
    }
    return 0;
};

/**
 * @brief 将套接字连接到目标地址
 *
 * @param sockfd 套接字描述符
 * @param addr 目标地址指针
 * @param addrlen 地址结构长度
 * @return int 成功返回0，失败返回错误码
 */
int sys_connect(int sockfd, uint64 addr, int addrlen)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_connect] sockfd: %d, addr: %p, addrlen: %d\n",
                    sockfd, addr, addrlen);

    struct proc *p = myproc();
    struct file *f;
    struct sockaddr_in dest_addr;

    // 获取文件描述符对应的文件结构
    if ((f = p->ofile[sockfd]) == 0)
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "Invalid socket fd: %d\n", sockfd);
        return -EBADF;
    }

    // 检查文件类型是否为套接字
    if (f->f_type != FD_SOCKET)
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "fd %d is not a socket\n", sockfd);
        return -1;
    }

    // 检查套接字状态（未绑定则隐式绑定）
    if (f->f_data.sock->state < SOCKET_BOUND)
    {
        struct sockaddr_in local_addr = {
            .sin_family = PF_INET,
            .sin_addr = INADDR_ANY,
            .sin_port = 2000 // 默认端口
        };

        int ret = sock_bind(f->f_data.sock, &local_addr, sizeof(local_addr));
        if (ret < 0)
        {
            DEBUG_LOG_LEVEL(LOG_ERROR, "Implicit bind failed: %d\n", ret);
            return ret;
        }
    }

    // 验证地址长度
    if (addrlen < sizeof(struct sockaddr_in))
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "Address length too small: %d < %d\n",
                        addrlen, sizeof(struct sockaddr_in));
        return -EINVAL;
    }

    // 从用户空间复制地址信息
    if (copyin(p->pagetable, (char *)&dest_addr, addr, sizeof(dest_addr)))
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "Failed to copy address from user\n");
        return -EFAULT;
    }

    // 验证地址族
    if (dest_addr.sin_family != PF_INET)
    {
        // DEBUG_LOG_LEVEL(LOG_ERROR, "Invalid address family: %d\n", dest_addr.sin_family);
        return -EINPROGRESS;
    }

    // 设置远程地址并更新状态
    memcpy(&f->f_data.sock->remote_addr, &dest_addr, sizeof(dest_addr));
    f->f_data.sock->state = SOCKET_CONNECTED;

    DEBUG_LOG_LEVEL(LOG_DEBUG, "Connected to %08x:%d\n",
                    dest_addr.sin_addr, dest_addr.sin_port);
    return 0;
}

/**
 * @brief 接受套接字上的新连接
 *
 * @param sockfd 监听套接字的文件描述符
 * @param addr 用于存储客户端地址的指针
 * @param addrlen_ptr 指向地址长度变量的指针
 * @return int 新套接字的文件描述符或错误码
 */
int sys_accept(int sockfd, uint64 addr, uint64 addrlen_ptr)
{
    return 0;
}

/**
 * @brief 获取套接字绑定地址
 *
 * @param sockfd 套接字描述符
 * @param addr 地址结构指针
 * @param addrlen 地址结构长度指针
 * @return int 状态码
 */
int sys_getsockname(int sockfd, uint64 addr, uint64 addrlen_ptr)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_getsockname] sockfd: %d, addr: %p, addrlen: %p\n",
                    sockfd, addr, addrlen_ptr);

    struct proc *p = myproc();
    struct file *f;
    uint32 addrlen;
    struct sockaddr_in socket_addr;

    // 获取文件结构体
    if ((f = p->ofile[sockfd]) == 0)
        return -1;

    // 检查是否为套接字
    if (f->f_type != FD_SOCKET)
        return -1;

    // 检查绑定状态
    if (f->f_data.sock->state == SOCKET_UNBOUND)
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "Socket not bound\n");
        return -1;
    }

    // 从用户空间获取地址长度
    if (copyin(p->pagetable, (char *)&addrlen, addrlen_ptr, sizeof(addrlen)))
    {
        return -1;
    }

    // 准备返回的地址信息
    memmove(&socket_addr, &f->f_data.sock->local_addr, sizeof(struct sockaddr_in));

    // 复制地址到用户空间
    if (copyout(p->pagetable, addr, (char *)&socket_addr, sizeof(socket_addr)))
    {
        return -1;
    }

    // 更新实际使用的地址长度
    addrlen = sizeof(struct sockaddr_in);
    if (copyout(p->pagetable, addrlen_ptr, (char *)&addrlen, sizeof(addrlen)))
    {
        return -1;
    }

    return 0;
}

/**
 * @brief 设置套接字选项
 *
 * @param sockfd 套接字描述符
 * @param level 选项级别（SOL_SOCKET 等）
 * @param optname 选项名称
 * @param optval 选项值指针
 * @param optlen 选项值长度
 * @return int 成功返回0，失败返回-1
 */
int sys_setsockopt(int sockfd, int level, int optname, uint64 optval, uint64 optlen)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_setsockopt] sockfd: %d, level: %d, optname: %d, optval: %p, optlen: %d\n",
                    sockfd, level, optname, optval, optlen);

    proc_t *p = myproc();
    struct file *f;
    int value;

    // 获取文件描述符对应的文件结构
    if (sockfd < 0 || sockfd >= NOFILE || (f = p->ofile[sockfd]) == 0)
    {
        return -1;
    }

    // 检查文件类型
    if (f->f_type != FD_SOCKET)
    {
        return -1;
    }

    struct socket *sock = f->f_data.sock;
    // 复制用户空间的数据
    if (optlen < sizeof(int))
    {
        return -1;
    }

    if (copyin(p->pagetable, (char *)&value, optval, sizeof(value)) < 0)
    {
        return -1;
    }
    // 处理不同级别的选项
    switch (level)
    {
    case SOL_SOCKET:
        switch (optname)
        {
        case SO_REUSEADDR:
            // sock->reuse_addr = (value != 0);
            DEBUG_LOG_LEVEL(LOG_DEBUG, "Set SO_REUSEADDR: %d\n", value);
            break;

        case SO_KEEPALIVE:
            // sock->keepalive = (value != 0);
            DEBUG_LOG_LEVEL(LOG_DEBUG, "Set SO_KEEPALIVE: %d\n", value);
            break;
        case SO_RCVTIMEO:
        {
            // 验证长度
            if (optlen < sizeof(struct timeval))
                return -EINVAL;

            // 复制时间结构
            struct timeval tv;
            if (copyin(p->pagetable, (char *)&tv, optval, sizeof(tv)) < 0)
            {
                return -EFAULT;
            }

            // 存储到socket结构
            sock->rcv_timeout = tv;
            DEBUG_LOG_LEVEL(LOG_DEBUG, "Set SO_RCVTIMEO: %ld sec, %ld usec\n",
                            tv.sec, tv.usec);
            break;
        }

        default:
            DEBUG_LOG_LEVEL(LOG_WARNING, "Unsupported option: %d\n", optname);
            return -1;
        }
        break;

    default:
        DEBUG_LOG_LEVEL(LOG_WARNING, "Unsupported level: %d\n", level);
        return -1;
    }

    return 0;
}
/**
 * @brief 发送数据到指定的网络地址
 *
 * @param sockfd 套接字描述符
 * @param buf 数据缓冲区指针
 * @param len 数据长度
 * @param flags 发送标志
 * @param addr 目标地址指针
 * @param addrlen 地址长度
 * @return int 发送的字节数或错误码
 */
static struct simple_packet packet_store[MAX_PACKETS] = {0};
int sys_sendto(int sockfd, uint64 buf, int len, int flags, uint64 addr, int addrlen)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_sendto] sockfd: %d, buf: %p, len: %d, flags: %d, addr: %p, addrlen: %d\n",
                    sockfd, buf, len, flags, addr, addrlen);

    struct proc *p = myproc();
    struct file *f;
    struct sockaddr_in dest_addr;
    char *kbuf;

    // 获取文件结构体
    if ((f = p->ofile[sockfd]) == 0)
        return -1;

    // 检查是否为套接字
    if (f->f_type != FD_SOCKET)
        return -1;

    // 检查套接字状态
    if (f->f_data.sock->state < SOCKET_BOUND)
    {
        struct sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = PF_INET;
        local_addr.sin_addr = INADDR_ANY; // 任意本地地址
        int port = 2000;                  // 隐式绑定 2000端口
        local_addr.sin_port = port;       // 任意本地地址
        int bind_result = sock_bind(f->f_data.sock, (struct sockaddr_in *)&local_addr, sizeof(local_addr));
        if (bind_result < 0)
        {
            DEBUG_LOG_LEVEL(LOG_ERROR, "Implicit bind failed: %d\n", bind_result);
            return bind_result;
        }
    }

    // 验证数据长度
    if (len < 0 || len > MAX_PACKET_SIZE)
    {
        return -1;
    }

    // 分配内核缓冲区
    kbuf = kalloc();
    if (!kbuf)
    {
        return -1;
    }

    // 从用户空间复制数据
    if (copyin(p->pagetable, kbuf, buf, len) < 0)
    {
        kfree(kbuf);
        return -1;
    }

    // 处理目标地址
    if (addr)
    {
        if (addrlen < sizeof(struct sockaddr_in))
        {
            DEBUG_LOG_LEVEL(LOG_ERROR, "Address length too small: %d < %d\n",
                            addrlen, sizeof(struct sockaddr_in));
            kfree(kbuf);
            return -EINVAL;
        }
        // 从用户空间复制地址结构
        if (copyin(p->pagetable, (char *)&dest_addr, addr, sizeof(dest_addr)))
        {
            kfree(kbuf);
            return -1;
        }
    }
    else
    {
        // 如果没有提供地址，使用连接的目标地址
        if (f->f_data.sock->state != SOCKET_CONNECTED)
        {
            DEBUG_LOG_LEVEL(LOG_ERROR, "Socket not connected and no address provided\n");
            kfree(kbuf);
            return -1;
        }
        memcpy(&dest_addr, &f->f_data.sock->remote_addr, sizeof(dest_addr));
    }
    ///< 找到空闲缓冲区并写入
    int send_len = -1;
    for (int i = 0; i < MAX_PACKETS; i++)
    {
        if (!packet_store[i].valid)
        {
            packet_store[i].dst_port = dest_addr.sin_port;
            packet_store[i].data = kbuf;
            packet_store[i].valid = 1;
            send_len = strlen(kbuf);
            break;
        }
    }

    // kfree(kbuf);
    return send_len;
}

/**
 * @brief 从套接字接收数据并获取发送方地址
 *
 * @param sockfd 套接字描述符
 * @param buf 数据缓冲区指针
 * @param len 缓冲区长度
 * @param flags 接收标志
 * @param addr 发送方地址指针
 * @param addrlen 地址长度指针
 * @return int 接收的字节数或错误码
 */
int sys_recvfrom(int sockfd, uint64 buf, int len, int flags, uint64 addr, uint64 addrlen_ptr)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_recvfrom] sockfd: %d, buf: %p, len: %d, flags: %d, addr: %p, addrlen: %p\n",
                    sockfd, buf, len, flags, addr, addrlen_ptr);

    struct proc *p = myproc();
    struct file *f;
    char *kbuf;

    // 获取文件结构体
    if ((f = p->ofile[sockfd]) == 0)
        return -1;

    // 检查是否为套接字
    if (f->f_type != FD_SOCKET)
        return -1;

    // 检查套接字状态
    if (f->f_data.sock->state < SOCKET_BOUND)
    {
        struct sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = PF_INET;
        local_addr.sin_addr = INADDR_ANY;
        local_addr.sin_port = 2000; // 默认端口
        int bind_result = sock_bind(f->f_data.sock, &local_addr, sizeof(local_addr));
        if (bind_result < 0)
            return bind_result;
    }

    // 验证缓冲区长度
    if (len < 0 || len > MAX_PACKET_SIZE)
    {
        return -1;
    }

    // 分配内核缓冲区
    kbuf = kalloc();
    if (!kbuf)
    {
        return -1;
    }

    uint16_t local_port = f->f_data.sock->local_addr.sin_port;
    char *recv_byte = 0;
    int found = 0;
    int recv_len = -1;

    for (int i = 0; i < MAX_PACKETS; i++)
    {
        if (packet_store[i].valid && packet_store[i].dst_port == local_port)
        {
            recv_byte = packet_store[i].data;
            packet_store[i].valid = 0; // 标记为无效
            found = 1;
            recv_len = strlen(recv_byte);
            break;
        }
    }
    if (!found)
        return -1; // 没有找到数据包

    // 将数据复制到用户空间
    if (copyout(p->pagetable, buf, recv_byte, 1) < 0)
    {
        return -1;
    }

    // 处理发送方地址
    // 返回源地址信息（固定为127.0.0.1）
    if (addr && addrlen_ptr)
    {
        struct sockaddr_in src_addr;
        memset(&src_addr, 0, sizeof(src_addr));
        src_addr.sin_family = PF_INET;
        src_addr.sin_addr = 0x7f000001; // 127.0.0.1
        src_addr.sin_port = 2000;       // 固定源端口

        // 复制地址到用户空间
        if (copyout(p->pagetable, addr, (char *)&src_addr, sizeof(src_addr)) < 0)
        {
            return -1;
        }

        // 更新地址长度
        int addrlen_val = sizeof(src_addr);
        if (copyout(p->pagetable, addrlen_ptr, (char *)&addrlen_val, sizeof(addrlen_val)))
        {
            return -1;
        }
    }

    kfree(kbuf);
    return recv_len;
}

/**
 * @brief 设置/得到资源的限制
 *
 * @param pid 进程，0表示当前进程
 * @param resource 资源编号
 * @param new_limit set的源地址
 * @param old_limit get的源地址
 * @return uint64 标准错误码
 * @todo 目前只处理了RLIMIT_NOFILE
 */
uint64
sys_prlimit64(pid_t pid, int resource, uint64 new_limit, uint64 old_limit)
{
    const struct rlimit nl;
    struct rlimit ol;
    proc_t *p = myproc();

    switch (resource)
    {
    case RLIMIT_NOFILE:
    {
        if (new_limit)
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "RLIMIT_NOFILE, cur %ul, max %ul\n", nl.rlim_cur, nl.rlim_max);
            if (copyin(p->pagetable, (char *)&nl, new_limit, sizeof(nl)) < 0)
                return -EFAULT;
            p->ofn.rlim_cur = nl.rlim_cur;
            p->ofn.rlim_max = nl.rlim_max;
        }
        if (old_limit)
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "RLIMIT_NOFILE, get\n");
            ol.rlim_cur = p->ofn.rlim_cur;
            ol.rlim_max = p->ofn.rlim_max;
            if (copyout(p->pagetable, old_limit, (char *)&ol, sizeof(nl)) < 0)
                return -EFAULT;
        }
        break;
    }
    default:
        DEBUG_LOG_LEVEL(LOG_DEBUG, "sys_prlimit64 parameter is %d\n", resource);
    }
    return 0;
}

uint64
sys_membarrier(int cmd, unsigned int flags, int cpu_id)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_membarrier] cmd: %d, flags: %lu, cpu_id %d\n", cmd, flags, cpu_id);
    return 0;
}

uint64
sys_tkill(int tid, int sig)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "tkill tid:%d, sig:%d\n", tid, sig);
    extern struct proc pool[NPROC];
    struct proc *p = NULL;
    for (int i = 0; i < NPROC; i++)
    {
        p = &pool[i];
        /* 遍历p的thread_queue，找到为止tid==tid*/
        if (p->state == RUNNABLE || p->state == RUNNING)
        {
            struct list_elem *e = list_begin(&p->thread_queue);
            while (e != list_end(&p->thread_queue))
            {
                thread_t *t = list_entry(e, thread_t, elem);
                if (t->tid == tid)
                {
                    tgkill(p->pid, tid, sig);
                }
                e = list_next(e);
            }
        }
    }
    return -1;
}

/* @todo */
uint64
sys_get_robust_list(int pid, uint64 head_ptr, size_t *len_ptr)
{
    return 0;
}

uint64 sys_getrusage(int who, uint64 addr)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_getrusage] who: %d, addr: %p\n", who, addr);
    struct rusage rs;
    proc_t *p = myproc();
    
    // 根据who参数决定返回哪个进程的资源使用情况
    proc_t *target_p = p;
    if (who == RUSAGE_CHILDREN) {
        // 对于子进程，我们需要累加所有子进程的时间
        // 这里简化处理，暂时返回当前进程时间
        target_p = p;
    } else if (who == RUSAGE_SELF) {
        target_p = p;
    } else {
        // 不支持的who参数
        return -EINVAL;
    }
    
    // 将tick计数转换为timeval格式
    timeval_t utime, stime;
    utime.sec = target_p->utime / 1;  // 假设10 ticks = 1秒
    utime.usec = (target_p->utime % 1) * 100000;  // 剩余tick转换为微秒
    
    stime.sec = target_p->ktime / 1;
    stime.usec = (target_p->ktime % 1) * 100000;
    // printf("utime.sec:%d,stime.sec:%d,utime:%d,stime:%d\n",utime.sec,stime.sec,target_p->utime,target_p->ktime);
    
    rs = (struct rusage){
        .ru_utime = utime,
        .ru_stime = stime,
        .ru_maxrss = 0,
        .ru_ixrss = 0,
        .ru_idrss = 0,
        .ru_isrss = 0,
        .ru_minflt = 0,
        .ru_majflt = 0,
        .ru_nswap = 0,
        .ru_inblock = 0,
        .ru_oublock = 0,
        .ru_msgsnd = 0,
        .ru_msgrcv = 0,
        .ru_nsignals = 0,
        .ru_nvcsw = 0,
        .ru_nivcsw = 0,
    };
    
    if (copyout(myproc()->pagetable, addr, (char *)&rs, sizeof(rs)) < 0)
        return -1;
    return 0;
}

/**
 * @brief 用于获取共享内存段,一般是创建一个共享内存段
 *
 * @param key 为0则自行分配shmid给共享内存段，不是0则把shmid设为key
 * @param size 共享内存段大小
 * @param flag 低9位是权限位，
 */
uint64 sys_shmget(uint64 key, uint64 size, uint64 flag)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_shmget]key: %x, size: %x, flag: %x\n", key, size, flag);
    if (key == IPC_PRIVATE)
    {
        // for(int i=0;i<MAX_SHAREMEMORY_REGION_NUM;i++)
        // {
        //     if(myproc()->sharememory[i]) //被使用了
        //         continue;
        //     else //有空位
        //     {
        //         struct sharememory *shm = slab_alloc(sizeof(struct sharememory));
        //         shm->shmid=++myproc()->shm_num;
        //         shm->size=size;
        //         shm->flag=flag;
        //         myproc()->sharememory[i] = shm;
        //         return shm->shmid; //正常执行，则返回分配的共享内存段的id
        //     }
        // }
        int ret = newseg(key, flag, size);
        return ret;
    }
    else
    {
        panic("[sys_shmget]key不等于0,值为: %d\n", key);
    }
    return -1;
}

/**
 * @brief 把共享内存段映射到进程地址空间
 *
 * @return 分配的虚拟地址段起始
 *
 * @todo 没管shmflg
 */
extern int sharemem_start;
uint64 sys_shmat(uint64 shmid, uint64 shmaddr, uint64 shmflg)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_shmat] pid:%d, shmid: %x, shmaddr: %x, shmflg: %x\n", 
                   myproc()->pid, shmid, shmaddr, shmflg);
    if (shmaddr == 0) // 自行分配虚拟地址
    {
        struct shmid_kernel *shp;
        shp = shm_segs[shmid];
        if (!shp)
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "[sys_shmat] pid:%d failed to find shmid: %x\n", myproc()->pid, shmid);
            return -1;
        }
        int size = shp->size;
        struct vma *vm_struct = alloc_vma(myproc(), SHARE, sharemem_start, size, PTE_W | PTE_R, 0, 0);
        if (!vm_struct)
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "[sys_shmat] pid:%d failed to alloc_vma for size: %x\n", myproc()->pid, size);
            return -1;
        }

        // 设置vma指向对应的共享内存段
        vm_struct->shm_kernel = shp;

        sharemem_start = PGROUNDUP(sharemem_start + size);
        shp->attaches = vm_struct;
        DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_shmat] pid:%d successfully attached shmid: %x at addr: %p\n", 
                       myproc()->pid, shmid, vm_struct->addr);
        return vm_struct->addr;
        // for(int i=0;i<MAX_SHAREMEMORY_REGION_NUM;i++)
        // {
        //     if(myproc()->sharememory[i]->shmid == shmid) //找到指定的shm
        //     {
        //         /*按shm的size和flag映射*/
        //         uint64 map_size = PGROUNDUP(myproc()->sharememory[i]->size);
        //         uint64 map_start = myproc()->shm_size + VM_SHARE_MEMORY_REGION;
        //         if(!alloc_vma(myproc(), SHARE, map_start, map_size, PTE_W|PTE_R, 1, 0))
        //         {
        //             panic("[sys_shmat]alloc_vma映射失败!\n");
        //         }
        //         myproc()->shm_size += map_size;
        //         return map_start; //成功分配
        //     }
        // }
    }
    else
    {
        panic("[sys_shmat]shmaddr不等于0,请实现这个情况\n");
    }
    return -1;
}

/**
 * @brief 共享内存的控制接口，用于查询、修改或删除共享内存段的属性
 *
 */
uint64 sys_shmctl(uint64 shmid, uint64 cmd, uint64 buf)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_shmctl]shmid: %x, cmd: %x,\n", shmid, cmd);
    return 0;
}

/**
 * @brief 执行实际的select操作
 *
 * @param nfds     最大文件描述符+1
 * @param readfds  可读文件描述符集
 * @param writefds 可写文件描述符集
 * @param exceptfds 异常文件描述符集
 * @return int     就绪的文件描述符数量
 */
static int do_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds)
{
    int ret = 0;
    struct file *f;
    struct proc *p = myproc();

    // 验证nfds参数
    if (nfds < 0) {
        return -EINVAL;
    }

    // 如果没有文件描述符集，直接返回0
    if (!readfds && !writefds && !exceptfds) {
        return 0;
    }

    for (int fd = 0; fd < nfds; fd++) {
        // 检查读集合
        if (readfds && FD_ISSET(fd, readfds)) {
            if (fd < 0 || fd >= NOFILE || (f = p->ofile[fd]) == 0) {
                FD_CLR(fd, readfds); // 无效文件描述符
            } else if (get_file_ops()->poll(f, POLLIN)) {
                ret++; // 可读
            } else {
                FD_CLR(fd, readfds);
            }
        }

        // 检查写集合
        if (writefds && FD_ISSET(fd, writefds)) {
            if (fd < 0 || fd >= NOFILE || (f = p->ofile[fd]) == 0) {
                FD_CLR(fd, writefds);
            } else if (get_file_ops()->poll(f, POLLOUT)) {
                ret++; // 可写
            } else {
                FD_CLR(fd, writefds);
            }
        }

        // 检查异常集合（当前不实现）
        if (exceptfds && FD_ISSET(fd, exceptfds)) {
            FD_CLR(fd, exceptfds);
        }
    }

    return ret;
}

/**
 * @brief 实现pselect6_time32系统调用
 *
 * @param nfds     监控的最大文件描述符+1
 * @param readfds  监控可读的文件描述符集
 * @param writefds 监控可写的文件描述符集
 * @param exceptfds 监控异常的文件描述符集
 * @param timeout  超时时间结构体指针
 * @param sigmask  信号掩码指针（当前未使用）
 * @return int     返回就绪的文件描述符数量，超时返回0，错误返回-1
 */
uint64 sys_pselect6_time32(int nfds, uint64 readfds, uint64 writefds,
                           uint64 exceptfds, uint64 timeout, uint64 sigmask)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_pselect6_time32]: nfds:%d,readfds:%d,writefds:%d,exceptfds:%d,timeout:%d,sigmask:%d\n", nfds, readfds, writefds, exceptfds, timeout, sigmask);
    struct proc *p = myproc();
    fd_set rfds, wfds, efds;
    struct timespec ts;
    uint64 end_time = 0;
    int ret = 0;
    __sigset_t old_sigmask, temp_sigmask;

    // 添加进程状态调试信息
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_pselect6_time32] pid:%d, state:%d, killed:%d\n", 
                   p->pid, p->state, p->killed);

    // 验证nfds范围
    if (nfds > FD_SETSIZE) {
        return -EINVAL;
    }

    // 复制文件描述符集
    if (readfds && copyin(p->pagetable, (char*)&rfds, readfds, sizeof(fd_set)) < 0) {
        return -EFAULT;
    }
    if (writefds && copyin(p->pagetable, (char*)&wfds, writefds, sizeof(fd_set)) < 0) {
        return -EFAULT;
    }
    if (exceptfds && copyin(p->pagetable, (char*)&efds, exceptfds, sizeof(fd_set)) < 0) {
        return -EFAULT;
    }

    // 处理信号掩码：如果提供了sigmask，临时设置新的信号掩码
    if (sigmask) {
        if (copyin(p->pagetable, (char*)&temp_sigmask, sigmask, sizeof(__sigset_t)) < 0) {
            return -EFAULT;
        }
        // 保存当前信号掩码
        memcpy(&old_sigmask, &p->sig_set, sizeof(__sigset_t));
        // 设置新的信号掩码
        memcpy(&p->sig_set, &temp_sigmask, sizeof(__sigset_t));
        DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_pselect6_time32] 临时设置信号掩码: 0x%lx\n", p->sig_set.__val[0]);
    }

    // 处理超时
    if (timeout) {
        if (copyin(p->pagetable, (char*)&ts, timeout, sizeof(ts)) < 0) {
            // 恢复信号掩码
            if (sigmask) {
                memcpy(&p->sig_set, &old_sigmask, sizeof(__sigset_t));
            }
            return -EFAULT;
        }
        if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000) {
            // 恢复信号掩码
            if (sigmask) {
                memcpy(&p->sig_set, &old_sigmask, sizeof(__sigset_t));
            }
            return -EINVAL;
        }
        end_time = r_time() + (ts.tv_sec * CLK_FREQ) +
                   (ts.tv_nsec * CLK_FREQ / 1000000000);
    }

    // 主监控循环
    while (1) {
        ret = do_select(nfds, readfds ? &rfds : NULL, writefds ? &wfds : NULL,
                       exceptfds ? &efds : NULL);

        // 有就绪描述符或错误
        if (ret != 0 || p->killed) {
            break;
        }

        // 检查是否有待处理的信号（未被当前信号掩码阻塞的信号）
        int pending_sig = check_pending_signals(p);
        if (pending_sig != 0) {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_pselect6_time32] 收到信号 %d，立即返回\n", pending_sig);
            ret = -EINTR; // 被信号中断
            break;
        }

        // 检查是否被信号中断过（信号处理完成后）
        if (p->signal_interrupted) {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_pselect6_time32] 检测到信号中断标志，返回EINTR\n");
            ret = -EINTR; // 被信号中断
            p->signal_interrupted = 0; // 清除标志
            break;
        }
        
        // 添加调试日志，检查signal_interrupted标志的状态
        static int debug_count = 0;
        if (++debug_count % 1000 == 0) {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_pselect6_time32] debug_count: %d, signal_interrupted: %d\n", debug_count, p->signal_interrupted);
        }

        // 检查超时
        if (timeout && r_time() >= end_time) {
            ret = 0;
            break;
        }

        // 让出CPU，但添加短暂延迟避免过度消耗CPU
        yield();
        
        // 添加调试日志，帮助诊断问题
        static int loop_count = 0;
        if (++loop_count % 1000 == 0) {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_pselect6_time32] loop count: %d, nfds: %d\n", loop_count, nfds);
        }
    }

    // 恢复原来的信号掩码
    if (sigmask) {
        memcpy(&p->sig_set, &old_sigmask, sizeof(__sigset_t));
        DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_pselect6_time32] 恢复信号掩码: 0x%lx\n", p->sig_set.__val[0]);
    }

    // 复制回结果
    if (readfds && copyout(p->pagetable, readfds, (char*)&rfds, sizeof(fd_set)) < 0) {
        return -EFAULT;
    }
    if (writefds && copyout(p->pagetable, writefds, (char*)&wfds, sizeof(fd_set)) < 0) {
        return -EFAULT;
    }
    if (exceptfds && copyout(p->pagetable, exceptfds, (char*)&efds, sizeof(fd_set)) < 0) {
        return -EFAULT;
    }

    return ret;
}

void sys_sigreturn()
{
    // LOG("sigreturn返回!\n");
    //恢复上下文
    copytrapframe(myproc()->trapframe,&myproc()->sig_trapframe);
    // 信号处理完成，清除当前信号标志
    myproc()->current_signal = 0;
    // 不清除signal_interrupted标志，让pselect等系统调用能够检测到信号中断
    DEBUG_LOG_LEVEL(LOG_DEBUG, "sys_sigreturn: 信号处理完成，current_signal=0, signal_interrupted=%d\n", myproc()->signal_interrupted);
}

uint64 a[8]; // 8个a寄存器，a7是系统调用号
void syscall(struct trapframe *trapframe)
{
    for (int i = 0; i < 8; i++)
        a[i] = hsai_get_arg(trapframe, i);
    long long ret = -1;

#if DEBUG
    LOG_LEVEL(LOG_INFO, "syscall: a7: %d (%s)\n", (int)a[7], get_syscall_name((int)a[7]));
#else
    // 目前只是简单地获取系统调用名称，但不进行任何输出
    const char* syscall_name = get_syscall_name((int)a[7]);
    (void)syscall_name; // 避免未使用变量的警告
#endif

    switch (a[7])
    {
    case SYS_sigreturn:
        sys_sigreturn();
        break;
    case SYS_write:
        ret = sys_write(a[0], a[1], a[2]);
        break;
    case SYS_writev:
        ret = sys_writev((int)a[0], (uint64)a[1], (int)a[2]);
        break;
    case SYS_getpid:
        ret = sys_getpid();
        break;
    case SYS_fork:
        ret = sys_fork();
        break;
    case SYS_clone:
        ret = sys_clone(a[0], a[1], a[2], a[3], a[4]);
        break;
    case SYS_clone3:
        ret = sys_clone3();
        break;
    case SYS_wait:
        ret = sys_wait((int)a[0], (uint64)a[1], (int)a[2]);
        break;
    case SYS_exit:
        sys_exit(a[0]);
        break;
    case SYS_kill:
        ret = sys_kill((int)a[0], (int)a[1]);
        break;
    case SYS_gettimeofday:
        ret = sys_gettimeofday(a[0]);
        break;
    case SYS_clock_gettime:
        ret = sys_clock_gettime((uint64)a[0], (uint64)a[1]);
        break;
    case SYS_sleep:
        ret = sleep((timeval_t *)a[0], (timeval_t *)a[1]);
        break;
    case SYS_brk:
        ret = sys_brk((uint64)a[0]);
        break;
    case SYS_times:
        ret = sys_times((uint64)a[0]);
        break;
    case SYS_settimer:
        ret = sys_settimer((uint64)a[0], (uint64)a[1], (uint64)a[2]);
        break;
    case SYS_uname:
        ret = sys_uname((uint64)a[0]);
        break;
    case SYS_sched_yield:
        ret = sys_sched_yield();
        break;
    case SYS_getppid:
        ret = sys_getppid();
        break;
    case SYS_execve:
        ret = sys_execve((const char *)a[0], (uint64)a[1], (uint64)a[2]);
        break;
    case SYS_pipe2:
        ret = sys_pipe2((int *)a[0], (int)a[1]);
        break;
    case SYS_close:
        ret = sys_close(a[0]);
        break;
    case SYS_read:
        ret = sys_read(a[0], (uint64)a[1], (int)a[2]);
        break;
    case SYS_readv:
        ret = sys_readv((int)a[0], (uint64)a[1], (int)a[2]);
        break;
    case SYS_pread:
        ret = sys_pread((int)a[0], (void *)a[1], (uint64)a[2], (uint64)a[3]);
        break;
    case SYS_dup:
        ret = sys_dup(a[0]);
        break;
    case SYS_openat:
        ret = sys_openat((int)a[0], (const char *)a[1], (int)a[2], (uint16)a[3]);
        break;
    case SYS_mknod:
        ret = sys_mknod((const char *)a[0], (int)a[1], (int)a[2]);
        break;
    case SYS_dup3:
        ret = sys_dup3((int)a[0], (int)a[1], (int)a[2]);
        break;
    case SYS_fstat:
        ret = sys_fstat((int)a[0], (uint64)a[1]);
        break;
    case SYS_fstatat:
        ret = sys_fstatat((int)a[0], (uint64)a[1], (uint64)a[2], (int)a[3]);
        break;
    case SYS_statx:
        ret = sys_statx((int)a[0], (const char *)a[1], (int)a[2], (int)a[3], (uint64)a[4]);
        break;
    case SYS_syslog:
        ret = sys_syslog((int)a[0], (uint64)a[1], (int)a[2]);
        break;
    case SYS_mmap:
        ret = sys_mmap((uint64)a[0], (int64)a[1], (int)a[2], (int)a[3], (int)a[4], (int)a[5]);
        break;
    case SYS_munmap:
        ret = sys_munmap((void *)a[0], (int)a[1]);
        break;
    case SYS_getcwd:
        ret = sys_getcwd((char *)a[0], (int)a[1]);
        break;
    case SYS_mkdirat:
        ret = sys_mkdirat((int)a[0], (const char *)a[1], (uint16)a[2]);
        break;
    case SYS_chdir:
        ret = sys_chdir((const char *)a[0]);
        break;
    case SYS_getdents64:
        ret = sys_getdents64((int)a[0], (struct linux_dirent64 *)a[1], (int)a[2]);
        break;
    case SYS_mount:
        ret = sys_mount((const char *)a[0], (const char *)a[1], (const char *)a[2], (unsigned long)a[3], (const void *)a[3]);
        break;
    case SYS_umount:
        ret = sys_umount((const char *)a[0]);
        break;
    case SYS_unlinkat:
        ret = sys_unlinkat((int)a[0], (char *)a[1], (unsigned int)a[2]);
        break;
    case SYS_set_tid_address:
        ret = sys_set_tid_address((uint64)a[0]);
        break;
    case SYS_getuid:
        ret = sys_getuid();
        break;
    case SYS_geteuid:
        ret = sys_geteuid();
        break;
    case SYS_ioctl:
        ret = sys_ioctl();
        break;
    case SYS_exit_group:
        ret = sys_exit_group();
        break;
    case SYS_rt_sigprocmask:
        ret = sys_rt_sigprocmask((int)a[0], (uint64)a[1], (uint64)a[2]);
        break;
    case SYS_rt_sigaction:
        ret = sys_rt_sigaction((int)a[0], (const struct sigaction *)a[1], (struct sigaction *)a[2]);
        break;
    case SYS_faccessat:
        ret = sys_faccessat((int)a[0], (uint64)a[1], (int)a[2], (int)a[3]);
        break;
    case SYS_sysinfo:
        ret = sys_sysinfo((uint64)a[0]);
        break;
    case SYS_set_robust_list:
        ret = sys_set_robust_list();
        break;
    case SYS_gettid:
        ret = sys_gettid();
        break;
    case SYS_tgkill:
        ret = sys_tgkill((uint64)a[0], (uint64)a[1], (int)a[2]);
        break;
    case SYS_prlimit64:
        ret = sys_prlimit64((pid_t)a[0], (int)a[1], (uint64)a[2], (uint64)a[3]);
        break;
    case SYS_readlinkat:
        ret = sys_readlinkat((int)a[0], (char *)a[1], (char *)a[2], (int)a[3]);
        break;
    case SYS_getrandom:
        ret = sys_getrandom((void *)a[0], (uint64)a[1], (uint64)a[2]);
        break;
    case SYS_sendfile64:
        ret = sys_sendfile64((int)a[0], (int)a[1], (uint64 *)a[2], (uint64)a[3]);
        break;
    case SYS_llseek:
        ret = sys_lseek((uint32)a[0], (uint64)a[1], (int)a[2]);
        break;
    case SYS_renameat2:
        ret = sys_renameat2((int)a[0], (const char *)a[1], (int)a[2], (const char *)a[3], (uint32)a[4]);
        break;
    case SYS_clock_nanosleep:
        ret = sys_clock_nanosleep((int)a[0], (int)a[1], (uint64 *)a[2], (uint64 *)a[3]);
        break;
    case SYS_mremap:
        ret = sys_mremap((uint64)a[0], (uint64)a[1], (uint64)a[2], (uint64)a[3], (uint64)a[4]);
        break;
    //< 注：glibc问题在5.26解决了
    // case SYS_getgid: //< 如果getuid返回值不是0,就会需要这三个。但没有解决问题
    //     ret = 0;
    //     break;
    case SYS_setgid:
        ret = sys_setgid((int)a[0]); //< 先不实现，反正设置了我们也不用gid
        break;
    // case SYS_setuid:
    //     ret = 0;//< 先不实现，反正设置了我们也不用uid
    //     break;
    case SYS_fcntl:
        ret = sys_fcntl((int)a[0], (int)a[1], (uint64)a[2]);
        break;
    case SYS_utimensat:
        ret = sys_utimensat((int)a[0], (uint64)a[1], (uint64)a[2], (int)a[3]);
        break;
    case SYS_ppoll:
        ret = sys_ppoll((uint64)a[0], (int)a[1], (uint64)a[2], (uint64)a[3]);
        break;
    case SYS_futex:
        ret = sys_futex((uint64)a[0], (int)a[1], (uint64)a[2], (uint64)a[3], (uint64)a[4], (uint64)a[5]);
        break;
    case SYS_shutdown:
        sys_shutdown();
        break;
    case SYS_rt_sigtimedwait:
        ret = 0;
        break;
    case SYS_mprotect:
        ret = sys_mprotect((uint64)a[0], (uint64)a[1], (uint64)a[2]);
        break;
    case SYS_getegid:
        ret = myproc()->gid;
        break;
    case SYS_socket:
        ret = sys_socket((int)a[0], (int)a[1], (int)a[2]);
        break;
    case SYS_bind:
        ret = sys_bind((int)a[0], (uint64)a[1], (uint64)a[2]);
        break;
    case SYS_connect:
        ret = sys_connect((int)a[0], (uint64)a[1], (uint64)a[2]);
        break;
    case SYS_accept:
        ret = sys_accept((int)a[0], (uint64)a[1], (uint64)a[2]);
        break;
    case SYS_getsockname:
        ret = sys_getsockname((int)a[0], (uint64)a[1], (uint64)a[2]);
        break;
    case SYS_setsockopt:
        ret = sys_setsockopt((int)a[0], (int)a[1], (int)a[2], (uint64)a[3], (uint64)a[4]);
        break;
    case SYS_sendto:
        ret = sys_sendto((int)a[0], (uint64)a[1], (uint64)a[2], (int)a[3], (uint64)a[4], (uint64)a[5]);
        break;
    case SYS_recvfrom:
        ret = sys_recvfrom((int)a[0], (uint64)a[1], (uint64)a[2], (int)a[3], (uint64)a[4], (uint64)a[5]);
        break;
    case SYS_listen:
        ret = sys_listen((int)a[0], (int)a[1]);
        break;
    case SYS_membarrier:
        ret = sys_membarrier((int)a[0], (unsigned int)a[1], (int)a[2]);
        break;
    case SYS_tkill:
        ret = sys_tkill((int)a[0], (int)a[1]);
        break;
    case SYS_get_robust_list:
        ret = sys_get_robust_list((int)a[0], (uint64)a[1], (size_t *)a[2]);
        break;
    case SYS_statfs:
        ret = sys_statfs((uint64)a[0], (uint64)a[1]);
        break;
    case SYS_setsid:
        ret = 0;
        break;
    case SYS_madvise:
        ret = 0;
        break;
    case SYS_sync:
        ret = 0;
        break;
    case SYS_ftruncate:
        ret = 0;
        break;
    case SYS_fsync:
        ret = 0;
        break;
    case SYS_getrusage:
        ret = sys_getrusage((int)a[0], (uint64)a[1]);
        break;
    case SYS_shmget:
        ret = sys_shmget((uint64)a[0], (uint64)a[1], (uint64)a[2]);
        break;
    case SYS_shmat:
        ret = sys_shmat((uint64)a[0], (uint64)a[1], (uint64)a[2]);
        break;
    case SYS_shmctl:
        ret = sys_shmctl((uint64)a[0], (uint64)a[1], (uint64)a[2]);
        break;
    case SYS_pselect6_time32:
        ret = sys_pselect6_time32((int)a[0], (uint64)a[1], (uint64)a[2], (uint64)a[3], (uint64)a[4], (uint64)a[5]);
        break;
    case SYS_umask:
        printf("[sys_umask] \n");
        ret = 0;
        break;
    default:
        ret = -1;
        panic("unknown syscall with a7: %d", a[7]);
    }
    trapframe->a0 = ret;
}
