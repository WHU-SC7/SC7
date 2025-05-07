#ifndef __FS_H__
#define __FS_H__
#include "inode.h"
#include "fs_defs.h"
#include "spinlock.h"

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 50    // 目录项保存文件名字符串的最大长度
#define ROOTINO  1   // root i-number
#define BSIZE 4096   // 块大小
#define CONSOLE 1    // 终端设备号

/**
 * @brief 文件系统类型对应编号
 * 
 */
typedef enum 
{
    VFAT = 1,
    EXT4 = 2,
} fs_t;

/**
 * @brief 文件系统结构体
 * 
 */
struct filesystem 
{
    int dev;                        // 设备号
    fs_t type;                      // 文件系统类型
    struct filesystem_op *fs_op;    // 文件系统操作函数指针
    const char *path;               // 挂载点路径
    void *fs_data;                  // 文件系统私有数据
};

typedef struct filesystem filesystem_t;

/**
 * @brief 文件系统操作
 * 
 * mount,umount,statfs
 * 
 */
struct filesystem_op {
    int (*mount)(struct filesystem *fs, unsigned long rwflag, void *data);
    int (*umount)(struct filesystem *fs);
    int (*statfs)(struct filesystem *fs, struct statfs *buf);
};

typedef struct filesystem_op filesystem_op_t;

/**
 * @brief 文件系统表
 * 
 */
extern filesystem_t *fs_table[VFS_MAX_FS];

/** 
 * @brief 文件系统操作函数表
 * 
 */
extern filesystem_op_t *fs_ops_table[VFS_MAX_FS];

/** 
 * @brief 文件系统表锁
 * 
 * 该结构体用于保护对文件系统表的访问。
 */
extern struct spinlock fs_table_lock;

extern filesystem_t vfat_fs;

void filesystem_init(void);
// void filesystem2_init(void);
void init_fs_table(void);
void fs_init(filesystem_t *fs, int dev, fs_t fs_type, const char *path);

filesystem_t *get_fs_from_path(const char *path);
/**
 * TODO:
 * 支持挂载
 */
int fs_mount(filesystem_t *fs, uint64 rwflag, void *data);
int fs_umount(filesystem_t *fs);
filesystem_t *get_fs_by_type(fs_t type);
filesystem_t *get_fs_by_mount_point(const char *path);
#endif








