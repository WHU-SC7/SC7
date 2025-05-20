#ifndef __VFS_EXT4_EXT_H__
#define __VFS_EXT4_EXT_H__
#include "timer.h"
#include "file.h"
#include "fs.h"
#include "inode.h"

/**
 * @brief linux目录项结构体，用于遍历目录时使用
 * 
 */
struct linux_dirent64 {
    uint64 d_ino;               // 文件的 inode 号（标识文件的唯一标识符）
    int64 d_off;                // 在当前目录下的index
    unsigned short d_reclen;    // 当前目录项的长度（包括结构体及文件名字符串），单位是字节
    unsigned char d_type;       // 文件类型（常用来判断该目录项是普通文件、目录、符号链接等）
    char d_name[0];             // 文件名数组，实际大小由 d_reclen 决定，字符串以'\'开头，以'\0' 结尾，长度可变
};

int vfs_ext4_init(void);

/* 开放的VFS层的EXT4系统的接口 */
int vfs_ext4_flush(struct filesystem *fs);
int vfs_ext4_ioctl(struct file *f, int cmd, void *args);
extern struct filesystem_op EXT4_FS_OP;

/* 开放的VFS层的EXT4文件操作接口 */
int vfs_ext4_read(struct file *f, int is_user_addr, const uint64 addr, int n);
int vfs_ext4_readat(struct file *f, int is_user_addr, const uint64 addr, int n, int offset);
int vfs_ext4_write(struct file *f, int is_user_addr, const uint64 addr, int n);
int vfs_ext4_lseek(struct file *f, int offset, int startflag); 
int vfs_ext4_dirclose(struct file *f);
int vfs_ext4_fclose(struct file *f);
int vfs_ext4_openat(struct file *f);
int vfs_ext4_link(const char *oldpath, const char *newpath);
int vfs_ext_readlink(const char *path, uint64 ubuf, size_t bufsize);
int vfs_ext4_rm(const char *path);
int vfs_ext4_stat(const char *path, struct kstat *st);
int vfs_ext4_fstat(struct file *f, struct kstat *st);
int vfs_ext4_statx(struct file *f, struct statx *st);
int vfs_ext4_mkdir(const char *path, uint64_t mode);
int vfs_ext4_is_dir(const char *path);
int vfs_ext4_getdents(struct file *f, struct linux_dirent64 *dirp, int count);
int vfs_ext4_frename(const char *oldpath, const char *newpath);
int vfs_ext4_get_filesize(const char *path, uint64_t *size); 
int vfs_ext4_mknod(const char *path, uint32 mode, uint32 dev);
int vfs_ext4_utimens(const char *path, const struct timespec *ts);
int vfs_ext4_futimens(struct file *f, const struct timespec *ts);

/*
 * 时间单位转换
 */
#define NS_to_S(ns) (ns / (1000000000))
#define S_to_NS(s) (s * 1UL * 1000000000)

#endif /* __VFS_EXT4_EXT_H__ */