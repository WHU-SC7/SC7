#ifndef __VFS_VFAT_H__
#define __VFS_VFAT_H__

extern struct filesystem_op VFAT_FS_OP;

struct vfat_blockdev_iface 
{
    /**
     * @brief   Block read function.
     * @param   bdev block device
     * @param   buf output buffer
     * @param   blk_id block id
     * @param   blk_cnt block count*/
    int (*bread)(int bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt);

    /**
     * @brief   Block write function.
     * @param   bdev block device
     * @param   buf input buffer
     * @param   blk_id block id
     * @param   blk_cnt block count*/
    int (*bwrite)(int bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt);
};

extern struct vfat_blockdev_iface fatiface;

/* 文件系统 */
int vfs_vfat_mount(struct filesystem *fs, uint64_t rwflag, const void *data);
int vfs_vfat_umount(struct filesystem *fs);
int vfs_vfat_statfs(struct filesystem *fs, struct statfs *buf);

/* 块设备操作 */
int vfat_blockdev_read (int bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt);
int vfat_blockdev_write(int bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt);

#endif ///< __VFS_VFAT_H__