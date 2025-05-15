#if defined RISCV // riscv不使用这个文件

#else

// 对应于virtio_disk
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

// 之后应该放到virtio_pci.h
//  #define PGSIZE 4096
#define NUM 256
#define WAIT_TIME 100 * 1024                  ///< 100M大约1秒，1M大约10ms,100k大约1ms,100大概1微秒，用于loongarch的磁盘读写延时

// #define BSIZE 1024 //< 相当于两个扇区，设置为1024是为了减少读写次数，一次读取更多数据
// struct buf { //之后可能要移走
//     int valid;   // has data been read from disk?
//     int disk;    // does disk "own" buf?
//     uint dev;
//     uint blockno;
//     //struct sleeplock lock;
//     uint refcnt;
//     struct buf *prev; // LRU cache list
//     struct buf *next;
//     uchar data[BSIZE];
//   };

struct VRingDesc
{ //<和riscv的virtq_desc一样，只是名字不同
    uint64 addr;
    uint32 len;
    uint16 flags;
    uint16 next;
};

//< virtio.h
struct VRingUsedElem
{
    uint32 id; // index of start of completed descriptor chain
    uint32 len;
};
struct UsedArea
{
    uint16 flags;
    uint16 id;
    struct VRingUsedElem elems[NUM];
};

struct virtio_blk_req
{
    uint32 type; // VIRTIO_BLK_T_IN or ..._OUT
    uint32 reserved;
    uint64 sector;
};

static struct disk
{
    // memory for virtio descriptors &c for queue 0.
    // this is a global instead of allocated because it must
    // be multiple contiguous pages, which kalloc()
    // doesn't support, and page aligned.
    char pages[2 * PGSIZE];
    struct VRingDesc *desc;
    uint16 *avail;
    struct UsedArea *used;

    // our own book-keeping.
    char free[NUM];  // is a descriptor free?
    uint16 used_idx; // we've looked this far in used[2..NUM].

    // track info about in-flight operations,
    // for use when completion interrupt arrives.
    // indexed by first descriptor index of chain.
    struct
    {
        struct buf *b;
        char status;
    } info[NUM];

    struct virtio_blk_req ops[NUM];

    // struct spinlock vdisk_lock;

} __attribute__((aligned(PGSIZE))) disk;

extern uint64 pci_device_probe(uint16 vendor_id, uint16 device_id); //< pci.c
void virtio_probe()
{
    pci_base1 = pci_device_probe(0x1af4, 0x1001);
    // pci_base2 = pci_device_probe(0x1af4, 0x1001);
}

/*
***以下三个函数来自vm.c
*/
#define DMWIN1_MASK 0x8000000000000000        //< memlayout.h 直接映射窗口1
#define PCIE0_ECAM (0x20000000 | DMWIN1_MASK) //< pci.h
#define PCIE0_ECAM_V 0x20000000               //< pci.h

pte_t *
walk_device(pagetable_t pagetable, uint64 va, int alloc)
{
    for (int level = 3; level > 0; level--)
    {
        pte_t *pte = &pagetable[PX(level, va)];
        if (*pte & PTE_V)
        {
            pagetable = (pagetable_t)(PTE2PA(*pte) | DMWIN1_MASK);
        }
        else
        {
            if (!alloc || (pagetable = (uint64 *)pmem_alloc_pages(1)) == 0)
            {
                return 0;
            }

            memset(pagetable, 0, PGSIZE);
            *pte = PA2PTE(pagetable) | PTE_V | DMWIN1_MASK;
        }
    }
    return &pagetable[PX(0, va)];
}

