
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
#include "vfs_ext4_blockdev_ext.h"
#include "vfs_ext4_ext.h"
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

#define min(a, b) ((a) < (b) ? (a) : (b))

//TODO：测试成功后替换为信号量
struct spinlock extlock;
// static void ext4_lock(void);
// static void ext4_unlock(void);

// static struct ext4_lock ext4_lock_ops = {ext4_lock, ext4_unlock};

// static uint vfs_ext4_filetype(uint filetype);

int vfs_ext4_init(void) {
    initlock(&extlock, "ext4 fs");
    ext4_device_unregister_all();
    ext4_init_mountpoints();
    return 0;
}

// static void ext4_lock() {
//     //acquire(&extlock);
// }

// static void ext4_unlock() {
//     //release(&extlock);
// }

int vfs_ext_mount(struct filesystem *fs, uint64_t rwflag, void *data) {
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

//For rootfs
// int vfs_ext_mount2(struct filesystem *fs, uint64_t rwflag, void *data) {
//     int r = 0;
//     // struct ext4_blockdev *bdev = NULL;
//     struct vfs_ext4_blockdev *vbdev = vfs_ext4_blockdev_create2(fs->dev);

//     if (vbdev == NULL) {
//         r = -ENOMEM;
//         goto out;
//     }

//     printf("MOUNT BEGIN %s\n", fs->path);
//     // bdev = &vbdev->bd;
//     r = ext4_mount("root_fs", fs->path, false);
//     printf("EXT4 mount result: %d\n", r);

//     if (r != EOK) {
//         vfs_ext4_blockdev_destroy(vbdev);
//         goto out;
//     } else {
//         // ext4_mount_setup_locks(fs->path, &ext4_lock_ops);
//         //获得ext4文件系统的超级块
//         // ext4_get_sblock(fs->path, (struct ext4_sblock **)(&(fs->fs_data)));
//     }
//     out:
//         return r;
// }

int vfs_ext_statfs(struct filesystem *fs, struct statfs *buf) {
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


struct filesystem_op ext4_fs_op = {
    .mount = vfs_ext_mount,
    .umount = vfs_ext_umount,
    .statfs = vfs_ext_statfs,
};


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

int vfs_ext_ioctl(struct file *f, int cmd, void *args) {
    int r = 0;
    struct ext4_file *file = (struct ext4_file *)f -> f_extfile;
    if (file == NULL) {
        panic("vfs_ext_ioctl: cannot get ext4 file\n");
    }

    switch (cmd) {
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
int vfs_ext_read(struct file *f, int user_addr, const uint64 addr, int n) {
    uint64 byteread = 0;
    struct ext4_file *file = (struct ext4_file *)f -> f_extfile;
    if (file == NULL) {
        panic("vfs_ext_read: cannot get ext4 file\n");
    }
    int r = 0;
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
    f -> f_pos = file->fpos;

    return byteread;
}

int vfs_ext_readat(struct file *f, int user_addr, const uint64 addr, int n, int offset) {
    uint64 byteread = 0;
    struct ext4_file *file = (struct ext4_file *)f -> f_extfile;
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

int vfs_ext_write(struct file *f, int user_addr, const uint64 addr, int n) {
    uint64 bytewrite = 0;
    struct ext4_file *file = (struct ext4_file *)f -> f_extfile;
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

//清除缓存
int vfs_ext_flush(struct filesystem *fs) {
    const char *path = fs->path;
    int err = ext4_cache_flush(path);
    if (err != EOK) {
        return -err;
    }
    return EOK;
}
//更改文件偏移位置
int vfs_ext_lseek(struct file *f, int offset, int whence) {
    int r = 0;
    struct ext4_file *file = (struct ext4_file *)f -> f_extfile;
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

int vfs_ext_dirclose(struct file *f) {
    struct ext4_dir *dir = (struct ext4_dir *)f -> f_extfile;
    if (dir == NULL) {
        panic("vfs_ext_dirclose: cannot get ext4 file\n");
    }
    int r = ext4_dir_close(dir);
    if (r != EOK) {
        // printf("vfs_ext_dirclose: cannot close directory\n");
        return -1;
    }
    free_ext4_dir(dir);
    f->f_extfile = NULL;
    return 0;
}

int vfs_ext_fclose(struct file *f) {
    struct ext4_file *file = (struct ext4_file *)f -> f_extfile;
    // if (strncmp(f->f_path, "/tmp", 4) == 0) {
    //     free_ext4_file(file);
    //     f->f_extfile = NULL;
    //     return ext4_fremove(f->f_path);
    // }
    if (file == NULL) {
        panic("vfs_ext_close: cannot get ext4 file\n");
    }
    int r = ext4_fclose(file);
    if (r != EOK) {
        return -1;
    }
    free_ext4_file(file);
    f->f_extfile = NULL;
    return 0;
}

/*通过file打开文件
 *需要在file中存储path 和 flag
 *会分配存储文件的内存
 */
int vfs_ext_openat(struct file *f) {

    struct ext4_dir *dir = NULL;
    struct ext4_file *file = NULL;

    union {
        ext4_dir dir;
        ext4_file file;
    } var;
    // printf("11\n");

    int r = ext4_dir_open(&(var.dir), f->f_path);

    if (r == EOK) {
        dir = alloc_ext4_dir();
        if (dir == NULL) {
            return -ENOMEM;
        }
        *dir = var.dir;
        f->f_extfile = dir;
    } else {
        file = alloc_ext4_file();
        if (file == NULL) {
            return -ENOMEM;
        }
        r = ext4_fopen2(file, f->f_path, f->f_flags);
        if (r != EOK) {
            free_ext4_file(file);
            return -ENOMEM;
        }
        f->f_extfile = file;
        f->f_pos = file->fpos;
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


/*
 *硬链接
 */
int vfs_ext_link(const char *oldpath, const char *newpath) {
    int r = ext4_flink(oldpath, newpath);
    if (r != EOK) {
        return -r;
    }
    return EOK;
}

int vfs_ext_rm(const char *path) {
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

int vfs_ext_stat(const char *path, struct kstat *st) {
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

int vfs_ext_fstat(struct file *f, struct kstat *st) {
    struct ext4_file *file = (struct ext4_file *)f -> f_extfile;
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

int vfs_ext_statx(struct file *f, struct statx *st) {
    struct ext4_file *file = (struct ext4_file *)f -> f_extfile;
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


/*
 *遍历目录
 */
int vfs_ext_getdents(struct file *f, struct linux_dirent64 *dirp, int count) {
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
        rentry = ext4_dir_entry_next(f->f_extfile);
        if (rentry == NULL) {
            break;
        }

        int namelen = strlen((const char*)rentry->name);
        int reclen = sizeof d->d_ino + sizeof d->d_off + sizeof d->d_reclen + sizeof d->d_type + namelen + 1;
        if (reclen < sizeof(struct linux_dirent64)) {
            reclen = sizeof(struct linux_dirent64);
        }
        if (totlen + reclen >= count) {
            break;
        }
        char name[MAXPATH] = {0};
        name[0] = '/';
        strcat(name, (const char*)rentry->name);
        strncpy(d->d_name, name, MAXPATH);
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

int vfs_ext_frename(const char *oldpath, const char *newpath) {
    int r = ext4_frename(oldpath, newpath);
    if (r != EOK) {
        return -r;
    }
    return -r;
}

int vfs_ext_mkdir(const char *path, uint64_t mode) {
    /* Create the directory. */
    int r = ext4_dir_mk(path);
    if (r != EOK) {
        return -r;
    }

    /* Set mode. */
    r = ext4_mode_set(path, mode);

    return -r;
}
/*
 *判断这个路径是否是目录
 */
int vfs_ext_is_dir(const char *path) {
    // struct proc *p = myproc();
    struct ext4_dir *dir = alloc_ext4_dir();
    int r = ext4_dir_open(dir, path);
    if (r != EOK) {
        free_ext4_dir(dir);
        return -r;
    }
    r = ext4_dir_close(dir);
    free_ext4_dir(dir);
    if (r != EOK) {
        return -r;
    }
    return EOK;
}

static uint32 vfs_ext4_filetype_from_vfs_filetype(uint32 filetype) {
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

int vfs_ext_mknod(const char *path, uint32 mode, uint32 dev) {
    int r = ext4_mknod(path, vfs_ext4_filetype_from_vfs_filetype(mode), dev);
    return -r;
}


int vfs_ext_get_filesize(const char *path, uint64_t *size) {
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

int vfs_ext_utimens(const char *path, const struct timespec *ts) {
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

int vfs_ext_futimens(struct file *f, const struct timespec *ts) {
    int resp = EOK;
    struct ext4_file *file = (struct ext4_file *) f->f_extfile;

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

//通过路径构造inode
struct inode *vfs_ext_namei(const char *name) {
    struct inode *inode = NULL;
    struct ext4_inode *ext4_i = NULL;
    uint32_t ino;

    inode = get_inode();
    if (inode == NULL) {
        return NULL;
    }

    ext4_i = (struct ext4_inode *)(&(inode->i_info));
    int r = ext4_raw_inode_fill(name, &ino, ext4_i);
    if (r != EOK) {
        // printf("ext4_raw_inode_fill failed\n");
        free_inode(inode);
        return NULL;
    }

    strncpy(inode->i_info.fname, name, EXT4_PATH_LONG_MAX - 1);
    inode->i_ino = ino;
    inode->i_op = &ext4_inode_op;

    /* Other fields are not needed. */

    return inode;
}

//通过inode读取
ssize_t vfs_ext_readi(struct inode *self, int user_addr, uint64 addr, uint off, uint n) {
    // struct ext4_inode *ext4_i =(struct ext4_inode *)(&(self->i_info));
    struct ext4_file file;
    int r;
    // size_t bytesread = 0;
    // char *kbuf = NULL;

    uint64 byteread = 0;
    r = ext4_fopen2(&file, self->i_info.fname, O_RDONLY);
    if (r != EOK) {
        return -r;
    }

    uint64_t oldoff = file.fpos;
    r = ext4_fseek(&file, off, SEEK_SET);
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
            r = ext4_fread(&file, buf, expect, &mread);
            if (r != EOK) {
                kfree(buf);
                return 0;
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
        r = ext4_fread(&file, kbuf, n, &byteread);
        if (r != EOK) {
            return 0;
        }
        memmove((char *) addr, kbuf, byteread);
    }
    r = ext4_fseek(&file, oldoff, SEEK_SET);
    if (r != EOK) {
        return -1;
    }
    return byteread;
}

void vfs_ext_locki(struct inode *self) {
    // ext4_lock();
}

/**
 * Free the inode.
 */
void vfs_ext_unlock_puti(struct inode *self) {
    // ext4_unlock();
    free_inode(self);
}

struct inode_operations ext4_inode_op = {
    .unlockput = vfs_ext_unlock_puti,
    .lock = vfs_ext_locki,
    .read = vfs_ext_readi,
    .unlock = vfs_ext_unlock_puti,
};

struct inode_operations *get_ext4_inode_op(void) { return &ext4_inode_op; }




