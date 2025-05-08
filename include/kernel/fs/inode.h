#ifndef __INODE_H__
#define __INODE_H__
#include "list.h"
#include "inodehash.h"
/* 我们还没有信号量，我看华科也注释了 */
// #include "semaphore.h"
#include "sleeplock.h"
#include "fs_defs.h"
#include "stat.h"

#include "ext4.h"
#include "vfs_ext4_inode.h"

/**
 * @brief 超级块操作函数
 * //TODO
 */
struct super_operations
{
};

/**
 * @brief 超级块描述符
 *
 */
struct superblock
{
    uint8 s_dev;        // 块设备标识符
    uint32 s_blocksize; // 数据块大小，字节单位

    uint32 s_magic;     // 文件系统的魔数
    uint32 s_maxbytes;  // 最大文件大小
    struct inode *root; // 指根目录

    struct super_operations *s_op; // 超级块操作函数指针

    struct spinlock dirty_lock;      // 脏inode锁
    struct list_head s_dirty_inodes; // 脏inode表
};

/*  vfs_ext4_ext.c
struct inode_operations ext4_inode_op = {
    .unlockput = vfs_ext_unlock_puti,
    .lock = vfs_ext_locki,
    .read = vfs_ext_readi,
    .unlock = vfs_ext_unlock_puti,
};
*/

/**
 * @brief inode操作函数
 * unlockput, unlock, put, lock, update, read, write, isdir, dup, dirlookup, delete, dir_empty, create
 */
struct inode_operations
{
    /* 解锁并释放inode */
    void (*unlockput)(struct inode *self);
    /* 解锁inode */
    void (*unlock)(struct inode *self);
    /* 释放inode */
    void (*put)(struct inode *self);
    /* 锁定inode */
    void (*lock)(struct inode *self);
    /* 更新inode(刷新到磁盘/底层存储) */
    void (*update)(struct inode *self);

    /* 从文件读取数据 */
    ssize_t (*read)(struct inode *self, int user_dst, uint64 dst, uint off, uint n);
    /* 向文件写入数据 */
    int (*write)(struct inode *self, int user_src, uint64 src, uint off, uint n);
    /* 判断inode是否是目录 */
    int (*isdir)(struct inode *self);
    /* 复制 inode（引用计数加1）*/
    struct inode *(*dup)(struct inode *self);

    /* 目录操作 */
    /* 在目录 inode 中查找名为 name 的子项（文件或子目录） */
    struct inode *(*dirlookup)(struct inode *self, const char *name, uint *poff);
    /* 从目录 inode 中删除子 inode ip */
    int (*delete)(struct inode *self, struct inode *ip);
    /* 判断目录是否为空 */
    int (*dir_empty)(struct inode *self);
    /* 在目录 inode 中创建一个新的子 inode 返回时要持有新ip的锁 */
    struct inode *(*create)(struct inode *self, const char *name, uchar type, short major, short minor);
    /* 获取 inode 的状态信息，填充 stat 结构体 */
    void (*stat)(struct inode *self, struct stat *st);
};

extern struct inode_operations inode_ops;

// /* 路径或文件名的最大长度限制 */
// #define EXT4_PATH_LONG_MAX 512

// /**
//  * @brief 针对 ext4 文件系统的 inode 信息的扩展
//  *
//  */
// struct vfs_ext4_inode_info {
//     char fname[EXT4_PATH_LONG_MAX]; // 缓存该inode文件名或路径相关的字符串
// };

/**
 * @brief inode描述符
 *
 */
struct inode
{
    uint8 i_dev;
    uint16 i_mode; // 类型 & 访问权限
    uint16 i_type;
    uint32 i_ino;   // 编号
    uint32 i_valid; // 是否可用 0 -> 未使用

    uint16 i_count; // 引用计数
    uint16 i_nlink; // 硬链接数
    uint i_uid;     // 拥有者编号
    uint i_gid;     // 拥有者组编号
    uint64 i_rdev;  // 当文件代表设备的时候，rdev代表这个设备的实际编号
    uint64 i_size;  // 文件大小

    long i_atime; // 文件最后一次访问时间
    long i_mtime; // 文件最后一次修改时间
    long i_ctime; // inode最后一次修改时间

    uint64 i_blocks;  // 文件有多少块
    uint64 i_blksize; // 块大小 bytes
    // struct semaphore i_sem; //同步信号量
    struct spinlock lock;          // 测试完成后再换成信号量
    struct inode_operations *i_op; // inode操作函数

    struct superblock *i_sb;

    struct vfs_ext4_inode_info i_info; // EXT4 inode结构
};

void inodeinit();
struct inode *get_inode();
void free_inode(struct inode *inode);
#endif /* __INODE_H__ */