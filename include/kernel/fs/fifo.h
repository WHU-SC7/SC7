#ifndef __FIFO_H__
#define __FIFO_H__

// FIFO设备实现
#define FIFO_SIZE (17 * 4096)
#define MAX_FIFOS 16

struct fifo {
  struct spinlock lock;
  char data[FIFO_SIZE];
  uint nread;     // 已读取的字节数
  uint nwrite;    // 已写入的字节数
  int readopen;   // 读端是否打开
  int writeopen;  // 写端是否打开
  char path[256]; // FIFO 文件路径
  int in_use;     // 是否正在使用
};

void                init_fifo_table(void);
struct fifo*        find_or_create_fifo(const char* path);
struct fifo*        fifo_open(const char* path, int flags);
void                fifo_close(struct fifo* fi, int flags);
int                 is_fifo_readopen_by_path(const char* path);
int                 fiforead(struct file *f, int, uint64, int);
int                 fifowrite(struct file *f, int, uint64, int);

#endif // __FIFO_H__