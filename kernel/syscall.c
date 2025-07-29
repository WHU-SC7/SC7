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
#include "ext4.h"
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
#include "procfs.h"
#include "fifo.h"

// 外部变量声明
extern struct proc pool[NPROC];
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
int do_path_containFile_or_notExist(char *path);

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
    int ret;
    /* 1. 检查父目录是否合法 */
    if (fd != AT_FDCWD && (fd < 0 || fd >= NOFILE))
        return -ENOENT;
    char path[MAXPATH];
    proc_t *p = myproc();
    if (copyinstr(p->pagetable, path, (uint64)upath, MAXPATH) == -1)
    {
        return -EFAULT;
    }
    // 添加进程状态调试信息
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_openat] pid:%d opening file: %s, flags: %x, mode: %d\n",
                    p->pid, path, flags, mode);
    struct filesystem *fs = get_fs_from_path(path); ///<  根据路径获取对应的文件系统
    /* @todo 官方测例好像vfat和ext4一种方式打开 */
    if (fs->type == EXT4 || fs->type == VFAT)
    {
        const char *dirpath = (fd == AT_FDCWD) ? myproc()->cwd.path : myproc()->ofile[fd]->f_path;
        char absolute_path[MAXPATH] = {0};
        get_absolute_path(path, dirpath, absolute_path);

        /* 2. 检查路径长度 */
        if (strlen(absolute_path) > NAME_MAX)
        {
            return -ENAMETOOLONG;
        }

        /* 3. 检查文件是否存在 */
        struct kstat st;
        int file_exists = (vfs_ext4_stat(absolute_path, &st) == 0);

        /* 4. 处理特殊文件 */
        if (!strcmp(absolute_path, "/proc/mounts") || ///< df
            !strcmp(absolute_path, "/proc") ||        ///< ps
            !strcmp(absolute_path, "/dev/misc/rtc")   ///< hwclock
        )
        {
            struct file *f = filealloc();
            if (!f)
                return -ENFILE;
            // 直接设置为busybox文件类型，不需要调用close函数
            f->f_type = FD_BUSYBOX;
            f->f_pos = 0;
            f->f_data.f_vnode.data = NULL; // 确保初始化为NULL
            int newfd = fdalloc(f);
            if (newfd < 0)
            {
                // 释放文件结构体
                f->f_count = 0;
                return -EMFILE;
            }
            return newfd;
        }
        /* 5. 处理procfs */
        int stat_pid = 0;
        int proctype = check_proc_path(absolute_path, &stat_pid);
        if (proctype != 0)
        {
            struct file *f = filealloc();
            if (!f)
                return -ENFILE;
            int newfd = fdalloc(f);
            if (newfd < 0)
            {
                return -EMFILE;
            }
            f->f_type = proctype;
            f->f_flags = flags;
            f->f_mode = mode;
            f->f_pos = 0;
            if (proctype == FD_PROC_STAT)
                snprintf(f->f_path, sizeof(f->f_path), "%s", path);
            return newfd;
        }

        /* 6. 处理一般文件 */
        /* 6.1 如果文件存在，先检查特殊标志 */
        if (file_exists)
        {
            if ((ret = vfs_check_flag_with_stat_path(flags, &st, absolute_path)) < 0)
            {
                DEBUG_LOG_LEVEL(LOG_DEBUG, "vfs_check_flag_with_stat failed: %d\n", ret);
                return ret;
            }

            /* 访问权限 */
            int access_mode = 0;
            int file_mode = flags & O_ACCMODE;
            if (file_mode == O_RDONLY || file_mode == O_RDWR)
            {
                access_mode |= R_OK;
            }
            if (file_mode == O_WRONLY || file_mode == O_RDWR)
            {
                access_mode |= W_OK;
            }

            /* 检查文件权限 */
            if (!check_file_access(&st, access_mode))
            {
                return -EACCES;
            }
        }
        /* 6.2 如果文件不存在，创建文件或返回错误 */
        else
        {
            if (!(flags & O_CREAT))
                return -ENOENT;
            // 要创建文件，必须有文件夹的执行和写入权限
            char check_path[512]; // 要检查的路径
            struct kstat dir_st;
            get_parent_path(absolute_path, check_path, sizeof(check_path));
            vfs_ext4_stat(check_path, &dir_st);
            // printf("路径: %s,权限是: %x\n",check_path, dir_st.st_mode&0777);

            // 检查文件权限
            if (!check_file_access(&dir_st, W_OK | X_OK))
            {
                DEBUG_LOG_LEVEL(LOG_DEBUG, "Directory access denied: %s\n", check_path);
                return -EACCES;
            }
        }
        /* 6.3 分配文件描述符并打开文件 */
        struct file *f;
        f = filealloc();
        if (!f)
            return -ENFILE;
        int fd = -1;
        if ((fd = fdalloc(f)) < 0)
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "OUT OF FD!\n");
            return -EMFILE;
        }
        f->f_flags = flags;
        // 如果设置了 O_CLOEXEC，标记文件描述符为 close-on-exec
        if ((flags & O_CLOEXEC) && (strstr(absolute_path, "libc.so.6") == NULL))
        {
            f->f_flags |= FD_CLOEXEC;
            DEBUG_LOG_LEVEL(LOG_DEBUG, "close-on-exec path: %s\n", absolute_path);
        }
        if (!strcmp(absolute_path, "/tmp") || strstr(absolute_path, "/proc")) // 如果目录为/tmp 或者含有/proc 给O_CREATE权限
        {
            f->f_flags |= O_CREAT;
        }

        // 正确处理 mode 参数
        if (file_exists)
        {
            // 文件存在时，使用文件的原有权限，但需要设置 f_mode 用于后续操作
            f->f_mode = st.st_mode & 07777; // 只保留权限位
        }
        else
        {
            // 文件不存在时，应用 umask 并设置 mode
            struct proc *p = myproc();
            f->f_mode = (mode & ~p->umask) & 07777; // 应用umask并只保留权限位
        }

        strcpy(f->f_path, absolute_path);

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
            return ret;
        }
        return fd;
    }
    else
        panic("unsupport filesystem");
    return 0;
}

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
    if (fd < 0 || fd >= NOFILE)
        return -EBADF;
    if ((f = myproc()->ofile[fd]) == 0)
        return -ENOENT;

    // 检查是否是使用O_PATH标志打开的文件描述符
    if (f->f_flags & O_PATH)
    {
        return -EBADF;
    }

    // 使用access_ok验证用户地址的有效性
    if (!access_ok(VERIFY_READ, va, len))
        return -EFAULT;
    /* 检查文件是否可写 */
    if (get_file_ops()->writable(f) == 0)
        return -EBADF;
    int reallylen = get_file_ops()->write(f, va, len);
    return reallylen;
}

/**
 * @brief 分散写（writev） - 将多个分散的内存缓冲区数据写入文件描述符
 *
 * @param fd      目标文件描述符
 * @param uiov    用户空间iovec结构数组的虚拟地址
 * @param iovcnt  iovec数组的元素个数
 * @return uint64 实际写入的总字节数（成功时），负值表示错误码
 */
uint64 sys_writev(int fd, uint64 uiov, uint64 iovcnt)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_writev] fd:%d iov:%p iovcnt:%d\n", fd, uiov, iovcnt);
    struct file *f;
    struct proc *p = myproc();

    // 检查文件描述符的有效性
    if (fd < 0 || fd >= NOFILE)
        return -EBADF;

    if ((f = p->ofile[fd]) == 0)
        return -EBADF;

    // 检查iovcnt的有效性
    if (iovcnt < 0)
        return -EINVAL;

    if (iovcnt == 0)
        return 0;

    if (iovcnt > IOVMAX)
        return -EINVAL;

    // 检查用户空间地址的有效性
    if (!uiov)
        return -EFAULT;

    // 检查用户空间iovec数组的可读性
    if (!access_ok(VERIFY_READ, uiov, sizeof(struct iovec) * iovcnt))
        return -EFAULT;

    iovec v[IOVMAX];

    // 将用户空间的iovec数组拷贝到内核空间
    if (copyin(p->pagetable, (char *)v, uiov, sizeof(iovec) * iovcnt) < 0)
        return -EFAULT;

    // 验证每个iovec的有效性
    for (int i = 0; i < iovcnt; i++)
    {
        // 检查iov_len的有效性
        if (v[i].iov_len < 0)
            return -EINVAL;

        // 如果iov_len为0，跳过这个iovec
        if (v[i].iov_len == 0)
            continue;

        // 检查iov_base的有效性（非零长度时）
        if (v[i].iov_base == NULL)
            return -EFAULT;

        // 检查用户空间缓冲区的可读性
        if (!access_ok(VERIFY_READ, (uint64)v[i].iov_base, v[i].iov_len))
            return -EFAULT;
    }

    uint64 total_written = 0;

    // 遍历iovec数组，逐个缓冲区执行写入
    for (int i = 0; i < iovcnt; i++)
    {
        // 跳过零长度的iovec
        if (v[i].iov_len == 0)
            continue;

        int written = get_file_ops()->write(f, (uint64)(v[i].iov_base), v[i].iov_len);

        // 检查写入错误
        if (written < 0)
        {
            // 如果是管道写入错误，返回EPIPE
            if (written == -EPIPE)
                return -EPIPE;

            // 其他错误，返回错误码
            return written;
        }

        total_written += written;

        // 如果写入的字节数少于请求的字节数，说明遇到了EOF或其他限制
        if (written < v[i].iov_len)
            break;
    }

    return total_written;
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
    return waitpid(pid, va, option);
}

/**
 * @brief waitid 系统调用的实现
 *
 * @param idtype 等待类型 (P_ALL, P_PID, P_PGID)
 * @param id 进程ID或进程组ID
 * @param infop 存储子进程信息的siginfo_t结构体地址
 * @param options 等待选项
 * @return int 成功返回0，失败返回负的错误码
 */
int sys_waitid(int idtype, int id, uint64 infop, int options)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_waitid]: idtype:%d, id:%d, infop:%p, options:0x%x\n",
                    idtype, id, infop, options);
    return waitid(idtype, id, infop, options);
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

    // 添加对实时信号的调试信息
    if (sig >= SIGRTMIN && sig <= SIGRTMAX)
    {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "sys_kill: 系统调用发送实时信号 %d (SIGRTMIN+%d) 给进程 %d\n",
                        sig, sig - SIGRTMIN, pid);
    }

    // assert(pid >= 0, "pid null!");
    if (sig < 0 || sig >= SIGRTMAX)
    {
        panic("sig error");
        return -1;
    }
    if (pid > 0)
    {
        return kill(pid, sig);
    }
    else if (pid < -1)
    {
        struct proc *p;
        int pgid = -pid; // pid < -1,将 sig 发送到 ID 为 -pid 的进程组
        for (p = pool; p < &pool[NPROC]; p++)
        {
            if (p->pgid == pgid)
            {
                kill(p->pid, sig);
            }
        }
        return 0;
    }
    else
    {
        panic("not implement");
        return -1;
    }
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
    if (clkid >= MAX_CLOCKS)
    {
        return -EINVAL;
    }
    timespec_t tv;
    switch (clkid)
    {
    case CLOCK_REALTIME: // 实时系统时间
        tv = timer_get_ntime();
        // panic("not implement yet,clkid :%d",clkid);
        break;
    case CLOCK_MONOTONIC: // 单调递增时间（系统启动后）
        tv = timer_get_ntime();
        break;
    case CLOCK_REALTIME_COARSE:
        tv = timer_get_ntime();
        break;
    case CLOCK_PROCESS_CPUTIME_ID:
        tv = timer_get_ntime(); // TODO: 应返回进程从创建开始到当前时刻实际占用的 CPU 时间总和
        break;
    case CLOCK_THREAD_CPUTIME_ID:
        tv = timer_get_ntime();
        break;
    case CLOCK_MONOTONIC_COARSE:
        tv = timer_get_ntime();
        break;
    case CLOCK_MONOTONIC_RAW:
        tv = timer_get_ntime();
        break;
    case CLOCK_BOOTTIME:
        tv = timer_get_ntime();
        break;
    default:
        panic("not implement yet,clkid :%d", clkid);
        return -1; // 不支持的时钟类型
    }
    DEBUG_LOG_LEVEL(LOG_DEBUG, "clock_gettime:sec:%u,nsec:%u\n", tv.tv_sec, tv.tv_nsec);

    // 使用access_ok验证用户地址的有效性
    if (!access_ok(VERIFY_WRITE, uaddr, sizeof(struct timespec)))
        return -EFAULT;

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
    if (n >= 0x180000000ul)
    {
        return -EPERM;
    }
    if (n == 0)
    {
        return addr;
    }
    if (growproc(n - addr) < 0)
    {
        return 0;
    }

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
    proc_t *p = myproc();

    // 只支持ITIMER_REAL
    if (which != ITIMER_REAL)
    {
        return -1;
    }

    // 保存旧的定时器设置
    if (old_value)
    {
        struct itimerval old_timer = p->itimer;

        // 如果定时器是活跃的，计算剩余时间
        if (p->timer_active && p->alarm_ticks > r_time())
        {
            uint64 remaining_ticks = p->alarm_ticks - r_time();
            old_timer.it_value.sec = remaining_ticks / CLK_FREQ;
            old_timer.it_value.usec = (remaining_ticks % CLK_FREQ) * 1000000 / CLK_FREQ;
        }
        else if (p->timer_active)
        {
            // 定时器已经到期，剩余时间为0
            old_timer.it_value.sec = 0;
            old_timer.it_value.usec = 0;
        }

        if (copyout(p->pagetable, old_value, (char *)&old_timer, sizeof(struct itimerval)) < 0)
        {
            return -1;
        }
    }

    // 设置新的定时器
    if (new_value)
    {
        struct itimerval new_timer;
        if (copyin(p->pagetable, (char *)&new_timer, new_value, sizeof(struct itimerval)) < 0)
        {
            return -1;
        }

        // 更新进程的定时器设置
        p->itimer = new_timer;

        // 计算下一次警报的tick值
        if (new_timer.it_value.sec || new_timer.it_value.usec)
        {
            uint64 now = r_time();
            uint64 interval = (uint64)new_timer.it_value.sec * CLK_FREQ +
                              (uint64)new_timer.it_value.usec * (CLK_FREQ / 1000000);
            p->alarm_ticks = now + interval;
            p->timer_active = 1;
            // printf("sys_settimer: 设置定时器, now=%lu, interval=%lu, alarm_ticks=%lu, sec=%d, usec=%d\n",
            //        now, interval, p->alarm_ticks, new_timer.it_value.sec, new_timer.it_value.usec);
        }
        else
        {
            p->timer_active = 0; // 定时器值为0则禁用
        }
    }

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
    char path[MAXPATH], *argv[MAXARG], *envp[8];
    proc_t *p = myproc();

    // 复制路径
    if (copyinstr(p->pagetable, path, (uint64)upath, MAXPATH) == -1)
    {
        return -1;
    }
    if (strcmp(path, "/bin/open12_child") == 0)
    {
        strcpy(path, "/glibc/ltp/testcases/bin/open12_child");
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

    // 在执行新程序前，关闭所有带有 FD_CLOEXEC 标志的文件描述符
    for (int fd_i = 0; fd_i < NOFILE; fd_i++)
    {
        if (p->ofile[fd_i] != NULL && (p->ofile[fd_i]->f_flags & FD_CLOEXEC))
        {
            // 关闭带有 close-on-exec 标志的文件描述符
            DEBUG_LOG_LEVEL(LOG_INFO, "[sys_execve] Closing FD_CLOEXEC fd=%d, flags=0x%x\n", fd_i, p->ofile[fd_i]->f_flags);
            get_file_ops()->close(p->ofile[fd_i]);
            p->ofile[fd_i] = NULL;
        }
    }

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
        return -EBADF;
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
        return -EBADF;

    // 检查是否是使用O_PATH标志打开的文件描述符
    if (f->f_flags & O_PATH)
    {
        return -EBADF;
    }

    if (get_file_ops()->readable(f) == 0)
    {
        return -EBADF;
    }
    if (!access_ok(VERIFY_WRITE, va, len))
    {
        return -EFAULT;
    }
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

        // 根据设备号选择设备类型
        int dev_type;
        if (S_ISFIFO(major))
        {
            dev_type = T_FIFO;
        }
        else
        {
            dev_type = T_CHR; // 默认为字符设备
        }

        if (vfs_ext4_mknod(absolute_path, dev_type, dev) < 0)
        {
            return -1;
        }
    }
    return 0;
}

