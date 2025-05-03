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

//fs operations
int vfs_ext_mount(struct filesystem *fs, uint64_t rwflag, void *data);
int vfs_ext_mount2(struct filesystem *fs, uint64_t rwflag, void *data);
int vfs_ext_umount(struct filesystem *fs);
int vfs_ext_fstat(struct file *f, struct kstat *st);
int vfs_ext_flush(struct filesystem *fs);

extern struct filesystem_op ext4_fs_op;

//file operations
int vfs_ext_openat(struct file *f);
int vfs_ext_fclose(struct file *f);
int vfs_ext_read(struct file *f, int is_user_addr, const uint64 addr, int n);
int vfs_ext_readat(struct file *f, int is_user_addr, const uint64 addr, int n, int offset);
int vfs_ext_write(struct file *f, int is_user_addr, const uint64 addr, int n);
int vfs_ext_fflush(struct file *f);
int vfs_ext_link(const char *oldpath, const char *newpath);
int vfs_ext_rm(const char *path);
int vfs_ext_stat(const char *path, struct kstat *st);
int vfs_ext_fstat(struct file *f, struct kstat *st);
int vfs_ext_statx(struct file *f, struct statx *st);
int vfs_ext_mkdir(const char *path, uint64_t mode);
int vfs_ext_is_dir(const char *path);
int vfs_ext_dirclose(struct file *f);
int vfs_ext_getdents(struct file *f, struct linux_dirent64 *dirp, int count);


//inode operations
struct inode *vfs_ext_namei(const char *name);
ssize_t vfs_ext_readi(struct inode *self, int user_dst, uint64 addr, uint off, uint n);
void vfs_ext_locki(struct inode *self);
void vfs_ext_unlock_puti(struct inode *self);
extern struct inode_operations ext4_inode_op;
struct inode_operations *get_ext4_inode_op(void);
int vfs_ext_mknod(const char *path, uint32 mode, uint32 dev);

/*
 *时间单位转换
 */
#define NS_to_S(ns) (ns / (1000000000))
#define S_to_NS(s) (s * 1UL * 1000000000)

#endif /* __VFS_EXT4_EXT_H__ */