#ifndef __VFS_EXT4_BLOCKDEV_EXT_H__
#define __VFS_EXT4_BLOCKDEV_EXT_H__

#include "ext4_blockdev.h"

struct vfs_ext4_blockdev {
  int dev;                  ///< 当前块设备的设备编号或标识符
  struct ext4_blockdev bd;  ///< ext4 文件系统块设备的结构体
  uint8 ph_bbuf[4096];      ///< 当前块设备的物理块缓冲区
};

#define DEV_NAME "virtio_disk"

struct vfs_ext4_blockdev *vfs_ext4_blockdev_create(int dev);
int vfs_ext4_blockdev_destroy(struct vfs_ext4_blockdev *bdev);
struct vfs_ext4_blockdev *vfs_ext4_blockdev_from_bd(struct ext4_blockdev *bd);

// For rootfs
// static int blockdev_write2(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt);
// static int blockdev_read2(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt);
// struct vfs_ext4_blockdev *vfs_ext4_blockdev_create2(int dev);

#endif /* __VFS_EXT4_BLOCKDEV_EXT_H__ */