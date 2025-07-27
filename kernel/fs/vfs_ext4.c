#include "types.h"
#include "defs.h"
#include "timer.h"
#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif
#include "file.h"
#include "fs.h"
#include "stat.h"
#include "fcntl.h"
#include "blockdev.h"
#include "vfs_ext4.h"
#include "inode.h"
#include "fcntl.h"
#include "fifo.h"

#include "ext4_oflags.h"
#include "ext4_errno.h"
#include "ext4_fs.h"
#include "ext4_inode.h"
#include "ext4_super.h"
#include "ext4_types.h"

#include "ext4.h"
#include "ioctl.h"
#include "string.h"
#include "process.h"
#include "timer.h"

#include "vmem.h"
#include "cpu.h"

/**
 * @brief 初始化ext4文件系统，主要是清空块设备和挂载点
 *
 * @return int，0表示成功
 */
int vfs_ext4_init(void)
{
    ext4_device_unregister_all();
    ext4_init_mountpoints();
    return 0;
}

/**
 * @brief 挂载ext4文件系统
 *
 * @param fs
 * @param rwflag
 * @param data
 * @return int 负的标准错误码
 */
int vfs_ext4_mount(struct filesystem *fs, uint64_t rwflag, const void *data)
{
    struct vfs_ext4_blockdev *vbdev = vfs_ext4_blockdev_create(fs->dev);

    if (vbdev == NULL)
        return -ENOMEM;

#if DEBUG
    printf("MOUNT BEGIN %s\n", fs->path);
#endif
    int status = ext4_mount(DEV_NAME, fs->path, false);
#if DEBUG
    printf("EXT4 mount result: %d\n", status);
#endif

    if (status != EOK)
        vfs_ext4_blockdev_destroy(vbdev);
    else
    {
        fs->fs_data = vbdev;
        fs->rwflag = rwflag;
    }
    return status;
}

/**
 * @brief 获取文件系统的状态信息
 *
 * @param fs 文件系统
 * @param buf 状态信息结构体指针
 * @return int 负的标准错误码
 */
int vfs_ext4_statfs(struct filesystem *fs, struct statfs *buf)
{
    struct ext4_sblock *sb = NULL;
    int err = EOK;

    err = ext4_get_sblock(fs->path, &sb);
    if (err != EOK)
        return err;

    buf->f_bsize = ext4_sb_get_block_size(sb);
    buf->f_blocks = ext4_sb_get_blocks_cnt(sb);
    buf->f_bfree = ext4_sb_get_free_blocks_cnt(sb);
    buf->f_bavail = ext4_sb_get_free_blocks_cnt(sb);
    buf->f_type = sb->magic;
    buf->f_files = sb->inodes_count;
    buf->f_ffree = sb->free_inodes_count;
    buf->f_frsize = ext4_sb_get_block_size(sb);
    buf->f_bavail = sb->free_inodes_count;
    buf->f_fsid.val[0] = EXT4;
    buf->f_flags = fs->rwflag;
    buf->f_namelen = MAXFILENAME;
    return err;
}

/**
 * @brief ext4文件系统卸载
 *
 * @param fs
 * @return int 负的标准错误码
 */
int vfs_ext4_umount(struct filesystem *fs)
{
    struct vfs_ext4_blockdev *vbdev = fs->fs_data;

    if (vbdev == NULL)
        return -ENOMEM;

    int status = ext4_umount(fs->path);
    if (status != EOK)
        return -status;

    vfs_ext4_blockdev_destroy(vbdev);
    return EOK;
}

/**
 * @brief ext4文件系统操作结构体
 *
 */
struct filesystem_op EXT4_FS_OP =
    {
        .mount = vfs_ext4_mount,
        .umount = vfs_ext4_umount,
        .statfs = vfs_ext4_statfs,
};

/**************************上面是文件系统相关，下面是文件相关*******************************/

/**
 * @brief ext4的cache写回磁盘(就是所有脏了的buf都写回磁盘)
 *
 * @param fs 文件系统
 * @return int 负的标准错误码
 */
int vfs_ext4_flush(struct filesystem *fs)
{
    const char *path = fs->path;
    int err = ext4_cache_flush(path);
    if (err != EOK)
        return -err;

    return EOK;
}

/**
 * @brief ext4文件系统的ioctl操作
 *
 * @param f
 * @param cmd 指令
 * @param args 额外的参数
 * @return int，状态码
 */
int vfs_ext4_ioctl(struct file *f, int cmd, void *args)
{
    int status = 0;
    struct ext4_file *file = (struct ext4_file *)f->f_data.f_vnode.data;
    if (file == NULL)
        panic("Getting f's ext4 file failed.\n");

    switch (cmd)
    {
    case FIOCLEX:
        f->f_flags |= O_CLOEXEC;
        break;
    case FIONCLEX:
        f->f_flags &= ~O_CLOEXEC;
        break;
    case FIONREAD:
        status = ext4_fsize(file);
        break;
    case FIONBIO:
        break;
    case FIOASYNC:
        break;
    default:
        status = -EINVAL;
        break;
    }
    return status;
}

