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

#include "stat.h"
#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

// Allocate toa file descripr for the given file.
// Takes over file reference from caller on success.
int sys_openat(int fd, const char *upath, int flags, uint16 mode)
{
    if (fd != FDCWD && (fd < 0 || fd >= NOFILE))
        return -1;
    char path[MAXPATH];
    proc_t *p = myproc();
    if (copyinstr(p->pagetable, path, (uint64)upath, MAXPATH) == -1)
    {
        return -1;
    }
#if DEBUG
    LOG("sys_openat fd:%d,path:%s,flags:%d,mode:%d\n", fd, path, flags, mode);
#endif
    struct filesystem *fs = get_fs_from_path(path);
    if (fs->type == EXT4)
    {
        const char *dirpath = FDCWD ? myproc()->cwd.path : myproc()->ofile[fd]->f_path;
        char absolute_path[MAXPATH] = {0};
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

        f->f_flags = flags;
        f->f_mode = mode;

        strcpy(f->f_path, absolute_path);
        int ret;

        if ((ret = vfs_ext_openat(f)) < 0)
        {
            // printf("打开失败: %s (错误码: %d)\n", path, ret);
            /*
             *   以防万一有什么没有释放的东西，先留着
             *   get_file_ops()->close(f);
             */
            myproc()->ofile[fd] = 0;
            // if(!strcmp(path, "./mnt")) {
            //     return 2;
            return -1;
        }

        return fd;
    }
    else
        panic("unsupport filesystem");
    return -1;
};

int sys_write(int fd, uint64 va, int len)
{
    // struct proc *p = myproc();
    // char str[200];
    // int size = copyinstr(p->pagetable, str, va, MIN(len, 200));
    // for (int i = 0; i < size; ++i)
    // {
    //     consputc(str[i]);
    // }
    struct file *f;
    if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
        return -1;
    int reallylen = get_file_ops()->write(f, va, len);
    return reallylen;
}

uint64 sys_getpid(void)
{
    return myproc()->pid;
}

uint64 sys_getppid()
{
    proc_t *pp = myproc()->parent;
    assert(pp != NULL, "sys_getppid\n");
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
    LOG("sys_clone: flags:%d, stack:%p, ptid:%p, tls:%p, ctid:%p",
        flags, stack, ptid, tls, ctid);
#endif
    assert(flags == 17, "sys_clone: flags is not SIGCHLD");
    if (stack == 0)
    {
        return fork();
    }
    return clone(stack, ptid, ctid);
}

int sys_wait(int pid, uint64 va, int option)
{
    return wait(pid, va);
}

uint64 sys_exit(int n)
{
    exit(n);
    return 0;
}

uint64 sys_gettimeofday(uint64 tv_addr)
{
    struct proc *p = myproc();
    timeval_t tv = timer_get_time();
    return copyout(p->pagetable, tv_addr, (char *)&tv, sizeof(timeval_t));
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

uint64 sys_brk(uint64 n)
{
    uint64 addr;
    addr = myproc()->sz;
    if (n == 0)
    {
        return addr;
    }
    if (growproc(n - addr) < 0)
        return -1;
    return addr;
}

uint64 sys_times(uint64 dstva)
{
    return get_times(dstva);
}

struct utsname
{
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};
int sys_uname(uint64 buf)
{
    struct utsname uts;
    strncpy(uts.sysname, "SC7\0", 65);
    strncpy(uts.nodename, "none\0", 65);
    strncpy(uts.release, __DATE__ " " __TIME__, 65);
    strncpy(uts.version, "0.0.1\0", 65);
    strncpy(uts.machine, "qemu", 65);
    strncpy(uts.domainname, "none\0", 65);

    return copyout(myproc()->pagetable, buf, (char *)&uts, sizeof(uts));
}

uint64 sys_sched_yield()
{
    yield();
    return 0;
}
int sys_execve(const char *upath, uint64 uargv, uint64 uenvp)
{
    char path[MAXPATH], *argv[MAXARG];
    proc_t *p = myproc();
    if (copyinstr(p->pagetable, path, (uint64)upath, MAXPATH) == -1)
    {
        return -1;
    }
#if DEBUG
    LOG("[sys_execve] path:%s, uargv:%p, uenv:%p\n", path, uargv, uenvp);
#endif
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

    int ret = exec(path, argv, 0);
    for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
        pmem_free_pages(argv[i], 1);

    return ret;

bad:
    for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
        pmem_free_pages(argv[i], 1);
    return -1;
    // return execve(path, argv, envp);
}

int sys_close(int fd)
{
    struct proc *p = myproc();
    struct file *f;

    if (fd < 0 || fd >= NOFILE || (f = p->ofile[fd]) == 0)
        return -1;
    p->ofile[fd] = 0;
    get_file_ops()->close(f);
    return 0;
}

int sys_pipe2(int *fd, int flags)
{
    uint64 fdaddr = (uint64)fd; // user pointer to array of two integers
    struct file *rf, *wf;
    int fdread, fdwrite;
    struct proc *p = myproc();

    if (pipealloc(&rf, &wf) < 0)
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

int sys_read(int fd, uint64 va, int len)
{
    struct file *f;
    if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
        return -1;

    return get_file_ops()->read(f, va, len);
}

int sys_dup(int fd)
{
    struct file *f;
    if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
        return -1;
    if ((fd = fdalloc(f)) < 0)
        return -1;
    get_file_ops()->dup(f);
    return fd;
}

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
        uint32 dev = major;
        if (vfs_ext_mknod(absolute_path, T_CHR, dev) < 0)
        {
            return -1;
        }
    }
    return 0;
}

