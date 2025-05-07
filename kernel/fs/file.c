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

#include "process.h"
#include "vfs_ext4.h"
#include "string.h"

#include "cpu.h"
#include "vmem.h"

struct devsw devsw[NDEV];

/**
 * @brief 文件描述符表
 * 
 * 该结构体用于管理系统中打开的文件描述符。
 * 包含一个锁和一个文件数组，锁用于保护对文件数组的访问。
 */
struct {
    struct spinlock lock;
    struct file file[NFILE];
} ftable;

/**
 * @brief ext4目录表
 * 
 * 该结构体用于管理ext4文件系统中的目录。
 * 包含一个锁和一个目录数组，锁用于保护对数组的访问。
 */
struct {
    struct spinlock lock;
    struct ext4_dir dir[NFILE];
    int valid[NFILE];
} ext4_dir_table;

/**
 * @brief ext4文件表
 * 
 * 该结构体用于管理ext4文件系统中的文件。
 * 包含一个锁和一个文件数组，锁用于保护对数组的访问。
 */
struct {
    struct spinlock lock;
    struct ext4_file file[NFILE];
    int valid[NFILE];
} ext4_file_table;


/**
 * @brief 分配文件描述符
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
    for(f = ftable.file; f < ftable.file + NFILE; f++){
        if(f->f_count == 0){
            f->f_count = 1;
            release(&ftable.lock);
            return f;
        }
    }
    release(&ftable.lock);
    return 0;
}

int 
fdalloc(struct file *f){
    int fd;
    proc_t *p = myproc();
    for(fd = 0 ; fd < NOFILE; fd++){
        if(p->ofile[fd] == 0){
            p->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
}

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
    f->f_count = 0;
    f->f_type = FD_NONE;
    release(&ftable.lock);

    if(ff.f_type == FD_PIPE)
    {
        pipeclose(ff.f_pipe, get_fops()->writable(&ff));
    } 
    else if(ff.f_type == FD_REG || ff.f_type == FD_DEVICE)
    {
        /*
         * file中需要得到filesystem的类型
         * 但是这里暂时只支持EXT4
         */
        if (vfs_ext_is_dir(ff.f_path) == 0) 
            vfs_ext_dirclose(&ff);
        else 
            vfs_ext_fclose(&ff);
        if (ff.removed) 
        {
            vfs_ext_rm(ff.f_path);
            ff.removed = 0;
        }
    }
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
 * @return int 状态码，0成功，-1失败
 */
int
filestat(struct file *f, uint64 addr)
{
    struct proc *p = myproc();
    struct kstat st;
    if(f->f_type == FD_REG || f->f_type == FD_DEVICE)
    {
        vfs_ext_fstat(f, &st);
        if(copyout(p->pagetable, addr, (char *)(&st), sizeof(st)) < 0)
            return -1;
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
filestatx(struct file *f, uint64 addr) {
    struct proc *p = myproc();
    struct statx st;
    if(f->f_type == FD_REG || f->f_type == FD_DEVICE)
    {
        vfs_ext_statx(f, &st);
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

    if(get_fops()->readable(f) == 0)
        return -1;

    if(f->f_type == FD_PIPE)
    {
        r = piperead(f->f_pipe, addr, n);
    } 
    else if(f->f_type == FD_DEVICE)
    {
        if(f->f_major < 0 || f->f_major >= NDEV || !devsw[f->f_major].read)
            return -1;
        r = devsw[f->f_major].read(1, addr, n);
    } 
    else if(f->f_type == FD_REG)
    {
        r = vfs_ext_read(f, 1, addr, n);
    } 
    else 
    {
        panic("fileread");
    }

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

    if(get_fops()->readable(f) == 0)
        return -1;
    if (f->f_type == FD_REG) 
    {
        r = vfs_ext_readat(f, 0, addr, n, offset);
    }
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

    if(get_fops()->writable(f) == 0)
        return -1;

    if(f->f_type == FD_PIPE)
    {
        ret = pipewrite(f->f_pipe, addr, n);
    } 
    else if(f->f_type == FD_DEVICE)
    {
        if(f->f_major < 0 || f->f_major >= NDEV || !devsw[f->f_major].write)
            return -1;
        ret = devsw[f->f_major].write(1, addr, n);
    } 
    else if(f->f_type == FD_REG)
    {
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

            r = vfs_ext_write(f, 1, addr + i, n1);

            if(r != n1)
            {
                // error from writei
                break;
            }
            i += r;
        }
        ret = (i == n ? n : -1);
    } 
    else 
    {
        panic("filewrite");
    }

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

struct file_operations file_ops = 
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
};

struct file_operations *
get_fops(void) 
{
    return &file_ops;
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
    initlock(&ext4_dir_table.lock, "ext4_dir_table");
    initlock(&ext4_file_table.lock, "ext4_file_table");
	memset(ftable.file, 0, sizeof(ftable.file));
}

/**
 * @brief 分配ext4目录结构体
 * 
 * @return struct ext4_dir* 
 */
struct ext4_dir *
alloc_ext4_dir(void) {
    int i;
    acquire(&ext4_dir_table.lock);
    for (i = 0;i < NFILE;i++) {
        if (ext4_dir_table.valid[i] == 0) {
            ext4_dir_table.valid[i] = 1;
            break;
        }
    }
    release(&ext4_dir_table.lock);
    if (i == NFILE) {
        return NULL;
    }
    return &ext4_dir_table.dir[i];
}

/**
 * @brief 分配ext4文件结构体
 * 
 * @return struct ext4_file* 
 */
struct ext4_file *
alloc_ext4_file(void) {
    int i;
    acquire(&ext4_file_table.lock);
    for (i = 0;i < NFILE;i++) {
        if (ext4_file_table.valid[i] == 0) {
            ext4_file_table.valid[i] = 1;
            break;
        }
    }
    release(&ext4_file_table.lock);
    if (i == NFILE) {
        return NULL;
    }
    return &ext4_file_table.file[i];
}

/**
 * @brief 释放ext4目录结构体
 * 
 * @param dir 
 */
void 
free_ext4_dir(struct ext4_dir *dir) 
{
    int i;
    acquire(&ext4_dir_table.lock);
    for (i = 0;i < NFILE;i++) {
        if (dir == &ext4_dir_table.dir[i]) {
            ext4_dir_table.valid[i] = 0;
            release(&ext4_dir_table.lock);
            return;
        }
    }
}

/**
 * @brief 释放ext4文件结构体
 * 
 * @param file 
 */
void 
free_ext4_file(struct ext4_file *file) 
{
    int i;
    acquire(&ext4_file_table.lock);
    for (i = 0;i < NFILE;i++) {
        if (file == &ext4_file_table.file[i]) {
            ext4_file_table.valid[i] = 0;
            release(&ext4_file_table.lock);
            return;
        }
    }
}
























