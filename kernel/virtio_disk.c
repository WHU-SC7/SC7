//对应于virtio_disk
#include "types.h"
#include "print.h"
#include "loongarch.h"
#include "pmem.h"
#include "pci.h"
#include "virt_la.h"
#include "string.h"

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

struct VRingDesc { //<和riscv的virtq_desc一样，只是名字不同
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
    struct VRingDesc *desc;
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

/*
***以下三个函数来自vm.c
*/
#define DMWIN1_MASK 0x8000000000000000 //< memlayout.h 直接映射窗口1
#define PCIE0_ECAM          (0x20000000 | DMWIN1_MASK) //< pci.h
#define PCIE0_ECAM_V        0x20000000 //< pci.h

pte_t *
walk_device(pagetable_t pagetable, uint64 va, int alloc) {
  for(int level = 3; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)(PTE2PA(*pte) | DMWIN1_MASK);
    } else {
      if(!alloc || (pagetable = (uint64*)pmem_alloc_pages(1)) == 0) {
        return 0;
      }

      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V | DMWIN1_MASK;
    }
  }
  return &pagetable[PX(0, va)];
}

int
mappages_device(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, uint64 perm)
{
  uint64 a, last;
  pte_t *pte;

  if(size == 0)
    panic("mappages: size");

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk_device(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

extern pgtbl_t kernel_pagetable;
#define DMWIN_MASK 0x9000000000000000 //< memlayout.h
/*pci.h*/
#define PCIE0_MMIO          (0x40000000 | DMWIN_MASK)
#define PCIE0_MMIO_V          0x40000000

/*我们的vmem.c*/
extern int mappages(pgtbl_t pt, uint64 va, uint64 pa, uint64 len, uint64 perm);

void pci_map(int bus, int dev, int func, void *pages) {
  
    uint64 va = PCIE0_ECAM_V + ((bus << 16) | (dev << 11) | (func << 8));
    uint64 pa = PCIE0_ECAM + ((bus << 16) | (dev << 11) | (func << 8));
    mappages_device(kernel_pagetable, va, PGSIZE, pa, PTE_MAT | PTE_W | PTE_P | PTE_D);
    static int first = 0;
    if (!first) {
      va = PCIE0_MMIO_V;
      pa = PCIE0_MMIO;
      mappages(kernel_pagetable, va, pa, 16 * PGSIZE, PTE_MAT | PTE_W | PTE_P | PTE_D);
      first = 1;
    }
  
  
    // mappages(kernel_pagetable, ((uint64)pages) & (~(DMWIN_MASK)), 2 * PGSIZE, pages, PTE_W | PTE_P | PTE_D | PTE_MAT);
  
  }

void pci_device_init(uint64 pci_base, unsigned char bus, unsigned char device, unsigned char function) {
    pci_map(bus, device, function, disk.pages);
    // printf("map finish\n");
    unsigned int val = 0;
    val = pci_config_read16(pci_base + PCI_STATUS_COMMAND);
    // printf("%d\n", val);
    val |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER | PCI_COMMAND_IO;
    pci_config_write16(pci_base + PCI_STATUS_COMMAND, val);
    val = pci_config_read16(pci_base + PCI_STATUS_COMMAND);
    // printf("%d\n",val);
    // uint8 irq = PCIE_IRQ;
    // pci_config_write8(pci_base + 0x3C, irq);
    // // irq = 0;
    // irq = pci_config_read8(pci_base + 0x3C);
    // printf("%d\n", irq);

    uint64 off = (bus << 16) | (device << 11) | (function << 8);
    volatile uint32 *base = (uint32 *)(PCIE0_ECAM + (uint64)(off)); //< 消除警告, (uint32 *)
    for(int i = 0; i < 6; i++) {
        uint32 old = base[4+i];
        // printf("bar%d origin value: 0x%08x\t", i, old);
        if(old & 0x1) {
            // printf("IO space\n");
            continue;                                      // 未用到 IO 空间, 暂时也不进行分配等
        } else {                                           // 仅为 Mem 空间进行进一步的处理
            // printf("Mem space\t");
        }
        if(old & 0x4) {
            // printf("%x %x\n", base[4 + i], base[4 + i + 1]);             // 64bit 系统映射
            base[4+i] = 0xffffffff;
            base[4+i+1] = 0xffffffff;
            __sync_synchronize();

            uint64 sz = ((uint64)base[4+i+1] << 32) | base[4+i];
            sz = ~(sz & 0xFFFFFFFFFFFFFFF0) + 1;
            printf("bar%d need size: 0x%x\n", i, sz);
            uint64 mem_addr = pci_alloc_mmio(sz);
            // 写入分配的大小
            base[4+i] = (uint32)(mem_addr);
            base[4+i+1] = (uint32)(mem_addr >> 32);
            __sync_synchronize();
            printf("%p\n", mem_addr);
            i++;                                    // 跳过下一个 BAR
        }
    }
}

/*pci.h*/
struct pci_msix {
  int bar_num;            // bar number
  int irq_num;            // interrupt number
  uint32 tbl_addr;           // tbl address
  uint32 pba_addr;           // pba address
};

/*之后放到virtio_pci.h*/
typedef struct virtio_pci_hw {
  uint8      bar;
  uint8	    use_msix;
//uint8      modern;
  uint32     notify_off_multiplier;
  void    *common_cfg;
  void    *isr_cfg;
  void    *device_cfg;
  void    *notify_cfg;
  struct pci_msix msix;
} virtio_pci_hw_t;

virtio_pci_hw_t gs_virtio_blk_hw = { 0 };

/*之后放到virtio_pci.h*/
typedef int (*trap_handler_fn)(int);
extern int virtio_pci_read_caps(virtio_pci_hw_t *hw, uint64 pci_base, trap_handler_fn *msix_isr);
extern void virtio_pci_set_status(virtio_pci_hw_t *hw, uint8 status);
extern uint64 virtio_pci_get_device_features(virtio_pci_hw_t *hw);
extern void virtio_pci_set_driver_features(virtio_pci_hw_t *hw, uint64 features);
extern uint8 virtio_pci_get_status(virtio_pci_hw_t *hw);
extern uint16 virtio_pci_get_queue_enable(virtio_pci_hw_t *hw, int qid);
extern uint16 virtio_pci_get_queue_size(virtio_pci_hw_t *hw, int qid);
extern void virtio_pci_set_queue_size(virtio_pci_hw_t *hw, int qid, int qsize);
extern void virtio_pci_set_queue_addr2(virtio_pci_hw_t *hw, int qid, void *desc, void *avail, void *used);
extern void virtio_pci_set_queue_enable(virtio_pci_hw_t *hw, int qid);

/*virtio.h,在我们的项目是virt_la.h*/
#define BLK_QSIZE       (128)   // blk queue0 size

void la_virtio_disk_init() {
    // pci_scan_buses();

    pci_device_init(pci_base1, bus1, device1, function1);
    if (pci_base1) {
        virtio_pci_read_caps(&gs_virtio_blk_hw, pci_base1, 0);
        // virtio_pci_print_common_cfg(&gs_virtio_blk_hw);
    } else {
        printf("virtion-blk-pci device not found!\n");
        return ;
    }
    /*读取vendor_id, device_id*/
    uint16 vendor_id = pci_config_read16(pci_base1 + 0); //< Vendor ID: 0x1AF4 该厂商ID归属于 Red Hat, Inc.
    uint16 device_id = pci_config_read16(pci_base1 + 2); //< Device ID: 0x1001 对应 Virtio块设备（Virtio Block Device）
    printf("\n[la_virtio_disk_init] Device ID: %x, Vendor ID: %x\n\n", device_id, vendor_id);

    // for (int i = 0;i < PGSIZE;i+=8) {
    //     printf("%x ", *(volatile uint64 *)((uint64)gs_virtio_blk_hw.common_cfg + i));
    // }

    // 1. reset device
    virtio_pci_set_status(&gs_virtio_blk_hw, 0);

    uint8 status = 0;
    // 2. set ACKNOWLEDGE status bit
    status |= VIRTIO_STAT_ACKNOWLEDGE;
    virtio_pci_set_status(&gs_virtio_blk_hw, status);

    // 3. set DRIVER status bit
    status |= VIRTIO_STAT_DRIVER;
    virtio_pci_set_status(&gs_virtio_blk_hw, status);

    // 4. negotiate features
    uint64 features = virtio_pci_get_device_features(&gs_virtio_blk_hw);

    features &= ~(1 << VIRTIO_BLK_F_RO);
    features &= ~(1 << VIRTIO_BLK_F_SCSI);
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
    features &= ~(1 << VIRTIO_BLK_F_MQ);
    features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
    features &= ~(1 << VIRTIO_F_EVENT_IDX);
    features &= ~(1 << VIRTIO_F_INDIRECT_DESC);

    virtio_pci_set_driver_features(&gs_virtio_blk_hw, features);

    // 5. tell device that feature negotiation is complete.
    status |= VIRTIO_STAT_FEATURES_OK;
    virtio_pci_set_status(&gs_virtio_blk_hw, status);

    // 6. re-read status to ensure FEATURES_OK is set.
    status = virtio_pci_get_status(&gs_virtio_blk_hw);
    if(!(status & VIRTIO_STAT_FEATURES_OK)) {
        printf("virtio disk FEATURES_OK unset");
        return ;
    }

    // 7. initialize queue 0.
    int qnum = 0;
    int qsize = BLK_QSIZE;
    // ensure queue 0 is not in use.

    if (virtio_pci_get_queue_enable(&gs_virtio_blk_hw, qnum)) {
        printf("virtio disk should not be ready");
        return ;
    }

    // check maximum queue size.
    uint32 max = virtio_pci_get_queue_size(&gs_virtio_blk_hw, qnum);
    printf("queue_0 max size: %d\n", max);
    if(max == 0){
        printf("virtio disk has no queue 0");
        return ;
    }
    if(max < qsize){
        printf("virtio disk max queue too short");
        return ;
    }

    /*先不考虑锁*/
    //initlock(&disk.vdisk_lock, "virtio disk lock");
    virtio_pci_set_queue_size(&gs_virtio_blk_hw, 0, NUM);
    memset(disk.pages, 0, sizeof(disk.pages));
    disk.desc = (struct VRingDesc *) disk.pages;
    disk.avail = (uint16*)(((char*)disk.desc) + NUM*sizeof(struct VRingDesc));
    disk.used = (struct UsedArea *) (disk.pages + PGSIZE);
    for(int i = 0; i < NUM; i++)
        disk.free[i] = 1;

    virtio_pci_set_queue_addr2(&gs_virtio_blk_hw, qnum, disk.desc, disk.avail, disk.used);

    virtio_pci_set_queue_enable(&gs_virtio_blk_hw, qnum);

    status |= VIRTIO_STAT_DRIVER_OK;
    virtio_pci_set_status(&gs_virtio_blk_hw, status);
    return ;

}