// user_addr = 1 indicate that user space pointer
/**
 * @brief ext4文件系统读取数据
 *
 * @param f
 * @param user_addr 表示地址是否是用户空间地址
 * @param addr
 * @param n
 * @return int 实际读取大小
 */
int vfs_ext4_read(struct file *f, int user_addr, const uint64 addr, int n)
{
    uint64 byteread = 0;
    struct ext4_file *ext4_f = (struct ext4_file *)f->f_data.f_vnode.data;
    if (ext4_f == NULL)
        panic("Getting f's ext4 file failed.\n");

    int status = 0;
    if (user_addr)
    {
        char *buf = kalloc();
        /* 分配缓冲区失败 */
        if (buf == NULL)
            panic("Allocating one page failed.\n");

        uint64 uaddr = addr, expect_read = 0, realread = 0;
        for (uint64 has_read = 0; has_read < n;
             has_read += realread, uaddr += realread)
        {
            expect_read = min(n - has_read, PGSIZE);
            realread = 0;
            int lock = 0;
            if (holding(&f->f_lock))
            {
                lock = 1;
                release(&f->f_lock); // 新增：释放VFS层锁
            }
            status = ext4_fread(ext4_f, buf, expect_read, &realread);
            if (!holding(&f->f_lock) && lock)
                acquire(&f->f_lock);
            if (status != EOK)
            {
                kfree(buf);
                return 0;
            }
            if (realread == 0)
            {
                break;
            }
            if (either_copyout(user_addr, uaddr, buf, realread) == -1)
            {
                kfree(buf);
                printf("vfs_ext4_read: copyout failed\n");
                return 0;
            }
            byteread += realread;
        }
        kfree(buf);
    }
    else
    {
        char *kbuf = (char *)addr;
        status = ext4_fread(ext4_f, kbuf, n, &byteread);
        if (status != EOK)
            return 0;
        memmove((char *)addr, kbuf, byteread);
    }
    f->f_pos = ext4_f->fpos;
    return byteread;
}

/**
 * @brief ext4文件系统从指定偏移读取数据
 *
 * @param f
 * @param user_addr
 * @param addr
 * @param n
 * @param offset
 * @return int 状态码(-1)或实际读取的大小
 */
int vfs_ext4_readat(struct file *f, int user_addr, const uint64 addr, int n, int offset)
{
    uint64 byteread = 0;
    struct ext4_file *ext4_f = (struct ext4_file *)f->f_data.f_vnode.data;
    if (ext4_f == NULL)
        panic("Getting f's ext4 file failed.\n");

    int status = ext4_fseek(ext4_f, offset, SEEK_SET);
    if (status != EOK)
        return -1;

    if (user_addr)
    {
        char *buf = kalloc();
        if (buf == NULL)
            panic("Allocating one page failed.\n");

        uint64 uaddr = addr, expect_read = 0, realread = 0;
        for (uint64 tot = 0; tot < n; tot += realread, uaddr += realread)
        {
            expect_read = min(n - tot, PGSIZE);
            realread = 0;
            status = ext4_fread(ext4_f, buf, expect_read, &realread);
            if (status != EOK)
            {
                kfree(buf);
                return 0;
            }
            if (realread == 0)
                break;
            if (either_copyout(user_addr, uaddr, buf, realread) == -1)
            {
                kfree(buf);
                printf("vfs_ext4_read: copyout failed\n");
                return 0;
            }
            byteread += realread;
        }
        kfree(buf);
    }
    else
    {
        char *kbuf = (char *)addr;
        status = ext4_fread(ext4_f, kbuf, n, &byteread);
        if (status != EOK)
            return 0;
        memmove((char *)addr, kbuf, byteread);
    }
    /* 复原ext4_file的偏移 */
    status = ext4_fseek(ext4_f, f->f_pos, SEEK_SET);
    if (status != EOK)
        return -1;
    return byteread;
}

/**
 * @brief ext4文件系统写数据
 *
 * @param f
 * @param user_addr 是否是用户地址
 * @param addr
 * @param n
 * @return int 写字节数
 */
