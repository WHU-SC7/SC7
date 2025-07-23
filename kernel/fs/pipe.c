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

#define PIPESIZE 512

/**
 * @brief 管道结构体
 * 
 */
struct pipe {
  struct spinlock lock;
  char data[PIPESIZE];
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
  if(pi)
    kfree((char*)pi);
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
pipewrite(struct pipe *pi, uint64 addr, int n)
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
    if(pi->nwrite == pi->nread + PIPESIZE){ //DOC: pipewrite-full
      wakeup(&pi->nread);
      sleep_on_chan(&pi->nwrite, &pi->lock);
    } else {
      char ch;
      /* 用户地址无效 */
      if(copyin(pr->pagetable, &ch, addr + i, 1) == -1)
        break;
      /* 写入1 byte的数据 */
      pi->data[pi->nwrite++ % PIPESIZE] = ch;
      i++;
    }
  }
  wakeup(&pi->nread);
  release(&pi->lock);

  return i;
}

/**
 * @brief 读取管道数据
 * 
 * @param pi 管道
 * @param addr 数据地址(用户地址空间) 
 * @param n 字节
 * @return int 状态码，-1表示失败，0表示成功 
 */
int
piperead(struct pipe *pi, uint64 addr, int n)
{
  int i;
  struct proc *pr = myproc();
  char ch;

  acquire(&pi->lock);
  while(pi->nread == pi->nwrite && pi->writeopen){  //DOC: pipe-empty
    if(killed(pr)){
      release(&pi->lock);
      return -1;
    }
    sleep_on_chan(&pi->nread, &pi->lock); //DOC: piperead-sleep
  }
  for(i = 0; i < n; i++){  //DOC: piperead-copy
    if(pi->nread == pi->nwrite)
      break;
    ch = pi->data[pi->nread++ % PIPESIZE];
    if(copyout(pr->pagetable, addr + i, &ch, 1) == -1)
      break;
  }
  wakeup(&pi->nwrite);  //DOC: piperead-wakeup
  release(&pi->lock);
  return i;
}
