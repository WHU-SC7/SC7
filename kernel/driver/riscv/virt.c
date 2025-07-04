// virt.c - 最小化RISC-V VirtIO磁盘驱动
#include "virt.h"
#include "print.h"
#include "string.h"

#include "process.h" //sleep_on_chan, wakeup
#include "hsai_service.h"

static struct disk
{
    char pages[2 * PGSIZE];

    /*三个队列的指针，队列的内容存在上面的pages中*/
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtq_used *used;

    // our own book-keeping.
    char free[NUM];  // is a descriptor free?
    uint16 used_idx; // we've looked this far in used[2..NUM].

    struct
    {
        struct buf *b;
        char status;
    } info[NUM];

    // disk command headers.
    // one-for-one with descriptors, for convenience.
    struct virtio_blk_req ops[NUM];

    struct spinlock vdisk_lock;

} __attribute__((aligned(PGSIZE))) disk;

void virtio_disk_showStatus()
{
    printf("*R(VIRTIO_MMIO_MAGIC_VALUE): %x\n", *R(VIRTIO_MMIO_MAGIC_VALUE));
    printf("*R(VIRTIO_MMIO_VERSION): %x\n", *R(VIRTIO_MMIO_VERSION));
    printf("*R(VIRTIO_MMIO_DEVICE_ID): %x\n", *R(VIRTIO_MMIO_DEVICE_ID));
    printf("*R(VIRTIO_MMIO_VENDOR_ID): %x\n", *R(VIRTIO_MMIO_VENDOR_ID));
}
// 初始化VirtIO磁盘
void virtio_disk_init()
{
    uint32 status = 0;

    initlock(&disk.vdisk_lock, "virtio_disk");
    // 检查设备ID
    if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
        *R(VIRTIO_MMIO_VERSION) != 1 ||
        *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
        *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551)
    { // 0，错误
        printf("could not find virtio disk.");
    }

    *R(VIRTIO_MMIO_STATUS) = status; //< 把设备重置

    /*告诉设备，OS已经识别设备*/
    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    *R(VIRTIO_MMIO_STATUS) = status;

    /*OS已经准备好驱动*/
    status |= VIRTIO_CONFIG_S_DRIVER;
    *R(VIRTIO_MMIO_STATUS) = status;

    // 特性协商，禁用高级功能
    uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES); //< 读取设备支持的特性
    features &= ~(1 << VIRTIO_BLK_F_RO); //< 只读
    features &= ~(1 << VIRTIO_BLK_F_SCSI); //< SCSI
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE); //< 写缓存
    features &= ~(1 << VIRTIO_BLK_F_MQ); //< 多队列
    features &= ~(1 << VIRTIO_F_ANY_LAYOUT); //< 任意内存布局
    features &= ~(1 << VIRTIO_RING_F_EVENT_IDX); //< 事件索引
    features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC); //< 间接描述符
    *R(VIRTIO_MMIO_DRIVER_FEATURES) = features; //< 写入设定

    // 协商完成
    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    *R(VIRTIO_MMIO_GUEST_PAGE_SIZE) = PGSIZE; //< 告知设备，客户机的页大小

    // initialize queue 0.
    *R(VIRTIO_MMIO_QUEUE_SEL) = 0; //< 选择队列0
    uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX); //< 读取最大队列大小
    if (max == 0)
        panic("virtio disk has no queue 0");
    if (max < NUM) //< 队列太小
        panic("virtio disk max queue too short");
    *R(VIRTIO_MMIO_QUEUE_NUM) = NUM; //< 设置队列大小！
    memset(disk.pages, 0, sizeof(disk.pages)); //< 清零队列？内存
    *R(VIRTIO_MMIO_QUEUE_PFN) = ((uint64)disk.pages) >> PGSHIFT; //< 告诉virt设备，队列的内存地址。（之后可能往这里写数据

    // desc = pages -- num * virtq_desc
    // avail = pages + 0x40 -- 2 * uint16, then num * uint16
    // used = pages + 4096 -- 2 * uint16, then num * vRingUsedElem

    /*
        之后客户机通过这三个地址访问三个队列
        客户机与virt设备通过共享内存通信
    */
    disk.desc = (struct virtq_desc *)disk.pages; 
    disk.avail = (struct virtq_avail *)(disk.pages + NUM * sizeof(struct virtq_desc));
    disk.used = (struct virtq_used *)(disk.pages + PGSIZE);

    // free应该标识某个队列的可用状态
    for (int i = 0; i < NUM; i++)
        disk.free[i] = 1;

    // 准备好了，通知设备开始工作
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    // plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
}

// find a free descriptor, mark it non-free, return its index.
// alloc3_desc使用
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
        panic("free_desc 1");
    if (disk.free[i])
        panic("free_desc 2");
    disk.desc[i].addr = 0;
    disk.desc[i].len = 0;
    disk.desc[i].flags = 0;
    disk.desc[i].next = 0;
    disk.free[i] = 1;
    wakeup(&disk.free[0]);
}

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