int vfs_ext4_write(struct file *f, int user_addr, const uint64 addr, int n)
{
    uint64 bytewrite = 0;
    struct ext4_file *ext4_f = (struct ext4_file *)f->f_data.f_vnode.data;
    if (ext4_f == NULL)
        panic("Getting f's ext4 file failed.\n");

    int status = 0;
    if (user_addr)
    {
        char *buf = kalloc();
        if (buf == NULL)
            panic("Allocating one page failed.\n");

        uint64 uaddr = addr, expect = 0, real_write;
        for (uint64 has_write = 0; has_write < n; has_write += real_write, uaddr += real_write)
        {
            expect = min(n - has_write, PGSIZE);
            real_write = 0;
            if (either_copyin((void *)buf, user_addr, (uint64)uaddr, expect) == -1)
            {
                kfree(buf);
                printf("vfs_ext4_read: copyout failed\n");
                return 0;
            }
            release(&f->f_lock); // 新增：释放VFS层锁
            status = ext4_fwrite(ext4_f, buf, expect, &real_write);
            acquire(&f->f_lock); // 新增：重新获取VFS锁
            if (status != EOK)
            {
                kfree(buf);
                return 0;
            }
            if (real_write == 0)
                break;

            bytewrite += real_write;
        }
        kfree(buf);
    }
    else
    {
        char *kbuf = (char *)addr;
        status = ext4_fwrite(ext4_f, kbuf, n, &bytewrite);
        if (status != EOK)
            return 0;
    }
    f->f_pos = ext4_f->fpos;
    return bytewrite;
}

/**
 * @brief 调整文件读写位置（扩展文件系统实现）
 *
 * @param f        文件对象指针，需通过f_data.f_vnode.data字段关联ext4_file结构体
 * @param offset   偏移量（字节单位），当whence为SEEK_END时允许负值表示反向偏移
 * @param startflag   起始位置标志：
 *                 - SEEK_SET：从文件头开始
 *                 - SEEK_CUR：从当前位置开始
 *                 - SEEK_END：从文件末尾开始
 *
 * @return int     成功时返回新的文件位置，失败返回负的错误码（如-EINVAL）
 *
 * @note
 * 1. 当检测到ext4_file结构体为空时会触发内核panic（仅调试阶段）
 * 2. 内部调用ext4_fseek完成实际偏移计算，并将结果同步到vfs层的f_pos
 * 3. 若ext4_fseek返回非EOK错误码，会将其转换为负值返回（如-EIO）
 */
int vfs_ext4_lseek(struct file *f, int64_t offset, int startflag)
{
    int status = 0;
    struct ext4_file *file = (struct ext4_file *)f->f_data.f_vnode.data;
    if (file == NULL)
        panic("Getting f's ext4 file failed.\n");

    // if (startflag == SEEK_END && offset < 0) // offset可正可负!
    //     offset = -offset;

    status = ext4_fseek(file, offset, startflag);
    if (status != EOK)
        return -status;

    f->f_pos = file->fpos;
    return f->f_pos;
}

/**
 * @brief 关闭ext4目录文件
 *
 * @param f
 * @return int
 */
int vfs_ext4_dirclose(struct file *f)
{
    struct ext4_dir *dir = (struct ext4_dir *)f->f_data.f_vnode.data;
    if (dir == NULL)
        panic("Getting f's ext4 file failed.\n");

    int status = ext4_dir_close(dir);
    if (status != EOK)
    {
        printf("vfs_ext4_dirclose: cannot close directory\n");
        return -1;
    }
    vfs_free_dir(dir);
    f->f_data.f_vnode.data = NULL;
    return 0;
}

/**
 * @brief 关闭ext4文件
 *
 * @param f
 * @return int 状态码
 */
int vfs_ext4_fclose(struct file *f)
{
    struct ext4_file *file = (struct ext4_file *)f->f_data.f_vnode.data;
    if (file == NULL)
        panic("Getting f's ext4 file failed.\n");

    int status = ext4_fclose(file);
    if (status != EOK)
        return -1;

    vfs_free_file(file);
    f->f_data.f_vnode.data = NULL;
    return 0;
}

int vfs_ext4_open(const char *path, const char *dirpath, int flags)
{
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
    strcpy(f->f_path, absolute_path);
    int ret;
    if ((ret = vfs_ext4_openat(f)) < 0)
    {
        panic("vfs_ext4_openat error\n");
    }
    return ret;
}

/**
 * @brief 通过file打开文件
 *
 * 通过file打开文件
 * 需要在file中存储path 和 flag
 * 会分配存储文件的内存
 *
 * @param f 文件对象指针
 * @return int 成功返回0，失败返回错误码负数
 */