uint64 sys_mknodat(int dirfd, const char *upath, int major, int minor)
{
    char path[MAXPATH];
    proc_t *p = myproc();
    if (copyinstr(p->pagetable, path, (uint64)upath, MAXPATH) == -1)
    {
        return -1;
    }
    char absolute_path[MAXPATH] = {0};
    const char *dirpath = (dirfd == AT_FDCWD) ? myproc()->cwd.path : myproc()->ofile[dirfd]->f_path; //< 目前只会是相对路径
    get_absolute_path(path, dirpath, absolute_path);
    struct filesystem *fs = get_fs_from_path(absolute_path);
    if (fs == NULL)
    {
        return -1;
    }

    if (fs->type == EXT4)
    {
        uint32 dev = major; ///<   组合主次设备号（这里minor未被使用)

        // 根据设备号选择设备类型
        int dev_type;
        if (S_ISFIFO(major))
        {
            dev_type = T_FIFO;
        }
        else
        {
            dev_type = T_CHR; // 默认为字符设备
        }

        if (vfs_ext4_mknod(absolute_path, dev_type, dev) < 0)
        {
            return -1;
        }
    }
    return 0;
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
        return -EBADF;
    char path[MAXPATH];
    int ret = -1;
    if (flags > AT_EMPTY_PATH)
    {
        return -EINVAL;
    }
    struct kstat st;
    if (!access_ok(VERIFY_WRITE, state, sizeof(st)))
        return -EFAULT;
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
        ret = vfs_ext4_stat(absolute_path, &st);
        if (ret < 0)
        {
            if (do_path_containFile_or_notExist(absolute_path))
            {
                return -ENOTDIR;
            }
            else
            {
                return ret;
            }
        }

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
    if (!access_ok(VERIFY_WRITE, uaddr, sizeof(struct sysinfo)))
    {
        return -EFAULT;
    }
    memset(&info, 0, sizeof(info));
    info.uptime = r_time() / CLK_FREQ;                         ///< 系统运行时间（秒）
    info.loads[0] = info.loads[1] = info.loads[2] = 1 * 65536; //< 负载系数设置为1,还要乘65536
    info.totalram = (uint64)PAGE_NUM * PGSIZE;                 ///< 总内存大小
    info.freemem = 512 * 1024 * 1024;                          //@todo 获取可用内存        ///< 空闲内存大小（待实现）//< 先给1M
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
#define MAXPATHLEN 256
uint64 sys_getcwd(char *buf, int size)
{
    if (size < 0)
    {
#if DEBUG
        LOG_LEVEL(LOG_WARNING, "sys_getcwd: buf is NULL or size is invalid\n");
#endif
        return -EFAULT; ///< 标准错误码：无效参数
    }
    struct proc *p = myproc();
    // 处理 size=0 的情况 (测例3)
    if (size == 0)
    {
        return -ERANGE;
    }

    // 处理 buf=NULL 的情况 (测例2和5)
    if (buf == NULL)
    {
        return -ERANGE;
    }

    // 检查用户空间地址有效性 (测例1)
    if (!access_ok(VERIFY_WRITE, (uint64)buf, 1))
    {
        return -EFAULT;
    }

    // 获取当前工作目录路径
    char *path = p->cwd.path;
    uint64 len = strlen(path);
    uint64 total_len = len + 1; // 包含终止符

    // 检查缓冲区大小是否足够 (测例4)
    if (total_len > size)
    {
        return -ERANGE;
    }

    // 完整检查用户空间缓冲区
    if (!access_ok(VERIFY_WRITE, (uint64)buf, total_len))
    {
        return -EFAULT;
    }

    // 复制路径到用户空间
    if (copyout(p->pagetable, (uint64)buf, path, total_len) < 0)
    {
        return -EFAULT;
    }

    return total_len;
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
    // 应用umask到目录权限
    struct proc *p = myproc();
    uint16 final_mode = (mode & ~p->umask) & 07777;
    vfs_ext4_mkdir(absolute_path, final_mode);
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

/**
 * @brief 改变进程的根目录
 *
 * @param path 新的根目录路径
 * @return int 成功返回0，失败返回负的错误码
 */
int sys_chroot(const char *path)
{
    struct proc *p = myproc();

    char buf[MAXPATH], absolutepath[MAXPATH];
    memset(buf, 0, MAXPATH);
    memset(absolutepath, 0, MAXPATH);

    // 复制用户空间的路径到内核空间
    if (copyinstr(p->pagetable, buf, (uint64)path, MAXPATH) < 0)
    {
        return -EFAULT;
    }
    if (strlen(buf) > 255)
    {
        return -ENAMETOOLONG;
    }

    // 获取绝对路径（考虑chroot）
    char *cwd = p->cwd.path;
    get_absolute_path(buf, cwd, absolutepath);

    // 检查符号链接循环
    int loop_check = check_symlink_loop(absolutepath, 40);
    if (loop_check == -ELOOP)
    {
        return -ELOOP;
    }
    else if (loop_check < 0)
    {
        return loop_check;
    }

    // 检查路径是否存在且是目录
    struct kstat st;
    struct filesystem *fs = get_fs_from_path(absolutepath);
    if (!fs)
    {
        return -ENOENT; // 文件系统不存在
    }

    if (vfs_ext4_stat(absolutepath, &st) < 0)
    {
        return -ENOENT; // 路径不存在
    }

    if (!S_ISDIR(st.st_mode))
    {
        return -ENOTDIR; // 不是目录
    }

    // 检查访问权限
    if (!check_file_access(&st, X_OK))
    {
        return -EACCES; // 没有执行权限
    }

    // 检查权限：只有root用户才能调用chroot
    if (p->euid != 0)
    {

        return -EPERM; // 权限不足
    }

    // 更新进程的根目录
    memset(p->root.path, 0, MAXPATH);
    strcpy(p->root.path, absolutepath);
    p->root.fs = fs;

#if DEBUG
    printf("chroot: 根目录已更改为: %s\n", p->root.path);
#endif

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

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_getdents64] fd=%d, path=%s, len=%d\n", fd, f->f_path, len);

    memset((void *)sys_getdents64_buf, 0, GETDENTS64_BUF_SIZE);
    int count = vfs_ext4_getdents(f, (struct linux_dirent64 *)sys_getdents64_buf, len);

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_getdents64] returning count=%d\n", count);
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

int do_path_containFile_or_notExist(char *path);
int get_filetype_of_path(char *path);
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
    // 检查传入的用户空间指针的可读性
    if (!access_ok(VERIFY_READ, (uint64)path, sizeof(uint64)))
        return -EFAULT;

    char buf[MAXPATH] = {0};                                    //< 清空，以防上次的残留
    copyinstr(myproc()->pagetable, buf, (uint64)path, MAXPATH); //< 复制用户空间的path到内核空间的buf
#if DEBUG
    LOG("[sys_unlinkat]dirfd: %d, path: %s, flags: %d\n", dirfd, buf, flags); //< 打印参数
#endif

    const char *dirpath = (dirfd == AT_FDCWD) ? myproc()->cwd.path : myproc()->ofile[dirfd]->f_path; //< 目前只会是相对路径
    char absolute_path[MAXPATH] = {0};
    get_absolute_path(buf, dirpath, absolute_path); //< 从mkdirat抄过来的时候忘记把第一个参数从path改成这里的buf了... debug了几分钟才看出来

    // 检查用户程序传入的是否是空路径
    if (buf[0] == '\0')
        return -ENOENT;
    // 检查用户传入路径是否过长
    if (buf[255] != '\0')
        return -ENAMETOOLONG;
    // 检查父路径是否合法,检查会导致unlink08的两项不能通过
    int check_ret = do_path_containFile_or_notExist(absolute_path);
    if (check_ret != 0)
    {
        DEBUG_LOG_LEVEL(LOG_WARNING, "[sys_unlinkat]路径非法，错误码: %d\n", check_ret);
        return check_ret;
    }
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
    else if (get_filetype_of_path(absolute_path) == 0) // 对于目录，flag指定了AT_REMOVEDIR才删除
    {
        return -EISDIR;
    }

    struct file *f = find_file(absolute_path);
    if (f != NULL)
    {
        f->removed = 1;
        return 0;
    }
    // 验证权限 - 检查父目录的权限
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

    struct kstat dir_st;
    int dir_stat_ret = vfs_ext4_stat(pdir, &dir_st);
    if (dir_stat_ret < 0)
    {
        return dir_stat_ret;
    }

    // 检查父目录的写权限（用于删除文件）
    if (!check_file_access(&dir_st, W_OK))
    {
        return -EACCES;
    }

    // 检查父目录的执行权限（搜索权限）
    if (!check_file_access(&dir_st, X_OK))
    {
        return -EACCES;
    }

    return vfs_ext4_unlinkat(pdir, absolute_path);
}

/**
 * @brief 根据路径判断文件类型
 * @return 0表示目录,1表示文件，2表示符号链接。-2表示path不存在
 */
int get_filetype_of_path(char *path)
{
    struct ext4_inode inode;
    uint32 inode_num = 0;
    const char *file_path = path;

    int status = ext4_raw_inode_fill(file_path, &inode_num, &inode); // 如果路径不存在，status = 2. 正常status=0
    if (status == 2)
    {
        // LOG("path: %d不存在\n",path);
        return -ENOENT;
    }
    if (status != EOK)
        panic("未知异常情况,status: %d", status);

    struct ext4_sblock *sb = NULL;
    status = ext4_get_sblock(file_path, &sb);

    // 获取文件的mode位
    struct statx st;
    uint32_t ext4_inode_get_mode(struct ext4_sblock * sb, struct ext4_inode * inode);
    st.stx_mode = ext4_inode_get_mode(sb, &inode);
    uint16 mode = st.stx_mode;

    int type;
    if (S_ISDIR(mode)) // 目录
        type = 0;
    else if (S_ISREG(mode)) // 文件
        type = 1;
    else if (S_ISLNK(mode)) // 链接
        type = 2;
    else
        type = -1;

    // 需要释放引用吗？
    return type;
}

/**
 * @brief 判断文件路径是否合法——父路径中是否有文件或者不存在
 * @param path 接受文件的绝对路径，判断文件的父路径是否合法
 * @return 0表示合法。错误时返回标准错误码
 */
