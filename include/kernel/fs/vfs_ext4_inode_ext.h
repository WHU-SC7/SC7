#ifndef __VFS_EXT4_INODE_EXT_H__
#define __VFS_EXT4_INODE_EXT_H__
#include "ext4_fs.h"
#include "ext4_types.h"

#define EXT4_PATH_LONG_MAX 512

struct vfs_ext4_inode_info {
    char fname[EXT4_PATH_LONG_MAX];
};
#endif /* __VFS_EXT4_INODE_EXT_H__ */