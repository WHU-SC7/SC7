#ifndef __FILE_H__
#define __FILE_H__
#include "fs_defs.h"
#include "types.h"
#include "spinlock.h"
#include "socket.h"
#include "ext4.h"

struct file;

/**
 * @brief 文件操作函数结构体
 *
 * read,readat,write,writable,readable,close,fstat,statx
 *
 */
struct file_operations
{
    struct file *(*dup)(struct file *self);
    int (*read)(struct file *self, uint64 addr, int n); // addr 为用户空间地址
    int (*readat)(struct file *self, uint64 addr, int n, uint64 offset);
    int (*write)(struct file *self, uint64 addr, int n); // add 为用户空间地址
    char (*writable)(struct file *self);
    char (*readable)(struct file *self);

    int (*close)(struct file *self);
    int (*fstat)(struct file *self, uint64 addr);
    int (*statx)(struct file *self, uint64 addr);
};

/*
+-------------------+           +-------------------+
|       vnode       |           |       file        |
| (文件系统对象)      |<--------- | (进程打开文件句柄) |
|                   |    指向   |                   |
|  - 文件元信息      |           | - 偏移量 f_pos     |
|  - 文件权限        |           | - 访问模式 f_mode  |
|  - inode号         |           | - 引用计数 f_count |
+-------------------+           +-------------------+
          ^                               ^
          |                               |
   多个 file 共享同一个 vnode           单个 file 对应唯一 vnode
*/

/**
 * @brief vfs的文件vnode结构体
 *
 * 抽象文件系统中的文件对象，它类似于类Unix系统中的“vnode”
 * (虚拟节点，virtual node)概念，用于统一表示不同文件系统
 * 中的文件或目录等对象。
 */
typedef struct file_vnode
{
    char path[MAXPATH];    /* full path */
    struct filesystem *fs; /* filesystem */
    void *data;            /* file-specific data
                            * e.g. ext4 file/dir */
} file_vnode_t;

union file_data
{
    struct pipe *f_pipe;        //< FD_PIPE
    struct socket *sock;        //< FD_SOCKET
    file_vnode_t f_vnode;       //< FD_REG    
};

/**
 * @brief 文件句柄
 *
 */
struct file 
{
    enum { FD_NONE, FD_PIPE, FD_REG, FD_DEVICE,FD_SOCKET, FD_BUSYBOX } f_type;
    uint8 f_mode;         ///< 访问模式
    uint f_flags;         ///< 打开文件时的标志（如O_APPEND等）
    uint64 f_pos;         ///< 偏移量
    uint16 f_count;       ///< 引用计数，表示有多少用户或进程持有此文件结构
    short f_major;        ///< 设备号（如果是设备文件）

    // void *private_data;   ///< 文件私有数据，一般由对应子系统维护
    // int f_owner;          ///< 拥有这个文件的进程ID或进程标识
    char f_path[MAXPATH]; ///< 文件完整路径，便于调试或日志，也可能有管理作用

    uint32 removed; /* 
                     * when calling sys_unlinkat, mark as removed;
                     * when file ref is 0, REMOVE it in generic_fileclose
                     * 防止重复移除文件
                     */
    union file_data f_data; ///< 文件数据
};


typedef struct {
    void  *iov_base;    /* Starting address */
    uint32 iov_len;     /* Number of bytes to transfer */
  }iovec;
#define IOVMAX 64

/**
 * @brief  device switch table （设备切换表）
 *
 * 用来抽象和管理设备驱动操作的结构体。它包含指向设备
 * 驱动实现的函数指针，使内核可以通过统一接口调用不同设备的读写操作。
 */
struct devsw
{
    int (*read)(int, uint64, int);  ///< 第一个int表示是否是用户空间地址
    int (*write)(int, uint64, int); ///< 第一个int表示是否是用户空间地址
};

extern struct devsw devsw[];

void fileinit(void);
file_vnode_t *vfs_alloc_dir(void);
file_vnode_t *vfs_alloc_file(void);
void vfs_free_dir(void* dir);
void vfs_free_file(void *file);
struct file_operations *get_file_ops();
struct file *filealloc(void);
int fdalloc(struct file *f);
int fdalloc2(struct file *f,int begin);


#endif /* __FILE_H__ */