int vfs_ext4_openat(struct file *f)
{
    file_vnode_t *vnode = NULL;
    int status = 0;

    if (vfs_ext4_is_dir(f->f_path) == 0)
    {
        vnode = vfs_alloc_dir();
        if (vnode == NULL)
            return -ENOMEM;
        int lock = 0;
        if (holding(&f->f_lock))
        {
            lock = 1;
            release(&f->f_lock); // 新增：释放VFS层锁
        }
        status = ext4_dir_open((ext4_dir *)vnode->data, f->f_path);
        if (!holding(&f->f_lock) && lock)
            acquire(&f->f_lock); // 新增：释放VFS层锁
        if (status != EOK)
        {
            vfs_free_dir(vnode->data);
            return -ENOMEM;
        }
        f->f_data.f_vnode = *vnode;
    }
    else
    {
        vnode = vfs_alloc_file();
        if (vnode == NULL)
            return -ENOMEM;
        int lock = 0;
        if (holding(&f->f_lock))
        {
            lock = 1;
            release(&f->f_lock); // 新增：释放VFS层锁
        }
        status = ext4_fopen2(vnode->data, f->f_path, f->f_flags);
        if (!holding(&f->f_lock) && lock)
            acquire(&f->f_lock); // 新增：释放VFS层锁
        if (status != EOK)
        {
            vfs_free_file(vnode->data);
            return -status;
        }
        f->f_data.f_vnode = *vnode;
        f->f_pos = ((ext4_file *)vnode->data)->fpos;

        // 如果文件是新创建的，设置正确的权限
        if ((f->f_flags & O_CREAT) && f->f_mode != 0)
        {
            int lock2 = 0;
            if (holding(&f->f_lock))
            {
                lock2 = 1;
                release(&f->f_lock);
            }
            // 设置文件权限，使用 f->f_mode
            status = ext4_mode_set(f->f_path, f->f_mode);
            if (!holding(&f->f_lock) && lock2)
                acquire(&f->f_lock);
            if (status != EOK)
            {
                DEBUG_LOG_LEVEL(LOG_WARNING, "Failed to set file mode for %s: %d\n", f->f_path, status);
            }
        }
    }
    f->f_count = 1;
    struct ext4_inode inode;
    uint32 ino;
    if (ext4_raw_inode_fill(f->f_path, &ino, &inode) == EOK)
    {
        struct ext4_sblock *sb = NULL;
        ext4_get_sblock(f->f_path, &sb);
        if (sb != NULL)
        {
            int inode_type = ext4_inode_type(sb, &inode);
            if (inode_type == EXT4_INODE_MODE_CHARDEV)
            {
                f->f_type = FD_DEVICE;
                f->f_major = ext4_inode_get_dev(&inode);
            }
            else if (inode_type == EXT4_INODE_MODE_FIFO)
            {
                struct fifo *fi = fifo_open(f->f_path, f->f_flags);
                if (!fi)
                {
                    return -ENXIO; // 非阻塞写模式且没有读者
                }
                f->f_type = FD_FIFO;
                f->f_major = ext4_inode_get_dev(&inode);
                f->f_data.f_fifo = fi; // 将FIFO结构体指针存储在文件数据中
            }
            else
                f->f_type = FD_REG;
        }
        else
        {
            f->f_type = FD_REG;
        }
    }
    [[maybe_unused]] struct ext4_file *file = (struct ext4_file *)f->f_data.f_vnode.data;
    DEBUG_LOG_LEVEL(LOG_DEBUG, "openat打开文件的路径: %s,大小是: 0x%x\n", f->f_path, file->fsize);
    return EOK;
}

/**
 * @brief ext4硬链接
 *
 * @param oldpath 旧路径
 * @param newpath 新路径
 * @return int 0表示成功，负数的标准错误码
 */
int vfs_ext4_link(const char *oldpath, const char *newpath)
{
    int r = ext4_flink(oldpath, newpath);
    if (r != EOK)
        return -r;
    return EOK;
}

/**
 * @brief 读取符号链接
 *
 * @param path 符号链接路径
 * @param ubuf 用户空间缓冲区
 * @param bufsize 缓冲区大小
 * @return int 负的标准错误码
 */
int vfs_ext_readlink(const char *path, uint64 ubuf, size_t bufsize)
{
    char linkpath[MAXPATH];
    size_t readbytes = 0;

    // 调用 ext4_readlink 读取符号链接的内容
    int r = ext4_readlink(path, linkpath, min(bufsize, MAXPATH), &readbytes);
    if (r != EOK)
    {
        return -r;
    }

    // 确保不会超出缓冲区大小
    if (readbytes > bufsize)
    {
        readbytes = bufsize;
    }

#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[vfs_ext_readlink] path: %s, linkpath: %s, readbytes: %zu\n", path, linkpath, readbytes);
#endif

    // 将读取的符号链接内容复制到用户空间
    if (copyout(myproc()->pagetable, ubuf, linkpath, readbytes) != 0)
    {
        return -EFAULT;
    }

    return readbytes; // 返回实际读取的字节数
}

/**
 * @brief ext4移除path路径的文件或目录
 *
 * @param path
 * @return int 负的标准错误码
 */
