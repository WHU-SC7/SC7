#ifndef __VFS_EXT4_BLOCKDEV_EXT_H__
#define __VFS_EXT4_BLOCKDEV_EXT_H__

#include "ext4_blockdev.h"

#define DEV_NAME "virtio_disk"

/**
 * @brief VFS的块设备结构体
 *
 */
struct vfs_ext4_blockdev
{
    int dev;                 ///< 当前块设备的设备编号或标识符
    struct ext4_blockdev bd; ///< ext4 文件系统块设备的结构体
    uint8 ph_bbuf[4096];     ///< 当前块设备的物理块缓冲区
    char dev_name[64];       ///< 设备名，用于 ext4_device_register/unregister
};

struct vfs_ext4_blockdev *vfs_ext4_blockdev_create(int dev);
int vfs_ext4_blockdev_destroy(struct vfs_ext4_blockdev *bdev);

#endif /* __VFS_EXT4_BLOCKDEV_EXT_H__ */