#include "types.h"

#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

#include "defs.h"
#include "fs_defs.h"
#include "spinlock.h"
#include "process.h"
#include "sleeplock.h"
#include "fcntl.h"
#include "file.h"
#include "cpu.h"
#include "pmem.h"
#include "vmem.h"
#include "errno-base.h"
#include "string.h"

#define PIPESIZE 512

/**
 * @brief 管道结构体
 * 
 */
struct pipe {
  struct spinlock lock;
  char *data;     // 动态分配的数据缓冲区
  uint size;      // 管道缓冲区大小
  uint nread;     // number of bytes read "已经"读的
  uint nwrite;    // number of bytes written "已经"写的
  int readopen;   // read fd is still open
  int writeopen;  // write fd is still open
};

/**
 * @brief 分配管道
 * 
 * @param f0 读端口
 * @param f1 写端口
 * @return int 
 */
int
pipealloc(struct file **f0, struct file **f1)
{
  struct pipe *pi;

  pi = 0;
  *f0 = *f1 = 0;
  if((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
    goto bad;
  if((pi = (struct pipe*)kalloc()) == 0)
    goto bad;
  if((pi->data = (char*)kalloc()) == 0)
    goto bad;
  pi->size = PIPESIZE;
  pi->readopen = 1;
  pi->writeopen = 1;
  pi->nwrite = 0;
  pi->nread = 0;
  initlock(&pi->lock, "pipe");
  (*f0)->f_type = FD_PIPE;
  // (*f0)->readable = 1;
  // (*f0)->writable = 0;
  (*f0)->f_flags = O_RDONLY;
  (*f0)->f_data.f_pipe = pi;
  (*f1)->f_type = FD_PIPE;
  // (*f1)->readable = 0;
  // (*f1)->writable = 1;
  (*f1)->f_flags = O_WRONLY;
  (*f1)->f_data.f_pipe = pi;
  return 0;

 bad:
  if(pi) {
    if(pi->data)
      kfree(pi->data);
    kfree((char*)pi);
  }
  if(*f0)
    get_file_ops()->close(*f0);
  if(*f1)
    get_file_ops()->close(*f1);
  return -1;
}

/**
 * @brief 关闭管道
 * 
 * @param pi 管道
 * @param writable 是否可写
 */
void
pipeclose(struct pipe *pi, int writable)
{
  acquire(&pi->lock);
  if(writable){
    pi->writeopen = 0;
    wakeup(&pi->nread);
  } else {
    pi->readopen = 0;
    wakeup(&pi->nwrite);
  }
  if(pi->readopen == 0 && pi->writeopen == 0){
    release(&pi->lock);
    kfree(pi->data);
    kfree((char*)pi);
  } else
    release(&pi->lock);
}

/**
 * @brief 管道写入数据
 * 
 * @param pi 管道
 * @param addr 数据地址(用户地址空间)
 * @param n 写入字节数
 * @return int 实际写入的字节数
 */
int
pipewrite(struct pipe *pi, uint64 addr, int n, struct file *f)
{
  int i = 0;
  struct proc *pr = myproc();

  acquire(&pi->lock);
  while(i < n)
  {
    /* 已经不让读/线程已经死掉了 */
    if(pi->readopen == 0 || killed(pr))
    {
      release(&pi->lock);
      kill(myproc()->pid,SIGPIPE);
      return -EPIPE;
    }
    /* 满了 */
    if(pi->nwrite == pi->nread + pi->size){ //DOC: pipewrite-full
      // 检查非阻塞模式
      int nonblock = (f && (f->f_flags & O_NONBLOCK));
      if(nonblock) {
        release(&pi->lock);
        if (i == 0) {
          return -EAGAIN;  // 一个字节都没写入，返回EAGAIN
        }
        return i;  // 已经写入了一些字节，返回已写入的字节数
      }
      wakeup(&pi->nread);
      sleep_on_chan(&pi->nwrite, &pi->lock);
    } else {
      char ch;
      /* 用户地址无效 */
      if(copyin(pr->pagetable, &ch, addr + i, 1) == -1)
        break;
      /* 写入1 byte的数据 */
      pi->data[pi->nwrite++ % pi->size] = ch;
      i++;
    }
  }
  wakeup(&pi->nread);
  release(&pi->lock);

  return i;
}

/**
 * @brief 设置管道缓冲区大小
 * 
 * @param pi 管道
 * @param size 新的缓冲区大小
 * @return int 成功返回0，失败返回-1
 */
int
pipeset_size(struct pipe *pi, uint size)
{
  if(size == 0)
    return -1;
  
  acquire(&pi->lock);
  
  // 如果新大小与当前大小相同，直接返回
  if(pi->size == size) {
    release(&pi->lock);
    return 0;
  }
  
  // 分配新的缓冲区
  char *new_data = (char*)kalloc();
  if(new_data == 0) {
    release(&pi->lock);
    return -1;
  }
  // 清空新缓冲区
  memset(new_data, 0, size);
  
  // 复制现有数据到新缓冲区
  uint data_size = 0;
  if(pi->nwrite > pi->nread) {
    data_size = pi->nwrite - pi->nread;
    if(data_size > size) {
      // 如果新缓冲区太小，只保留最后的数据
      data_size = size;
      pi->nread = pi->nwrite - size;
    }
    for(uint i = 0; i < data_size; i++) {
      new_data[i] = pi->data[(pi->nread + i) % pi->size];
    }
  }
  
  // 释放旧缓冲区并更新管道
  kfree(pi->data);
  pi->data = new_data;
  pi->size = size;
  pi->nread = 0;
  pi->nwrite = data_size;
  
  release(&pi->lock);
  return 0;
}

/**
 * @brief 获取管道缓冲区大小
 * 
 * @param pi 管道
 * @return uint 管道缓冲区大小
 */
uint
pipeget_size(struct pipe *pi)
{
  acquire(&pi->lock);
  uint size = pi->size;
  release(&pi->lock);
  return size;
}

/**
 * @brief 读取管道数据
 * 
 * @param pi 管道
 * @param addr 数据地址(用户地址空间) 
 * @param n 字节
 * @return int 状态码，-1表示失败，0表示EOF，>0表示读取的字节数
 */
int
piperead(struct pipe *pi, uint64 addr, int n, struct file *f)
{
  int i=0;
  struct proc *pr = myproc();
  char ch;

  acquire(&pi->lock);
  
  // 检查管道关闭状态：如果写端已关闭且没有数据，直接返回EOF
  if(pi->nread == pi->nwrite && !pi->writeopen) {
    release(&pi->lock);
    return 0; // EOF
  }
  
  while(pi->nread == pi->nwrite && pi->writeopen){  //DOC: pipe-empty
    if(killed(pr)){
      release(&pi->lock);
      return -1;
    }
    // 直接检查传入的文件描述符的非阻塞标志
    int nonblock = (f && (f->f_flags & O_NONBLOCK));
    if(nonblock) {
      release(&pi->lock);
      return -EAGAIN;
    }
    sleep_on_chan(&pi->nread, &pi->lock); //DOC: piperead-sleep
  }
  
  // 再次检查管道关闭状态：可能在睡眠期间写端被关闭
  if(pi->nread == pi->nwrite && !pi->writeopen) {
    release(&pi->lock);
    return 0; // EOF
  }
  
  for(i = 0; i < n; i++){  //DOC: piperead-copy
    if(pi->nread == pi->nwrite)
      break;
    ch = pi->data[pi->nread++ % pi->size];
    if(copyout(pr->pagetable, addr + i, &ch, 1) == -1)
      break;
  }
  wakeup(&pi->nwrite);  //DOC: piperead-wakeup
  release(&pi->lock);
  return i;
}