int vfs_ext4_rm(const char *path)
{
    int status = 0;
    ext4_dir ext4_d;
    status = ext4_dir_open(&ext4_d, path);
    if (status == EOK)
    {
        (void)ext4_dir_close(&ext4_d);
        ext4_dir_rm(path);
    }
    else
        status = ext4_fremove(path);
    return -status;
}

/**
 * @brief 获取ext4文件的metadata
 *
 * @param path 文件路径
 * @param st 状态信息结构体指针
 * @return int 状态码，0表示成功，-1表示失败
 */
int vfs_ext4_stat(const char *path, struct kstat *st)
{
    struct ext4_inode inode;
    uint32 ino = 0;

    const char *statpath = path;

    int status = ext4_raw_inode_fill(statpath, &ino, &inode);
    if (status != EOK)
        return -status;

    struct ext4_sblock *sb = NULL;
    status = ext4_get_sblock(statpath, &sb);
    if (status != EOK)
        return -status;

    st->st_dev = 0;
    st->st_ino = ino;
    st->st_mode = ext4_inode_get_mode(sb, &inode);
    st->st_nlink = ext4_inode_get_links_cnt(&inode);
    st->st_uid = ext4_inode_get_uid(&inode);
    st->st_gid = ext4_inode_get_gid(&inode);
    st->st_rdev = ext4_inode_get_dev(&inode);
    st->st_size = (uint64)inode.size_lo;
    /* 访问时间 */
    st->st_atime_sec = ext4_inode_get_access_time(&inode);
    /* 从 atime_extra 中提取纳秒，右移2位 */
    st->st_atime_nsec = (inode.atime_extra >> 2) & 0x3FFFFFFF; //< 30 bits for nanoseconds
    /* 修改时间 */
    st->st_mtime_sec = ext4_inode_get_modif_time(&inode);
    /* 从 mtime_extra 中提取纳秒，右移2位 */
    st->st_mtime_nsec = (inode.mtime_extra >> 2) & 0x3FFFFFFF;

    /* 状态改变时间 */
    st->st_ctime_sec = ext4_inode_get_change_inode_time(&inode);
    /* 从 ctime_extra 中提取纳秒，右移2位 */
    st->st_ctime_nsec = (inode.ctime_extra >> 2) & 0x3FFFFFFF;

    if (status == 0)
    {
        struct ext4_mount_stats s;
        status = ext4_mount_point_stats(statpath, &s);
        if (status == 0)
        {
            st->st_blksize = s.block_size;
            st->st_blocks = (st->st_size + s.block_size) / s.block_size;
        }
    }
    return -status;
}

/**
 * @brief 获取ext4文件的metadata
 *
 * @param f 文件对象指针
 * @param st 状态信息结构体指针
 * @return int 状态码，0表示成功，-1表示失败
 */
int vfs_ext4_fstat(struct file *f, struct kstat *st)
{
    struct ext4_inode inode;
    uint32 inode_num = 0;
    const char *file_path = f->f_path;

    int status = ext4_raw_inode_fill(file_path, &inode_num, &inode);
    if (status != EOK)
        return -status;

    struct ext4_sblock *sb = NULL;
    status = ext4_get_sblock(file_path, &sb);
    if (status != EOK)
        return -status;

    st->st_dev = 0;
    st->st_ino = inode_num;
    st->st_mode = ext4_inode_get_mode(sb, &inode);
    st->st_nlink = ext4_inode_get_links_cnt(&inode);
    st->st_uid = ext4_inode_get_uid(&inode);
    st->st_gid = ext4_inode_get_gid(&inode);
    st->st_rdev = ext4_inode_get_dev(&inode);
    st->st_size = inode.size_lo;
    st->st_blksize = inode.size_lo / inode.blocks_count_lo;
    st->st_blocks = (uint64)inode.blocks_count_lo;

    st->st_atime_sec = ext4_inode_get_access_time(&inode);
    st->st_atime_nsec = (inode.atime_extra >> 2) & 0x3FFFFFFF; //< 30 bits for nanoseconds
    st->st_ctime_sec = ext4_inode_get_change_inode_time(&inode);
    st->st_ctime_nsec = (inode.ctime_extra >> 2) & 0x3FFFFFFF; //< 30 bits for nanoseconds
    st->st_mtime_sec = ext4_inode_get_modif_time(&inode);
    st->st_mtime_nsec = (inode.mtime_extra >> 2) & 0x3FFFFFFF; //< 30 bits for nanoseconds
    return EOK;
}

/**
 * @brief 获取ext4文件的拓展metadata（statx）
 *
 * @param f
 * @param st
 * @return int 负的标准错误码
 */
