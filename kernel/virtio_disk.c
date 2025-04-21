//对应于virtio_disk
#include "types.h"
#include "print.h"

//< 代表第一个设备，
//< 有什么作用呢？
unsigned char bus1;
unsigned char device1;
unsigned char function1;

uint64 pci_base1;

//之后应该放到virtio_pci.h
#define PGSIZE 4096
#define NUM 8
#define BSIZE 1024 //< 相当于两个扇区，设置为1024是为了减少读写次数，一次读取更多数据
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

struct VRingDesc {
  uint64 addr;
  uint32 len;
  uint16 flags;
  uint16 next;
};

static struct disk {
    // memory for virtio descriptors &c for queue 0.
    // this is a global instead of allocated because it must
    // be multiple contiguous pages, which kalloc()
    // doesn't support, and page aligned.
    char pages[2*PGSIZE];
    struct virtq_desc *desc;
    uint16 *avail;
    struct UsedArea *used;

    // our own book-keeping.
    char free[NUM];  // is a descriptor free?
    uint16 used_idx; // we've looked this far in used[2..NUM].

    // track info about in-flight operations,
    // for use when completion interrupt arrives.
    // indexed by first descriptor index of chain.
    struct {
        struct buf *b;
        char status;
    } info[NUM];

    //struct spinlock vdisk_lock;

} __attribute__ ((aligned (PGSIZE))) disk;

extern uint64 pci_device_probe(uint16 vendor_id, uint16 device_id); //< pci.c
void virtio_probe() {
    pci_base1 = pci_device_probe(0x1af4, 0x1001);
    printf("pci_base: %p\n",pci_base1);
    //pci_base2 = pci_device_probe(0x1af4, 0x1001);
}

void avoid_warning()
{
  disk.used_idx=1;
}

// void pci_device_init(uint64 pci_base, unsigned char bus, unsigned char device, unsigned char function) {
//     pci_map(bus, device, function, disk.pages);
//     // printf("map finish\n");
//     unsigned int val = 0;
//     val = pci_config_read16(pci_base + PCI_STATUS_COMMAND);
//     // printf("%d\n", val);
//     val |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER | PCI_COMMAND_IO;
//     pci_config_write16(pci_base + PCI_STATUS_COMMAND, val);
//     val = pci_config_read16(pci_base + PCI_STATUS_COMMAND);
//     // printf("%d\n",val);
//     // uint8 irq = PCIE_IRQ;
//     // pci_config_write8(pci_base + 0x3C, irq);
//     // // irq = 0;
//     // irq = pci_config_read8(pci_base + 0x3C);
//     // printf("%d\n", irq);

//     uint64 off = (bus << 16) | (device << 11) | (function << 8);
//     volatile uint32 *base = (PCIE0_ECAM + (uint64)(off));
//     for(int i = 0; i < 6; i++) {
//         uint32 old = base[4+i];
//         // printf("bar%d origin value: 0x%08x\t", i, old);
//         if(old & 0x1) {
//             // printf("IO space\n");
//             continue;                                      // 未用到 IO 空间, 暂时也不进行分配等
//         } else {                                           // 仅为 Mem 空间进行进一步的处理
//             // printf("Mem space\t");
//         }
//         if(old & 0x4) {
//             // printf("%x %x\n", base[4 + i], base[4 + i + 1]);             // 64bit 系统映射
//             base[4+i] = 0xffffffff;
//             base[4+i+1] = 0xffffffff;
//             __sync_synchronize();

//             uint64 sz = ((uint64)base[4+i+1] << 32) | base[4+i];
//             sz = ~(sz & 0xFFFFFFFFFFFFFFF0) + 1;
//             printf("bar%d need size: 0x%x\n", i, sz);
//             uint64 mem_addr = pci_alloc_mmio(sz);
//             // 写入分配的大小
//             base[4+i] = (uint32)(mem_addr);
//             base[4+i+1] = (uint32)(mem_addr >> 32);
//             __sync_synchronize();
//             printf("%p\n", mem_addr);
//             i++;                                    // 跳过下一个 BAR
//         }
//     }
// }

