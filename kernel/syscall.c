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
#include "print.h"
#include "pmem.h"
#include "fcntl.h"
#include "ext4_oflags.h"
#include "fs.h"
#include "vfs_ext4_ext.h"
#include "ops.h"

#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

// Allocate toa file descripr for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
    int fd;
    struct proc *p = myproc();

    for(fd = 0; fd < NOFILE; fd++){
        if(p->ofile[fd] == 0){
        p->ofile[fd] = f;
        return fd;
        }
    }
    return -1;
}

int sys_openat(int fd, const char *upath, int flags, uint16 mode)
{
    if (fd != AT_FDCWD && (fd < 0 || fd >= NOFILE))
        return -1;
    char path[MAXPATH];
    proc_t *p = myproc();
    if (copyinstr(p->pagetable, path, (uint64)upath, MAXPATH) == -1)
    {
        return -1;
    }
    LOG("sys_openat fd:%d,path:%s,flags:%d\n,mode:%d", fd, path, flags, mode);
    struct filesystem *fs = get_fs_from_path(path);
    if (fs->type == EXT4)
    {
        const char *dirpath = AT_FDCWD ? myproc()->cwd.path : myproc()->ofile[fd]->f_path;
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
            printf("打开失败: %s (错误码: %d)\n", path, ret);
            /*
             *   以防万一有什么没有释放的东西，先留着
             *   get_fops()->close(f);
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
    if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
        return -1;
    int reallylen = get_fops()->write(f, va, len);
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

int sys_wait(int pid, uint64 va,int option)
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

int sys_brk(uint64 n)
{
    uint64 addr;
    addr = myproc()->sz;
    if (n == 0)
    {
        return 0;
    }
    if (n >= addr)
    {
        if (growproc(n - addr) < 0)
            return -1;
        else
            return 0;
    }
    return 0;
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
#define FAT32_MAX_PATH 260
#define MAXARG 32
int sys_execve(const char *upath, uint64 uargv, uint64 uenvp)
{
    char path[FAT32_MAX_PATH], *argv[MAXARG];
    proc_t *p = myproc();
    if (copyinstr(p->pagetable, path, (uint64)upath, FAT32_MAX_PATH) == -1)
    {
        return -1;
    }
    LOG("[sys_execve] path:%s, uargv:%p, uenv:%p\n", path, uargv, uenvp);
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

int 
sys_close(int fd) 
{
    struct proc *p = myproc();
    struct file *f;

    if(fd < 0 || fd >= NOFILE || (f = p->ofile[fd]) == 0)
        return -1;
    p->ofile[fd] = 0;
    get_fops()->close(f);
    return 0;
}

int 
sys_pipe2(int *fd, int flags)
{
    uint64 fdaddr = (uint64)fd; // user pointer to array of two integers
    struct file *rf, *wf;
    int fdread, fdwrite;
    struct proc *p = myproc();

    if(pipealloc(&rf, &wf) < 0)
        return -1;
    fdread = -1;
    if((fdread = fdalloc(rf)) < 0 || (fdwrite = fdalloc(wf)) < 0){
        if(fdread >= 0)
        p->ofile[fdread] = 0;
        get_fops()->close(rf);
        get_fops()->close(wf);
        return -1;
    }
    if(copyout(p->pagetable, fdaddr, (char*)&fdread, sizeof(fdread)) < 0 ||
        copyout(p->pagetable, fdaddr+sizeof(fdread), (char *)&fdwrite, sizeof(fdwrite)) < 0){
        p->ofile[fdread] = 0;
        p->ofile[fdwrite] = 0;
        get_fops()->close(rf);
        get_fops()->close(wf);
        return -1;
    }
    return 0;
}

int 
sys_read(int fd, uint64 va, int len)
{
    struct file *f;
    if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
        return -1;
        
    return get_fops()->read(f, va, len);
}

int
sys_dup(int fd)
{
  struct file *f;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
        return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  get_fops()->dup(f);
  return fd;
}

uint64 sys_mknod(const char *upath, int major, int minor)
{
    char path[MAXPATH];
    proc_t *p = myproc();
    if (copyinstr(p->pagetable, path, (uint64)upath, MAXPATH) 
    == -1)
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
    if (oldfd < 0 || oldfd >= NOFILE 
        || (f = myproc()->ofile[oldfd]) == 0)
        return -1;
    if (oldfd == newfd)
        return newfd;
    if (newfd < 0 || newfd >= NOFILE)
        return -1;
    if (myproc()->ofile[newfd] != 0)
        return -1;
    myproc()->ofile[newfd] = f;
    get_fops () -> dup (f);
    return newfd;
}

uint64 a[8]; // 8个a寄存器，a7是系统调用号
void syscall(struct trapframe *trapframe)
{
    for (int i = 0; i < 8; i++)
        a[i] = hsai_get_arg(trapframe, i);
    int ret = -1;
#if DEBUG
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
    case SYS_wait:
        ret = sys_wait((int)a[0], (uint64)a[1],(int)a[2]);
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
    default:
        ret = -1;
        panic("unknown syscall with a7: %d", a[7]);
    }
    trapframe->a0 = ret;
}
