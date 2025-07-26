#include "file.h"
#include "spinlock.h"
#include "fifo.h"
#include "errno-base.h"
#include "string.h"
#include "defs.h"
#include "cpu.h"
#include "process.h"
#include "fcntl.h"
#include "socket.h"  // 为了使用 O_NONBLOCK

// FIFO 文件表
static struct fifo fifo_table[MAX_FIFOS];
static struct spinlock fifo_table_lock;

/**
 * @brief 初始化FIFO表
 */
void 
init_fifo_table(void) 
{
  initlock(&fifo_table_lock, "fifo_table");
  // 初始化FIFO表
  for (int i = 0; i < MAX_FIFOS; i++) 
  {
    fifo_table[i].in_use = 0;
    fifo_table[i].readopen = 0;
    fifo_table[i].writeopen = 0;
    fifo_table[i].nread = 0;
    fifo_table[i].nwrite = 0;
    memset(fifo_table[i].path, 0, sizeof(fifo_table[i].path));
  }
}

/**
 * @brief 根据路径查找或创建 FIFO
 * 
 * @param path FIFO 文件路径
 * @return struct fifo* 返回 FIFO 结构指针，失败返回 NULL
 */
struct fifo* find_or_create_fifo(const char* path) {
  acquire(&fifo_table_lock);
  
  // 先查找是否已存在
  for (int i = 0; i < MAX_FIFOS; i++) {
    if (fifo_table[i].in_use && strcmp(fifo_table[i].path, path) == 0) {
      release(&fifo_table_lock);
      return &fifo_table[i];
    }
  }
  
  // 没找到，创建新的
  for (int i = 0; i < MAX_FIFOS; i++) {
    if (!fifo_table[i].in_use) {
      fifo_table[i].in_use = 1;
      strncpy(fifo_table[i].path, path, sizeof(fifo_table[i].path) - 1);
      fifo_table[i].path[sizeof(fifo_table[i].path) - 1] = '\0';
      fifo_table[i].readopen = 0;
      fifo_table[i].writeopen = 0;
      fifo_table[i].nread = 0;
      fifo_table[i].nwrite = 0;
      initlock(&fifo_table[i].lock, "fifo");
      release(&fifo_table_lock);
      return &fifo_table[i];
    }
  }
  
  release(&fifo_table_lock);
  return NULL; // 没有空闲槽位
}

/**
 * @brief 打开 FIFO，类似于 pipe 的处理方式
 * 
 * @param path FIFO 文件路径
 * @param flags 打开标志
 * @return struct fifo* 成功返回 FIFO 指针，失败返回 NULL
 */
struct fifo* fifo_open(const char* path, int flags) {
  struct fifo* fi = find_or_create_fifo(path);
  if (!fi) return NULL;
  
  acquire(&fi->lock);
  
  int access_mode = flags & O_ACCMODE;
  
  // 检查非阻塞模式下的特殊情况
  if (flags & O_NONBLOCK) {
    if (access_mode == O_WRONLY) {
      // 如果是非阻塞写模式，但没有读者，应该返回错误
      if (fi->readopen == 0) {
        release(&fi->lock);
        return NULL; // 返回 NULL 表示应该返回 ENXIO
      }
    }
    // 对于非阻塞读模式，即使没有写者也可以成功打开
    // 只是后续读取时可能立即返回EOF或EAGAIN
  }
  
  // 更新打开状态
  if (access_mode == O_RDONLY || access_mode == O_RDWR) {
    fi->readopen++;
  }
  if (access_mode == O_WRONLY || access_mode == O_RDWR) {
    fi->writeopen++;
  }
  
  release(&fi->lock);
  return fi;
}

/**
 * @brief 关闭 FIFO，类似于 pipeclose
 * 
 * @param fi FIFO 指针
 * @param flags 文件打开时的标志
 */
void fifo_close(struct fifo* fi, int flags) {
  if (!fi) return;
  
  acquire(&fi->lock);
  
  int access_mode = flags & O_ACCMODE;
  
  if (access_mode == O_RDONLY || access_mode == O_RDWR) {
    if (fi->readopen > 0) {
      fi->readopen--;
      wakeup(&fi->nwrite); // 唤醒等待的写者
    }
  }
  if (access_mode == O_WRONLY || access_mode == O_RDWR) {
    if (fi->writeopen > 0) {
      fi->writeopen--;
      wakeup(&fi->nread); // 唤醒等待的读者
    }
  }
  
  // 如果没有读者和写者了，清理这个 FIFO
  if (fi->readopen == 0 && fi->writeopen == 0) {
    acquire(&fifo_table_lock);
    fi->in_use = 0;
    memset(fi->path, 0, sizeof(fi->path));
    release(&fifo_table_lock);
  }
  
  release(&fi->lock);
}

