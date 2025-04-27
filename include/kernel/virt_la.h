#ifndef __VIRT_LA_H__
#define __VIRT_LA_H__
//status字段的值和riscv的virt一致，不知道其他部分的差别，先独立放一个文件
//应该是给virtio_dick使用
//
#include "buf.h"

// block device feature bits
#define VIRTIO_BLK_F_RO              5	/* Disk is read-only */
#define VIRTIO_BLK_F_SCSI            7	/* Supports scsi command passthru */
#define VIRTIO_BLK_F_FLUSH           9  // Cache flush command support
#define VIRTIO_BLK_F_CONFIG_WCE     11	/* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ             12	/* support more than one vq */

#define VIRTIO_STAT_ACKNOWLEDGE		1
#define VIRTIO_STAT_DRIVER		    2
#define VIRTIO_STAT_DRIVER_OK		4
#define VIRTIO_STAT_FEATURES_OK		8
#define VIRTIO_STAT_NEEDS_RESET		64
#define VIRTIO_STAT_FAILED		    128

/* VIRTIO 1.0 Device independent feature bits */
#define VIRTIO_F_ANY_LAYOUT         (27)
#define VIRTIO_F_INDIRECT_DESC	    (28)
#define VIRTIO_F_EVENT_IDX	        (29)
#define VIRTIO_F_VERSION_1	        (32)
#define VIRTIO_F_RING_PACKED	    (34)
#define VIRTIO_F_IN_ORDER	        (35)
#define VIRTIO_F_ORDER_PLATFORM	    (36)
#define VIRTIO_F_SR_IOV	            (37)
#define VIRTIO_F_NOTIFICATION_DATA  (38)
#define VIRTIO_F_NOTIF_CONFIG_DATA  (39)
#define VIRTIO_F_RING_RESET	        (40)

void la_virtio_disk_rw(struct buf *b, int write);
void la_virtio_disk_init(void);

#endif ///< __VIRT_LA_H__