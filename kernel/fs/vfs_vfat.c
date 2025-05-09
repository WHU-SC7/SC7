#include "types.h"
#include "defs.h"
#include "timer.h"
#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif
#include "file.h"
#include "fs.h"
#include "stat.h"
#include "fcntl.h"
#include "inode.h"
#include "vfs_vfat.h"

#include "ioctl.h"
#include "string.h"
#include "process.h"
#include "timer.h"
#include "fs.h"
#include "buf.h"

struct vfat_blockdev_iface fatiface;

/**
 * @brief vfat文件系统操作结构体
 * 
 */
struct filesystem_op VFAT_FS_OP = 
{
    .mount = vfs_vfat_mount,
    .umount = vfs_vfat_umount,
    .statfs = vfs_vfat_statfs,
};

/**
 * @brief VFAT文件系统挂载函数
 * @todo: not implemented yet
 * @param fs 文件系统
 * @param rwflag 标志
 * @param data 额外数据
 * @return int 成功0，失败-1
 */
int 
vfs_vfat_mount(struct filesystem *fs, uint64_t rwflag, const void *data) 
{
    struct vfat_blockdev_iface *pfatiface = &fatiface;
    pfatiface->bread = vfat_blockdev_read;
    pfatiface->bwrite = vfat_blockdev_write;
    return EOK;
}

/**
 * @brief VFAT文件系统卸载函数
 * @todo: not implemented yet
 * @param fs 文件系统
 * @return int 成功0，失败-1
 */
int
vfs_vfat_umount(struct filesystem *fs) 
{
    return EOK;
}

/**
 * @brief VFAT文件系统状态函数
 * @todo: not implemented yet
 * @param fs 文件系统
 * @param buf 状态缓冲区
 * @return int 成功0，失败-1
 */
int
vfs_vfat_statfs(struct filesystem *fs, struct statfs *buf) 
{
    return EOK;
}

/**
 * @brief 读取物理块设备上的数据
 * @todo 根据真正的设备编号读写
 * @param bdev 物理块设备
 * @param buf 
 * @param blk_id 
 * @param blk_cnt 
 * @return int 状态码，成功返回0
 */
int 
vfat_blockdev_read (int bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) 
{
    uint64 buf_ptr = (uint64)buf;
    for(int i = 0; i < blk_cnt; i++) 
    {
        struct buf *b = bread(0, blk_id + i);
        memmove((void*)buf_ptr, b->data, BSIZE);
        buf_ptr += BSIZE;
        brelse(b);
    }
    return EOK;
}

/**
 * @brief 写到物理块设备上
 * @todo 根据真正的设备编号读写
 * @param bdev 
 * @param buf 
 * @param blk_id 
 * @param blk_cnt 
 * @return int 状态码，成功返回0
 */
int 
vfat_blockdev_write(int bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt) 
{
	uint64 buf_ptr = (uint64)buf;
	for(int i = 0; i < blk_cnt; i++) 
    {
		struct buf *b = bget(0, blk_id + i);
		memmove(b->data, (void*)buf_ptr, BSIZE);
		bwrite(b);
		buf_ptr += BSIZE;
		brelse(b);
	}
	return EOK;
}