// void virtio_disk_init() {
//     // pci_scan_buses();

//     pci_device_init(pci_base1, bus1, device1, function1);
//     if (pci_base1) {
//         virtio_pci_read_caps(&gs_virtio_blk_hw, pci_base1, 0);
//         // virtio_pci_print_common_cfg(&gs_virtio_blk_hw);
//     } else {
//         printf("virtion-blk-pci device not found!\n");
//         return ;
//     }

//     // for (int i = 0;i < PGSIZE;i+=8) {
//     //     printf("%x ", *(volatile uint64 *)((uint64)gs_virtio_blk_hw.common_cfg + i));
//     // }

//     // 1. reset device
//     virtio_pci_set_status(&gs_virtio_blk_hw, 0);

//     uint8 status = 0;
//     // 2. set ACKNOWLEDGE status bit
//     status |= VIRTIO_STAT_ACKNOWLEDGE;
//     virtio_pci_set_status(&gs_virtio_blk_hw, status);

//     // 3. set DRIVER status bit
//     status |= VIRTIO_STAT_DRIVER;
//     virtio_pci_set_status(&gs_virtio_blk_hw, status);

//     // 4. negotiate features
//     uint64 features = virtio_pci_get_device_features(&gs_virtio_blk_hw);

//     features &= ~(1 << VIRTIO_BLK_F_RO);
//     features &= ~(1 << VIRTIO_BLK_F_SCSI);
//     features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
//     features &= ~(1 << VIRTIO_BLK_F_MQ);
//     features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
//     features &= ~(1 << VIRTIO_F_EVENT_IDX);
//     features &= ~(1 << VIRTIO_F_INDIRECT_DESC);

//     virtio_pci_set_driver_features(&gs_virtio_blk_hw, features);

//     // 5. tell device that feature negotiation is complete.
//     status |= VIRTIO_STAT_FEATURES_OK;
//     virtio_pci_set_status(&gs_virtio_blk_hw, status);

//     // 6. re-read status to ensure FEATURES_OK is set.
//     status = virtio_pci_get_status(&gs_virtio_blk_hw);
//     if(!(status & VIRTIO_STAT_FEATURES_OK)) {
//         printf("virtio disk FEATURES_OK unset");
//         return ;
//     }

//     // 7. initialize queue 0.
//     int qnum = 0;
//     int qsize = BLK_QSIZE;
//     // ensure queue 0 is not in use.

//     if (virtio_pci_get_queue_enable(&gs_virtio_blk_hw, qnum)) {
//         printf("virtio disk should not be ready");
//         return ;
//     }

//     // check maximum queue size.
//     uint32 max = virtio_pci_get_queue_size(&gs_virtio_blk_hw, qnum);
//     printf("queue_0 max size: %d\n", max);
//     if(max == 0){
//         printf("virtio disk has no queue 0");
//         return ;
//     }
//     if(max < qsize){
//         printf("virtio disk max queue too short");
//         return ;
//     }

//     initlock(&disk.vdisk_lock, "virtio disk lock");
//     virtio_pci_set_queue_size(&gs_virtio_blk_hw, 0, NUM);
//     memset(disk.pages, 0, sizeof(disk.pages));
//     disk.desc = (struct VRingDesc *) disk.pages;
//     disk.avail = (uint16*)(((char*)disk.desc) + NUM*sizeof(struct VRingDesc));
//     disk.used = (struct UsedArea *) (disk.pages + PGSIZE);
//     for(int i = 0; i < NUM; i++)
//         disk.free[i] = 1;

//     virtio_pci_set_queue_addr2(&gs_virtio_blk_hw, qnum, disk.desc, disk.avail, disk.used);

//     virtio_pci_set_queue_enable(&gs_virtio_blk_hw, qnum);

//     status |= VIRTIO_STAT_DRIVER_OK;
//     virtio_pci_set_status(&gs_virtio_blk_hw, status);
//     return ;

// }