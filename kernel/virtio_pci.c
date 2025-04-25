#if defined RISCV //riscv不使用这个文件

#else

#include "types.h"
#include "pci.h"
#include "print.h"






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

typedef int (*trap_handler_fn)(int);

/*virtio_pci.h*/
#define le64 uint64 //< 部分结构用到
#define le32 uint32
#define le16 uint16
struct virtio_pci_cap {
    uint8 cap_vndr; /* Generic PCI field: PCI_CAP_ID_VNDR */
    uint8 cap_next; /* Generic PCI field: next ptr. */
    uint8 cap_len; /* Generic PCI field: capability length */
    uint8 cfg_type; /* Identifies the structure. */
    uint8 bar; /* Where to find it. */
    uint8 id; /* Multiple capabilities of the same type */
    uint8 padding[2]; /* Pad to full dword. */
    le32 offset; /* Offset within bar. */
    le32 length; /* Length of the structure, in bytes. */
};
/* Common configuration */
#define VIRTIO_PCI_CAP_COMMON_CFG 1
/* Notifications */
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
/* ISR Status */
#define VIRTIO_PCI_CAP_ISR_CFG 3
/* Device specific configuration */
#define VIRTIO_PCI_CAP_DEVICE_CFG 4
/* PCI configuration access */
#define VIRTIO_PCI_CAP_PCI_CFG 5
/* Shared memory region */
#define VIRTIO_PCI_CAP_SHARED_MEMORY_CFG 8
/* Vendor-specific data */
#define VIRTIO_PCI_CAP_VENDOR_CFG 9

/*pci.h*/
#define PCI_ADDR_CAP    		0x34
#define PCI_CAP_ID_VNDR		    0x09
#define PCI_ADDR_BAR0    		0x10

static void *get_cfg_addr(uint64 pci_base, struct virtio_pci_cap *cap)
{
    // 对于 64bit来说是 BAR4 & BAR5
    uint64 reg = pci_base + PCI_ADDR_BAR0 + 4 * cap->bar;
    return (void *)((pci_config_read64(reg) & 0xFFFFFFFFFFFFFFF0) + cap->offset);
}

int virtio_pci_read_caps(virtio_pci_hw_t *hw, uint64 pci_base, trap_handler_fn *msix_isr)
{
    struct virtio_pci_cap cap;
    uint64 pos = 0;
    //struct virtio_pci_hw *hw = &g_hw;

    pos = pci_config_read8(pci_base + PCI_ADDR_CAP);         // 第一个 cap's offset in ECAM
    // printf("cap: 0x%016llx\n", pci_base + PCI_ADDR_CAP);     // cap pointer 在 ECAM 整个区域中的 offset

    // 遍历所有的 capability
    while (pos) {
        pos += pci_base;
        pci_config_read(&cap, sizeof(cap), pos);

        if (cap.cap_vndr != PCI_CAP_ID_VNDR) {         // PCI_CAP_ID_VNDR vendor-specific Cap
			// printf("[%2llx] skipping non VNDR cap id: %02x\n",
				    // pos, cap.cap_vndr);
			goto next;
		}

       //  printf("[%2llx] cfg type: %u, bar: %u, offset: %04x, len: %u\n",
			    // pos, cap.cfg_type, cap.bar, cap.offset, cap.length);

        switch (cap.cfg_type) {
		case VIRTIO_PCI_CAP_COMMON_CFG:
			hw->common_cfg = get_cfg_addr(pci_base, &cap);
            // printf("common_cfg addr: %016llx\n", (uint64)hw->common_cfg);
			break;
		case VIRTIO_PCI_CAP_NOTIFY_CFG:
			pci_config_read(&hw->notify_off_multiplier,
					4, pos + sizeof(cap));
            hw->notify_cfg = get_cfg_addr(pci_base, &cap);
            // printf("notify_cfg addr: %016llx\n", (uint64)hw->notify_cfg);
			break;
		case VIRTIO_PCI_CAP_DEVICE_CFG:
			hw->device_cfg = get_cfg_addr(pci_base, &cap);
            // printf("device_cfg addr: %016llx\n", (uint64)hw->device_cfg);
			break;
		case VIRTIO_PCI_CAP_ISR_CFG:
			hw->isr_cfg = get_cfg_addr(pci_base, &cap);
            // printf("isr_cfg addr: %016llx\n", (uint64)hw->isr_cfg);
			break;
		}
next:
		pos = cap.cap_next;
    }

    if (hw->common_cfg == NULL || hw->notify_cfg == NULL ||
	    hw->device_cfg == NULL    || hw->isr_cfg == NULL) {
		printf("no modern virtio pci device found.\n");
		return -1;
	}

    printf("found modern virtio pci device.\n");
    printf("use_msix: %d\n", hw->use_msix);
	printf("common cfg mapped at: %p\n", hw->common_cfg);
    printf("isr cfg mapped at: %p\n", hw->isr_cfg);
	printf("device cfg mapped at: %p\n", hw->device_cfg);
	printf("notify base: %p, notify off multiplier: %d\n", hw->notify_cfg, hw->notify_off_multiplier);

    return 0;
}


#define PCI_REG8(reg)   (*(volatile uint8 *)(reg))
#define PCI_REG16(reg)  (*(volatile uint16 *)(reg))
#define PCI_REG32(reg)  (*(volatile uint32 *)(reg))
#define PCI_REG64(reg)  (*(volatile uint64 *)(reg))