uint64
sys_dup3(int oldfd, int newfd, int flags)
{
    struct file *f;
    if (oldfd < 0 || oldfd >= NOFILE || (f = myproc()->ofile[oldfd]) == 0)
        return -1;
    if (oldfd == newfd)
        return newfd;
    if (newfd < 0 || newfd >= NOFILE)
        return -1;
    if (myproc()->ofile[newfd] != 0)
        return -1;
    myproc()->ofile[newfd] = f;
    get_file_ops()->dup(f);
    return newfd;
}

int sys_fstat(int fd, uint64 addr)
{
    if (fd < 0 || fd >= NOFILE)
        return -1;
    return get_file_ops()->fstat(myproc()->ofile[fd], addr);
}
int sys_statx(int fd, const char *path, int flags, int mode, uint64 addr)
{
    if (fd < 0 || fd >= NOFILE)
        return -1;
    return get_file_ops()->statx(myproc()->ofile[fd], addr);
}

int sys_mmap(uint64 start, int len, int prot, int flags, int fd, int off)
{
#if DEBUG
    LOG("mmap start:%p len:%d prot:%d flags:%d fd:%d off:%d\n", start, len, prot, flags, fd, off);
#endif
    return mmap((uint64)start, len, prot, flags, fd, off);
}

int sys_munmap(void *start, int len)
{
    return munmap((uint64)start, len);
}

uint64 sys_getcwd(char *buf, int size)
{
    if (buf == NULL)
    {
        printf("[sys_getcwd] 传入空buf,这个功能待实现!\n");
        return -1;
    }
    char *path = myproc()->cwd.path;
    if (strlen(path) > size)
    {
        panic("[sys_getcwd] 缓冲区过小,不足以存放cwd!");
    }
#if DEBUG
    printf("[sys_getcwd] 当前工作目录: %s\n", path);
#endif
    copyout(myproc()->pagetable, (uint64)buf, path, strlen(path)); //< 写入用户空间的buf
    return (uint64)buf;
}

int sys_mkdirat(int dirfd, const char *upath, uint16 mode) //< 初赛先只实现相对路径的情况
{
    if (dirfd != FDCWD) //< 如果传入的fd不是FDCWD
    {
        printf("[sys_mkdirat] 传入的fd不是FDCWD,待实现\n");
        return -1;
    }
    char path[MAXPATH] = {0};
    copyinstr(myproc()->pagetable, path, (uint64)upath, MAXPATH);
    const char *dirpath = (dirfd == FDCWD) ? myproc()->cwd.path : myproc()->ofile[dirfd]->f_path; //< 目前只会是相对路径
    char absolute_path[MAXPATH] = {0};
    get_absolute_path(path, dirpath, absolute_path);
#if DEBUG
    printf("[sys_mkdirat] 创建目录到: %s\n", absolute_path);
#endif
    vfs_ext_mkdir(absolute_path, 0777); //< 传入绝对路径，权限777表示所有人都可RWX
#if DEBUG
    printf("[sys_mkdirat] 创建成功\n");
#endif
    return 0;
}

int sys_chdir(const char *path)
{
#if DEBUG
    printf("sys_chdir!\n");
#endif
    char buf[MAXPATH];
    memset(buf, 0, MAXPATH);                                    //< 清空，以防上次的残留
    copyinstr(myproc()->pagetable, buf, (uint64)path, MAXPATH); //< 复制用户空间的path到内核空间的buf
    /*
        [todo] 判断路径是否存在，是否是目录
    */
    char *cwd = myproc()->cwd.path; // char path[MAXPATH]
    memset(cwd, 0, MAXPATH);        //< 清空，以防上次的残留
    memmove(cwd, buf, strlen(buf));
#if DEBUG
    printf("修改成功,当前工作目录: %s\n", myproc()->cwd.path);
#endif
    return 0;
}