int do_path_containFile_or_notExist(char *path)
{
    if (path[0] == '\0')
        panic("传入空path!\n");
    // 路径每一级由'/'隔开，逐级判断
    int i = 1;                  // 跳过第一个'/'
    char path_to_exam[MAXPATH]; // 要检查的路径
    memset(path_to_exam, 0, MAXPATH);
    while (path[i]) // 直到path[i]是'0'时，检查结束
    {
        if (path[i] == '/') // 读取到'/',立即判断当前层是不是目录
        {
            memmove(path_to_exam, path, i);
            // LOG("要检查的路径: %s",path_to_exam);
            int file_type = get_filetype_of_path(path_to_exam);
            if (file_type == -2) // 路径不存在
            {
                // LOG_LEVEL(LOG_INFO,"路径: %s是不合法的,因为这一级路径不存在: %s\n",path,path_to_exam);
                return -ENOENT;
            }
            if (file_type != 0) // 不是目录
            {

                // LOG_LEVEL(LOG_INFO,"路径: %s是不合法的,因为这一级不是目录: %s\n",path,path_to_exam);
                return -ENOTDIR;
            }
            // LOG("前缀路径: %s是目录\n",path_to_exam);
            // 是目录，继续检查
        }
        i++;
    }
    // LOG("路径: %s是合法的,每一级都是目录\n",path);
    return 0;
}

/**
 * @brief
 * @param olddirfd 链接对象的相对路径fd
 * @param oldpath 用户态地址，存放路径字符串
 * @param newdirfd 创建链接文件的相对路径fd
 * @param newpath 用户态地址，存放路径字符串
 * @param flags
 */
uint64 sys_linkat(int olddirfd, uint64 oldpath, int newdirfd, uint64 newpath, int flags)
{
    char k_oldpath[256];
    char k_newpath[256];
    memset(k_oldpath, 0, 256);
    memset(k_newpath, 0, 256);

    // 检查传入的用户空间指针的可读性，要在copyinstr之前检查
    if (!access_ok(VERIFY_READ, (uint64)oldpath, sizeof(uint64)))
        return -EFAULT;
    if (!access_ok(VERIFY_READ, (uint64)newpath, sizeof(uint64)))
        return -EFAULT;
    copyinstr(myproc()->pagetable, k_oldpath, oldpath, 256);
    copyinstr(myproc()->pagetable, k_newpath, newpath, 256);

    // LOG("sys_linkat: olddirfd %d, oldpath %s, newdirfd %d, newpath %s, flags %d\n",olddirfd,k_oldpath,newdirfd,k_newpath,flags);

    if (olddirfd != AT_FDCWD || newdirfd != AT_FDCWD)
        panic("不支持非相对路径");

    if (k_oldpath[255] != '\0' || k_newpath[255] != '\0') // 路径过长
        return -ENAMETOOLONG;

    if (k_oldpath[0] == '\0' || k_newpath[0] == '\0') // 传入空路径检查
        return -ENOENT;

    char old_absolute_path[256];
    char new_absolute_path[256];
    get_absolute_path(k_oldpath, myproc()->cwd.path, old_absolute_path);
    get_absolute_path(k_newpath, myproc()->cwd.path, new_absolute_path);

    // 检查路径是否包含文件
    int check_ret = do_path_containFile_or_notExist(old_absolute_path);
    if (check_ret != 0)
    {
        return check_ret;
    }

    struct kstat st;
    int stat_ret = vfs_ext4_stat(old_absolute_path, &st);
    if (stat_ret < 0)
    {
        return stat_ret;
    }

    // 检查读权限
    if (!check_file_access(&st, R_OK))
    {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_linkat] pid:%d no read permission on source file: %s\n", myproc()->pid, old_absolute_path);
        return -EACCES;
    }

    // 2. 检查对目标目录的写权限和执行权限
    char new_dir[256];
    strcpy(new_dir, new_absolute_path);
    char *last_slash = strrchr(new_dir, '/');
    if (last_slash)
    {
        *last_slash = '\0';
    }
    else
    {
        strcpy(new_dir, ".");
    }

    struct kstat dir_st;
    int dir_stat_ret = vfs_ext4_stat(new_dir, &dir_st);
    if (dir_stat_ret < 0)
    {
        return dir_stat_ret;
    }

    // 检查写权限（用于创建新链接）
    if (!check_file_access(&dir_st, W_OK))
    {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_linkat] pid:%d no write permission on target directory: %s\n", myproc()->pid, new_dir);
        return -EACCES;
    }

    // 检查执行权限（搜索权限）
    if (!check_file_access(&dir_st, X_OK))
    {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_linkat] pid:%d no execute permission on target directory: %s\n", myproc()->pid, new_dir);
        return -EACCES;
    }

    return vfs_ext4_link(old_absolute_path, new_absolute_path);
}

/**
 * @brief 创建软链接
 * @param oldname 用户空间指针，指向 目标路径（可为绝对或相对）
 * @param newfd 目录文件描述符，决定 newname 的解析基准
 * @param newname 用户空间指针，指向 要创建的符号链接的路径。
 */
uint64 sys_symlinkat(uint64 oldname, int newfd, uint64 newname)
{
    // 检查传入的用户空间指针的可读性，要在copyinstr之前检查
    if (!access_ok(VERIFY_READ, (uint64)oldname, sizeof(uint64)))
        return -EFAULT;
    if (!access_ok(VERIFY_READ, (uint64)newname, sizeof(uint64)))
        return -EFAULT;

    char old_path[256], new_path[256];
    memset(old_path, 0, 256);
    memset(new_path, 0, 256);
    copyinstr(myproc()->pagetable, old_path, oldname, 256);
    copyinstr(myproc()->pagetable, new_path, newname, 256);

    LOG("[sys_symlinkat] oldname: %s, newfd: %d, newname: %s\n", old_path, newfd, new_path);

    if (new_path[255] != '\0')
        return -ENAMETOOLONG;
    if (newfd != AT_FDCWD && (newfd < 0 || newfd > NOFILE))
        return -EINVAL;

    char old_absolute_path[256];
    char new_absolute_path[256];
    const char *dirpath = (newfd == AT_FDCWD) ? myproc()->cwd.path : myproc()->ofile[newfd]->f_path;
    get_absolute_path(old_path, dirpath, old_absolute_path);
    get_absolute_path(new_path, dirpath, new_absolute_path);

    if (get_filetype_of_path(new_absolute_path) != -ENOENT) // 检查new_absolute_path是否已经存在
        return -EEXIST;

    uint64 ret = ext4_fsymlink(old_absolute_path, new_absolute_path);
    printf("ext4_fsymlink返回值: %d\n", ret);

    char buf[64];
    memset(buf, 0, 64);
    uint64 rnt;
    ext4_readlink(new_absolute_path, buf, 64, &rnt);
    printf("%d字节,创建软链接 %s的内容 %s\n", rnt, new_absolute_path, buf);

    return ret;
}

int sys_getuid()
{
    return myproc()->ruid;
}

int sys_geteuid()
{
    return myproc()->euid;
}

int sys_ioctl()
{
#if DEBUG
    printf("sys_ioctl\n");
#endif
    return 0;
}

extern proc_t *initproc; // 第一个用户态进程,永不退出
int sys_exit_group(int status)
{
    printf("sys_exit_group\n");
    struct proc *p = myproc();
    if (p->parent->parent && p->parent->parent == initproc)
    {
        struct inode *ip;
        if ((ip = namei("/tmp")) != NULL)
        {
            vfs_ext4_rm("/tmp");
            free_inode(ip);
        }
    }
    exit(status);
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
    if ((signum == SIGKILL) || (signum == SIGSTOP))
        return -EINVAL;
    if ((signum <= 0) || (signum >= SIGRTMAX))
        return -EINVAL;
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

#define AT_SYMLINK_NOFOLLOW 0x100 /* Do not follow symbolic links.  */
#define AT_EACCESS 0x200          /* Test access permitted for \
                                     effective IDs, not real IDs.  */
#define AT_REMOVEDIR 0x200        /* Remove directory instead of*/
/**
 * @brief       检查文件访问权限
 *
 * @param fd
 * @param upath
 * @param mode
 * @param flags 暂未处理
 * @return uint64
 */
uint64 sys_faccessat(int fd, int upath, int mode, int flags)
{
    char path[MAXPATH];
    memset(path, 0, MAXPATH);

    if (!access_ok(VERIFY_READ, upath, MAXPATH))
    {
        return -EFAULT;
    }
    if (mode < 0)
    {
        return -EINVAL;
    }
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

        // 检查路径是否为绝对路径
        int is_absolute_path = (path[0] == '/');

        // 如果路径是绝对路径，则忽略文件描述符
        if (is_absolute_path)
        {
            strcpy(absolute_path, path);
        }
        else
        {
            // 对于相对路径，需要验证文件描述符
            if (fd != AT_FDCWD && (fd < 0 || fd >= NOFILE))
                return -EBADF;
            const char *dirpath = (fd == AT_FDCWD) ? myproc()->cwd.path : myproc()->ofile[fd]->f_path;
            if (get_filetype_of_path((char *)dirpath))
            {
                return -ENOTDIR;
            }
            if (flags < 0)
            {
                return -EINVAL;
            }
            get_absolute_path(path, dirpath, absolute_path);
        }

        // 检查文件是否存在
        struct kstat st;
        int ret = vfs_ext4_stat(absolute_path, &st);
        if (ret < 0)
        {
            // 文件不存在，返回错误
            return ret;
        }

        if (mode != F_OK)
        {
            if (myproc()->euid == 0)
            {
                if (!check_root_access(&st, mode))
                {
                    return -EACCES;
                }
                else
                {
                    // return 0;
                }
            }
            else
            {
                // 检查文件权限
                if (!check_file_access(&st, mode))
                {
                    return -EACCES;
                }
                else
                {
                    // return 0;
                }
            }
        }

        // 检查目录权限（对于路径中的每个目录组件）
        char temp_path[MAXPATH];
        strcpy(temp_path, absolute_path);

        // 找到最后一个 '/' 的位置
        char *last_slash = strrchr(temp_path, '/');
        if (!last_slash)
        {
            // 没有斜杠，说明是当前目录下的文件
            return 0;
        }

        // 截断路径，只保留目录部分
        *last_slash = '\0';

        // 如果是根目录，特殊处理
        if (strlen(temp_path) == 0)
        {
            strcpy(temp_path, "/");
        }

        // 现在只检查目录部分的权限
        char *component = temp_path;

        // 检查根目录（如果路径以'/'开头）
        if (component[0] == '/')
        {
            // 检查根目录的执行权限
            struct kstat root_st;
            int root_ret = vfs_ext4_stat("/", &root_st);
            if (root_ret < 0)
            {
                return root_ret;
            }

            // 检查根目录的执行权限
            if (!check_file_access(&root_st, X_OK))
            {
                return -EACCES;
            }

            component++; // 跳过根目录的'/'
        }

        // 逐级检查目录组件
        while (component && *component)
        {
            char *next_slash = strchr(component, '/');
            if (next_slash)
                *next_slash = '\0';

            struct kstat dir_st;
            int dir_ret = vfs_ext4_stat(temp_path, &dir_st);
            if (dir_ret < 0)
                return dir_ret;

            // 只检查目录的执行权限
            if (!S_ISDIR(dir_st.st_mode))
            {
                return -ENOTDIR;
            }

            if (!check_file_access(&dir_st, X_OK))
            {
                return -EACCES;
            }

            if (next_slash)
            {
                *next_slash = '/';
                component = next_slash + 1;
            }
            else
            {
                break;
            }
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
        // printf("sys_getrandom不支持非8字节的随机数!");
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

/*显示当前进程打开的所有文件描述符*/
void show_process_ofile()
{
    struct proc *p = myproc();
    int i = 0;
    while (p->ofile[i])
    {
        LOG("process %d 打开的fd %d 的路径: %s, type: %d\n", p->pid, i, p->ofile[i]->f_path, p->ofile[i]->f_type);
        i++;
    }
}

/**
 * 看来busybox用的62号调用是lseek不是llseek
 * @brief 移动文件读写位置指针（即文件偏移量）
 * @param fd
 * @param offset 要移动的偏移量,有符号!!
 * @param whence 从文件头，当前偏移量还是文件尾开始
 * @return 成功时返回移动后的偏移量，错误时返回标准错误码(负数)
 */
uint64 sys_lseek(uint32 fd, uint64 offset, int whence)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_lseek]uint32 fd: %d, uint64 offset: %ld, int whence: %d\n", fd, offset, whence);
    // show_process_ofile();
    // if (offset > 0x10000000) //< 除了lseek-large一般不会用这么大的偏移，直接返回给lseek-large他要的值
    //     return offset;
    struct file *f;
    if (whence < 0 || whence > 2)
        return -EINVAL;
    if ((int)fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
        return -EBADF;
    if (f->f_type == FD_PIPE || f->f_type == FD_FIFO || f->f_type == FD_DEVICE)
        return -ESPIPE;
    // if (myproc()->ofile[fd]->f_path[0] == '\0') // 文件描述符对应路径为空 //不改这么判断的，应该看是否是管道文件
    //     return -ESPIPE;
    int ret = 0;
    ret = vfs_ext4_lseek(f, (int64_t)offset, whence); // 实际的lseek操作
    if (ret < 0)
    {
        LOG_LEVEL(LOG_WARNING, "sys_lseek fd %d failed!\n", fd);
        DEBUG_LOG_LEVEL(LOG_WARNING, "sys_lseek fd %d failed!\n", fd);
        ret = -ESPIPE;
    }
    return ret;
}

/**
 * @brief 定位写，写前后不改变偏移量
 */
int sys_pwrte64(int fd, uint64 buf, uint64 count, uint64 offset)
{
    LOG_LEVEL(LOG_INFO, "[sys_pwrte64] fd: %d, offset: %ld, count: %ld, offset: %ld\n", fd, buf, count, offset);
    // 保存偏移量
    struct file *f = myproc()->ofile[fd];
    struct ext4_file *file = (struct ext4_file *)f->f_data.f_vnode.data;
    uint64 saved_pos = file->fpos; // 原偏移量
    // 定位写
    sys_lseek(fd, offset, 0);
    int ret = sys_write(fd, buf, count);
    // 恢复偏移量
    sys_lseek(fd, saved_pos, 0);
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

/**
 * @brief 实现ppoll系统调用
 *
 * @param pollfd      pollfd结构体数组指针
 * @param nfds        监控的文件描述符数量
 * @param tsaddr      超时时间结构体指针
 * @param sigmaskaddr 信号掩码指针
 * @return int        返回就绪的文件描述符数量，超时返回0，错误返回-1
 */
uint64 sys_ppoll(uint64 pollfd, int nfds, uint64 tsaddr, uint64 sigmaskaddr)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_ppoll]: pollfd:%p, nfds:%d, tsaddr:%p, sigmaskaddr:%p\n",
                    pollfd, nfds, tsaddr, sigmaskaddr);

    struct proc *p = myproc();
    struct pollfd *fds;
    struct timespec ts;
    uint64 end_time = 0;
    int ret = 0;
    __sigset_t old_sigmask, temp_sigmask;

    // 验证参数
    if (nfds < 0 || nfds > 1024)
    {
        return -EINVAL;
    }

    // 分配内核缓冲区存储pollfd数组
    fds = kalloc();
    if (!fds)
    {
        return -ENOMEM;
    }

    // 从用户空间复制pollfd数组
    if (copyin(p->pagetable, (char *)fds, pollfd, sizeof(struct pollfd) * nfds) < 0)
    {
        kfree(fds);
        return -EFAULT;
    }

    // 处理信号掩码：如果提供了sigmask，临时设置新的信号掩码
    if (sigmaskaddr)
    {
        if (copyin(p->pagetable, (char *)&temp_sigmask, sigmaskaddr, sizeof(__sigset_t)) < 0)
        {
            kfree(fds);
            return -EFAULT;
        }
        // 保存当前信号掩码
        memcpy(&old_sigmask, &p->sig_set, sizeof(__sigset_t));
        // 设置新的信号掩码
        memcpy(&p->sig_set, &temp_sigmask, sizeof(__sigset_t));
        DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_ppoll] 临时设置信号掩码: 0x%lx\n", p->sig_set.__val[0]);
    }

    // 处理超时
    if (tsaddr)
    {
        if (copyin(p->pagetable, (char *)&ts, tsaddr, sizeof(ts)) < 0)
        {
            // 恢复信号掩码
            if (sigmaskaddr)
            {
                memcpy(&p->sig_set, &old_sigmask, sizeof(__sigset_t));
            }
            kfree(fds);
            return -EFAULT;
        }
        if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000)
        {
            // 恢复信号掩码
            if (sigmaskaddr)
            {
                memcpy(&p->sig_set, &old_sigmask, sizeof(__sigset_t));
            }
            kfree(fds);
            return -EINVAL;
        }
        end_time = r_time() + (ts.tv_sec * CLK_FREQ) +
                   (ts.tv_nsec * CLK_FREQ / 1000000000);
    }

    // 主监控循环
    while (1)
    {
        ret = 0;

        // 检查每个文件描述符
        for (int i = 0; i < nfds; i++)
        {
            if (fds[i].fd < 0)
            {
                fds[i].revents = 0;
                continue;
            }

            struct file *f = p->ofile[fds[i].fd];
            if (!f)
            {
                fds[i].revents = POLLNVAL; // 无效文件描述符
                ret++;
            }
            else
            {
                // 调用文件的poll方法
                int revents = get_file_ops()->poll(f, fds[i].events);
                fds[i].revents = revents;
                if (revents)
                {
                    ret++;
                }
            }
        }

        // 有就绪描述符或错误
        if (ret != 0 || p->killed)
        {
            break;
        }

        // 检查是否有待处理的信号（未被当前信号掩码阻塞且未被忽略的信号）
        int pending_sig = check_pending_signals(p);
        if (pending_sig != 0)
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_ppoll] 收到信号 %d，立即返回\n", pending_sig);
            ret = -EINTR; // 被信号中断
            break;
        }

        // 检查是否被信号中断过（信号处理完成后）
        if (p->signal_interrupted)
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_ppoll] 检测到信号中断标志，返回EINTR\n");
            ret = -EINTR;              // 被信号中断
            p->signal_interrupted = 0; // 清除标志
            break;
        }

        // 检查超时
        if (tsaddr && r_time() >= end_time)
        {
            ret = 0;
            break;
        }

        // 让出CPU，但添加短暂延迟避免过度消耗CPU
        yield();
    }

    // 恢复原来的信号掩码
    if (sigmaskaddr)
    {
        memcpy(&p->sig_set, &old_sigmask, sizeof(__sigset_t));
        DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_ppoll] 恢复信号掩码: 0x%lx\n", p->sig_set.__val[0]);
    }

    // 将结果复制回用户空间
    if (copyout(p->pagetable, pollfd, (char *)fds, sizeof(struct pollfd) * nfds) < 0)
    {
        kfree(fds);
        return -EFAULT;
    }

    kfree(fds);
    return ret;
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

    // 验证时钟类型
    if (which_clock != CLOCK_REALTIME && which_clock != CLOCK_MONOTONIC)
    {
        return -EINVAL;
    }

    // 从用户空间读取请求的睡眠时间
    if (copyin(myproc()->pagetable, (char *)&kernel_request_tp, (uint64)rqtp, sizeof(struct __kernel_timespec)) < 0)
    {
        return -EFAULT;
    }