int vfs_ext4_statx(const char *path, struct statx *st)
{
    struct ext4_inode inode;
    uint32 inode_num = 0;
    const char *file_path = path;

    int status = ext4_raw_inode_fill(file_path, &inode_num, &inode);
    if (status != EOK)
        return -status;

    struct ext4_sblock *sb = NULL;
    status = ext4_get_sblock(file_path, &sb);
    if (status != EOK)
        return -status;

    st->stx_rdev_major = 0;
    st->stx_rdev_minor = 0;
    st->stx_dev_major = 0;
    st->stx_dev_minor = 0;
    st->stx_ino = inode_num;
    st->stx_mode = ext4_inode_get_mode(sb, &inode);
    st->stx_nlink = ext4_inode_get_links_cnt(&inode);
    st->stx_uid = ext4_inode_get_uid(&inode);
    st->stx_gid = ext4_inode_get_gid(&inode);
    st->stx_rdev_major = ext4_inode_get_dev(&inode);
    st->stx_size = inode.size_lo;
    st->stx_blksize = inode.size_lo / inode.blocks_count_lo;
    st->stx_blocks = (uint64)inode.blocks_count_lo;

    st->stx_atime.tv_sec = ext4_inode_get_access_time(&inode);
    st->stx_atime.tv_nsec = (inode.atime_extra >> 2) & 0x3FFFFFFF; //< 30 bits for nanoseconds
    st->stx_ctime.tv_sec = ext4_inode_get_change_inode_time(&inode);
    st->stx_ctime.tv_nsec = (inode.ctime_extra >> 2) & 0x3FFFFFFF; //< 30 bits for nanoseconds
    st->stx_mtime.tv_sec = ext4_inode_get_modif_time(&inode);
    st->stx_mtime.tv_nsec = (inode.mtime_extra >> 2) & 0x3FFFFFFF; //< 30 bits for nanoseconds
    return EOK;
}

/**
 * @brief ext4遍历目录项，获取目录项信息，填充到用户缓冲区
 *
 * @param f     已打开的目录文件指针
 * @param dirp  用户提供的缓冲区，用于存放目录项信息 (struct linux_dirent64 数组)
 * @param count 缓冲区大小，字节数
 * @return int  返回写入缓冲区的字节数，失败返回负错误码
 */
/*全新版本!支持busybox和basic*/
int vfs_ext4_getdents(struct file *f, struct linux_dirent64 *dirp, int count)
{
    int index = 0;
    struct linux_dirent64 *d;
    const ext4_direntry *rentry;
    int totlen = 0;
    uint64 current_offset = 0;

    /* make integer count */
    if (count == 0)
    {
        return -EINVAL;
    }

    d = dirp;

    while (1)
    {
        rentry = ext4_dir_entry_next(f->f_data.f_vnode.data);
        if (rentry == NULL)
            break;

        int namelen = strlen((const char *)rentry->name);
        /*
         * 长度是前四项的19加上namelen(字符串长度包括结尾的\0)
         * reclen是namelen+2,如果是+1会错误。原因是没考虑name[]开头的'\'
         */
        int reclen = sizeof d->d_ino + sizeof d->d_off + sizeof d->d_reclen + sizeof d->d_type + namelen + 1;
        if (reclen % 8)
            reclen = reclen - reclen % 8 + 8; //<对齐
        if (reclen < sizeof(struct linux_dirent64))
            reclen = sizeof(struct linux_dirent64);

        if (totlen + reclen >= count)
            break;

        char name[MAXPATH] = {0};
        strcat(name, (const char *)rentry->name); //< 追加，二者应该都以'/'开头
        strncpy(d->d_name, name, MAXPATH);

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
        d->d_ino = rentry->inode;
        d->d_off = current_offset + reclen; // start from 1
        d->d_reclen = reclen;
        ++index;
        totlen += d->d_reclen;
        current_offset += reclen;
        d = (struct linux_dirent64 *)((char *)d + d->d_reclen);
    }

    return totlen;
}

/**
 * @brief ext4重命名文件或目录
 *
 * @param oldpath
 * @param newpath
 * @return int
 */
int vfs_ext4_frename(const char *oldpath, const char *newpath)
{
    int status = ext4_frename(oldpath, newpath);
    if (status != EOK)
        return -status;

    return -status;
}

/**
 * @brief ext4创建目录
 *
 * @param path
 * @param mode New mode bits (for example 0777).
 * @return int
 */
int vfs_ext4_mkdir(const char *path, uint64_t mode)
{
    /* Create the directory. */
    int status = ext4_dir_mk(path);
    if (status != EOK)
        return -status;

    /* Set mode. */
    status = ext4_mode_set(path, mode);

    return -status;
}

/**
 * @brief 判断这个路径是否是目录
 */
