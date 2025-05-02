#include "vfs_ext4_blockdev_ext.h"

#include "types.h"
#include "defs.h"
#include "list.h"
#include "spinlock.h"
#include "fs.h"
#include "buf.h"

#include "virt.h"

#include "ext4.h"
#include "ext4_blockdev.h"
#include "ext4_errno.h"
#include "queue.h"
#include "string.h"

static int blockdev_lock(struct ext4_blockdev *bdev);

static int blockdev_unlock(struct ext4_blockdev *bdev);

static int blockdev_open(struct ext4_blockdev *bdev);

static int blockdev_read(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt);

static int blockdev_write(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt);

static int blockdev_close(struct ext4_blockdev *bdev);


// static int bdevice_lock = 0;
//只有一个vbd
/* 物理块设备接口 */
struct ext4_blockdev_iface biface;
/* 对应设备的VFS */
struct vfs_ext4_blockdev bvbdev;

// struct ext4_blockdev_iface biface2;
// struct vfs_ext4_blockdev bvbdev2;

/**
 * @brief 从ext4_blockdev结构体中获取vfs_ext4_blockdev结构体
 * 
 * @param bdev 
 * @return struct vfs_ext4_blockdev* 
 */
struct vfs_ext4_blockdev *
vfs_ext4_blockdev_from_bd(struct ext4_blockdev *bdev) {
    if (bdev == NULL) 
    {
        return NULL;
    }
    return container_of(bdev, struct vfs_ext4_blockdev, bd);
}

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
vfs_ext4_blockdev_init(struct vfs_ext4_blockdev *vbdev, int dev) {
    uint8_t *ph_bbuf = NULL;
    struct ext4_blockdev *bd = NULL;
    //TODO: 支持动态内存分配
    //struct ext4_blockdev_iface *iface = NULL;
    struct ext4_blockdev_iface *iface = &biface;

    if (vbdev) 
    {
        vbdev->dev = dev;

        bd = &vbdev->bd;
        bd->bdif = iface;
        bd->part_offset = 0;
        bd->part_size = (uint64)512 * 8 *1024 * 1024; ///< 块设备分区大小,未来可能更改
        ph_bbuf = &vbdev->ph_bbuf[0];

        iface->lock = blockdev_lock;
        iface->unlock = blockdev_unlock;
        iface->open = blockdev_open;
        iface->bread = blockdev_read;
        iface->bwrite = blockdev_write;
        iface->close = blockdev_close;

        iface -> ph_bsize = BSIZE;
        iface->ph_bbuf = ph_bbuf;
        iface->ph_bcnt = bd->part_size / (uint64) bd->bdif->ph_bsize;

        printf("vfs_ext4_blockdev_init: ph_bsize=%p, ph_bcnt=%p\n", iface->ph_bsize, iface->ph_bcnt);
    }
    return EOK;
}

//For rootfs
// static int vfs_ext4_blockdev_init2(struct vfs_ext4_blockdev *vbdev, int dev) {
//     uint8_t *ph_bbuf = NULL;
//     struct ext4_blockdev *bd = NULL;
//     //TODO: 支持动态内存分配
//     //struct ext4_blockdev_iface *iface = NULL;
//     struct ext4_blockdev_iface *iface = &biface2;

//     if (vbdev) {
//         vbdev->dev = dev;

//         bd = &vbdev->bd;
//         bd->bdif = iface;
//         bd->part_offset = 0;
// #if defined(QEMU)
//         bd->part_size = 512 * 1024 * 1024;
// #endif
//         ph_bbuf = &vbdev->ph_bbuf[0];

//         iface->lock = blockdev_lock;
//         iface->unlock = blockdev_unlock;
//         iface->open = blockdev_open;
//         iface->bread = blockdev_read2;
//         iface->bwrite = blockdev_write2;
//         iface->close = blockdev_close;

// #if defined(QEMU)
//         iface -> ph_bsize = BSIZE;
// #endif
//         iface->ph_bbuf = ph_bbuf;
//         iface->ph_bcnt = bd->part_size / (uint64) bd->bdif->ph_bsize;

//         printf("vfs_ext4_blockdev_init: ph_bsize=%p, ph_bcnt=%p\n", iface->ph_bsize, iface->ph_bcnt);
//     }
//     return EOK;
// }

//TODO：支持动态分配
/**
 * @brief 创建并初始化bvbdev和biface
 * 
 * @param dev 设备号
 * @return struct vfs_ext4_blockdev* 
 */
