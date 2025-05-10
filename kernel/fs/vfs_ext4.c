
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
 * @return int 状态码，0表示成功，-1表示失败
 */
int 
vfs_ext_mount(struct filesystem *fs, uint64_t rwflag, const void *data) {
    int r = 0;
    // struct ext4_blockdev *bdev = NULL;
    struct vfs_ext4_blockdev *vbdev = vfs_ext4_blockdev_create(fs->dev);

    if (vbdev == NULL) {
        r = -ENOMEM;
        goto out;
    }

    printf("MOUNT BEGIN %s\n", fs->path);
    // bdev = &vbdev->bd;
    r = ext4_mount(DEV_NAME, fs->path, false);
    printf("EXT4 mount result: %d\n", r);

    // r = ext4_cache_write_back(fs->path, true);
    // if (r != EOK) {
    //     printf("EXT4 cache write back error! r=%d\n", r);
    //     return -1;
    // }

    if (r != EOK) {
        vfs_ext4_blockdev_destroy(vbdev);
        goto out;
    } else {
        // ext4_mount_setup_locks(fs->path, &ext4_lock_ops);
        //获得ext4文件系统的超级块
        // ext4_get_sblock(fs->path, (struct ext4_sblock **)(&(fs->fs_data)));
    }
out:
    return r;
}

/**
 * @brief 获取文件系统的状态信息
 * 
 * @param fs 文件系统
 * @param buf 状态信息结构体指针
 * @return int 状态码，0表示成功，-1表示失败
 */
int 
vfs_ext_statfs(struct filesystem *fs, struct statfs *buf) {
    struct ext4_sblock *sb = NULL;
    int err = EOK;

    err = ext4_get_sblock(fs->path, &sb);
    if (err != EOK) {
        return err;
    }

    buf->f_bsize = ext4_sb_get_block_size(sb);
    buf->f_blocks = ext4_sb_get_blocks_cnt(sb);
    buf->f_bfree = ext4_sb_get_free_blocks_cnt(sb);
    buf->f_bavail = ext4_sb_get_free_blocks_cnt(sb);
    buf->f_type = sb->magic;
    buf->f_files = sb->inodes_count;
    buf->f_ffree = sb->free_inodes_count;
    buf->f_frsize = ext4_sb_get_block_size(sb);
    buf->f_bavail = sb->free_inodes_count;
    buf->f_fsid.val[0] = 2; /* why 2? */
    buf->f_flags = 0;
    buf->f_namelen = 32;
    return err;
}

/**
 * @brief ext4文件系统操作结构体
 * 
 */
struct filesystem_op EXT4_FS_OP = {
    .mount = vfs_ext_mount,
    .umount = vfs_ext_umount,
    .statfs = vfs_ext_statfs,
};

/**
 * @brief ext4文件系统卸载
 * 
 * @param fs 
 * @return int 状态码
 */
