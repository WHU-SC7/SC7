#include "blockdev.h"

#include "types.h"
#include "defs.h"
#include "container_of.h"
#include "spinlock.h"
#include "fs.h"
#include "buf.h"

#include "virt.h"

#include "ext4.h"
#include "ext4_blockdev.h"
#include "ext4_errno.h"
#include "queue.h"
#include "string.h"
#include "spinlock.h"

/* 函数定义 */
static int blockdev_lock(struct ext4_blockdev *bdev);

static int blockdev_unlock(struct ext4_blockdev *bdev);

static int blockdev_open(struct ext4_blockdev *bdev);

static int blockdev_read(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt);

static int blockdev_write(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt);

static int blockdev_close(struct ext4_blockdev *bdev);

/* 物理块设备锁 */
// static struct spinlock bdev_lock; 

/* 物理块设备接口 */

struct ext4_blockdev_iface biface;

/* 对应设备的VFS */
struct vfs_ext4_blockdev vfs_ext4_bdev;

/**
 * @brief 初始化ext4块设备
 * 
 * 设置vbdev的设备号，vbdev的bd的接口函数指针，物理块大小和块数
 * 设置biface的锁函数，读写函数，打开和关闭函数，缓冲区，缓冲区大小和块数
 * 
 * @param vbdev 
 * @param dev 
 * @return int 
 */
static int 
vfs_ext4_blockdev_init(struct vfs_ext4_blockdev *vbdev, int dev) 
{
    uint8_t *ph_bbuf = NULL;
    struct ext4_blockdev *bd = NULL;
    struct ext4_blockdev_iface *iface = &biface;

    if (vbdev) 
    {
        vbdev->dev = dev;

        bd = &vbdev -> bd;
        bd -> bdif = iface;
        bd -> part_offset = 0;

        /* TODO: 这里的分区大小是 512*8*1024*1024 = 4GB，未来可能会更改 */
        bd -> part_size = (uint64) 512 * 8 *1024 * 1024;
        
        ph_bbuf = &vbdev -> ph_bbuf[0];

        iface -> lock = blockdev_lock;
        iface -> unlock = blockdev_unlock;
        iface -> open = blockdev_open;
        iface -> bread = blockdev_read;
        iface -> bwrite = blockdev_write;
        iface -> close = blockdev_close;

        iface -> ph_bsize = BSIZE;
        iface -> ph_bbuf = ph_bbuf;
        iface -> ph_bcnt = bd -> part_size / (uint64) bd -> bdif -> ph_bsize;
#if DEBUG
        printf("vfs_ext4_blockdev_init: part_size = %d, ph_bsize = %d, ph_bcnt = %d\n",
              bd -> part_size, iface->ph_bsize, iface->ph_bcnt);
#endif
    }
    return EOK;
}

/**
 * @brief 创建并初始化bvbdev和biface
 * 
 * @param dev 设备号
 * @return struct vfs_ext4_blockdev* 
 */
struct vfs_ext4_blockdev *
vfs_ext4_blockdev_create(int dev) 
{
    // printf("vfs_ext4_blockdev_create() called with dev=%d\n", dev);
    
    struct vfs_ext4_blockdev *vbdev = &vfs_ext4_bdev;
    // printf("  vbdev address: %p\n", vbdev);
    
    if (vbdev == NULL) {
        printf("  ERROR: vbdev is NULL\n");
        return NULL;
    }

    // printf("  initializing block device...\n");
    int status = vfs_ext4_blockdev_init(vbdev, dev);
    if (status != EOK) 
    {
        printf("  ERROR: vfs_ext4_blockdev_init failed with status=%d\n", status);
        return NULL;
    }
    // printf("  block device initialized successfully\n");

    // printf("  registering device '%s' with ext4...\n", DEV_NAME);
    status = ext4_device_register(&vbdev->bd, DEV_NAME);
    if (status != EOK) 
    {
        printf("  ERROR: ext4_device_register failed with status=%d\n", status);
        return NULL;
    }
    // printf("  device registered successfully\n");

    // printf("  returning vbdev=%p\n", vbdev);
    return vbdev;
}

/**
 * @brief 销毁vbdev
 * 
 * TODO:现在啥也没做
 * 
 * @param vbdev 
 * @return int 
 */
int 
vfs_ext4_blockdev_destroy(struct vfs_ext4_blockdev *vbdev) 
{
    if (vbdev == NULL)
        return -EINVAL;
    return EOK;
}

/**
 * @brief 对物理块设备上锁
 * 
 * @todo 没实现
 * 
 * @param bdev 
 * @return int 
 */
static int 
blockdev_lock(struct ext4_blockdev *bdev) 
{
    // acquire(&bdev_lock);
    return EOK;
}

/**
 * @brief 对物理块设备解锁
 * 
 * @todo 没实现
 * 
 * @param bdev 
 * @return int 
 */
static int 
blockdev_unlock(struct ext4_blockdev *bdev) 
{
    // release(&bdev_lock);
    return EOK;
}

/** 
 * @brief 打开物理块设备
 * 
 * TODO:现在啥也没做
 * 
 * @param bdev 
 * @return int 
 */
static int blockdev_open(struct ext4_blockdev *bdev) { return EOK; }

/**
 * @brief 关闭物理块设备
 * 
 * TODO:现在啥也没做
 * 
 * @param bdev 
 * @return int 
 */
static int blockdev_close(struct ext4_blockdev *bdev) { return EOK; }

/**
 * @brief 读取物理块设备上的数据
 * 
 * @param bdev 物理块设备
 * @param buf 
 * @param blk_id 
 * @param blk_cnt 
 * @return int 状态码，成功返回0
 */
static int 
blockdev_read (struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) 
{
    uint64 _buf = (uint64) buf;
    for(int i = 0; i < blk_cnt; i++) 
    {
        struct buf *b = bread(0, blk_id + i);
        memmove((void*)_buf, b->data, BSIZE);
        _buf += BSIZE;
        brelse(b);
    }
    return EOK;
}

/**
 * @brief 写到物理块设备上
 * 
 * @param bdev 
 * @param buf 
 * @param blk_id 
 * @param blk_cnt 
 * @return int 状态码，成功返回0
 */
static int 
blockdev_write(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt) 
{
	uint64 _buf = (uint64)buf;
	for(int i = 0; i < blk_cnt; i++) 
    {
		struct buf *b = bget(0, blk_id + i);
		memmove(b->data, (void*)_buf, BSIZE);
		bwrite(b);
		_buf += BSIZE;
		brelse(b);
	}
	return EOK;
}