struct virtio_pci_common_cfg {
    /* About the whole device. */
    le32 device_feature_select; /* read-write */
    le32 device_feature; /* read-only for driver */
    le32 driver_feature_select; /* read-write */
    le32 driver_feature; /* read-write */
    le16 config_msix_vector; /* read-write */
    le16 num_queues; /* read-only for driver */
    uint8 device_status; /* read-write */
    uint8 config_generation; /* read-only for driver */

    /* About a specific virtqueue. */
    le16 queue_select; /* read-write */
    le16 queue_size; /* read-write */
    le16 queue_msix_vector; /* read-write */
    le16 queue_enable; /* read-write */
    le16 queue_notify_off; /* read-only for driver */
    le64 queue_desc; /* read-write */
    le64 queue_driver; /* read-write avail ring */
    le64 queue_device; /* read-write used ring */
    le16 queue_notify_data; /* read-only for driver */
    le16 queue_reset;       /* read-write */
};

void virtio_pci_set_status(virtio_pci_hw_t *hw, uint8 status)
{
    struct virtio_pci_common_cfg *cfg = hw->common_cfg;
    PCI_REG8(&cfg->device_status) = status;

}

uint64 virtio_pci_get_device_features(virtio_pci_hw_t *hw)
{
    struct virtio_pci_common_cfg *cfg = hw->common_cfg;

    PCI_REG32(&cfg->device_feature_select) = 0;
    uint64 f1 = PCI_REG32(&cfg->device_feature);

    PCI_REG32(&cfg->device_feature_select) = 1;
    uint64 f2 = PCI_REG32(&cfg->device_feature);

    return (f2 << 32) | f1;
}

#define dsb() __sync_synchronize() //For virtio-blk-pci
void virtio_pci_set_driver_features(virtio_pci_hw_t *hw, uint64 features)
{
    struct virtio_pci_common_cfg *cfg = hw->common_cfg;
    PCI_REG32(&cfg->driver_feature_select) = 0;
    dsb();
    PCI_REG32(&cfg->driver_feature) = features & 0xFFFFFFFF;

    PCI_REG32(&cfg->driver_feature_select) = 1;
    dsb();
    PCI_REG32(&cfg->driver_feature) = features >> 32;
}

uint8 virtio_pci_get_status(virtio_pci_hw_t *hw)
{
    struct virtio_pci_common_cfg *cfg = hw->common_cfg;
    return PCI_REG8(&cfg->device_status);
}

uint16 virtio_pci_get_queue_enable(virtio_pci_hw_t *hw, int qid)
{
    struct virtio_pci_common_cfg *cfg = hw->common_cfg;

    PCI_REG16(&cfg->queue_select) = qid;
    dsb();

    return PCI_REG16(&cfg->queue_enable);
}

uint16 virtio_pci_get_queue_size(virtio_pci_hw_t *hw, int qid)
{
    struct virtio_pci_common_cfg *cfg = hw->common_cfg;

    PCI_REG16(&cfg->queue_select) = qid;
    dsb();

    return PCI_REG16(&cfg->queue_size);
}

void virtio_pci_set_queue_size(virtio_pci_hw_t *hw, int qid, int qsize)
{
    struct virtio_pci_common_cfg *cfg = hw->common_cfg;

    PCI_REG16(&cfg->queue_select) = qid;
    dsb();

    PCI_REG16(&cfg->queue_size) = qsize;
}

#define DMWIN_MASK 0x9000000000000000 //< memlayout.h
void virtio_pci_set_queue_addr2(virtio_pci_hw_t *hw, int qid, void *desc, void *avail, void *used)
{
    struct virtio_pci_common_cfg *cfg = hw->common_cfg;

    PCI_REG16(&cfg->queue_select) = qid;
    dsb();
    // printf("%x %x %x\n", ((uint64)desc) & (~(DMWIN_MASK)), ((uint64)avail) & (~(DMWIN_MASK)), ((uint64)used) & (~(DMWIN_MASK)));
    PCI_REG64(&cfg->queue_desc) = ((uint64)desc) & (~(DMWIN_MASK));
    PCI_REG64(&cfg->queue_driver) = ((uint64)avail) & (~(DMWIN_MASK));
    PCI_REG64(&cfg->queue_device) = ((uint64)used) & (~(DMWIN_MASK));
}

void virtio_pci_set_queue_enable(virtio_pci_hw_t *hw, int qid)
{
    struct virtio_pci_common_cfg *cfg = hw->common_cfg;

    PCI_REG16(&cfg->queue_select) = qid;
    dsb();

    PCI_REG16(&cfg->queue_enable) = 1;;
}


/*
  下面是写磁盘的功能
*/

void *virtio_pci_get_queue_notify_addr(virtio_pci_hw_t *hw, int qid)
{
    struct virtio_pci_common_cfg *cfg = hw->common_cfg;

    // 对应的 virtqueue
    PCI_REG16(&cfg->queue_select) = qid;
    dsb();

    // 获得地址
    uint16 notify_off = PCI_REG16(&cfg->queue_notify_off);
    return (void *)((uint64)hw->notify_cfg + notify_off * hw->notify_off_multiplier);
}

// 通知 qid 对应的 virt queue
void virtio_pci_set_queue_notify(virtio_pci_hw_t *hw, int qid)
{
    // notify addr, 计算 notify 地址, 然后通知
    void *pt = virtio_pci_get_queue_notify_addr(hw, qid);
    PCI_REG32(pt) = 1;
}

#endif