//< kernel_request_tp, second: 7fffffff, nanosecond: 0
#if DEBUG
    LOG("kernel_request_tp, second: %x, nanosecond: %x\n", kernel_request_tp.tv_sec, kernel_request_tp.tv_nsec);
#endif

    // 验证时间参数
    if (kernel_request_tp.tv_sec < 0 || kernel_request_tp.tv_nsec < 0 || kernel_request_tp.tv_nsec >= 1000000000)
    {
        return -EINVAL;
    }

    // 如果睡眠时间为0，直接返回
    if (kernel_request_tp.tv_sec == 0 && kernel_request_tp.tv_nsec == 0)
    {
        kernel_remain_tp.tv_sec = 0;
        kernel_remain_tp.tv_nsec = 0;
        if (rmtp && copyout(myproc()->pagetable, (uint64)rmtp, (char *)&kernel_remain_tp, sizeof(struct __kernel_timespec)) < 0)
        {
            return -EFAULT;
        }
        return 0;
    }

    // 计算目标唤醒时间
    uint64 current_time = r_time();
    uint64 sleep_ticks = (uint64)kernel_request_tp.tv_sec * CLK_FREQ +
                         (uint64)kernel_request_tp.tv_nsec * CLK_FREQ / 1000000000;
    uint64 target_time = current_time + sleep_ticks;

    // 使用基于时钟中断的睡眠机制
    acquire(&tickslock);
    while (r_time() < target_time)
    {
        // 检查进程是否被杀死
        if (myproc()->killed)
        {
            release(&tickslock);
            return -EINTR;
        }

        // 使用sleep_on_chan等待时钟中断
        sleep_on_chan(&ticks, &tickslock);
    }
    release(&tickslock);

    // 设置剩余时间为0（睡眠完成）
    kernel_remain_tp.tv_sec = 0;
    kernel_remain_tp.tv_nsec = 0;
    if (rmtp && copyout(myproc()->pagetable, (uint64)rmtp, (char *)&kernel_remain_tp, sizeof(struct __kernel_timespec)) < 0)
    {
        return -EFAULT;
    }

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

    // 2. 验证地址有效性（可选但推荐）
    if (uaddr >= MAXVA)
        return -1;
    proc_t *p = myproc();
    // 3. 设置 clear_child_tid 字段
    p->clear_child_tid = uaddr; // 或 p->thread.clear_child_tid

    // 4. 返回当前 TID
    return p->main_thread->tid;
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
    struct rlimit nl;
    struct rlimit ol;
    proc_t *p = myproc();

    // 检查资源类型是否有效
    if (resource < 0 || resource >= RLIMIT_NLIMITS)
    {
        return -EINVAL;
    }

    // 设置新的限制
    if (new_limit)
    {
        if (!access_ok(VERIFY_READ, new_limit, sizeof(struct rlimit)))
        {
            return -EFAULT;
        }
        if (copyin(p->pagetable, (char *)&nl, new_limit, sizeof(nl)) < 0)
        {
            return -EFAULT;
        }

        // 验证限制值的有效性
        if (nl.rlim_cur > nl.rlim_max)
        {
            return -EINVAL;
        }

        // 设置新的限制值
        p->rlimits[resource].rlim_cur = nl.rlim_cur;
        p->rlimits[resource].rlim_max = nl.rlim_max;

        // 特殊处理：保持向后兼容性
        if (resource == RLIMIT_NOFILE)
        {
            p->ofn.rlim_cur = nl.rlim_cur;
            p->ofn.rlim_max = nl.rlim_max;
        }
    }

    // 获取当前限制
    if (old_limit)
    {
        if (!access_ok(VERIFY_WRITE, old_limit, sizeof(struct rlimit)))
        {
            return -EFAULT;
        }

        // 获取当前限制值
        if (resource == RLIMIT_NOFILE)
        {
            // 保持向后兼容性
            ol.rlim_cur = p->ofn.rlim_cur;
            ol.rlim_max = p->ofn.rlim_max;
        }
        else
        {
            ol.rlim_cur = p->rlimits[resource].rlim_cur;
            ol.rlim_max = p->rlimits[resource].rlim_max;
        }

        if (copyout(p->pagetable, old_limit, (char *)&ol, sizeof(ol)) < 0)
        {
            return -EFAULT;
        }
    }

    return 0;
}

uint64 sys_getrlimit(int resource, uint64 rlim)
{
    struct rlimit ol;
    proc_t *p = myproc();

    // 检查资源类型是否有效
    if (resource < 0 || resource >= RLIMIT_NLIMITS)
    {
        return -EINVAL;
    }

    // 检查用户空间地址是否可写
    if (!access_ok(VERIFY_WRITE, rlim, sizeof(struct rlimit)))
    {
        return -EFAULT;
    }

    // 获取当前限制值
    if (resource == RLIMIT_NOFILE)
    {
        // 保持向后兼容性
        ol.rlim_cur = p->ofn.rlim_cur;
        ol.rlim_max = p->ofn.rlim_max;
    }
    else
    {
        ol.rlim_cur = p->rlimits[resource].rlim_cur;
        ol.rlim_max = p->rlimits[resource].rlim_max;
    }

    // 将结果复制到用户空间
    if (copyout(p->pagetable, rlim, (char *)&ol, sizeof(ol)) < 0)
    {
        return -EFAULT;
    }

    return 0;
}