int vfs_ext4_is_dir(const char *path)
{
    struct ext4_dir *dir = vfs_alloc_dir()->data;
    int status = ext4_dir_open(dir, path);
    if (status != EOK)
    {
        vfs_free_dir(dir);
        return -status;
    }
    status = ext4_dir_close(dir);
    vfs_free_dir(dir);
    if (status != EOK)
        return -status;

    return EOK;
}

/**
 * @brief 把linux_dirent64文件类型转换为对应ext4文件类型
 *
 * @param filetype
 * @return uint32
 */
static uint32
vfs_ext4_filetype_from_vfs_filetype(uint32 filetype)
{
    switch (filetype)
    {
    case T_DIR:
        return EXT4_DE_DIR;
    case T_FILE:
        return EXT4_DE_REG_FILE;
    case T_CHR:
        return EXT4_DE_CHRDEV;
    case T_FIFO:
        return EXT4_DE_FIFO;
    default:
        return EXT4_DE_UNKNOWN;
    }
}

/**
 * @brief 创建一个特殊文件
 *
 * @param path     Path to new special file.
 * @param filetype Filetype of the new special file.
 * 	           (that must not be regular file, directory, or unknown type)
 * @param dev      If filetype is char device or block device,
 * 	           the device number will become the payload in the inode.
 * @return  Standard error code的相反数.
 */
int vfs_ext4_mknod(const char *path, uint32 mode, uint32 dev)
{
    int status = ext4_mknod(path, vfs_ext4_filetype_from_vfs_filetype(mode), dev);
    return -status;
}

/**
 * @brief 得到ext4文件的大小
 *
 * @param path
 * @param size 文件大小
 * @return int 状态码，0表示成功，负的状态码
 */
int vfs_ext4_get_filesize(const char *path, uint64_t *size)
{
    struct ext4_inode inode;
    struct ext4_sblock *sb = NULL;
    uint32_t ino;
    int status = ext4_get_sblock(path, &sb);
    if (status != EOK)
        return -status;

    status = ext4_raw_inode_fill(path, &ino, &inode);
    if (status != EOK)
        return -status;

    *size = ext4_inode_get_size(sb, &inode);
    return EOK;
}

/**
 * @brief 设置指定路径文件的访问时间和修改时间
 *
 * 该函数根据参数 ts 指定的时间数组，更新文件的访问时间（atime）和修改时间（mtime）。
 * 如果 ts 为 NULL，则将访问时间和修改时间都设置为当前时间。
 * ts 为长度为 2 的 timespec 数组，ts[0] 表示访问时间，ts[1] 表示修改时间。
 * ts[0].tv_nsec 或 ts[1].tv_nsec 可以为特殊值：
 *   - UTIME_NOW：使用当前时间更新对应时间戳。
 *   - UTIME_OMIT：不更新对应时间戳。
 *
 * @param path 文件路径字符串
 * @param ts   指向长度为 2 的 timespec 结构体数组，或者 NULL（表示使用当前时间）
 * @return int 成功返回 EOK，失败返回负错误码
 */
int vfs_ext4_utimens(const char *path, const struct timespec *ts)
{
    int status = EOK;
    if (!ts)
    {
        status = ext4_atime_set(path, NS_to_S(TIME2NS(r_time())));
        if (status != EOK)
            return -status;
        status = ext4_mtime_set(path, NS_to_S(TIME2NS(r_time())));
        if (status != EOK)
            return -status;
        return EOK;
    }

    if (ts[0].tv_nsec == UTIME_NOW)
        status = ext4_atime_set(path, NS_to_S(TIME2NS(r_time())));
    else if (ts[0].tv_nsec != UTIME_OMIT)
        status = ext4_atime_set(path, NS_to_S(TIMESEPC2NS(ts[0])));

    if (status != EOK)
        return -status;

    if (ts[1].tv_nsec == UTIME_NOW)
        status = ext4_mtime_set(path, NS_to_S(TIME2NS(r_time())));
    else if (ts[1].tv_nsec != UTIME_OMIT)
        status = ext4_mtime_set(path, NS_to_S(TIMESEPC2NS(ts[1])));

    if (status != EOK)
        return -status;
    return EOK;
}

/**
 * @brief 设置已打开文件的访问时间和修改时间
 *
 * 该函数根据参数 ts 指定的时间数组，更新已打开文件的访问时间（atime）和修改时间（mtime）。
 * 如果 ts 为 NULL，则将访问时间和修改时间都设置为当前时间。
 * ts 为长度为 2 的 timespec 数组，ts[0] 表示访问时间，ts[1] 表示修改时间。
 * ts[0].tv_nsec 或 ts[1].tv_nsec 可以为特殊值：
 *   - UTIME_NOW：使用当前时间更新对应时间戳。
 *   - UTIME_OMIT：不更新对应时间戳。
 *
 * @param f    指向已打开的文件结构体指针
 * @param ts   指向长度为 2 的 timespec 结构体数组，或者 NULL（表示使用当前时间）
 * @return int 成功返回 EOK，失败返回负错误码
 */
