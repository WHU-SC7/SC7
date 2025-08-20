#ifndef __FS_H__
#define __FS_H__
#include "inode.h"
#include "fs_defs.h"
#include "spinlock.h"

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 50    // 目录项保存文件名字符串的最大长度
#define ROOTINO 1    // root i-number
#define BSIZE 4096   // 块大小
#define CONSOLE 1    // 终端设备号
#define DEVNULL 2    // NULL设备号
#define DEVZERO 3    // ZERO设备号
#define DEVFIFO 4    // FIFO设备号
#define DEVLOOP 5    // Loop设备号
#define DEVRANDOM 6  // /dev/random设备号
#define DEVURANDOM 7 // /dev/urandom设备号

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
typedef struct filesystem
{
    int dev;                     // 设备号
    fs_t type;                   // 文件系统类型
    struct filesystem_op *fs_op; // 文件系统操作函数指针
    const char *path;            // 挂载点路径
    uint64_t rwflag;             // 读写权限
    void *fs_data;               // 文件系统私有数据
} filesystem_t;

/**
 * @brief 文件系统操作
 *
 * mount,umount,statfs
 *
 */
typedef struct filesystem_op
{
    int (*mount)(struct filesystem *fs, unsigned long rwflag, const void *data);
    int (*umount)(struct filesystem *fs);
    int (*statfs)(struct filesystem *fs, struct statfs *buf);
} filesystem_op_t;

void filesystem_init(void);
void init_fs(void);
void fs_register(int dev, fs_t fs_type, const char *path);
void dir_init(void);

filesystem_t *get_fs_from_path(const char *path);
/**
 * TODO:
 * 支持挂载
 */
int fs_mount(int dev, fs_t fs_type, const char *path, uint64 rwflag, const void *data);
int fs_umount(filesystem_t *fs);
filesystem_t *get_fs_by_type(fs_t type);
filesystem_t *get_fs_by_mount_point(const char *path);
#endif