/**
 * @brief 检查指定路径的 FIFO 是否有读者
 * 
 * @param path FIFO 文件路径
 * @return int 1表示有读者，0表示没有读者
 */
int is_fifo_readopen_by_path(const char* path) {
  acquire(&fifo_table_lock);
  
  for (int i = 0; i < MAX_FIFOS; i++) {
    if (fifo_table[i].in_use && strcmp(fifo_table[i].path, path) == 0) {
      int readopen = fifo_table[i].readopen;
      release(&fifo_table_lock);
      return readopen > 0;
    }
  }
  
  release(&fifo_table_lock);
  return 0; // 没找到或没有读者
}

/**
 * @brief FIFO设备读取函数
 * 
 * @param f 文件结构体指针
 * @param user_dst 是否是用户空间地址
 * @param dst 目标地址
 * @param n 要读取的字节数
 * @return int 实际读取的字节数，错误返回负数
 */
int
fiforead(struct file *f, int user_dst, uint64 dst, int n)
{
  int i;
  struct proc *pr = myproc();
  char ch;
  
  if (!f || f->f_type != FD_FIFO) {
    return -EBADF;
  }
  
  struct fifo* fi = f->f_data.f_fifo;
  if (!fi) return -EBADF;

  acquire(&fi->lock);
  
  // 检查是否为非阻塞模式且没有数据可读
  if ((f->f_flags & O_NONBLOCK) && fi->nread == fi->nwrite && fi->writeopen > 0) {
    release(&fi->lock);
    return -EAGAIN;
  }
  
  // 等待有数据可读或写端关闭
  while(fi->nread == fi->nwrite && fi->writeopen > 0){
    if(killed(pr)){
      release(&fi->lock);
      return -1;
    }
    
    // 非阻塞模式下不应该到这里，但为了安全起见再检查一次
    if (f->f_flags & O_NONBLOCK) {
      release(&fi->lock);
      return -EAGAIN;
    }
    
    sleep_on_chan(&fi->nread, &fi->lock);
  }
  
  // 读取数据
  for(i = 0; i < n; i++){
    if(fi->nread == fi->nwrite)
      break;
    ch = fi->data[fi->nread++ % FIFO_SIZE];
    if(either_copyout(user_dst, dst + i, &ch, 1) == -1)
      break;
  }
  
  wakeup(&fi->nwrite);
  release(&fi->lock);
  return i;
}

/**
 * @brief FIFO设备写入函数
 * 
 * @param f 文件结构体指针
 * @param user_src 是否是用户空间地址
 * @param src 源地址
 * @param n 要写入的字节数
 * @return int 实际写入的字节数，错误返回负数
 */
int
fifowrite(struct file *f, int user_src, uint64 src, int n)
{
  int i = 0;
  struct proc *pr = myproc();
  
  if (!f || f->f_type != FD_FIFO) {
    return -EBADF;
  }
  
  struct fifo* fi = f->f_data.f_fifo;
  if (!fi) return -EBADF;

  acquire(&fi->lock);
  
  // 检查读端是否关闭
  if(fi->readopen == 0) {
    release(&fi->lock);
    kill(myproc()->pid, SIGPIPE);
    return -EPIPE;
  }
  
  // 如果是非阻塞模式且缓冲区已满，立即返回EAGAIN
  if ((f->f_flags & O_NONBLOCK) && (fi->nwrite == fi->nread + FIFO_SIZE)) {
    release(&fi->lock);
    return -EAGAIN;
  }
  
  while(i < n){
    // 检查读端是否关闭或进程是否被杀死
    if(fi->readopen == 0 || killed(pr)){
      release(&fi->lock);
      kill(myproc()->pid, SIGPIPE);
      return -EPIPE;
    }
    
    // 检查FIFO是否已满
    if(fi->nwrite == fi->nread + FIFO_SIZE){
      // 如果是非阻塞模式，返回EAGAIN
      if (f->f_flags & O_NONBLOCK) {
        release(&fi->lock);
        if (i == 0) {
          return -EAGAIN;  // 一个字节都没写入，返回EAGAIN
        }
        return i;  // 已经写入了一些字节，返回已写入的字节数
      }
      
      // 阻塞模式：唤醒读者并等待
      wakeup(&fi->nread);
      sleep_on_chan(&fi->nwrite, &fi->lock);
    } else {
      char ch;
      // 从用户空间读取一个字节
      if(either_copyin(&ch, user_src, src + i, 1) == -1)
        break;
      // 写入FIFO
      fi->data[fi->nwrite++ % FIFO_SIZE] = ch;
      i++;
    }
  }
  
  wakeup(&fi->nread);
  release(&fi->lock);
  return i;
}
