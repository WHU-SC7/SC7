#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

#include "fs_defs.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "stat.h"
#include "fs.h"
#include "file.h"

#include "ext4_oflags.h"
#include "errno-base.h"

#include "process.h"
#include "vfs_ext4.h"
#include "string.h"

#include "cpu.h"
#include "vmem.h"
#include "print.h"

#include "select.h"
#include "procfs.h"
#include "fifo.h"

// 前向声明pipe结构体
struct pipe {
    struct spinlock lock;
    char data[512];
    uint nread;
    uint nwrite;
    int readopen;
    int writeopen;
};

#define PIPESIZE 512

struct devsw devsw[NDEV];
char zeros[ZERO_BYTES];

/**
 * @brief 文件表
 * 
 * 该结构体用于管理系统中打开的文件描述符。
 * 包含一个锁和一个文件数组，锁用于保护对文件数组的访问。
 */
struct {
    struct spinlock lock;
    struct file file[NFILE];
} ftable;

/**
 * @brief vnode表
 * 
 * 该结构体用于管理文件系统中的vnode(包括对应文件系统的目录和文件)。
 * 包含一个锁和一个目录数组，锁用于保护对数组的访问。
 */
struct {
    struct spinlock lock;
    file_vnode_t vnodes[NFILE];
    int valid[NFILE];
    bool isdir[NFILE];
} file_vnode_table;

/**
 * @brief 分配文件结构体
 * 
 * Allocate a file structure.
 * 
 * @return struct file* 
 */
struct file*
filealloc(void)
{
    struct file *f;

    acquire(&ftable.lock);
    for(f = ftable.file; f < ftable.file + NFILE; f++)
    {
        if(f->f_count == 0){
            f->f_count = 1;
            release(&ftable.lock);
            return f;
        }
    }
    release(&ftable.lock);
    return 0;
}

/**
 * @brief 分配进程文件描述符
 * 
 * @param f 文件
 * @return int 文件描述符，失败返回-1 
 */
