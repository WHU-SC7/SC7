// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "fs_defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#ifdef RISCV
#include "riscv.h"
#include "virt.h"
#else ///< LoongArch
#include "loongarch.h"
#include "virt_la.h"
#endif

// vf2_param的一些宏触发redefined了，先这样，之后处理
typedef enum {
    CARD_ERROR_NONE = 0,
    CARD_ERROR_INIT,
    CARD_ERROR_INTERRUPT,
    CARD_ERROR_TIMEOUT,
    CARD_ERROR_VOLTAGE_PATTERN,
    CARD_ERROR_DATA_TRANSFER_TIMEOUT,
    CARD_ERROR_BUS_BUSY
} card_error_t;
card_error_t sd_read_block(uint8 *buf, uint32 addr);
card_error_t sd_write_block(const uint8 *buf, uint32 addr);

/**
 * @brief Cache，也就是保存所有缓冲块
 * 
 */
struct {
  struct spinlock lock;   ///< protects buf cache
  struct buf buf[NBUF];   ///< array of buffers

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

/**
 * @brief Cache的初始化函数
 * 
 */
void
binit(void)
{
  struct buf *b;

  /* 1. 初始化锁 */
  initlock(&bcache.lock, "bcache");

  /* 2. 初始化buf cache链表 */
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  
  /* 3. 初始化每个缓冲块，头插法 */
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

/**
 * @brief 获取dev的blockno的buf结构体，无则创建
 * 
 * Look through buffer cache for block on device dev.
 * If not found, allocate a buffer.
 * In either case, return locked buffer.
 *
 * @param dev 设备号
 * @param blockno 扇区号
 * @return struct buf* 返回一个buf结构体指针 
 */
struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
  return NULL; // not reached
}

#define PARTITION_BASE_OFFSET 0x100000  //分区偏移 1M
#define SDCARD_SECTOR_SIZE 512          //sd卡扇区大小
#define PARTITION_BASE_SECTOR 2048 //分区起始扇区

/**
 * @brief 从设备读取指定块的内容到buf中
 * 
 * Return a locked buf with the contents of
 * the indicated block.
 *
 * @param dev 设备号
 * @param blockno 扇区号 
 * @return struct buf*  返回一个buf结构体指针 
 */
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    if (dev == 0) {
#ifdef RISCV
  #if VF
      uchar *read_buf = b->data;
      //一次读512字节，要读8次
      for(int i=0;i<8;i++)
      {
        sd_read_block(read_buf, PARTITION_BASE_SECTOR+blockno*8+i); // b->blockno*8也是一样的
        read_buf +=512;
      }
  #else
      virtio_rw(b, 0);
  #endif
#else
      la_virtio_disk_rw(b, 0);
#endif
    } 
    // else {
    //   virtio_disk_rw2(b, 0);
    // }

    b->valid = 1;
  }
  return b;
}

/**
 * @brief 写回buf的内容到磁盘
 * 
 * Write b's contents to disk.  Must be locked.
 *
 * @param b buf结构体指针
 */
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  if (b->dev == 0) {
#ifdef RISCV
  #if VF
    uchar *write_buf = b->data;
    //一次写512字节，要写8次
    for(int i=0;i<8;i++)
    {
      sd_write_block(write_buf, PARTITION_BASE_SECTOR+b->blockno*8+i);
      write_buf +=512;
    }
  #else
    virtio_rw(b, 1);
  #endif
#else
    la_virtio_disk_rw(b, 1);
#endif
  } 
  // else {
  //   virtio_disk_rw2(b, 1);
  // }
}

/**
 * @brief 释放一个上锁的buf
 * 
 * Release a locked buffer.
 * Move to the head of the most-recently-used list.
 * 
 * @param b 
 */
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    /* 
     * no one is waiting for it.
     * 插到开头
     */
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}

/**
 * @brief 增加buf的引用计数
 * 
 * @param b buf结构体指针
 */
void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

/**
 * @brief 减少buf的引用计数
 * 
 * @param b buf结构体指针
 */
void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}