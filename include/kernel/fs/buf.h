#ifndef __BUF_H__
#define __BUF_H__
#include "fs.h"

/**
 * @brief BUFFER结构体
 * 
 * 单个缓冲区的结构体，包含了缓冲区的状态、锁、引用计数、设备号等信息。
 */
struct buf {
  int valid;              ///< has data been read from disk?
  int disk;               ///< does disk "own" buf?
  uint dev;               ///< device number
  uint blockno;           ///< block number 
  struct sleeplock lock;  ///< buffer lock
  uint refcnt;            ///< reference count
  struct buf *prev;       ///< LRU cache list
  struct buf *next; 
  uchar data[BSIZE];      ///< data buffer
};

void            binit(void);
struct buf*     bget(uint dev, uint sectorno);
struct buf*     bread(uint, uint);
void            brelse(struct buf*);
void            bwrite(struct buf*);
void            bpin(struct buf*);
void            bunpin(struct buf*);
#endif  /* __BUF_H__ */