int mappages_device(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, uint64 perm)
{
    uint64 a, last;
    pte_t *pte;

    if (size == 0)
        panic("mappages: size");

    a = PGROUNDDOWN(va);
    last = PGROUNDDOWN(va + size - 1);
    for (;;)
    {
        if ((pte = walk_device(pagetable, a, 1)) == 0)
            return -1;
        if (*pte & PTE_V)
            panic("mappages: remap");
        *pte = PA2PTE(pa) | perm | PTE_V;
        if (a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

extern pgtbl_t kernel_pagetable;
#define DMWIN_MASK 0x9000000000000000         //< memlayout.h
/*pci.h*/
#define PCIE0_MMIO (0x40000000 | DMWIN_MASK)
#define PCIE0_MMIO_V 0x40000000

/*我们的vmem.c*/
extern int mappages(pgtbl_t pt, uint64 va, uint64 pa, uint64 len, uint64 perm);

void pci_map(int bus, int dev, int func, void *pages)
{

    uint64 va = PCIE0_ECAM_V + ((bus << 16) | (dev << 11) | (func << 8));
    uint64 pa = PCIE0_ECAM + ((bus << 16) | (dev << 11) | (func << 8));
    mappages_device(kernel_pagetable, va, PGSIZE, pa, PTE_MAT | PTE_W | PTE_P | PTE_D);
    static int first = 0;
    if (!first)
    {
        va = PCIE0_MMIO_V;
        pa = PCIE0_MMIO;
        mappages(kernel_pagetable, va, pa, 16 * PGSIZE, PTE_MAT | PTE_W | PTE_P | PTE_D);
        first = 1;
    }

    // mappages(kernel_pagetable, ((uint64)pages) & (~(DMWIN_MASK)), 2 * PGSIZE, pages, PTE_W | PTE_P | PTE_D | PTE_MAT);
}

void pci_device_init(uint64 pci_base, unsigned char bus, unsigned char device, unsigned char function)
{
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
    for (int i = 0; i < 6; i++)
    {
        uint32 old = base[4 + i];
        // printf("bar%d origin value: 0x%08x\t", i, old);
        if (old & 0x1)
        {
            // printf("IO space\n");
            continue; // 未用到 IO 空间, 暂时也不进行分配等
        }
        else
        { // 仅为 Mem 空间进行进一步的处理
            // printf("Mem space\t");
        }
        if (old & 0x4)
        {
            // printf("%x %x\n", base[4 + i], base[4 + i + 1]);             // 64bit 系统映射
            base[4 + i] = 0xffffffff;
            base[4 + i + 1] = 0xffffffff;
            __sync_synchronize();

            uint64 sz = ((uint64)base[4 + i + 1] << 32) | base[4 + i];
            sz = ~(sz & 0xFFFFFFFFFFFFFFF0) + 1;
            printf("bar%d need size: 0x%x\n", i, sz);
            uint64 mem_addr = pci_alloc_mmio(sz);
            // 写入分配的大小
            base[4 + i] = (uint32)(mem_addr);
            base[4 + i + 1] = (uint32)(mem_addr >> 32);
            __sync_synchronize();
            printf("%p\n", mem_addr);
            i++; // 跳过下一个 BAR
        }
    }
}

/*pci.h*/
struct pci_msix
{
    int bar_num;     // bar number
    int irq_num;     // interrupt number
    uint32 tbl_addr; // tbl address
    uint32 pba_addr; // pba address
};

/*之后放到virtio_pci.h*/
typedef struct virtio_pci_hw
{
    uint8 bar;
    uint8 use_msix;
    // uint8      modern;
    uint32 notify_off_multiplier;
    void *common_cfg;
    void *isr_cfg;
    void *device_cfg;
    void *notify_cfg;
    struct pci_msix msix;
} virtio_pci_hw_t;

virtio_pci_hw_t gs_virtio_blk_hw = {0};

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
#define BLK_QSIZE (128)      // blk queue0 size

void la_virtio_disk_init(void)
{
    // pci_scan_buses();

    pci_device_init(pci_base1, bus1, device1, function1);
    if (pci_base1)
    {
        virtio_pci_read_caps(&gs_virtio_blk_hw, pci_base1, 0);
        // virtio_pci_print_common_cfg(&gs_virtio_blk_hw);
    }
    else
    {
        printf("virtion-blk-pci device not found!\n");
        return;
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
    if (!(status & VIRTIO_STAT_FEATURES_OK))
    {
        printf("virtio disk FEATURES_OK unset");
        return;
    }

    // 7. initialize queue 0.
    int qnum = 0;
    int qsize = BLK_QSIZE;
    // ensure queue 0 is not in use.

    if (virtio_pci_get_queue_enable(&gs_virtio_blk_hw, qnum))
    {
        printf("virtio disk should not be ready");
        return;
    }

    // check maximum queue size.
    uint32 max = virtio_pci_get_queue_size(&gs_virtio_blk_hw, qnum);
    printf("queue_0 max size: %d\n", max);
    if (max == 0)
    {
        printf("virtio disk has no queue 0");
        return;
    }
    if (max < qsize)
    {
        printf("virtio disk max queue too short");
        return;
    }

    /*先不考虑锁*/
    // initlock(&disk.vdisk_lock, "virtio disk lock");
    virtio_pci_set_queue_size(&gs_virtio_blk_hw, 0, NUM);
    memset(disk.pages, 0, sizeof(disk.pages));
    disk.desc = (struct VRingDesc *)disk.pages;
    disk.avail = (uint16 *)(((char *)disk.desc) + NUM * sizeof(struct VRingDesc));
    disk.used = (struct UsedArea *)(disk.pages + PGSIZE);
    for (int i = 0; i < NUM; i++)
        disk.free[i] = 1;

    virtio_pci_set_queue_addr2(&gs_virtio_blk_hw, qnum, disk.desc, disk.avail, disk.used);

    virtio_pci_set_queue_enable(&gs_virtio_blk_hw, qnum);

    status |= VIRTIO_STAT_DRIVER_OK;
    virtio_pci_set_status(&gs_virtio_blk_hw, status);
    return;
}

/*
  下面是写磁盘的功能
*/

static int
alloc_desc()
{
    for (int i = 0; i < NUM; i++)
    {
        if (disk.free[i])
        {
            disk.free[i] = 0;
            return i;
        }
    }
    return -1;
}

// mark a descriptor as free.
static void
free_desc(int i)
{
    if (i >= NUM)
        panic("virtio_disk_intr 1");
    if (disk.free[i])
        panic("virtio_disk_intr 2");
    disk.desc[i].addr = 0;
    disk.desc[i].len = 0;
    disk.desc[i].flags = 0;
    disk.desc[i].next = 0;
    disk.free[i] = 1;
    // wakeup(&disk.free[0]);
}

//< virtio.h
#define VIRTIO_BLK_T_IN 0    // read the disk
#define VIRTIO_BLK_T_OUT 1   // write the disk
#define VIRTIO_BLK_T_FLUSH 4 // flush the disk
#define VRING_DESC_F_NEXT 1  // chained with another descriptor
#define VRING_DESC_F_WRITE 2 // device writes (vs read)

// free a chain of descriptors.
static void
free_chain(int i)
{
    while (1)
    {
        int flag = disk.desc[i].flags;
        int nxt = disk.desc[i].next;
        free_desc(i);
        if (flag & VRING_DESC_F_NEXT)
            i = nxt;
        else
            break;
    }
}

static int
alloc3_desc(int *idx)
{
    for (int i = 0; i < 3; i++)
    {
        idx[i] = alloc_desc();
        if (idx[i] < 0)
        {
            for (int j = 0; j < i; j++)
                free_desc(idx[j]);
            return -1;
        }
    }
    return 0;
}

/*
  下面两个函数没有使用
  我们的栈似乎布局有所不同，读不到正确的地址。那就不在栈上存变量了
  不过我先放在这里，之后可以删除
*/

//< memlayout.h
#define PA2VA(pa) ((pa) & (~(DMWIN_MASK)))

// extern pte_t *walk(pgtbl_t pt, uint64 va, int alloc); //< 应该能兼容

typedef uint64 pde_t;
pte_t *
la_virt_walk(pagetable_t pagetable, uint64 va, int alloc)
{
    // if(va >= MAXVA)
    //   panic("walk");

    for (int level = 3; level > 0; level--)
    {
        pte_t *pte = &pagetable[PX(level, va)];
        if (*pte & PTE_V)
        {
            pagetable = (pagetable_t)(PTE2PA(*pte) | DMWIN_MASK);
        }
        else
        {
            if (!alloc || (pagetable = (pde_t *)pmem_alloc_pages(1)) == 0)
            {
                return 0;
            }

            memset(pagetable, 0, PGSIZE);
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    return &pagetable[PX(0, va)];
}

uint64
kwalkaddr(uint64 va)
{
    pagetable_t kpt = kernel_pagetable;
    uint64 off = va % PGSIZE;
    pte_t *pte;
    uint64 pa;

    pte = la_virt_walk(kpt, va, 0);

    if (pte == 0)
        panic("kvmpa");
    if ((*pte & PTE_V) == 0)
        panic("kvmpa");
    pa = PTE2PA(*pte);
    return pa + off;
}

extern void virtio_pci_set_queue_notify(virtio_pci_hw_t *hw, int qid);

/*
  放到函数体里面还要从栈上读，放到bss段就很方便了
*/
struct virtio_blk_outhdr
{
    uint32 type;
    uint32 reserved;
    uint64 sector;
} buf0;

extern void countdown_timer_init(); //< la打开时钟中断

void la_virtio_disk_rw(struct buf *b, int write)
{
    // intr_off();
    // w_csr_tcfg(0);//< 关闭时钟中断
    // printf("[la_virtio_disk_rw()]\n");
    uint64 sector = b->blockno * (BSIZE / 512);

    // acquire(&disk.vdisk_lock);

    // the spec says that legacy block operations use three
    // descriptors: one for type/reserved/sector, one for
    // the data, one for a 1-byte status result.

    // allocate the three descriptors.
    int idx[3];
    while (1)
    {
        if (alloc3_desc(idx) == 0)
        {
            break;
        }
        // sleep(&disk.free[0], &disk.vdisk_lock);
    }

    // format the three descriptors.
    // qemu's virtio-blk.c reads them.

    if (write)
        buf0.type = VIRTIO_BLK_T_OUT; // write the disk
    else
        buf0.type = VIRTIO_BLK_T_IN; // read the disk
    buf0.reserved = 0;
    buf0.sector = sector;

    // buf0 is on a kernel stack, which is not direct mapped,
    // thus the call to kvmpa().
    disk.desc[idx[0]].addr = PA2VA((uint64)&buf0);

    // disk.desc[idx[0]].addr = (uint64) &buf0;
    disk.desc[idx[0]].len = sizeof(buf0);
    disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
    disk.desc[idx[0]].next = idx[1];

    disk.desc[idx[1]].addr = PA2VA((uint64)b->data);

    disk.desc[idx[1]].len = BSIZE;
    if (write)
        disk.desc[idx[1]].flags = 0; // device reads b->data
    else
        disk.desc[idx[1]].flags = VRING_DESC_F_WRITE; // device writes b->data
    disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
    disk.desc[idx[1]].next = idx[2];

    disk.info[idx[0]].status = 0;
    disk.desc[idx[2]].addr = PA2VA((uint64)&disk.info[idx[0]].status);
    disk.desc[idx[2]].len = 1;
    disk.desc[idx[2]].flags = VRING_DESC_F_WRITE; // device writes the status
    disk.desc[idx[2]].next = 0;

    // record struct buf for virtio_disk_intr().
    b->disk = 1;
    disk.info[idx[0]].b = b;

    // avail[0] is flags
    // avail[1] tells the device how far to look in avail[2...].
    // avail[2...] are desc[] indices the device should process.
    // we only tell device the first index in our chain of descriptors.
    disk.avail[2 + (disk.avail[1] % NUM)] = idx[0];
    __sync_synchronize();
    disk.avail[1] = disk.avail[1] + 1;
    virtio_pci_set_queue_notify(&gs_virtio_blk_hw, 0);

    // Wait for virtio_disk_intr() to say request has finished.
    // printf("Before\n");
    // while(b->disk == 1) {
    //   sleep(b, &disk.vdisk_lock);
    // }
    volatile uint16 *pt_used_idx = &disk.used_idx;
    volatile uint16 *pt_idx = &disk.used->id;
    //     wait cmd done
    while (*pt_used_idx == *pt_idx)
    {
        // printf("wait");
    }

    int id = disk.used->elems[disk.used_idx].id;

    /*
     *下面代码块用于显示读写磁盘信息
     *Note:逆天，下面这个不能注释掉，不然不知道为什么ext4磁盘写入有问题，
     *给我整无语了。
     */
    struct buf *bprint = disk.info[id].b;
    bprint->disk = 0; // disk is done with buf
    for (int i = 0; i < WAIT_TIME;)
        i++;
    //   if(write) printf("\n写请求!");
    // else printf("\n读请求!");
    //   printf("与磁盘交换的内容:\n");
    //   uint8 *data=bprint->data;
    // for(int i=0;i<1024;i++)
    // {
    //   printf("%x",(uint32)*data);data++;
    // }
    // printf("\n读写标号 %x\n",disk.used_idx); //< &disk.used->id不用管，没有用。disk.used_idx才标识读写次数
    // printf("准备发送请求到磁盘. %x, %x\n",disk.used_idx,&disk.used->id); //< 最初调试语句

    for (int i = 0; i < WAIT_TIME;)
        i++;
    /*
      结束！
    */

    if (disk.info[id].status != 0)
        panic("virtio_disk_intr status");

    // wakeup(disk.info[id].b);

    disk.used_idx = (disk.used_idx + 1) % NUM;
    while ((disk.used_idx % NUM) != (disk.used->id % NUM))
    {
        int id = disk.used->elems[disk.used_idx].id;

        if (disk.info[id].status != 0)
            panic("virtio_disk_intr status");

        // wakeup(disk.info[id].b);

        disk.used_idx = (disk.used_idx + 1) % NUM;
    }
    b->disk = 0;

    // printf("After\n");
    // for (int i=0; i<512;i++) {
    //     printf("%d ", b->data[i]);
    // }
    // printf("\n");

    disk.info[idx[0]].b = 0;
    free_chain(idx[0]);

    // release(&disk.vdisk_lock);
    // printf("[la_virtio_disk_rw] done\n\n");
    // intr_on();
    //countdown_timer_init();
}

#endif