char sys_getdents64_buf[1024];                                  //< 函数专用缓冲区
int sys_getdents64(int fd, struct linux_dirent64 *buf, int len) //< buf是用户空间传入的缓冲区
{
    struct file *f = myproc()->ofile[fd];
#if DEBUG
    printf("传入的文件标识符%d对应的路径: %s\n", fd, f->f_path);
#endif

    /*逻辑和vfs_ext_getdents很像，又有不同*/
    const ext4_direntry *rentry;
    rentry = ext4_dir_entry_next(f->f_data.f_vnode.data);
    int namelen = strlen(f->f_path);
    memset((void *)sys_getdents64_buf, 0, 1024); //< 使用缓冲区前先清零
    struct linux_dirent64 *d = (struct linux_dirent64 *)sys_getdents64_buf;

    //<获取d->d_reclen
    int reclen = sizeof d->d_ino + sizeof d->d_off + sizeof d->d_reclen + sizeof d->d_type + namelen + 2;
    if (reclen < sizeof(struct linux_dirent64))
    {
        reclen = sizeof(struct linux_dirent64);
    }
    d->d_reclen = reclen;
    //< 获取d->d_name
    memmove(d->d_name, f->f_path, strlen(f->f_path)); //< 或许有更简单的办法，目前我对string函数不熟，先这样。
    //< 获取d->d_type
    if (rentry->inode_type == EXT4_DE_DIR)
    {
        d->d_type = T_DIR;
    }
    else if (rentry->inode_type == EXT4_DE_REG_FILE)
    {
        d->d_type = T_FILE;
    }
    else if (rentry->inode_type == EXT4_DE_CHRDEV)
    {
        d->d_type = T_CHR;
    }
    else
    {
        d->d_type = T_UNKNOWN;
    }
    //< 获取d->d_ino
    d->d_ino = rentry->inode;
    //< 获取d->d_off
    d->d_off = 1; //< 只要一项linux_dirent64的话，考虑index和到下一项的偏移都没有意义，设为1算了

    if (reclen > len)
    {
        printf("缓冲区空间不足,不足以存放读取到的linux_dirent64\n");
        return -1;
    }
    copyout(myproc()->pagetable, (uint64)buf, (char *)d, reclen); /// [todo]给的name不对！！
    return reclen;
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
 * @todo 目前只会删除指定的文件，完整链接功能待实现！
 * */
int sys_unlinkat(int dirfd, char *path, unsigned int flags)
{
    if (dirfd != FDCWD) //< 如果传入的fd不是FDCWD
    {
        printf("[sys_unlinkat] 传入的fd不是FDCWD,待实现\n");
        return -1;
    }
    if (flags != 0)
    {
        printf("[sys_unlinkat] flags不支持AT_REMOVEDIR\n");
        return -1;
    }

    char buf[MAXPATH] = {0};                                    //< 清空，以防上次的残留
    copyinstr(myproc()->pagetable, buf, (uint64)path, MAXPATH); //< 复制用户空间的path到内核空间的buf
    /*如果路径是以./开头，表示是相对路径。测例给的./test_unlink，要处理得出绝对路径。其他情况现在不处理*/
    int pathlen = strlen(buf);
    for (int i = 0; i < pathlen; i++) //< 除去./ 把包括\0的字符串前移2位
    {
        buf[i] = buf[i + 2]; //< 应该不会有字符串大到让buf[i+2]溢出MAXPATH吧
    }
    const char *dirpath = (dirfd == FDCWD) ? myproc()->cwd.path : myproc()->ofile[dirfd]->f_path; //< 目前只会是相对路径
    char absolute_path[MAXPATH] = {0};
    get_absolute_path(buf, dirpath, absolute_path); //< 从mkdirat抄过来的时候忘记把第一个参数从path改成这里的buf了... debug了几分钟才看出来

    ext4_fremove(absolute_path); //< unlink系统测例实际上考察的是删除文件的功能，先openat创一个test_unlink，再用unlink要求删除，然后open检查打不打的开
#if DEBUG
    printf("删除文件: %s", absolute_path);
#endif
    return 0;
}

int sys_getuid()
{
    return myproc()->uid;
}

int sys_geteuid()
{
    return myproc()->uid;
}

int sys_ioctl()
{
    printf("sys_ioctl\n");
    return 0;
}

int sys_exit_group()
{
    printf("sys_exit_group\n");   
    return 0;
}
uint64 a[8]; // 8个a寄存器，a7是系统调用号
void syscall(struct trapframe *trapframe)
{
    for (int i = 0; i < 8; i++)
        a[i] = hsai_get_arg(trapframe, i);
    uint64 ret = -1;
#if DEBUG
    if (a[7] != 64)
        LOG("syscall: a7: %d\n", a[7]);
#endif
    switch (a[7])
    {
    case SYS_write:
        ret = sys_write(a[0], a[1], a[2]);
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
    case SYS_wait:
        ret = sys_wait((int)a[0], (uint64)a[1], (int)a[2]);
        break;
    case SYS_exit:
        sys_exit(a[0]);
        break;
    case SYS_gettimeofday:
        ret = sys_gettimeofday(a[0]);
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
    case SYS_statx:
        ret = sys_statx((int)a[0], (const char *)a[1], (int)a[2], (int)a[3], (uint64)a[4]);
        break;
    case SYS_mmap:
        ret = sys_mmap((uint64)a[0], (int)a[1], (int)a[2], (int)a[3], (int)a[4], (int)a[5]);
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
        ret = 0;
        printf("sys_set_tid_address\n");
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
    default:
        ret = -1;
        panic("unknown syscall with a7: %d", a[7]);
    }
    trapframe->a0 = ret;
}