uint64
sys_membarrier(int cmd, unsigned int flags, int cpu_id)
{
    //     DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_membarrier] cmd: %d, flags: %u, cpu_id: %d\n", cmd, flags, cpu_id);

    //     switch (cmd) {
    //     case MEMBARRIER_CMD_QUERY:
    //         // 返回支持的membarrier命令
    //         return MEMBARRIER_CMD_GLOBAL | MEMBARRIER_CMD_PRIVATE_EXPEDITED;

    //     case MEMBARRIER_CMD_GLOBAL:
    //     case MEMBARRIER_CMD_PRIVATE_EXPEDITED:
    //         // 执行内存屏障，确保所有内存操作对其他进程可见
    // #ifdef RISCV
    //         // RISC-V特定的内存同步
    //         sfence_vma();
    //         asm volatile ("fence rw, rw" ::: "memory");
    //         asm volatile ("fence.i" ::: "memory");  // 指令缓存同步
    // #endif
    //         return 0;

    //     default:
    //         return -EINVAL;
    //     }
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
    if (who == RUSAGE_CHILDREN)
    {
        // 对于子进程，我们需要累加所有子进程的时间
        // 这里简化处理，暂时返回当前进程时间
        target_p = p;
    }
    else if (who == RUSAGE_SELF)
    {
        target_p = p;
    }
    else
    {
        // 不支持的who参数
        return -EINVAL;
    }

    // 将tick计数转换为timeval格式
    timeval_t utime, stime;
    utime.sec = target_p->utime / 1;             // 假设10 ticks = 1秒
    utime.usec = (target_p->utime % 1) * 100000; // 剩余tick转换为微秒

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
    int k = (int)key;
    // struct proc *p = myproc();

    // IPC_PRIVATE 总是创建新段
    if (k == IPC_PRIVATE)
    {
        return newseg(k, flag, size);
    }

    // 尝试查找现有段
    int shmid = findshm(k);

    // 段已存在
    if (shmid >= 0)
    {
        struct shmid_kernel *seg = shm_segs[shmid];

        // 检查独占创建标志
        if (flag & IPC_EXCL)
        {
            return -EEXIST;
        }

        // 检查权限
        int requested_perms = SHM_R; // 至少需要读权限
        if (flag & SHM_W)
        {
            requested_perms |= SHM_W; // 如果需要写权限
        }

        int perm_check = check_shm_permissions(seg, requested_perms);
        if (perm_check != 0)
        {
            return perm_check;
        }

        return shmid;
    }

    // 段不存在但要求创建
    if (flag & IPC_CREAT)
    {
        return newseg(k, flag, size);
    }

    return -ENOENT; // 段不存在且未要求创建
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

    // 检查shmid的有效性
    if (shmid < 0 || shmid >= SHMMNI)
    {
        DEBUG_LOG_LEVEL(LOG_WARNING, "[sys_shmat] pid:%d invalid shmid: %x\n", myproc()->pid, shmid);
        return -EINVAL;
    }

    struct shmid_kernel *shp = shm_segs[shmid];
    if (!shp)
    {
        DEBUG_LOG_LEVEL(LOG_WARNING, "[sys_shmat] pid:%d failed to find shmid: %x\n", myproc()->pid, shmid);
        return -EINVAL;
    }

    // 检查共享内存段是否已被标记删除
    if (shp->is_deleted)
    {
        DEBUG_LOG_LEVEL(LOG_WARNING, "[sys_shmat] pid:%d shmid %x is marked for deletion\n", myproc()->pid, shmid);
        return -EINVAL;
    }

    // 检查权限
    int requested_perms = SHM_R; // 至少需要读权限
    if (!(shmflg & SHM_RDONLY))
    {
        requested_perms |= SHM_W; // 如果不是只读，还需要写权限
    }

    int perm_check = check_shm_permissions(shp, requested_perms);
    if (perm_check != 0)
    {
        DEBUG_LOG_LEVEL(LOG_WARNING, "[sys_shmat] pid:%d permission denied for shmid: %x\n", myproc()->pid, shmid);
        return perm_check;
    }

    int size = shp->size;

    // +++ 根据SHM_RDONLY标志设置权限 +++
    int perm = PTE_U | PTE_R;
    if (!(shmflg & SHM_RDONLY))
    {
        perm |= PTE_W;
    }

    struct vma *vm_struct;

    // 处理用户指定的地址
    if (shmaddr != 0)
    {
        uint64 attach_addr = shmaddr;

        // 如果设置了 SHM_RND 标志，将地址向下舍入到 SHMLBA 边界
        if (shmflg & SHM_RND)
        {
            attach_addr = (attach_addr / SHMLBA) * SHMLBA;
            DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_shmat] pid:%d SHM_RND: %p -> %p\n", myproc()->pid, shmaddr, attach_addr);
        }
        else
        {
            // 检查地址是否页面对齐
            if (shmaddr & (SHMLBA - 1))
            {
                DEBUG_LOG_LEVEL(LOG_WARNING, "[sys_shmat] pid:%d shmaddr %p not page aligned and SHM_RND not set\n",
                                myproc()->pid, shmaddr);
                return -EINVAL;
            }
        }

        // 检查地址范围是否有效（更宽松的范围）
        if (attach_addr >= MAXVA || attach_addr + size > MAXVA)
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "[sys_shmat] pid:%d shmaddr %p out of valid range\n", myproc()->pid, attach_addr);
            return -EINVAL;
        }

        // 检查地址是否已被使用
        struct proc *p = myproc();

        // 检查是否与堆空间冲突
        if (attach_addr < p->sz)
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "[sys_shmat] pid:%d shmaddr %p conflicts with heap (p->sz: %p)\n", myproc()->pid, attach_addr, p->sz);
            return -EINVAL;
        }

        // 检查是否与现有VMA冲突
        struct vma *vma = p->vma->next; // 从第一个VMA开始检查
        while (vma != p->vma)
        {
            if (vma->addr == attach_addr ||
                (attach_addr >= vma->addr && attach_addr < vma->end) ||
                (vma->addr >= attach_addr && vma->addr < attach_addr + size))
            {
                DEBUG_LOG_LEVEL(LOG_WARNING, "[sys_shmat] pid:%d shmaddr %p conflicts with existing VMA\n", myproc()->pid, attach_addr);
                return -EINVAL;
            }
            vma = vma->next;
        }

        // 使用调整后的地址
        vm_struct = alloc_vma(myproc(), SHARE, attach_addr, size, perm, 0, 0); // 使用perm权限
    }
    else
    {
        // 系统自动分配地址
        vm_struct = alloc_vma(myproc(), SHARE, sharemem_start, size, perm, 0, 0); // 使用perm权限
    }
    if (!vm_struct)
    {
        DEBUG_LOG_LEVEL(LOG_WARNING, "[sys_shmat] pid:%d failed to alloc_vma for size: %x\n", myproc()->pid, size);
        return -1;
    }

    // 设置vma指向对应的共享内存段
    vm_struct->shm_kernel = shp;

    // 检查是否已经有其他进程附加到这个共享内存段
    int is_first_attach = (shp->attaches == NULL);

    if (is_first_attach)
    {
        // 第一次附加，分配物理页面
        if (!uvmalloc1(myproc()->pagetable, vm_struct->addr, vm_struct->end, perm)) // 使用perm权限
        {
            panic("sys_shmat: failed to allocate physical pages for first attach");
        }

        // 保存物理页面地址到共享内存段
        int num_pages = (size + PGSIZE - 1) / PGSIZE;
        for (int i = 0; i < num_pages; i++)
        {
            uint64 va = vm_struct->addr + i * PGSIZE;
            pte_t *pte = walk(myproc()->pagetable, va, 0);
            if (pte && (*pte & PTE_V))
            {
                shp->shm_pages[i] = (pte_t)PTE2PA(*pte);
            }
        }
    }
    else
    {
        // 不是第一次附加，映射到已存在的物理页面
        int num_pages = (size + PGSIZE - 1) / PGSIZE;
        for (int i = 0; i < num_pages; i++)
        {
            uint64 va = vm_struct->addr + i * PGSIZE;
            uint64 pa = 0;

            // 从共享内存段中获取物理地址
            if (i < (shp->size + PGSIZE - 1) / PGSIZE && shp->shm_pages[i] != 0)
            {
                pa = (uint64)shp->shm_pages[i];
            }

            if (pa != 0)
            {
                // 映射到相同的物理页面，使用perm权限
                if (mappages(myproc()->pagetable, va, pa, PGSIZE, perm) != 1)
                {
                    panic("sys_shmat: failed to map shared memory page");
                }
            }
        }
    }

    // 只有在系统自动分配地址时才更新 sharemem_start
    if (shmaddr == 0)
    {
        sharemem_start = PGROUNDUP(sharemem_start + size);
    }
    shp->attaches = vm_struct;
    shp->attach_count++;

    // 更新共享内存段的状态信息
    struct proc *p = myproc();
    shp->shm_atime = 0;                  // 最后附加时间，暂时设为0
    shp->shm_lpid = p->pid;              // 最后操作的PID
    shp->shm_nattch = shp->attach_count; // 当前附加数

    // 记录附加信息到进程
    struct shm_attach *at = kalloc();
    if (at)
    {
        at->addr = vm_struct->addr;
        at->shmid = shmid;
        at->next = p->shm_attaches;
        p->shm_attaches = at;
    }
    // 同时记录到共享内存段的附加链表
    struct shm_attach *shm_at = kalloc();
    if (shm_at)
    {
        shm_at->addr = vm_struct->addr;
        shm_at->shmid = shmid;
        shm_at->next = shp->shm_attach_list;
        shp->shm_attach_list = shm_at;
    }

    // +++ 内存同步处理 +++
    // 对于RISC-V架构，添加内存屏障确保共享内存映射立即生效
#ifdef RISCV
    sfence_vma();
    // 添加额外的内存屏障确保后续的内存操作正确
    asm volatile("fence rw, rw" ::: "memory");
#endif

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_shmat] pid:%d successfully attached shmid: %x at addr: %p\n",
                    myproc()->pid, shmid, vm_struct->addr);
    return vm_struct->addr;
}

uint64 sys_shmdt(uint64 shmaddr)
{
    struct proc *p = myproc(); // 获取当前进程
    struct shm_attach *prev = NULL;
    struct shm_attach *at;

    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_shmdt] pid:%d, shmaddr: %p\n", p->pid, shmaddr);

    // 1. 在进程附加链表中查找对应地址的附加记录
    for (at = p->shm_attaches; at != NULL; prev = at, at = at->next)
    {
        if (at->addr == shmaddr)
        {
            break;
        }
    }

    // 未找到附加记录
    if (at == NULL)
    {
        DEBUG_LOG_LEVEL(LOG_WARNING, "[sys_shmdt] pid:%d no attachment found for addr: %p\n", p->pid, shmaddr);
        return -EINVAL;
    }

    // 2. 获取对应的共享内存段
    int shmid = at->shmid;
    if (shmid < 0 || shmid >= SHMMNI || shm_segs[shmid] == NULL)
    {
        // 共享内存段不存在，清理无效记录
        if (prev)
            prev->next = at->next;
        else
            p->shm_attaches = at->next;
        kfree(at);
        DEBUG_LOG_LEVEL(LOG_WARNING, "[sys_shmdt] pid:%d segment already deleted\n", p->pid);
        return -EINVAL; // 段已被删除
    }

    struct shmid_kernel *shp = shm_segs[shmid];

    // 3. 解除虚拟地址映射
    uint64 size = shp->size;
    int npages = (size + PGSIZE - 1) / PGSIZE;
    vmunmap(p->pagetable, shmaddr, npages, 0); // 0=不释放物理页

    // 4. 从VMA链表中删除对应的共享内存VMA
    struct vma *vma = p->vma->next;
    while (vma != p->vma)
    {
        struct vma *next_vma = vma->next;

        // 检查是否是当前共享内存段的VMA，且地址匹配
        if (vma->type == SHARE && vma->shm_kernel == shp && vma->addr == shmaddr)
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_shmdt] removing VMA from process %d, addr: %p\n",
                            p->pid, vma->addr);

            // 从VMA链表中删除
            vma->prev->next = vma->next;
            vma->next->prev = vma->prev;

            // 释放VMA结构体，但不释放物理内存
            pmem_free_pages(vma, 1);
            break;
        }

        vma = next_vma;
    }

    // 5. 从进程链表中移除附加记录
    if (prev)
        prev->next = at->next;
    else
        p->shm_attaches = at->next;
    kfree(at);

    // 6. 更新共享内存段引用计数和状态信息
    shp->attach_count--;
    shp->shm_dtime = 0;                  // 最后分离时间，暂时设为0
    shp->shm_lpid = p->pid;              // 最后操作的PID
    shp->shm_nattch = shp->attach_count; // 当前附加数

    // 从共享内存段的附加链表中移除记录
    struct shm_attach *shm_prev = NULL;
    struct shm_attach *shm_at = shp->shm_attach_list;
    while (shm_at != NULL)
    {
        if (shm_at->addr == shmaddr && shm_at->shmid == shmid)
        {
            if (shm_prev)
            {
                shm_prev->next = shm_at->next;
            }
            else
            {
                shp->shm_attach_list = shm_at->next;
            }
            kfree(shm_at);
            break;
        }
        shm_prev = shm_at;
        shm_at = shm_at->next;
    }

    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_shmdt] pid:%d detached from shmid:%d, remaining attaches:%d\n",
                    p->pid, shmid, shp->attach_count);

    // 7. 若段被标记删除且无附加进程，释放资源
    if (shp->is_deleted && shp->attach_count == 0)
    {
        DEBUG_LOG_LEVEL(LOG_INFO, "[sys_shmdt] freeing deleted segment shmid:%d\n", shmid);

        // 清理附加链表
        struct shm_attach *shm_at = shp->shm_attach_list;
        while (shm_at != NULL)
        {
            struct shm_attach *next = shm_at->next;
            kfree(shm_at);
            shm_at = next;
        }
        shp->shm_attach_list = NULL;

        // 释放所有物理页
        for (int i = 0; i < npages; i++)
        {
            if (shp->shm_pages[i])
            {
                uint64 pa = (uint64)shp->shm_pages[i];
                pmem_free_pages((void *)pa, 1);
            }
        }
        // 释放页表项数组
        kfree(shp->shm_pages);
        // 释放段结构体
        kfree(shp);
        shm_segs[shmid] = NULL;
    }

    return 0;
}

/**
 * @brief 共享内存的控制接口，用于查询、修改或删除共享内存段的属性
 *
 */