// allocate three descriptors (they need not be contiguous).
// disk transfers always use three descriptors.
// 似乎每次都使用0,1,2 desc,单线程的情况
static int
alloc3_desc(int *idx)
{
    // printf("alloc3");
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

// 磁盘读写操作
int virtio_rw(struct buf *b, int write)
{ // 0x8003e000
    acquire(&disk.vdisk_lock);
#if MUTI_CORE_DEBUG
    LOG_LEVEL(LOG_INFO,"hart %d read/write disk!\n",hsai_get_cpuid());
#endif
    uint64 sector = b->blockno * (BSIZE / 512);

    // the spec's Section 5.2 says that legacy block operations use
    // three descriptors: one for type/reserved/sector, one for the
    // data, one for a 1-byte status result.

    // allocate the three descriptors.
    int idx[3]; //< desc队列中有NUM个描述符，这里请求分配三个
    //< idx[0],[1],[2]分别是i,i+1,i+2。现在一般i=0
    while (1)
    {
        if (alloc3_desc(idx) == 0)
        {
            break;
        }
        sleep_on_chan(&disk.free[0], &disk.vdisk_lock);
    }
    // format the three descriptors.
    // qemu's virtio-blk.c reads them.

    /* virtio_blk_req应该用于向设备发送请求，buf0是个指针，内容在disk.ops结构体数组中*/
    struct virtio_blk_req *buf0 = &disk.ops[idx[0]]; //< 先指向第一个virtio_blk_req

    if (write)
        buf0->type = VIRTIO_BLK_T_OUT; // write the disk
    else
        buf0->type = VIRTIO_BLK_T_IN; // read the disk
    buf0->reserved = 0;
    buf0->sector = sector; //< 第一个virtio_blk_req包含要写的扇区号，一个扇区大小应该为512

    /* disk.desc与disk.ops协作，待解析 */
    disk.desc[idx[0]].addr = (uint64)buf0; //< 指向设置好的virtio_blk_req
    disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
    disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
    disk.desc[idx[0]].next = idx[1];

    disk.desc[idx[1]].addr = (uint64)b->data; //< 设定设备给的数据要放到哪里，b是struct buf
    disk.desc[idx[1]].len = BSIZE; //< 设备应该处理的数据长度
    if (write)
        disk.desc[idx[1]].flags = 0; // d 读或写
    else
        disk.desc[idx[1]].flags = VRING_DESC_F_WRITE; //  读或写
    disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
    disk.desc[idx[1]].next = idx[2];

    disk.info[idx[0]].status = 0xff; // device writes 0 on success
    disk.desc[idx[2]].addr = (uint64)&disk.info[idx[0]].status;
    disk.desc[idx[2]].len = 1;
    disk.desc[idx[2]].flags = VRING_DESC_F_WRITE; // device writes the status
    disk.desc[idx[2]].next = 0;

    // record struct buf for virtio_disk_intr().
    b->disk = 1;
    disk.info[idx[0]].b = b;

    // tell the device the first index in our chain of descriptors.
    disk.avail->ring[disk.avail->idx % NUM] = idx[0];

    __sync_synchronize();

    // tell the device another avail ring entry is available.
    disk.avail->idx += 1; // not % NUM ...

    __sync_synchronize();
    // if(write) printf("写请求！");
    // else printf("读请求！");
    // printf("准备发送请求到virtio\n");
    *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number

    // Wait for virtio_disk_intr() to say request has finished.
    while (b->disk == 1)
    {
        // printf("wait");
        sleep_on_chan(b, &disk.vdisk_lock);
    }
    // printf("中断返回响应!\n");

    disk.info[idx[0]].b = 0;
    free_chain(idx[0]);

    release(&disk.vdisk_lock);

    return 0;
}

void virtio_disk_intr()
{
    acquire(&disk.vdisk_lock);
#if MUTI_CORE_DEBUG
    LOG_LEVEL(LOG_INFO,"hart %d disk_intr!\n",hsai_get_cpuid());
#endif
    // while(1);
    *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

    __sync_synchronize();

    // the device increments disk.used->idx when it
    // adds an entry to the used ring.

    while (disk.used_idx != disk.used->idx)
    {
        __sync_synchronize();
        int id = disk.used->ring[disk.used_idx % NUM].id;

        if (disk.info[id].status != 0)
        {
            // 打印状态值和错误类型
            printf("Request id=%d, status=0x%x\n", id, disk.info[id].status);
            if (disk.info[id].status != 0)
            {
                printf("Error type: %s\n",
                       disk.info[id].status == VIRTIO_BLK_S_IOERR ? "I/O Error" : disk.info[id].status == VIRTIO_BLK_S_UNSUPP ? "Unsupported"
                                                                                                                              : "Unknown");
                panic("virtio_disk_intr status");
            }
        }

        struct buf *b = disk.info[id].b;
        b->disk = 0; // disk is done with buf
        wakeup(b);
        // printf("与磁盘交换的内容:\n");
        // uint8 *data = b->data;
        // for (int i = 0; i < 1024; i++)
        // {
        //     printf("%x", (uint32)*data);
        //     data++;
        //     if ((i + 1) % 16 == 0)
        //         printf("\n");
        // }

        disk.used_idx += 1;
    }
    release(&disk.vdisk_lock);
}