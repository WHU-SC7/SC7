#include "types.h"
// 硬件寄存器定义
#define VIRTIO_MMIO_MAGIC_VALUE		0x000 // 0x74726976
#define VIRTIO_MMIO_VERSION		0x004 // version; 1 is legacy
#define VIRTIO_MMIO_DEVICE_ID		0x008 // device type; 1 is net, 2 is disk
#define VIRTIO_MMIO_VENDOR_ID		0x00c // 0x554d4551
#define VIRTIO_MMIO_DEVICE_FEATURES	0x010
#define VIRTIO_MMIO_DRIVER_FEATURES	0x020
#define VIRTIO_MMIO_GUEST_PAGE_SIZE	0x028 // page size for PFN, write-only
#define VIRTIO_MMIO_QUEUE_SEL		0x030 // select queue, write-only
#define VIRTIO_MMIO_QUEUE_NUM_MAX	0x034 // max size of current queue, read-only
#define VIRTIO_MMIO_QUEUE_NUM		0x038 // size of current queue, write-only
#define VIRTIO_MMIO_QUEUE_ALIGN		0x03c // used ring alignment, write-only
#define VIRTIO_MMIO_QUEUE_PFN		0x040 // physical page number for queue, read/write
#define VIRTIO_MMIO_QUEUE_READY		0x044 // ready bit
#define VIRTIO_MMIO_QUEUE_NOTIFY	0x050 // write-only
#define VIRTIO_MMIO_INTERRUPT_STATUS	0x060 // read-only
#define VIRTIO_MMIO_INTERRUPT_ACK	0x064 // write-only
#define VIRTIO_MMIO_STATUS		0x070 // read/write

// status register bits, from qemu virtio_config.h
#define VIRTIO_CONFIG_S_ACKNOWLEDGE	1
#define VIRTIO_CONFIG_S_DRIVER		2
#define VIRTIO_CONFIG_S_DRIVER_OK	4
#define VIRTIO_CONFIG_S_FEATURES_OK	8

// device feature bits
#define VIRTIO_BLK_F_RO              5	/* Disk is read-only */
#define VIRTIO_BLK_F_SCSI            7	/* Supports scsi command passthru */
#define VIRTIO_BLK_F_CONFIG_WCE     11	/* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ             12	/* support more than one vq */
#define VIRTIO_F_ANY_LAYOUT         27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX     29


#define VIRTIO_BLK_T_IN  0  // 读操作
#define VIRTIO_BLK_T_OUT 1  // 写操作
#define SECTOR_SIZE 512

#define VIRTIO0 0x10001000
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

struct virtq_desc {
    uint64 addr;
    uint32 len;
    uint16 flags;
    uint16 next;
};

#define VRING_DESC_F_NEXT  1 // chained with another descriptor
#define VRING_DESC_F_WRITE 2 // device writes (vs read)

#define NUM 8
struct virtq_avail {
    uint16 flags; // always zero
    uint16 idx;   // driver will write ring[idx] next
    uint16 ring[NUM]; // descriptor numbers of chain heads
    uint16 unused;
  };

struct virtq_used_elem {
    uint32 id;
    uint32 len;
};

struct virtq_used {
    uint16 flags; // always zero
    uint16 idx;   // device increments when it adds a ring[] entry
    struct virtq_used_elem ring[NUM];
  };

#define VIRTIO_BLK_T_IN  0 // read the disk
#define VIRTIO_BLK_T_OUT 1 // write the disk

struct virtio_blk_req {
  uint32 type; // VIRTIO_BLK_T_IN or ..._OUT
  uint32 reserved;
  uint64 sector;
};

#define PGSIZE 4096

#define BSIZE 1024
struct buf { //之后可能要移走
    int valid;   // has data been read from disk?
    int disk;    // does disk "own" buf?
    uint dev;
    uint blockno;
    //struct sleeplock lock;
    uint refcnt;
    struct buf *prev; // LRU cache list
    struct buf *next;
    uchar data[BSIZE];
  };

#define PGSHIFT 12  // bits of offset within a page

// 初始化VirtIO磁盘
void virtio_disk_init();

// 磁盘读写操作
int virtio_rw(struct buf *b, int write);

void virtio_disk_intr();