int vfs_ext4_futimens(struct file *f, const struct timespec *ts)
{
    int status = EOK;
    struct ext4_file *file = (struct ext4_file *)f->f_data.f_vnode.data;

    if (file == NULL)
        panic("Getting file's ext file failed\n");

    if (!ts)
    {
        status = ext4_atime_set(f->f_path, NS_to_S(TIME2NS(r_time())));
        if (status != EOK)
            return -status;
        status = ext4_mtime_set(f->f_path, NS_to_S(TIME2NS(r_time())));
        if (status != EOK)
            return -status;
        return EOK;
    }

    if (ts[0].tv_nsec == UTIME_NOW)
        status = ext4_atime_set(f->f_path, NS_to_S(TIME2NS(r_time())));
    else if (ts[0].tv_nsec != UTIME_OMIT)
        status = ext4_atime_set(f->f_path, NS_to_S(TIMESEPC2NS(ts[0])));

    if (status != EOK)
        return -status;

    if (ts[1].tv_nsec == UTIME_NOW)
        status = ext4_mtime_set(f->f_path, NS_to_S(TIME2NS(r_time())));
    else if (ts[1].tv_nsec != UTIME_OMIT)
        status = ext4_mtime_set(f->f_path, NS_to_S(TIMESEPC2NS(ts[1])));

    if (status != EOK)
        return -status;
    return EOK;
}

/**
 * @brief 根据父路径和子路径unlink文件
 *
 * @param pdir
 * @param cdir
 * @return int 标准错误码
 * @todo 也许需要考虑mutex锁?
 */
int vfs_ext4_unlinkat(const char *pdir, const char *cdir)
{
    extern struct ext4_mountpoint *_ext4_get_mount(const char *path);
    struct ext4_mountpoint *mp = _ext4_get_mount("/");
    if (mp == NULL)
    {
        printf("vfs_ext4_unlinkat: mount point not found\n");
        return -ENOENT;
    }
    struct ext4_fs *fs = &mp->fs;
    struct ext4_inode parent, child;
    uint32_t pino, cino;
    struct ext4_inode_ref parent_ref, child_ref;
    int rc;

    /* 1. 查找父目录 inode_ref */
    rc = ext4_raw_inode_fill(pdir, &pino, &parent);
    if (rc != EOK)
        return -rc;
    rc = ext4_fs_get_inode_ref(fs, pino, &parent_ref);
    if (rc != EOK)
        return -rc;

    /* 2. 查找子节点 inode_ref */
    rc = ext4_raw_inode_fill(cdir, &cino, &child);
    if (rc != EOK)
        return -rc;
    rc = ext4_fs_get_inode_ref(fs, cino, &child_ref);

    if (rc != EOK)
        return -rc;
    if (rc != EOK)
    {
        ext4_fs_put_inode_ref(&parent_ref);
        return -rc;
    }

    /* 3. 获取子节点在父目录下的名字 */
    const char *name = strrchr(cdir, '/');
    if (name)
        name++;
    else
        name = cdir;
    uint32_t name_len = strlen(name);

    /* 4. 调用底层unlink */
    extern int _ext4_unlink(struct ext4_mountpoint * mp, struct ext4_inode_ref * parent, struct ext4_inode_ref * child,
                            const char *name, uint32_t name_len);
    rc = _ext4_unlink(mp, &parent_ref, &child_ref, name, name_len);

    /* 5. 释放inode_ref */
    ext4_fs_put_inode_ref(&parent_ref);
    ext4_fs_put_inode_ref(&child_ref);

    return -rc;
}

int create_file(const char *path, const char *content, int flags)
{
    struct file *f = filealloc();
    if (!f)
        return -1;

    strcpy(f->f_path, path);
    f->f_flags = flags | O_WRONLY | O_CREAT;
    // 应用umask到文件权限
    struct proc *p = myproc();
    f->f_mode = (0644 & ~p->umask) & 07777; // 应用umask到默认权限
    f->f_type = FD_REG;

    // 创建文件
    int ret = vfs_ext4_openat(f);
    if (ret < 0)
    {
        printf("创建失败: %s (错误码: %d)\n", path, ret);
        get_file_ops()->close(f);
        return ret;
    }

    // 写入数据
    int len = strlen(content);
    int written = get_file_ops()->write(f, (uint64)content, len);

    // 提交并关闭
    get_file_ops()->close(f);

    if (written != len)
    {
        printf("写入不完全: %d/%d 字节\n", written, len);
        return -2;
    }
    return 0;
}