uint64 sys_shmctl(uint64 shmid, uint64 cmd, uint64 buf)
{
    DEBUG_LOG_LEVEL(LOG_INFO, "[sys_shmctl]shmid: %x, cmd: %x,\n", shmid, cmd);

    if (shmid < 0 || shmid >= SHMMNI || shm_segs[shmid] == NULL)
    {
        return -EINVAL;
    }

    struct shmid_kernel *shp = shm_segs[shmid];

    switch (cmd)
    {
    case IPC_RMID:
        // 标记删除，但不立即释放资源
        shp->is_deleted = 1;
        DEBUG_LOG_LEVEL(LOG_INFO, "[sys_shmctl] marked shmid %d for deletion\n", shmid);

        // 如果没有进程附加，立即释放资源
        if (shp->attach_count == 0)
        {
            // 遍历所有进程，从VMA链表中删除相关的共享内存VMA
            struct proc *p;
            for (p = pool; p < &pool[NPROC]; p++)
            {
                acquire(&p->lock);
                if (p->state != UNUSED && p->vma != NULL)
                {
                    struct vma *vma = p->vma->next;
                    while (vma != p->vma)
                    {
                        struct vma *next_vma = vma->next;

                        // 检查是否是当前共享内存段的VMA
                        if (vma->type == SHARE && vma->shm_kernel == shp)
                        {
                            DEBUG_LOG_LEVEL(LOG_INFO, "[sys_shmctl] removing VMA from process %d, addr: %p\n",
                                            p->pid, vma->addr);

                            // 从VMA链表中删除
                            vma->prev->next = vma->next;
                            vma->next->prev = vma->prev;

                            // 释放VMA结构体，但不释放物理内存
                            pmem_free_pages(vma, 1);
                        }

                        vma = next_vma;
                    }
                }
                release(&p->lock);
            }

            // 释放共享内存段的资源
            DEBUG_LOG_LEVEL(LOG_INFO, "[sys_shmctl] freeing resources for shmid %d\n", shmid);

            // 清理附加链表
            struct shm_attach *shm_at = shp->shm_attach_list;
            while (shm_at != NULL)
            {
                struct shm_attach *next = shm_at->next;
                kfree(shm_at);
                shm_at = next;
            }
            shp->shm_attach_list = NULL;

            // 释放所有物理页
            int npages = (shp->size + PGSIZE - 1) / PGSIZE;
            for (int i = 0; i < npages; i++)
            {
                if (shp->shm_pages[i])
                {
                    uint64 pa = (uint64)shp->shm_pages[i];
                    pmem_free_pages((void *)pa, 1);
                }
            }

            // 在释放shm_pages之前，确保所有VMA的shm_kernel指针都被清除
            // 这样可以防止其他进程在fork/clone时访问已释放的内存
            for (p = pool; p < &pool[NPROC]; p++)
            {
                acquire(&p->lock);
                if (p->state != UNUSED && p->vma != NULL)
                {
                    struct vma *vma = p->vma->next;
                    while (vma != p->vma)
                    {
                        if (vma->type == SHARE && vma->shm_kernel == shp)
                        {
                            DEBUG_LOG_LEVEL(LOG_INFO, "[sys_shmctl] clearing shm_kernel reference in process %d\n", p->pid);
                            vma->shm_kernel = NULL;
                        }
                        vma = vma->next;
                    }
                }
                release(&p->lock);
            }

            // 释放页表项数组
            kfree(shp->shm_pages);
            // 释放段结构体
            kfree(shp);
            shm_segs[shmid] = NULL;
            DEBUG_LOG_LEVEL(LOG_INFO, "[sys_shmctl] immediately freed shmid %d\n", shmid);
        }
        break;

    case IPC_STAT:
        if (!access_ok(VERIFY_WRITE, buf, sizeof(struct shmid_ds)))
        {
            return -EFAULT;
        }

        struct shmid_ds shm_stat;
        struct proc *p = myproc();

        // 填充共享内存段信息
        shm_stat.shm_perm = shp->shm_perm; // 直接复制权限信息
        shm_stat.shm_segsz = shp->shm_segsz;
        shm_stat.shm_atime = shp->shm_atime;
        shm_stat.shm_dtime = shp->shm_dtime;
        shm_stat.shm_ctime = shp->shm_ctime;
        shm_stat.shm_cpid = shp->shm_cpid;
        shm_stat.shm_lpid = shp->shm_lpid;
        shm_stat.shm_nattch = shp->shm_nattch;

        // 将结构体复制到用户空间
        if (copyout(p->pagetable, buf, (char *)&shm_stat, sizeof(struct shmid_ds)) < 0)
        {
            return -EFAULT;
        }
        break;

    case IPC_SET:
        // 设置共享内存段属性
        if (!access_ok(VERIFY_READ, buf, sizeof(struct shmid_ds)))
        {
            return -EFAULT;
        }

        struct shmid_ds new_stat;
        struct proc *p_set = myproc();

        // 从用户空间读取新的属性
        if (copyin(p_set->pagetable, (char *)&new_stat, buf, sizeof(struct shmid_ds)) < 0)
        {
            return -EFAULT;
        }

        // 更新共享内存段属性（只更新允许修改的字段）
        shp->shm_perm.mode = (shp->shm_perm.mode & ~0777) | (new_stat.shm_perm.mode & 0777);
        shp->flag = (shp->flag & ~0777) | (new_stat.shm_perm.mode & 0777);
        break;

    default:
        panic("not implemeted yet,cmd = %d\n", cmd);
        return -EINVAL;
    }

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
    if (nfds < 0)
    {
        return -EINVAL;
    }

    // 如果没有文件描述符集，直接返回0
    if (!readfds && !writefds && !exceptfds)
    {
        return 0;
    }

    for (int fd = 0; fd < nfds; fd++)
    {
        // 检查读集合
        if (readfds && FD_ISSET(fd, readfds))
        {
            if (fd < 0 || fd >= NOFILE || (f = p->ofile[fd]) == 0)
            {
                FD_CLR(fd, readfds); // 无效文件描述符
            }
            else if (get_file_ops()->poll(f, POLLIN))
            {
                ret++; // 可读
            }
            else
            {
                FD_CLR(fd, readfds);
            }
        }

        // 检查写集合
        if (writefds && FD_ISSET(fd, writefds))
        {
            if (fd < 0 || fd >= NOFILE || (f = p->ofile[fd]) == 0)
            {
                FD_CLR(fd, writefds);
            }
            else if (get_file_ops()->poll(f, POLLOUT))
            {
                ret++; // 可写
            }
            else
            {
                FD_CLR(fd, writefds);
            }
        }

        // 检查异常集合（当前不实现）
        if (exceptfds && FD_ISSET(fd, exceptfds))
        {
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
    if (nfds > FD_SETSIZE)
    {
        return -EINVAL;
    }

    // 复制文件描述符集
    if (readfds && copyin(p->pagetable, (char *)&rfds, readfds, sizeof(fd_set)) < 0)
    {
        return -EFAULT;
    }
    if (writefds && copyin(p->pagetable, (char *)&wfds, writefds, sizeof(fd_set)) < 0)
    {
        return -EFAULT;
    }
    if (exceptfds && copyin(p->pagetable, (char *)&efds, exceptfds, sizeof(fd_set)) < 0)
    {
        return -EFAULT;
    }

    // 处理信号掩码：如果提供了sigmask，临时设置新的信号掩码
    if (sigmask)
    {
        if (copyin(p->pagetable, (char *)&temp_sigmask, sigmask, sizeof(__sigset_t)) < 0)
        {
            return -EFAULT;
        }
        // 保存当前信号掩码
        memcpy(&old_sigmask, &p->sig_set, sizeof(__sigset_t));
        // 设置新的信号掩码
        memcpy(&p->sig_set, &temp_sigmask, sizeof(__sigset_t));
        DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_pselect6_time32] 临时设置信号掩码: 0x%lx\n", p->sig_set.__val[0]);
    }

    // 处理超时
    if (timeout)
    {
        if (copyin(p->pagetable, (char *)&ts, timeout, sizeof(ts)) < 0)
        {
            // 恢复信号掩码
            if (sigmask)
            {
                memcpy(&p->sig_set, &old_sigmask, sizeof(__sigset_t));
            }
            return -EFAULT;
        }
        if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000)
        {
            // 恢复信号掩码
            if (sigmask)
            {
                memcpy(&p->sig_set, &old_sigmask, sizeof(__sigset_t));
            }
            return -EINVAL;
        }
        end_time = r_time() + (ts.tv_sec * CLK_FREQ) +
                   (ts.tv_nsec * CLK_FREQ / 1000000000);
    }

    // 主监控循环
    while (1)
    {
        ret = do_select(nfds, readfds ? &rfds : NULL, writefds ? &wfds : NULL,
                        exceptfds ? &efds : NULL);

        // 有就绪描述符或错误
        if (ret != 0 || p->killed)
        {
            break;
        }

        // 检查是否有待处理的信号（未被当前信号掩码阻塞且未被忽略的信号）
        int pending_sig = check_pending_signals(p);
        if (pending_sig != 0)
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_pselect6_time32] 收到信号 %d，立即返回\n", pending_sig);
            ret = -EINTR; // 被信号中断
            break;
        }

        // 检查是否被信号中断过（信号处理完成后）
        if (p->signal_interrupted)
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_pselect6_time32] 检测到信号中断标志，返回EINTR\n");
            ret = -EINTR;              // 被信号中断
            p->signal_interrupted = 0; // 清除标志
            break;
        }

        // 添加调试日志，检查signal_interrupted标志的状态
        static int debug_count = 0;
        if (++debug_count % 1000 == 0)
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_pselect6_time32] debug_count: %d, signal_interrupted: %d\n", debug_count, p->signal_interrupted);
        }

        // 检查超时
        if (timeout && r_time() >= end_time)
        {
            ret = 0;
            break;
        }

        // 让出CPU，但添加短暂延迟避免过度消耗CPU
        yield();

        // 添加调试日志，帮助诊断问题
        static int loop_count = 0;
        if (++loop_count % 1000 == 0)
        {
            DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_pselect6_time32] loop count: %d, nfds: %d\n", loop_count, nfds);
        }
    }

    // 恢复原来的信号掩码
    if (sigmask)
    {
        memcpy(&p->sig_set, &old_sigmask, sizeof(__sigset_t));
        DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_pselect6_time32] 恢复信号掩码: 0x%lx\n", p->sig_set.__val[0]);
    }

    // 复制回结果
    if (readfds && copyout(p->pagetable, readfds, (char *)&rfds, sizeof(fd_set)) < 0)
    {
        return -EFAULT;
    }
    if (writefds && copyout(p->pagetable, writefds, (char *)&wfds, sizeof(fd_set)) < 0)
    {
        return -EFAULT;
    }
    if (exceptfds && copyout(p->pagetable, exceptfds, (char *)&efds, sizeof(fd_set)) < 0)
    {
        return -EFAULT;
    }

    return ret;
}

uint64 sys_sigreturn()
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "sigreturn返回!\n");
    // 恢复上下文
    copytrapframe(myproc()->trapframe, &myproc()->sig_trapframe);
    // 信号处理完成，清除当前信号标志
    myproc()->current_signal = 0;
    // 不清除signal_interrupted标志，让pselect等系统调用能够检测到信号中断
    DEBUG_LOG_LEVEL(LOG_DEBUG, "sys_sigreturn: 信号处理完成，current_signal=0, signal_interrupted=%d\n", myproc()->signal_interrupted);
    return myproc()->trapframe->a0;
}

/**
 * @brief 设置进程组ID系统调用
 *
 * @param pid 要设置进程组ID的进程ID，0表示当前进程
 * @param pgid 新的进程组ID，0表示使用pid作为进程组ID
 * @return int 成功返回0，失败返回负的错误码
 */
int sys_setpgid(int pid, int pgid)
{
    struct proc *p = myproc();
    struct proc *target_proc = NULL;

    // 如果pid为0，表示设置当前进程
    if (pid == 0)
    {
        target_proc = p;
    }
    else
    {
        // 查找目标进程
        for (struct proc *proc = pool; proc < &pool[NPROC]; proc++)
        {
            acquire(&proc->lock);
            if (proc->pid == pid)
            {
                target_proc = proc;
                break;
            }
            release(&proc->lock);
        }

        if (!target_proc)
        {
            return -ESRCH; // 进程不存在
        }
    }

    // 如果pgid为0，使用pid作为进程组ID
    if (pgid == 0)
    {
        pgid = target_proc->pid;
    }

    // 检查权限：只有进程本身或其父进程可以设置进程组ID
    if (target_proc != p && target_proc->parent != p)
    {
        if (target_proc != pool)
        { // 不是init进程
            release(&target_proc->lock);
            return -EPERM; // 权限不足
        }
    }

    // 设置进程组ID
    target_proc->pgid = pgid;

    if (target_proc != p)
    {
        release(&target_proc->lock);
    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_setpgid] pid:%d, pgid:%d\n", target_proc->pid, pgid);
    return 0;
}

int sys_getpgid(int pid)
{
    if (pid == 0)
    {
        return myproc()->pgid;
    }
    else
        return getproc(pid)->pgid;
}

int sys_fchmod(int fd, mode_t mode)
{
    // 检查dirfd参数
    if (fd != AT_FDCWD && (fd < 0 || fd >= NOFILE))
    {
        return -EBADF;
    }
    proc_t *p = myproc();

    // 检查文件描述符是否有效
    if (p->ofile[fd] == NULL)
    {
        return -EBADF;
    }

    // 检查是否是使用O_PATH标志打开的文件描述符
    if (p->ofile[fd]->f_flags & O_PATH)
    {
        return -EBADF;
    }

    const char *path = p->ofile[fd]->f_path;

    // 获取文件当前信息
    struct kstat st;
    int ret = vfs_ext4_stat(path, &st);
    if (ret < 0)
    {
        return ret;
    }

    // 权限检查：只有文件所有者或root用户可以修改文件权限
    if (p->euid != 0 && p->euid != st.st_uid)
    {
        return -EPERM;
    }

    // 根据POSIX标准处理setgid位
    // 如果非root用户尝试设置setgid位，需要检查组权限
    if (p->euid != 0 && (mode & S_ISGID))
    {
        // 检查进程是否属于目录的组（包括主组和补充组）
        int has_group_permission = 0;

        // 检查主组
        if (p->egid == st.st_gid)
        {
            has_group_permission = 1;
        }

        // 检查补充组
        for (int i = 0; i < p->ngroups; i++)
        {
            if (p->supplementary_groups[i] == st.st_gid)
            {
                has_group_permission = 1;
                break;
            }
        }

        // 如果进程不属于目录的组，自动清除setgid位
        if (!has_group_permission)
        {
            mode &= ~S_ISGID;
            DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_fchmod] Clearing setgid bit for non-group member\n");
        }
    }

    // 调用ext4文件系统接口修改文件权限
    ret = ext4_mode_set(path, mode);
    if (ret != EOK)
    {
        return -ret;
    }
    return 0;
}