int vfs_ext_umount(struct filesystem *fs) {
    int r = 0;
    struct vfs_ext4_blockdev *vbdev = fs->fs_data;

    if (vbdev == NULL) {
        r = -ENOMEM;
        return r;
    }

    ext4_umount(fs->path);
    if (r != EOK) {
        return r;
    }

    vfs_ext4_blockdev_destroy(vbdev);
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
int 
vfs_ext_ioctl(struct file *f, int cmd, void *args) {
    int r = 0;
    struct ext4_file *file = (struct ext4_file *)f -> f_data.f_vnode.data;
    if (file == NULL) {
        panic("vfs_ext_ioctl: cannot get ext4 file\n");
    }

    switch (cmd) 
    {
        case FIOCLEX:
            f->f_flags |= O_CLOEXEC;
            break;
        case FIONCLEX:
            f->f_flags &= ~O_CLOEXEC;
            break;
        case FIONREAD:
            r = ext4_fsize(file);
            break;
        case FIONBIO:
            break;
        case FIOASYNC:
            break;
        default:
            r = -EINVAL;
            break;
    }
    return r;
}

//user_addr = 1 indicate that user space pointer
//TODO:动态分配缓冲区
/**
 * @brief ext4文件系统读取数据
 * 
 * @param f 
 * @param user_addr 表示地址是否是用户空间地址
 * @param addr 
 * @param n 
 * @return int 状态码
 */
int 
vfs_ext_read(struct file *f, int user_addr, const uint64 addr, int n) {
    uint64 byteread = 0;
    struct ext4_file *file = (struct ext4_file *)f -> f_data.f_vnode.data;
    if (file == NULL) {
        panic("vfs_ext_read: cannot get ext4 file\n");
    }
    int r = 0;
    if (user_addr) {
        char *buf = kalloc();
        uint64 mread = 0;
        /* 分配缓冲区失败 */
        if (buf == NULL) {
            panic("vfs_ext_read: kalloc failed\n");
        }
        uint64 uaddr = addr, expect = 0;
        for (uint64 tot = 0; tot < n; tot += mread, uaddr += mread) {
            expect = min(n - tot, PGSIZE);
            mread = 0;
            r = ext4_fread(file, buf, expect, &mread);
            if (r != EOK) {
                kfree(buf);
                return 0;
            }
            if (mread == 0) {
                break;
            }
            if (either_copyout(user_addr, uaddr, buf, mread) == -1) {
                kfree(buf);
                // printf("vfs_ext_read: copyout failed\n");
                return 0;
            }
            byteread += mread;
        }
        kfree(buf);
    } else {
        char *kbuf = (char *) addr;
        r = ext4_fread(file, kbuf, n, &byteread);
        if (r != EOK) {
            return 0;
        }
        memmove((char *) addr, kbuf, byteread);
    }
    f -> f_pos = file->fpos;

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
 * @return int 状态码
 */
int 
vfs_ext_readat(struct file *f, int user_addr, const uint64 addr, int n, int offset) {
    uint64 byteread = 0;
    struct ext4_file *file = (struct ext4_file *)f -> f_data.f_vnode.data;
    if (file == NULL) {
        panic("vfs_ext_read: cannot get ext4 file\n");
    }
    int r = ext4_fseek(file, offset, SEEK_SET);
    if (r != EOK) {
        return -1;
    }
    if (user_addr) {
        char *buf = kalloc();
        uint64 mread = 0;
        if (buf == NULL) {
            panic("vfs_ext_read: kalloc failed\n");
        }
        uint64 uaddr = addr, expect = 0;
        for (uint64 tot = 0; tot < n; tot += mread, uaddr += mread) {
            expect = min(n - tot, PGSIZE);
            mread = 0;
            r = ext4_fread(file, buf, expect, &mread);
            if (r != EOK) {
                kfree(buf);
                return 0;
            }
            if (mread == 0) {
                break;
            }
            if (either_copyout(user_addr, uaddr, buf, mread) == -1) {
                kfree(buf);
                // printf("vfs_ext_read: copyout failed\n");
                return 0;
            }
            byteread += mread;
        }
        kfree(buf);
    } else {
        char *kbuf = (char *) addr;
        r = ext4_fread(file, kbuf, n, &byteread);
        if (r != EOK) {
            return 0;
        }
        memmove((char *) addr, kbuf, byteread);
    }
    r = ext4_fseek(file, f->f_pos, SEEK_SET);
    if (r != EOK) {
        return -1;
    }
    return byteread;
}

/**
 * @brief ext4文件系统写数据
 * 
 * @param f 
 * @param user_addr 是否是用户地址
 * @param addr 
 * @param n 
 * @return int 状态码
 */
int 
vfs_ext_write(struct file *f, int user_addr, const uint64 addr, int n) {
    uint64 bytewrite = 0;
    struct ext4_file *file = (struct ext4_file *)f -> f_data.f_vnode.data;
    if (file == NULL) {
        panic("vfs_ext_write: cannot get ext4 file\n");
    }
    int r = 0;
    if (user_addr) {
        char *buf = kalloc();
        uint64 mwrite = 0;
        if (buf == NULL) {
            panic("vfs_ext_read: kalloc failed\n");
        }
        uint64 uaddr = addr, expect = 0;
        for (uint64 tot = 0; tot < n; tot += mwrite, uaddr += mwrite) {
            expect = min(n - tot, PGSIZE);
            mwrite = 0;
            if (either_copyin((void *)buf, user_addr, (uint64)uaddr, expect) == -1) {
                kfree(buf);
                // printf("vfs_ext_read: copyout failed\n");
                return 0;
            }
            r = ext4_fwrite(file, buf, expect, &mwrite);
            if (r != EOK) {
                kfree(buf);
                return 0;
            }
            if (mwrite == 0) {
                break;
            }
            bytewrite += mwrite;
        }
        kfree(buf);
    } else {
        char *kbuf = (char *) addr;
        r = ext4_fwrite(file, kbuf, n, &bytewrite);
        if (r != EOK) {
            return 0;
        }
    }
    f -> f_pos = file->fpos;
    return bytewrite;
}

/**
 * @brief ext4的cache写回磁盘(就是所有脏了的buf都写回磁盘)
 * 
 * @param fs 文件系统
 * @return int 状态码
 */
int 
vfs_ext_flush(struct filesystem *fs) {
    const char *path = fs->path;
    int err = ext4_cache_flush(path);
    if (err != EOK) {
        return -err;
    }
    return EOK;
}

/**
 * @brief 调整文件读写位置（扩展文件系统实现）
 * 
 * @param f        文件对象指针，需通过f_data.f_vnode.data字段关联ext4_file结构体
 * @param offset   偏移量（字节单位），当whence为SEEK_END时允许负值表示反向偏移
 * @param whence   起始位置标志：
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
int 
vfs_ext_lseek(struct file *f, int offset, int whence) {
    int r = 0;
    struct ext4_file *file = (struct ext4_file *)f -> f_data.f_vnode.data;
    if (file == NULL) {
        panic("vfs_ext_lseek: cannot get ext4 file\n");
    }
    if (whence == SEEK_END && offset < 0) {
        offset = -offset;
    }
    r = ext4_fseek(file, offset, whence);
    if (r != EOK) {
        return -r;
    }
    f->f_pos = file->fpos;
    return f->f_pos;
}

/**
 * @brief 关闭ext4目录文件
 * 
 * @param f 
 * @return int 
 */
int 
vfs_ext_dirclose(struct file *f) {
    struct ext4_dir *dir = (struct ext4_dir *)f -> f_data.f_vnode.data;
    if (dir == NULL) {
        panic("vfs_ext_dirclose: cannot get ext4 file\n");
    }
    int r = ext4_dir_close(dir);
    if (r != EOK) {
        // printf("vfs_ext_dirclose: cannot close directory\n");
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
int 
vfs_ext_fclose(struct file *f) {
    struct ext4_file *file = (struct ext4_file *)f -> f_data.f_vnode.data;
    // if (strncmp(f->f_path, "/tmp", 4) == 0) {
    //     vfs_free_file(file);
    //     f->f_data.f_vnode.data = NULL;
    //     return ext4_fremove(f->f_path);
    // }
    if (file == NULL) {
        panic("vfs_ext_close: cannot get ext4 file\n");
    }
    int r = ext4_fclose(file);
    if (r != EOK) {
        return -1;
    }
    vfs_free_file(file);
    f->f_data.f_vnode.data = NULL;
    return 0;
}

/**
 * @brief 通过file打开文件
 *
 * 通过file打开文件
 * 需要在file中存储path 和 flag
 * 会分配存储文件的内存
 * 
 * @param f 文件对象指针
 * @return int 成功返回0，失败返回负值
 */
int 
vfs_ext_openat(struct file *f) 
{
    file_vnode_t *vnode = NULL;

    union {
        ext4_dir dir;
        ext4_file file;
    } var;
    // printf("11\n");

    int r = ext4_dir_open(&(var.dir), f->f_path);

    if (r == EOK) {
        vnode = vfs_alloc_dir();
        if (vnode == NULL) {
            return -ENOMEM;
        }
        *(ext4_dir*) vnode->data = var.dir;
        f->f_data.f_vnode = *vnode;
    } else {
        vnode = vfs_alloc_file();
        if (vnode == NULL) {
            return -ENOMEM;
        }
        r = ext4_fopen2(vnode->data, f->f_path, f->f_flags);
        if (r != EOK) {
            vfs_free_file(vnode->data);
            return -ENOMEM;
        }
        f->f_data.f_vnode = *vnode;
        f->f_pos = ((ext4_file*) vnode->data)->fpos;
    }
    f->f_count = 1;
    struct ext4_inode inode;
    uint32 ino;
    if (ext4_raw_inode_fill(f->f_path, &ino, &inode) == EOK) {
        struct ext4_sblock *sb = NULL;
        ext4_get_sblock(f->f_path, &sb);
        if (ext4_inode_type(sb, &inode) == EXT4_INODE_MODE_CHARDEV) {
            f->f_type = FD_DEVICE;
            f->f_major = ext4_inode_get_dev(&inode);
        } else {
            f->f_type = FD_REG;
        }
    }
    return EOK;
}


/** 
 * @brief ext4硬链接
 */
int vfs_ext_link(const char *oldpath, const char *newpath) {
    int r = ext4_flink(oldpath, newpath);
    if (r != EOK) {
        return -r;
    }
    return EOK;
}

/**
 * @brief ext4移除path路径的文件或目录
 * 
 * @param path 
 * @return int 
 */
int 
vfs_ext_rm(const char *path) {
    int r = 0;
    union {
        ext4_dir dir;
        ext4_file file;
    } var;
    r = ext4_dir_open(&(var.dir), path);
    if (r == 0) {
        (void) ext4_dir_close(&(var.dir));
        ext4_dir_rm(path);
    } else {
        r = ext4_fremove(path);
    }
    return -r;
}

/**
 * @brief 获取ext4文件的metadata
 * 
 * @param path 文件路径
 * @param st 状态信息结构体指针
 * @return int 状态码，0表示成功，-1表示失败
 */
int 
vfs_ext_stat(const char *path, struct kstat *st) {
    struct ext4_inode inode;
    uint32 ino = 0;
    // uint32 dev = 0;

    // union {
    //     ext4_dir dir;
    //     ext4_file file;
    // } var;

    const char* statpath = path;

    int r = ext4_raw_inode_fill(statpath, &ino, &inode);
    if (r != EOK) {
        return -r;
    }

    struct ext4_sblock *sb = NULL;
    r = ext4_get_sblock(statpath, &sb);
    if (r != EOK) {
        return -r;
    }

    st->st_dev = ext4_inode_get_dev(&inode);
    st->st_ino = ino;
    st->st_mode = ext4_inode_get_mode(sb, &inode);
    st->st_nlink = ext4_inode_get_links_cnt(&inode);
    st->st_uid = ext4_inode_get_uid(&inode);
    st->st_gid = ext4_inode_get_gid(&inode);
    st->st_rdev = 0;
    st->st_size = (uint64) inode.size_lo;
    st->st_atime_sec = 0;
    st->st_atime_nsec = 0;
    st->st_mtime_sec = 0;
    st->st_mtime_nsec = 0;
    st->st_ctime_sec = 0;
    st->st_ctime_nsec = 0;

    if (r == 0) {
        struct ext4_mount_stats s;
        r = ext4_mount_point_stats(statpath, &s);
        if (r == 0) {
            st->st_blksize = s.block_size;
            st->st_blocks = (st->st_size + s.block_size) / s.block_size;
        }
    }
    return -r;
}

/**
 * @brief 获取ext4文件的metadata
 * 
 * @param f 文件对象指针
 * @param st 状态信息结构体指针
 * @return int 状态码，0表示成功，-1表示失败
 */
int 
vfs_ext_fstat(struct file *f, struct kstat *st) {
    struct ext4_file *file = (struct ext4_file *)f -> f_data.f_vnode.data;
    struct ext4_inode_ref ref;
    if (file == NULL) {
        panic("vfs_ext_fstat: cannot get ext4 file\n");
    }
    int r = ext4_fs_get_inode_ref(&file->mp->fs, file->inode, &ref);
    if (r != EOK) {
        return -r;
    }

    st->st_dev = 0;
    st->st_ino = ref.index;
    st->st_mode = 0x2000;
    st->st_nlink = 1;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_rdev = 0;
    st->st_size = ref.inode->size_lo;
    st->st_blksize = ref.inode->size_lo / ref.inode->blocks_count_lo;
    st->st_blocks = (uint64) ref.inode->blocks_count_lo;

    st->st_atime_sec = ext4_inode_get_access_time(ref.inode);
    st->st_ctime_sec = ext4_inode_get_change_inode_time(ref.inode);
    st->st_mtime_sec = ext4_inode_get_modif_time(ref.inode);
    return EOK;
}

/**
 * @brief 获取ext4文件的拓展metadata（statx）
 * 
 * @param f 
 * @param st 
 * @return int 状态码，0表示成功，-1表示失败
 */
int 
vfs_ext_statx(struct file *f, struct statx *st) {
    struct ext4_file *file = (struct ext4_file *)f -> f_data.f_vnode.data;
    struct ext4_inode_ref ref;
    if (file == NULL) {
        panic("vfs_ext_fstat: cannot get ext4 file\n");
    }
    int r = ext4_fs_get_inode_ref(&file->mp->fs, file->inode, &ref);
    if (r != EOK) {
        return -r;
    }

    st->stx_dev_major = 0;
    st->stx_ino = ref.index;
    st->stx_mode = 0x2000;
    st->stx_nlink = 1;
    st->stx_uid = 0;
    st->stx_gid = 0;
    st->stx_rdev_major = 0;
    st->stx_size = ref.inode->size_lo;
    st->stx_blksize = ref.inode->size_lo / ref.inode->blocks_count_lo;
    st->stx_blocks = (uint64) ref.inode->blocks_count_lo;

    st->stx_atime.tv_sec = ext4_inode_get_access_time(ref.inode);
    st->stx_ctime.tv_sec = ext4_inode_get_change_inode_time(ref.inode);
    st->stx_mtime.tv_sec = ext4_inode_get_modif_time(ref.inode);
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
int 
vfs_ext_getdents(struct file *f, struct linux_dirent64 *dirp, int count) {
    int index = 0;
    // int prev_reclen = -1;
    struct linux_dirent64 *d;
    const ext4_direntry *rentry;
    int totlen = 0;

    /* make integer count */
    if (count == 0) {
        return -EINVAL;
    }

    d = dirp;
    while (1) {
        rentry = ext4_dir_entry_next(f->f_data.f_vnode.data);
        if (rentry == NULL) {
            break;
        }

        int namelen = strlen((const char*)rentry->name);
        /* 
         * 长度是前四项的19加上namelen(字符串长度包括结尾的\0)
         * reclen是namelen+2,如果是+1会错误。原因是没考虑name[]开头的'\' 
         */
        int reclen = sizeof d->d_ino + sizeof d->d_off + sizeof d->d_reclen 
                     + sizeof d->d_type + namelen + 2;
        if (reclen < sizeof(struct linux_dirent64)) {
            reclen = sizeof(struct linux_dirent64);
        }
        if (totlen + reclen >= count) {
            break;
        }
        char name[MAXPATH] = {0};
        name[0] = '/';
        strcat(name, (const char*)rentry->name); //< 追加，二者应该都以'/'开头
        strncpy(d->d_name, name, MAXPATH);
        //printf("name: %s;\ndentry->d_name: %s\n",name,rentry->name); //< 调试
        if (rentry->inode_type == EXT4_DE_DIR) {
            d->d_type = T_DIR;
        } else if (rentry->inode_type == EXT4_DE_REG_FILE) {
            d->d_type = T_FILE;
        } else if (rentry->inode_type == EXT4_DE_CHRDEV) {
            d->d_type = T_CHR;
        } else {
            d->d_type = T_UNKNOWN;
        }
        d->d_ino = rentry->inode;
        d->d_off = index + 1; // start from 1
        d->d_reclen = reclen;
        ++index;
        totlen += d->d_reclen;
        d = (struct linux_dirent64 *) ((char *) d + d->d_reclen);
    }
    // f->f_pos += totlen;

    return totlen;
}

/**
 * @brief ext4重命名文件或目录
 * 
 * @param oldpath 
 * @param newpath 
 * @return int 
 */
int 
vfs_ext_frename(const char *oldpath, const char *newpath) {
    int r = ext4_frename(oldpath, newpath);
    if (r != EOK) {
        return -r;
    }
    return -r;
}

/**
 * @brief ext4创建目录
 * 
 * @param path 
 * @param mode New mode bits (for example 0777).
 * @return int 
 */
int 
vfs_ext_mkdir(const char *path, uint64_t mode) {
    /* Create the directory. */
    int r = ext4_dir_mk(path);
    if (r != EOK) {
        return -r;
    }

    /* Set mode. */
    r = ext4_mode_set(path, mode);

    return -r;
}

/** 
 * @brief 判断这个路径是否是目录
 */
int 
vfs_ext_is_dir(const char *path) 
{
    struct ext4_dir *dir = vfs_alloc_dir()->data;
    int r = ext4_dir_open(dir, path);
    if (r != EOK) {
        vfs_free_dir(dir);
        return -r;
    }
    r = ext4_dir_close(dir);
    vfs_free_dir(dir);
    if (r != EOK) {
        return -r;
    }
    return EOK;
}

/**
 * @brief 把linux_dirent64文件类型转换为对应ext4文件类型
 * 
 * @param filetype 
 * @return uint32 
 */
static uint32 
vfs_ext4_filetype_from_vfs_filetype(uint32 filetype) {
    switch (filetype) {
        case T_DIR:
            return EXT4_DE_DIR;
        case T_FILE:
            return EXT4_DE_REG_FILE;
        case T_CHR:
            return EXT4_DE_CHRDEV;
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
int 
vfs_ext_mknod(const char *path, uint32 mode, uint32 dev) {
    int r = ext4_mknod(path, vfs_ext4_filetype_from_vfs_filetype(mode), dev);
    return -r;
}

/**
 * @brief 得到ext4文件的大小
 * 
 * @param path 
 * @param size 文件大小
 * @return int 状态码，0表示成功，-1表示失败
 */
int 
vfs_ext_get_filesize(const char *path, uint64_t *size) {
    struct ext4_inode inode;
    struct ext4_sblock *sb = NULL;
    uint32_t ino;
    int r = ext4_get_sblock(path, &sb);
    if (r != EOK) {
        return -r;
    }
    r = ext4_raw_inode_fill(path, &ino, &inode);
    if (r != EOK) {
        return -r;
    }
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
int 
vfs_ext_utimens(const char *path, const struct timespec *ts) {
    int resp = EOK;
    if (!ts) {
        resp = ext4_atime_set(path, NS_to_S(TIME2NS(r_time())));
        if (resp != EOK)
            return -resp;
        resp = ext4_mtime_set(path, NS_to_S(TIME2NS(r_time())));
        if (resp != EOK)
            return -resp;
        return EOK;
    }

    if (ts[0].tv_nsec == UTIME_NOW) {
        resp = ext4_atime_set(path, NS_to_S(TIME2NS(r_time())));
    } else if (ts[0].tv_nsec != UTIME_OMIT) {
        resp = ext4_atime_set(path, NS_to_S(TIMESEPC2NS(ts[0])));
    }
    if (resp != EOK)
        return -resp;

    if (ts[1].tv_nsec == UTIME_NOW) {
        resp = ext4_mtime_set(path, NS_to_S(TIME2NS(r_time())));
    } else if (ts[1].tv_nsec != UTIME_OMIT) {
        resp = ext4_mtime_set(path, NS_to_S(TIMESEPC2NS(ts[1])));
    }
    if (resp != EOK)
        return -resp;
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
int 
vfs_ext_futimens(struct file *f, const struct timespec *ts) {
    int resp = EOK;
    struct ext4_file *file = (struct ext4_file *) f->f_data.f_vnode.data;

    if (file == NULL) {
        panic("can't get file");
    }

    if (!ts) {
        resp = ext4_atime_set(f->f_path, NS_to_S(TIME2NS(r_time())));
        if (resp != EOK)
            return -resp;
        resp = ext4_mtime_set(f->f_path, NS_to_S(TIME2NS(r_time())));
        if (resp != EOK)
            return -resp;
        return EOK;
    }

    if (ts[0].tv_nsec == UTIME_NOW) {
        resp = ext4_atime_set(f->f_path, NS_to_S(TIME2NS(r_time())));
    } else if (ts[0].tv_nsec != UTIME_OMIT) {
        resp = ext4_atime_set(f->f_path, NS_to_S(TIMESEPC2NS(ts[0])));
    }
    if (resp != EOK)
        return -resp;

    if (ts[1].tv_nsec == UTIME_NOW) {
        resp = ext4_mtime_set(f->f_path, NS_to_S(TIME2NS(r_time())));
    } else if (ts[1].tv_nsec != UTIME_OMIT) {
        resp = ext4_mtime_set(f->f_path, NS_to_S(TIMESEPC2NS(ts[1])));
    }
    if (resp != EOK)
        return -resp;
    return EOK;
}