struct vfs_ext4_blockdev *
vfs_ext4_blockdev_create(int dev) {
    struct vfs_ext4_blockdev *vbdev = &bvbdev;
    if (vbdev == NULL) {
        return NULL;
    }
    int r = vfs_ext4_blockdev_init(vbdev, dev);
    if (r != EOK) {
        printf("vfs_ext4_blockdev_init failed: %d\n", r);
        return NULL;
    }
    r = ext4_device_register(&vbdev->bd, DEV_NAME);
    if (r != EOK) {
        printf("ext4_device_register failed: %d\n", r);
        return NULL;
    }
    return vbdev;
}

//TODO：支持动态分配
// struct vfs_ext4_blockdev *vfs_ext4_blockdev_create2(int dev) {
//     struct vfs_ext4_blockdev *vbdev = &bvbdev2;
//     if (vbdev == NULL) {
//         return NULL;
//     }
//     int r = vfs_ext4_blockdev_init2(vbdev, dev);
//     if (r != EOK) {
//         printf("vfs_ext4_blockdev_init failed: %d\n", r);
//         return NULL;
//     }
//     r = ext4_device_register(&vbdev->bd, "root_fs");
//     if (r != EOK) {
//         printf("ext4_device_register failed: %d\n", r);
//         return NULL;
//     }
//     return vbdev;
// }

/**
 * @brief 销毁vbdev
 * 
 * TODO:现在啥也没做
 * 
 * @param vbdev 
 * @return int 
 */
int 
vfs_ext4_blockdev_destroy(struct vfs_ext4_blockdev *vbdev) {
    if (vbdev == NULL)
        return -EINVAL;
    //暂时什么都不干
    return EOK;
}

/**
 * @brief 对物理块设备上锁
 * 
 * TODO:现在啥也没做
 * 
 * @param bdev 
 * @return int 
 */
static int 
blockdev_lock(struct ext4_blockdev *bdev) {
    // acquire(&bdevice_lock);
    // while (__sync_lock_test_and_set(&bdevice_lock, 1) != 0) {
    // }

    return EOK;
}

/**
 * @brief 对物理块设备解锁
 * 
 * TODO:现在啥也没做
 * 
 * @param bdev 
 * @return int 
 */
static int 
blockdev_unlock(struct ext4_blockdev *bdev) {
    // __sync_lock_release(&bdevice_lock);
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
blockdev_read (struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    uint64 buf_ptr = (uint64)buf;
    for(int i = 0; i < blk_cnt; i++) 
    {
        // printf("[blockdev_bread] blk_id: %d, blk_cnt: %d\n", blk_id + i, blk_cnt);
        struct buf *b = bread(0, blk_id + i);
        memmove((void*)buf_ptr, b->data, BSIZE);
        buf_ptr += BSIZE;
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
blockdev_write(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt) {
	uint64 buf_ptr = (uint64)buf;
	for(int i = 0; i < blk_cnt; i++) 
    {
		// printf("[blockdev_bwrite] blk_id: %d, blk_cnt: %d\n", blk_id + i, blk_cnt);
		struct buf *b = bget(0, blk_id + i);
		memmove(b->data, (void*)buf_ptr, BSIZE);
		bwrite(b);
		buf_ptr += BSIZE;
		brelse(b);
	}
	return EOK;
}

//For rootfs
// static int blockdev_read2(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) {
//     uint64 buf_ptr = (uint64)buf;
//     for(int i = 0; i < blk_cnt; i++) {
//         // printf("[blockdev_bread] blk_id: %d, blk_cnt: %d\n", blk_id + i, blk_cnt);
//         struct buf *b = bread(1, blk_id + i);
//         memmove((void*)buf_ptr, b->data, BSIZE);
//         buf_ptr += BSIZE;
//         brelse(b);
//     }
//     return EOK;
// }

// static int blockdev_write2(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt) {
//     uint64 buf_ptr = (uint64)buf;
//     for(int i = 0; i < blk_cnt; i++) {
//         // printf("[blockdev_bwrite] blk_id: %d, blk_cnt: %d\n", blk_id + i, blk_cnt);
//         struct buf *b = bget(1, blk_id + i);
//         memmove(b->data, (void*)buf_ptr, BSIZE);
//         bwrite(b);
//         buf_ptr += BSIZE;
//         brelse(b);
//     }
//     return EOK;
// }