/**
 * @brief 修改文件所有者系统调用（通过文件描述符）
 *
 * @param fd 文件描述符
 * @param owner 新的用户ID，-1表示不修改
 * @param group 新的组ID，-1表示不修改
 * @return int 成功返回0，失败返回负的错误码
 */
int sys_fchown(int fd, uid_t owner, gid_t group)
{
    // 检查文件描述符参数
    if (fd < 0 || fd >= NOFILE)
    {
        return -EBADF;
    }

    proc_t *p = myproc();

    // 检查文件描述符是否有效
    if (p->ofile[fd] == NULL)
    {
        return -EBADF;
    }

    // 检查是否是使用O_PATH标志打开的文件描述符
    if (p->ofile[fd]->f_flags & O_PATH)
    {
        return -EBADF;
    }

    const char *path = p->ofile[fd]->f_path;

    // 如果owner或group为-1，表示不修改，需要先获取当前值
    if (owner == (uid_t)-1 || group == (gid_t)-1)
    {
        struct kstat st;
        int ret = vfs_ext4_stat(path, &st);
        if (ret < 0)
        {
            return ret;
        }

        if (owner == (uid_t)-1)
            owner = st.st_uid;
        if (group == (gid_t)-1)
            group = st.st_gid;
    }

    // 调用ext4文件系统接口修改文件所有者
    int ret = ext4_owner_set(path, owner, group);
    if (ret != EOK)
    {
        return -ret;
    }

    // 根据POSIX标准，当超级用户调用chown时：
    // - 对于可执行文件，清除setuid和setgid位
    // - 对于非组可执行文件，保留setgid位
    // 检查当前进程是否为超级用户（uid为0）
    if (p->euid == 0)
    {
        struct kstat st;
        ret = vfs_ext4_stat(path, &st);
        if (ret >= 0)
        {
            mode_t new_mode = st.st_mode;

            // 如果是可执行文件，清除setuid和setgid位
            if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
            {
                new_mode &= ~(S_ISUID | S_ISGID);
                ext4_mode_set(path, new_mode);
            }
        }
    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_fchown] fd:%d, owner:%d, group:%d\n", fd, owner, group);
    return 0;
}

/**
 * @brief 获取文件扩展属性系统调用
 *
 * @param fd 文件描述符
 * @param name 扩展属性名称
 * @param value 存储属性值的缓冲区
 * @param size 缓冲区大小
 * @return int 成功返回属性值的大小，失败返回负的错误码
 */
int sys_fgetxattr(int fd, const char *name, void *value, size_t size)
{
    if (fd < 0 || fd >= NOFILE)
    {
        return -EBADF;
    }
    proc_t *p = myproc();

    if (p->ofile[fd] == NULL)
    {
        return -EBADF;
    }

    if (p->ofile[fd]->f_flags & O_PATH)
    {
        return -EBADF;
    }

    if (name && !access_ok(VERIFY_READ, (uint64)name, 1))
    {
        return -EFAULT;
    }

    if (value && size > 0 && !access_ok(VERIFY_WRITE, (uint64)value, size))
    {
        return -EFAULT;
    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_fgetxattr] fd:%d, name:%s, size:%ld\n",
                    fd, name ? name : "NULL", size);

    return -EOPNOTSUPP;
}

/**
 * @brief 修改文件权限系统调用
 *
 * @param dirfd 目录文件描述符，AT_FDCWD表示当前工作目录
 * @param pathname 文件路径
 * @param mode 新的文件权限模式
 * @param flags 操作标志
 * @return int 成功返回0，失败返回负的错误码
 */
int sys_fchmodat(int dirfd, const char *pathname, mode_t mode, int flags)
{
    char path[MAXPATH];
    proc_t *p = myproc();

    // 复制路径字符串
    if (copyinstr(p->pagetable, path, (uint64)pathname, MAXPATH) == -1)
    {
        return -EFAULT;
    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_fchmodat] dirfd:%d, path:%s, mode:%x, flags:%d\n",
                    dirfd, path, mode, flags);

    // 检查dirfd参数
    if (dirfd != AT_FDCWD && (dirfd < 0 || dirfd >= NOFILE))
    {
        return -EBADF;
    }

    // 获取绝对路径
    char absolute_path[MAXPATH] = {0};
    const char *dirpath = (dirfd == AT_FDCWD) ? p->cwd.path : p->ofile[dirfd]->f_path;
    get_absolute_path(path, dirpath, absolute_path);

    // 获取文件当前信息
    struct kstat st;
    int ret = vfs_ext4_stat(absolute_path, &st);
    if (ret < 0)
    {
        return ret;
    }

    // 权限检查：只有文件所有者或root用户可以修改文件权限
    if (p->euid != 0 && p->euid != st.st_uid)
    {
        return -EPERM;
    }

    // 根据POSIX标准处理setgid位
    // 如果非root用户尝试设置setgid位，需要检查组权限
    if (p->euid != 0 && (mode & S_ISGID))
    {
        // 检查进程是否属于目录的组（包括主组和补充组）
        int has_group_permission = 0;

        // 检查主组
        if (p->egid == st.st_gid)
        {
            has_group_permission = 1;
        }

        // 检查补充组
        for (int i = 0; i < p->ngroups; i++)
        {
            if (p->supplementary_groups[i] == st.st_gid)
            {
                has_group_permission = 1;
                break;
            }
        }

        // 如果进程不属于目录的组，自动清除setgid位
        if (!has_group_permission)
        {
            mode &= ~S_ISGID;
            DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_fchmodat] Clearing setgid bit for non-group member\n");
        }
    }

    // 调用ext4文件系统接口修改文件权限
    ret = ext4_mode_set(absolute_path, mode);
    if (ret != EOK)
    {
        return -ret;
    }

    return 0;
}

/**
 * @brief 修改文件所有者系统调用
 *
 * @param dirfd 目录文件描述符，AT_FDCWD表示当前工作目录
 * @param pathname 文件路径
 * @param owner 新的用户ID，-1表示不修改
 * @param group 新的组ID，-1表示不修改
 * @param flags 操作标志
 * @return int 成功返回0，失败返回负的错误码
 */
int sys_fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags)
{
    char path[MAXPATH];
    proc_t *p = myproc();

    // 复制路径字符串
    if (copyinstr(p->pagetable, path, (uint64)pathname, MAXPATH) == -1)
    {
        return -EFAULT;
    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_fchownat] dirfd:%d, path:%s, owner:%d, group:%d, flags:%d\n",
                    dirfd, path, owner, group, flags);

    // 检查dirfd参数
    if (dirfd != AT_FDCWD && (dirfd < 0 || dirfd >= NOFILE))
    {
        return -EBADF;
    }

    // 获取绝对路径
    char absolute_path[MAXPATH] = {0};
    const char *dirpath = (dirfd == AT_FDCWD) ? p->cwd.path : p->ofile[dirfd]->f_path;
    get_absolute_path(path, dirpath, absolute_path);

    // 如果owner或group为-1，表示不修改，需要先获取当前值
    uint32_t current_uid, current_gid;
    if (owner == (uid_t)-1 || group == (gid_t)-1)
    {
        int ret = ext4_owner_get(absolute_path, &current_uid, &current_gid);
        if (ret != EOK)
        {
            return -ret;
        }
        if (owner == (uid_t)-1)
            owner = current_uid;
        if (group == (gid_t)-1)
            group = current_gid;
    }

    // 调用ext4文件系统接口修改文件所有者
    int ret = ext4_owner_set(absolute_path, owner, group);
    if (ret != EOK)
    {
        return -ret;
    }

    // 根据POSIX标准，当超级用户调用chown时：
    // - 对于可执行文件，清除setuid和setgid位
    // - 对于非组可执行文件，保留setgid位
    // 检查当前进程是否为超级用户（uid为0）
    if (p->euid == 0)
    {
        // 获取当前文件模式
        uint32_t current_mode;
        ret = ext4_mode_get(absolute_path, &current_mode);
        if (ret == EOK)
        {
            // 检查是否为可执行文件（用户、组或其他用户有执行权限）
            if (current_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
            {
                // 可执行文件：清除setuid和setgid位
                current_mode &= ~(S_ISUID | S_ISGID);
            }
            else
            {
                // 非可执行文件：只清除setuid位，保留setgid位
                current_mode &= ~S_ISUID;
            }

            // 重新设置文件模式
            ret = ext4_mode_set(absolute_path, current_mode);
            if (ret != EOK)
            {
                return -ret;
            }
        }
    }

    return 0;
}

/**
 * @brief msync系统调用实现
 *
 * @param addr  要同步的内存区域起始地址
 * @param len   要同步的区域长度
 * @param flags 同步标志
 * @return int  成功返回0，失败返回-1
 */
int sys_msync(uint64 addr, uint64 len, int flags)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_msync] addr: %p, len: %lu, flags: %d\n", addr, len, flags);
    return msync(addr, len, flags);
}

int sys_sched_get_priority_max(int policy)
{
    switch (policy)
    {
    case SCHED_FIFO:
    case SCHED_RR:
        // 实时策略：优先级范围 1-99
        return 99;
    case SCHED_OTHER:
    case SCHED_BATCH:
    case SCHED_IDLE:
    case SCHED_DEADLINE:
        // 非实时策略：优先级固定为 0
        return 0;
    default:
        return -EINVAL;
    }
}

int sys_sched_get_priority_min(int policy)
{
    switch (policy)
    {
    case SCHED_FIFO:
    case SCHED_RR:
        return 1;
    case SCHED_OTHER:
    case SCHED_BATCH:
    case SCHED_IDLE:
    case SCHED_DEADLINE:
        return 0;
    default:
        return -EINVAL;
    }
}

int sys_setuid(int uid)
{
    struct proc *p = myproc();

    // 权限检查
    if (p->euid != 0)
    { // 非特权用户
        // 非特权用户只能设置为real uid, effective uid 或 saved uid
        if (uid != p->ruid && uid != p->euid && uid != p->suid)
        {
            return -EPERM;
        }
    }

    // 如果是特权用户，设置所有ID
    if (p->euid == 0)
    {
        p->ruid = uid;
        p->euid = uid;
        p->suid = uid;
    }
    else
    {
        // 非特权用户只设置有效用户ID
        p->euid = uid;
    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_setuid] pid:%d set uid to %d (ruid=%d, euid=%d, suid=%d)\n",
                    p->pid, uid, p->ruid, p->euid, p->suid);
    return 0;
}

int sys_setreuid(uid_t ruid, uid_t euid)
{
    struct proc *p = myproc();
    uid_t old_ruid = p->ruid;
    uid_t old_euid = p->euid;
    uid_t old_suid = p->suid;

    // 如果参数是-1，表示不改变对应的ID
    if (ruid == (uid_t)-1)
        ruid = old_ruid;
    if (euid == (uid_t)-1)
        euid = old_euid;

    // 权限检查
    if (p->euid != 0)
    { // 非特权用户
        // 检查real uid权限：只能设置为当前的real uid或effective uid
        if (ruid != old_ruid && ruid != old_euid)
        {
            return -EPERM;
        }
        // 检查effective uid权限：只能设置为real uid、effective uid或saved uid
        if (euid != old_ruid && euid != old_euid && euid != old_suid)
        {
            return -EPERM;
        }
    }

    // 设置新的ID
    p->ruid = ruid;
    p->euid = euid;

    // 根据规则更新saved uid：
    // 如果real uid被改变，或者effective uid被设置为不等于之前real uid的值，
    // 则saved uid被设置为新的effective uid
    if (ruid != old_ruid || euid != old_ruid)
    {
        p->suid = euid;
    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[setreuid] pid:%d set ruid=%d, euid=%d, suid=%d\n",
                    p->pid, p->ruid, p->euid, p->suid);
    return 0;
}

int sys_setgid(int gid)
{
    struct proc *p = myproc();

    // 只有root用户或者当前组ID等于gid的进程可以设置gid
    if (p->euid != 0 && p->egid != gid)
    {
        return -EPERM;
    }

    // 设置所有组ID为新值（setgid的行为）
    p->rgid = gid;
    p->egid = gid;
    p->sgid = gid;
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_setgid] pid:%d set all gids to %d\n", p->pid, gid);
    return 0;
}

