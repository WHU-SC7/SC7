#ifndef __VFS_EXT4_INODE_EXT_H__
#define __VFS_EXT4_INODE_EXT_H__
#include "ext4_fs.h"
#include "ext4_types.h"

/* 路径或文件名的最大长度限制 */
#define EXT4_PATH_LONG_MAX 512

/**
 * @brief 针对 ext4 文件系统的 inode 信息的扩展
 * 
 */
struct vfs_ext4_inode_info {
    char fname[EXT4_PATH_LONG_MAX]; // 缓存该inode文件名或路径相关的字符串
};
#endif /* __VFS_EXT4_INODE_EXT_H__ */