int 
fdalloc(struct file *f){
    int fd;
    proc_t *p = myproc();
    for(fd = 0 ; fd < NOFILE && fd < myproc()->ofn.rlim_cur; fd++)
    {
        if(p->ofile[fd] == 0){
            p->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
}

int fdalloc2(struct file *f,int begin)
{
    int fd;
    proc_t *p = myproc();
    for(fd = begin; fd < NOFILE; fd++){
        if(p->ofile[fd] == 0){
            p->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
};

/**
 * @brief 增加文件描述符的引用计数
 * 
 * Increment ref count for file f.
 * 
 * @param f 
 * @return struct file* 
 */
struct file*
filedup(struct file *f)
{
    acquire(&ftable.lock);
    if(f->f_count < 1)
        panic("filedup");
    f->f_count++;
    release(&ftable.lock);
    return f;
}

/**
 * @brief 减少文件描述符引用计数，如果引用计数为0，彻底关闭
 * 
 * Close file f.  (Decrement ref count, close when reaches 0.)
 * 
 * @param f 
 */

int fileclose(struct file *f)
{
    struct file ff;

    acquire(&ftable.lock);
    if(f->f_count < 1){
        panic("fileclose");
        return -1;
    }
    if(--f->f_count > 0){
        release(&ftable.lock);
        return 0;
    }
    ff = *f;
    // 在关闭文件时，确保没有其他线程正在访问该文件
    acquire(&ff.f_lock);
    f->f_count = -1;               // 标记为"关闭中"
    release(&ff.f_lock);
    release(&ftable.lock);

    if(ff.f_type == FD_PIPE)
    {
        pipeclose(ff.f_data.f_pipe, get_file_ops()->writable(&ff));
    } 
    else if(ff.f_type == FD_FIFO)
    {
        fifo_close(ff.f_data.f_fifo, ff.f_flags);
    }
    else if(ff.f_type == FD_REG || ff.f_type == FD_DEVICE)
    {
        if (ff.f_data.f_vnode.fs->type == EXT4) 
        {       
            if (vfs_ext4_is_dir(ff.f_path) == 0) 
                vfs_ext4_dirclose(&ff);
            else 
                vfs_ext4_fclose(&ff);
            if (ff.removed) 
            {
                vfs_ext4_rm(ff.f_path);
                ff.removed = 0;
            }
        }
        else if (ff.f_data.f_vnode.fs->type == VFAT) 
        {
            // panic("我还没写(๑>؂<๑)\n");
            /* @todo 测例好像哪怕挂载了vfat，也是用ext4来读写的 */
            if (vfs_ext4_is_dir(ff.f_path) == 0) 
                vfs_ext4_dirclose(&ff);
            else 
                vfs_ext4_fclose(&ff);
            if (ff.removed) 
            {
                vfs_ext4_rm(ff.f_path);
                ff.removed = 0;
            }
        } 
        else 
            panic("fileclose: %s unknown filesystem type!", ff.f_path);
    }
    else if (ff.f_type == FD_BUSYBOX)
    {
#if DEBUG
        LOG_LEVEL(LOG_DEBUG, "close file or dir %s for busybox\n", ff.f_path);
#endif
    }else if(ff.f_type == FD_SOCKET){
        DEBUG_LOG_LEVEL(LOG_WARNING,"[todo] 释放socket资源\n");
    }else if(ff.f_type == FD_PROC_STAT || ff.f_type == FD_PROC_PIDMAX ||  ff.f_type == FD_PROC_TAINTED ){

    }
    else
        panic("fileclose: %s unknown file type!", ff.f_path);
        
    // 安全标记结构体为可用
    acquire(&ftable.lock);
    f->f_count = 0;                // 重置为可分配状态
    f->f_type = FD_NONE;
    f->removed = 0;
    release(&ftable.lock);
    return 0;
}

/**
 * @brief 得到文件描述符f的metadata到addr(用户地址)
 * 
 * Get metadata about file f.
 * addr is a user virtual address, pointing to a struct stat.
 * 
 * @param f 
 * @param addr 
 * @return int 标准错误码负数
 */
int
filestat(struct file *f, uint64 addr)
{
    struct proc *p = myproc();
    struct kstat st;
    if(f->f_type == FD_REG || f->f_type == FD_DEVICE)
    {
        int ret = vfs_ext4_fstat(f, &st);
        if (ret < 0) return ret;
        if (copyout(p->pagetable, addr, (char *)(&st), sizeof(st)) < 0)
            return -EFAULT;
        return 0;
    }
    return -1;
}

/**
 * @brief 得到文件描述符f的拓展的metadata到addr(用户地址)
 * 
 * @param f 
 * @param addr 
 * @return int 状态码，0成功，-1失败
 */
int 
filestatx(struct file *f, uint64 addr) 
{
    struct proc *p = myproc();
    struct statx st;
    if( f->f_type == FD_REG || f->f_type == FD_DEVICE 
        || f->f_type == FD_BUSYBOX)
    {
        vfs_ext4_statx(f->f_path, &st);
        if(copyout(p->pagetable, addr, (char *)(&st), sizeof(st)) < 0)
            return -1;
        return 0;
    }
    return -1;
}

/**
 * @brief 从文件f中读取n字节数据到addr(用户地址)
 * 
 * Read from file f.
 * addr is a user virtual address.
 * 
 * @param f 
 * @param addr 
 * @param n 
 * @return int 状态码，0成功，-1失败
 */
int
fileread(struct file *f, uint64 addr, int n)
{
    int r = 0;

    if(get_file_ops()->readable(f) == 0)
        return -1;

    // 获取文件锁，保护文件读取操作
    acquire(&f->f_lock);

    if(f->f_type == FD_PIPE)
    {
        int lock = 0;
        if(holding(&f->f_lock)){
            lock = 1;
            release(&f->f_lock); // 新增：释放VFS层锁 
        }
        r = piperead(f->f_data.f_pipe, addr, n);
        if(!holding(&f->f_lock) && lock)
            acquire(&f->f_lock); // 新增：释放VFS层锁
    } 
    else if(f->f_type == FD_FIFO)
    {
        int lock = 0;
        if(holding(&f->f_lock)){
            lock = 1;
            release(&f->f_lock); // 释放VFS层锁 
        }
        
        // 调用 fifo.c 中的读取函数
        r = fiforead(f, 1, addr, n);
        
        if(!holding(&f->f_lock) && lock)
            acquire(&f->f_lock); // 重新获取VFS层锁
    }
    else if(f->f_type == FD_DEVICE)
    {
        if(f->f_major < 0 || f->f_major >= NDEV || !devsw[f->f_major].read)
        {
            release(&f->f_lock);
            return -1;
        }
        int lock = 0;
        if(holding(&f->f_lock)){
            lock = 1;
            release(&f->f_lock); // 新增：释放VFS层锁 
        }
        r = devsw[f->f_major].read(1, addr, n);
        if(!holding(&f->f_lock) && lock)
        acquire(&f->f_lock); // 新增：释放VFS层锁
    } 
    else if(f->f_type == FD_REG)
    {
        if (f->f_data.f_vnode.fs->type == EXT4) 
        {
            r = vfs_ext4_read(f, 1, addr, n);
        } 
        else if (f->f_data.f_vnode.fs->type == VFAT) 
        {
            /* @todo 测例好像哪怕挂载了vfat，也是用ext4来读写的 */
            // panic("我还没写(๑>؂<๑)\n");
            r = vfs_ext4_read(f, 1, addr, n);
        } 
        else 
        {
            release(&f->f_lock);
            panic("fileread: unknown file type");
        }
    } 
    else if (f->f_type == FD_BUSYBOX)
    {
        copyout(myproc()->pagetable, addr, zeros, ZERO_BYTES);
        r = 0;
    }
    else if (f->f_type == FD_PROC_STAT)
    {
        char statbuf[256];
        int pid = 0;
        is_proc_pid_stat(f->f_path,&pid);
        int len = generate_proc_stat_content(pid, statbuf, sizeof(statbuf));
        if (len < 0) {
            release(&f->f_lock);
            return 0;
        }
        int tocopy = (n < len - f->f_pos) ? n : (len - f->f_pos);
        if (tocopy > 0) {
            if (copyout(myproc()->pagetable, addr, statbuf + f->f_pos, tocopy) < 0) {
                release(&f->f_lock);
                return 0;
            }
            f->f_pos += tocopy;
            r = tocopy;
        } else {
            r = 0;
        }
        release(&f->f_lock);
        return r;
    }else if(f->f_type == FD_PROC_PIDMAX)
    {
        char buf[20];
        int written = snprintf(buf,sizeof(buf),"100");
        int tocopy = (n < written  - f->f_pos) ? n : (written - f->f_pos);
        if (tocopy > 0) {
            if (copyout(myproc()->pagetable, addr, buf + f->f_pos, tocopy) < 0) {
                release(&f->f_lock);
                return 0;
            }
            f->f_pos += tocopy;
            r = tocopy;
        } else {
            r = 0;
        }
    }else if(f->f_type ==  FD_PROC_TAINTED){
        char buf[20];
        int written = snprintf(buf,sizeof(buf),"0");
        int tocopy = (n < written  - f->f_pos) ? n : (written - f->f_pos);
        if (tocopy > 0) {
            if (copyout(myproc()->pagetable, addr, buf + f->f_pos, tocopy) < 0) {
                release(&f->f_lock);
                return 0;
            }
            f->f_pos += tocopy;
            r = tocopy;
        } else {
            r = 0;
        }
    }
    else
    {
        release(&f->f_lock);
        panic("fileread");
    }

    // 释放文件锁
    release(&f->f_lock);
    return r;
}

/**
 * @brief 从指定偏移读文件f的n个字节到addr(内核地址)
 * 
 * @param f 
 * @param addr 
 * @param n 
 * @param offset 
 * @return int 状态码，0成功，-1失败
 */
int filereadat(struct file *f, uint64 addr, int n, uint64 offset) {
    int r = 0;

    if(get_file_ops()->readable(f) == 0)
        return -1;
        
    // 获取文件锁，保护文件读取操作
    acquire(&f->f_lock);
    
    if (f->f_type == FD_REG) 
    {
        if (f->f_data.f_vnode.fs->type == EXT4) 
        {
            r = vfs_ext4_readat(f, 0, addr, n, offset);
        } 
        else if (f->f_data.f_vnode.fs->type == VFAT) 
        {
            /* @todo 测例好像哪怕挂载了vfat，也是用ext4来读写的 */
            r = vfs_ext4_readat(f, 0, addr, n, offset);
            // panic("我还没写(๑>؂<๑)\n");
        } 
        else 
        {
            release(&f->f_lock);
            panic("filereadat: unknown file type");
        }
    }
    else
    {
        release(&f->f_lock);
        return -1;
    }
    
    // 释放文件锁
    release(&f->f_lock);
    return r;
}

/**
 * @brief 从addr(用户地址)写n字节数据到文件f
 * 
 * Write to file f.
 * addr is a user virtual address.
 * 
 * @param f 
 * @param addr 
 * @param n 
 * @return int 状态码，0成功，-1失败
 */
int
filewrite(struct file *f, uint64 addr, int n)
{
    int r, ret = 0;

    if(get_file_ops()->writable(f) == 0)
        return -1;

    // 获取文件锁，保护文件写入操作
    acquire(&f->f_lock);

    if(f->f_type == FD_PIPE)
    {
        ret = pipewrite(f->f_data.f_pipe, addr, n);
    } 
    else if(f->f_type == FD_FIFO)
    {
        // 调用 fifo.c 中的写入函数
        ret = fifowrite(f, 1, addr, n);
    }
    else if(f->f_type == FD_DEVICE)
    {
        if(f->f_major < 0 || f->f_major >= NDEV || !devsw[f->f_major].write)
        {
            release(&f->f_lock);
            return -1;
        }
        
        ret = devsw[f->f_major].write(1, addr, n);
    } 
    else if(f->f_type == FD_REG)
    {
        struct ext4_file *file = (struct ext4_file *)f -> f_data.f_vnode.data;
        // printf("当前文件偏移量位置: %x, 文件大小: %x.将要写入的长度: %x\n",file->fpos,file->fsize,n);
        if(file->fpos > file->fsize) //偏移量是否超出文件大小,目前只有llseek01是这种情况
        {
            LOG("[filewrite]偏移量超出文件大小,失败\n");
            release(&f->f_lock);
            return -EFBIG;
        }
        if(file->fpos + n > myproc()->rlimits[RLIMIT_FSIZE].rlim_cur) //不能超出限制的大小
        {
            LOG("[filewrite]超出文件大小限制,失败\n");
            release(&f->f_lock);
            return -EFBIG;
        }
        // write a few blocks at a time to avoid exceeding
        // the maximum log transaction size, including
        // i-node, indirect block, allocation blocks,
        // and 2 blocks of slop for non-aligned writes.
        // this really belongs lower down, since writei()
        // might be writing a device like the console.
        int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
        int i = 0;
        while(i < n)
        {
            int n1 = n - i;
            if (n1 > max)
                n1 = max;
            if (f->f_data.f_vnode.fs->type == EXT4) 
            {
                r = vfs_ext4_write(f, 1, addr + i, n1);
            } 
            else if (f->f_data.f_vnode.fs->type == VFAT) 
            {
                /* @todo 测例好像哪怕挂载了vfat，也是用ext4来读写的 */
                r = vfs_ext4_write(f, 1, addr + i, n1);
                // panic("我还没写(๑>؂<๑)\n");
            } 
            else 
            {
                release(&f->f_lock);
                panic("filewrite: unknown file type");
            }
            if(r != n1)
            {
                // error from writei
                break;
            }
            i += r;
        }
        ret = (i == n ? n : -EINVAL);
    } 
    else 
    {
        release(&f->f_lock);
        panic("filewrite");
    }

    // 释放文件锁
    release(&f->f_lock);
    return ret;
}

/**
 * @brief 文件是否可读
 * 
 * @param f 
 * @return char 
 */
char 
filereadable(struct file *f) 
{
    char readable = !(f->f_flags & O_WRONLY);
    return readable;
}

/**
 * @brief 文件是否可写
 * 
 * @param f 
 * @return char 
 */
char 
filewriteable(struct file *f) 
{
    char writeable = (f->f_flags & O_WRONLY) || (f->f_flags & O_RDWR);
    return writeable;
}

/**
 * @brief 文件poll操作
 * 
 * @param f 文件对象
 * @param events 请求的事件
 * @return int 返回就绪的事件
 */
int 
filepoll(struct file *f, int events) 
{
    int revents = 0;
    
    if (f->f_type == FD_PIPE) {
        struct pipe *pi = f->f_data.f_pipe;
        acquire(&pi->lock);
        
        if (events & POLLIN) {
            if (pi->nread != pi->nwrite || pi->writeopen == 0) {
                revents |= POLLIN;
            }
        }
        
        if (events & POLLOUT) {
            if (pi->nwrite < pi->nread + PIPESIZE) {
                revents |= POLLOUT;
            }
        }
        
        if (pi->readopen == 0 && (f->f_flags & O_RDONLY)) {
            revents |= POLLHUP;
        }
        
        if (pi->writeopen == 0 && (f->f_flags & O_WRONLY)) {
            revents |= POLLHUP;
        }
        
        release(&pi->lock);
    } else if (f->f_type == FD_FIFO) {
        struct fifo *fi = f->f_data.f_fifo;
        acquire(&fi->lock);
        
        if (events & POLLIN) {
            if (fi->nread != fi->nwrite || fi->writeopen == 0) {
                revents |= POLLIN;
            }
        }
        
        if (events & POLLOUT) {
            if (fi->nwrite < fi->nread + FIFO_SIZE) {
                revents |= POLLOUT;
            }
        }
        
        if (fi->readopen == 0 && (f->f_flags & O_RDONLY)) {
            revents |= POLLHUP;
        }
        
        if (fi->writeopen == 0 && (f->f_flags & O_WRONLY)) {
            revents |= POLLHUP;
        }
        
        release(&fi->lock);
    } 
    else if (f->f_type == FD_REG) {
        // 对于普通文件，总是可读可写
        if (events & POLLIN) {
            revents |= POLLIN;
        }
        if (events & POLLOUT) {
            revents |= POLLOUT;
        }
    }
    else if (f->f_type == FD_SOCKET) {
        // 对于socket，暂时返回可读可写
        if (events & POLLIN) {
            revents |= POLLIN;
        }
        if (events & POLLOUT) {
            revents |= POLLOUT;
        }
    }
    
    return revents;
}

struct file_operations FILE_OPS = 
{
    .dup = &filedup,
    .close = &fileclose,
    .read = &fileread,
    .readat = &filereadat,
    .write = &filewrite,
    .fstat = &filestat,
    .statx = &filestatx,
    .writable = &filewriteable,
    .readable = &filereadable,
    .poll = &filepoll,
};

struct file_operations *
get_file_ops(void) 
{
    return &FILE_OPS;
}

/**
 * @brief 初始化文件描述符表和ext4目录、文件表
 * 
 * Initialize the file descriptor table and ext4 directory and file tables.
 */
void 
fileinit(void) 
{
    initlock(&ftable.lock, "ftable");
    initlock(&file_vnode_table.lock, "file_vnode_table");
	memset(ftable.file, 0, sizeof(ftable.file));
    memset(file_vnode_table.vnodes, 0, sizeof(file_vnode_table.vnodes));
    
    // 初始化每个文件结构体的锁
    for (int i = 0; i < NFILE; i++) {
        initlock(&ftable.file[i].f_lock, "file_lock");
    }
    
    // 初始化 FIFO 表
    init_fifo_table();
}

/**
 * @brief 分配目录结构体
 * 
 * @return file_vnode_t* 对应VFS的vnode
 */
file_vnode_t *
vfs_alloc_dir(void) 
{
    int i;
    acquire(&file_vnode_table.lock);
    for (i = 0;i < NFILE;i++) 
    {
        if (file_vnode_table.valid[i] == 0) 
        {
            file_vnode_table.valid[i] = 1;
            file_vnode_table.isdir[i] = 1;
            file_vnode_table.vnodes[i].fs = get_fs_by_type(EXT4);
            file_vnode_table.vnodes[i].data = kalloc();
            break;
        }
    }
    release(&file_vnode_table.lock);
    if (i == NFILE)
        return NULL;
    return &file_vnode_table.vnodes[i];
}

/**
 * @brief 分配文件结构体
 * 
 * @return file_vnode_t* 对应VFS的vnode
 */
file_vnode_t *
vfs_alloc_file(void) 
{
    int i;
    acquire(&file_vnode_table.lock);
    for (i = 0;i < NFILE;i++) 
    {
        if (file_vnode_table.valid[i] == 0) 
        {
            file_vnode_table.valid[i] = 1;
            file_vnode_table.isdir[i] = 0;
            file_vnode_table.vnodes[i].fs = get_fs_by_type(EXT4);
            file_vnode_table.vnodes[i].data = kalloc();            
            break;
        }
    }
    release(&file_vnode_table.lock);
    if (i == NFILE) 
        return NULL;
    return &file_vnode_table.vnodes[i];
}

/**
 * @brief 释放目录结构体
 * 
 * @param dir 
 */
void 
vfs_free_dir(void *dir) 
{
    int i;
    acquire(&file_vnode_table.lock);
    for (i = 0; i < NFILE; i++) 
    {
        if (file_vnode_table.isdir[i] && 
            (dir == file_vnode_table.vnodes[i].data))
        {
            file_vnode_table.valid[i] = 0;
            kfree(file_vnode_table.vnodes[i].data);
            file_vnode_table.vnodes[i].data = NULL;
            release(&file_vnode_table.lock);
            return;
        }
    }
}

/**
 * @brief 释放文件结构体
 * 
 * @param file 
 */
void 
vfs_free_file(void *file) 
{
    int i;
    acquire(&file_vnode_table.lock);
    for (i = 0; i < NFILE; i++) 
    {
        if ((file_vnode_table.isdir[i]==0) && 
            (file == file_vnode_table.vnodes[i].data)) 
        {
            file_vnode_table.valid[i] = 0;
            kfree(file_vnode_table.vnodes[i].data);
            file_vnode_table.vnodes[i].data = NULL;
            release(&file_vnode_table.lock);
            return;
        }
    }
}