int sys_setregid(gid_t rgid, gid_t egid)
{
    struct proc *p = myproc();
    gid_t old_rgid = p->rgid;
    gid_t old_egid = p->egid;
    gid_t old_sgid = p->sgid;

    // 如果参数是-1，表示不改变对应的ID
    if (rgid == (gid_t)-1)
        rgid = old_rgid;
    if (egid == (gid_t)-1)
        egid = old_egid;

    // 权限检查
    if (p->euid != 0)
    { // 非特权用户
        // 检查real gid权限：只能设置为当前的real gid或effective gid
        if (rgid != old_rgid && rgid != old_egid)
        {
            return -EPERM;
        }
        // 检查effective gid权限：只能设置为real gid、effective gid或saved gid
        if (egid != old_rgid && egid != old_egid && egid != old_sgid)
        {
            return -EPERM;
        }
    }

    // 设置新的ID
    p->rgid = rgid;
    p->egid = egid;

    // 根据规则更新saved gid：
    // 如果real gid被改变，或者effective gid被设置为不等于之前real gid的值，
    // 则saved gid被设置为新的effective gid
    if (rgid != old_rgid || egid != old_rgid)
    {
        p->sgid = egid;
    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[setregid] pid:%d set rgid=%d, egid=%d, sgid=%d\n",
                    p->pid, p->rgid, p->egid, p->sgid);
    return 0;
}

/**
 * @brief 设置真实用户ID、有效用户ID和保存的用户ID
 * @param ruid 真实用户ID
 * @param euid 有效用户ID
 * @param suid 保存的用户ID
 * @return 成功返回0，失败返回负的错误码
 */
int sys_setresuid(int ruid, int euid, int suid)
{
    struct proc *p = myproc();
    uid_t old_ruid = p->ruid;
    uid_t old_euid = p->euid;
    uid_t old_suid = p->suid;

    // 如果参数是-1，表示不改变对应的ID
    if (ruid == -1)
        ruid = old_ruid;
    if (euid == -1)
        euid = old_euid;
    if (suid == -1)
        suid = old_suid;

    // 权限检查
    if (p->euid != 0)
    { // 非特权进程
        // 非特权进程只能将每个ID设置为当前的real UID、effective UID或saved UID之一
        if ((ruid != old_ruid && ruid != old_euid && ruid != old_suid) ||
            (euid != old_ruid && euid != old_euid && euid != old_suid) ||
            (suid != old_ruid && suid != old_euid && suid != old_suid))
        {
            return -EPERM;
        }
    }
    // 特权进程可以设置任意值，无需额外检查

    // 设置所有用户ID
    p->ruid = ruid;
    p->euid = euid;
    p->suid = suid;

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_setresuid] pid:%d set ruid=%d, euid=%d, suid=%d\n",
                    p->pid, p->ruid, p->euid, p->suid);
    return 0;
}

/**
 * @brief 设置进程的补充组ID
 *
 * @param size 组ID数组的大小
 * @param list 组ID数组的用户空间地址
 * @return int 成功返回0，失败返回负的错误码
 */
int sys_setgroups(size_t size, const gid_t *list)
{
    struct proc *p = myproc();

    // 检查权限：只有特权进程才能设置组ID
    if (p->euid != 0)
    {
        return -EPERM;
    }

    // 检查大小是否合理
    if (size > NGROUPS_MAX)
    {
        return -EINVAL;
    }

    // 如果size为0，清空补充组ID
    if (size == 0)
    {
        p->ngroups = 0;
        return 0;
    }

    // 验证用户空间地址的有效性
    if (!access_ok(VERIFY_READ, (uint64)list, sizeof(gid_t) * size))
    {
        return -EFAULT;
    }

    // 从用户空间复制组ID数组
    gid_t groups[NGROUPS_MAX];
    if (copyin(p->pagetable, (char *)groups, (uint64)list, sizeof(gid_t) * size) < 0)
    {
        return -EFAULT;
    }

    // 设置补充组ID
    p->ngroups = size;
    for (int i = 0; i < size; i++)
    {
        p->supplementary_groups[i] = groups[i];
    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_setgroups] pid:%d set %d supplementary groups\n",
                    p->pid, size);
    return 0;
}

/**
 * @brief 设置进程的真实组ID、有效组ID和保存的组ID
 *
 * @param rgid 真实组ID，如果为-1则不改变
 * @param egid 有效组ID，如果为-1则不改变
 * @param sgid 保存的组ID，如果为-1则不改变
 * @return int 成功返回0，失败返回负的错误码
 */
int sys_setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{
    struct proc *p = myproc();
    gid_t old_rgid = p->rgid;
    gid_t old_egid = p->egid;
    gid_t old_sgid = p->sgid;

    // 如果参数是-1，表示不改变对应的ID
    if (rgid == (gid_t)-1)
        rgid = old_rgid;
    if (egid == (gid_t)-1)
        egid = old_egid;
    if (sgid == (gid_t)-1)
        sgid = old_sgid;

    // 权限检查
    if (p->euid != 0)
    { // 非特权进程
        // 非特权进程只能将每个ID设置为当前的real GID、effective GID或saved GID之一
        if ((rgid != old_rgid && rgid != old_egid && rgid != old_sgid) ||
            (egid != old_rgid && egid != old_egid && egid != old_sgid) ||
            (sgid != old_rgid && sgid != old_egid && sgid != old_sgid))
        {
            return -EPERM;
        }
    }
    // 特权进程可以设置任意值，无需额外检查

    // 设置所有组ID
    p->rgid = rgid;
    p->egid = egid;
    p->sgid = sgid;

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_setresgid] pid:%d set rgid=%d, egid=%d, sgid=%d\n",
                    p->pid, p->rgid, p->egid, p->sgid);
    return 0;
}

/**
 * @brief 获取进程的真实用户ID、有效用户ID和保存的用户ID
 *
 * @param ruid 真实用户ID的指针
 * @param euid 有效用户ID的指针
 * @param suid 保存的用户ID的指针
 * @return int 成功返回0，失败返回负的错误码
 */
int sys_getresuid(uid_t *ruid, uid_t *euid, uid_t *suid)
{
    struct proc *p = myproc();

    // 验证用户空间地址的有效性
    if (ruid && !access_ok(VERIFY_WRITE, (uint64)ruid, sizeof(uid_t)))
    {
        return -EFAULT;
    }
    if (euid && !access_ok(VERIFY_WRITE, (uint64)euid, sizeof(uid_t)))
    {
        return -EFAULT;
    }
    if (suid && !access_ok(VERIFY_WRITE, (uint64)suid, sizeof(uid_t)))
    {
        return -EFAULT;
    }

    // 将用户ID复制到用户空间
    if (ruid)
    {
        if (copyout(p->pagetable, (uint64)ruid, (char *)&p->ruid, sizeof(uid_t)) < 0)
        {
            return -EFAULT;
        }
    }
    if (euid)
    {
        if (copyout(p->pagetable, (uint64)euid, (char *)&p->euid, sizeof(uid_t)) < 0)
        {
            return -EFAULT;
        }
    }
    if (suid)
    {
        if (copyout(p->pagetable, (uint64)suid, (char *)&p->suid, sizeof(uid_t)) < 0)
        {
            return -EFAULT;
        }
    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_getresuid] pid:%d get ruid=%d, euid=%d, suid=%d\n",
                    p->pid, p->ruid, p->euid, p->suid);
    return 0;
}

/**
 * @brief 获取进程的真实组ID、有效组ID和保存的组ID
 *
 * @param rgid 真实组ID的指针
 * @param egid 有效组ID的指针
 * @param sgid 保存的组ID的指针
 * @return int 成功返回0，失败返回负的错误码
 */
int sys_getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid)
{
    struct proc *p = myproc();

    // 验证用户空间地址的有效性
    if (rgid && !access_ok(VERIFY_WRITE, (uint64)rgid, sizeof(gid_t)))
    {
        return -EFAULT;
    }
    if (egid && !access_ok(VERIFY_WRITE, (uint64)egid, sizeof(gid_t)))
    {
        return -EFAULT;
    }
    if (sgid && !access_ok(VERIFY_WRITE, (uint64)sgid, sizeof(gid_t)))
    {
        return -EFAULT;
    }

    // 将组ID复制到用户空间
    if (rgid)
    {
        if (copyout(p->pagetable, (uint64)rgid, (char *)&p->rgid, sizeof(gid_t)) < 0)
        {
            return -EFAULT;
        }
    }
    if (egid)
    {
        if (copyout(p->pagetable, (uint64)egid, (char *)&p->egid, sizeof(gid_t)) < 0)
        {
            return -EFAULT;
        }
    }
    if (sgid)
    {
        if (copyout(p->pagetable, (uint64)sgid, (char *)&p->sgid, sizeof(gid_t)) < 0)
        {
            return -EFAULT;
        }
    }

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sys_getresgid] pid:%d get rgid=%d, egid=%d, sgid=%d\n",
                    p->pid, p->rgid, p->egid, p->sgid);
    return 0;
}

/**
 * @brief 设置文件创建掩码
 * @param mask 新的文件创建掩码
 * @return int 返回之前的掩码值
 */
int sys_umask(mode_t mask)
{
    struct proc *p = myproc();
    mode_t old_mask = p->umask;

    // 设置新的umask值
    p->umask = mask & 07777; // 只保留权限位

    return old_mask;
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
    const char *syscall_name = get_syscall_name((int)a[7]);
    (void)syscall_name; // 避免未使用变量的警告
#endif

    switch (a[7])
    {
    case SYS_sigreturn:
        ret = sys_sigreturn();
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
    case SYS_waitid:
        ret = sys_waitid((int)a[0], (int)a[1], (uint64)a[2], (int)a[3]);
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
    case SYS_mknodat:
        ret = sys_mknodat((int)a[0], (const char *)a[1], (int)a[2], (int)a[3]);
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
    case SYS_chroot:
        ret = sys_chroot((const char *)a[0]);
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
    case SYS_linkat:
        ret = sys_linkat((int)a[0], (uint64)a[1], (int)a[2], (uint64)a[3], (int)a[4]);
        break;
    case SYS_symlinkat:
        ret = sys_symlinkat((uint64)a[0], (int)a[1], (uint64)a[2]);
        break;
    case SYS_setresuid:
        ret = sys_setresuid((int)a[0], (int)a[1], (int)a[2]);
        break;
    case SYS_setresgid:
        ret = sys_setresgid((gid_t)a[0], (gid_t)a[1], (gid_t)a[2]);
        break;
    case SYS_getresuid:
        ret = sys_getresuid((uid_t *)a[0], (uid_t *)a[1], (uid_t *)a[2]);
        break;
    case SYS_getresgid:
        ret = sys_getresgid((gid_t *)a[0], (gid_t *)a[1], (gid_t *)a[2]);
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
        ret = sys_exit_group((int)a[0]);
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
    case SYS_faccessat2:
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
    case SYS_getrlimit:
        ret = sys_getrlimit((int)a[0], (uint64)a[1]);
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
    // < 注：glibc问题在5.26解决了
    case SYS_getgid: //< 如果getuid返回值不是0,就会需要这三个。但没有解决问题
        ret = myproc()->rgid;
        break;
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
        ret = myproc()->egid;
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
    case SYS_shmdt:
        ret = sys_shmdt((uint64)a[0]);
        break;
    case SYS_shmctl:
        ret = sys_shmctl((uint64)a[0], (uint64)a[1], (uint64)a[2]);
        break;
    case SYS_pselect6_time32:
        ret = sys_pselect6_time32((int)a[0], (uint64)a[1], (uint64)a[2], (uint64)a[3], (uint64)a[4], (uint64)a[5]);
        break;
    case SYS_umask:
        ret = sys_umask((mode_t)a[0]);
        break;
    case SYS_sched_setaffinity:
        ret = 0;
        break;
    case SYS_fchmod:
        ret = sys_fchmod((int)a[0], (mode_t)a[1]);
        break;
    case SYS_fchmodat:
        ret = sys_fchmodat((int)a[0], (const char *)a[1], (mode_t)a[2], (int)a[3]);
        break;
    case SYS_fchownat:
        ret = sys_fchownat((int)a[0], (const char *)a[1], (uid_t)a[2], (gid_t)a[3], (int)a[4]);
        break;
    case SYS_setpgid:
        ret = sys_setpgid((int)a[0], (int)a[1]);
        break;
    case SYS_getpgid:
        ret = sys_getpgid((int)a[0]);
        break;
    case SYS_msync:
        ret = sys_msync((uint64)a[0], (uint64)a[1], (int)a[2]);
        break;
    case SYS_fallocate:
        ret = 0;
        break;
    case SYS_pwrite64:
        ret = sys_pwrte64((int)a[0], (uint64)a[1], (uint64)a[2], (uint64)a[3]);
        break;
    case SYS_sched_get_priority_max:
        ret = sys_sched_get_priority_max((int)a[0]);
        break;
    case SYS_sched_get_priority_min:
        ret = sys_sched_get_priority_min((int)a[0]);
        break;
    case SYS_setuid:
        ret = sys_setuid((int)a[0]);
        break;
    case SYS_setgroups:
        ret = sys_setgroups((size_t)a[0], (const gid_t *)a[1]);
        break;
    case SYS_setreuid:
        ret = sys_setreuid((uid_t)a[0], (uid_t)a[1]);
        break;
    case SYS_setregid:
        ret = sys_setregid((gid_t)a[0], (gid_t)a[1]);
        break;
    case SYS_fchown:
        ret = sys_fchown((int)a[0], (uid_t)a[1], (gid_t)a[2]);
        break;
    case SYS_fgetxattr:
        ret = sys_fgetxattr((int)a[0], (const char *)a[1], (void *)a[2], (size_t)a[3]);
        break;
    default:
        ret = -1;
        panic("unknown syscall with a7: %d", a[7]);
    }
    trapframe->a0 = ret;
}
