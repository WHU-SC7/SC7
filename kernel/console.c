//
// Console input and output, to the uart.
// Reads are line at a time.
// Implements special input characters:
//   newline -- end of line
//   control-h -- backspace
//   control-u -- kill line
//   control-d -- end of file
//   control-p -- print process list
//

#include <stdarg.h>

#include "types.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "uart.h"
#include "cpu.h"
#include "print.h"
#include "errno-base.h"

#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

#include "defs.h"
#include "process.h"

#define BACKSPACE 0x100
#define C(x)  ((x)-'@')  // Control-x

//
// send one character to the uart.
// called by printf(), and to echo input characters,
// but not from write().
//
void
consputc(int c)
{
  if(c == BACKSPACE){
    // if the user typed backspace, overwrite with a space.
    put_char_sync('\b'); put_char_sync(' '); put_char_sync('\b');
  } else {
    put_char_sync(c);
  }
}

struct {
  struct spinlock lock;
  
  // input
#define INPUT_BUF_SIZE 128
  char buf[INPUT_BUF_SIZE];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} cons;

void service_process_write(int c);
extern proc_t *initproc;
//
// user write()s to the console go here.
//
int
consolewrite(int user_src, uint64 src, int n)
{
  int i;
  if(myproc() == initproc) //init进程允许直接输出
  {
    for(i = 0; i < n; i++){
      char c;
      if(either_copyin(&c, user_src, src+i, 1) == -1)
      break;
      uartputc(c);
    }
  }
  else //其他进程要请求服务进程输出
  {
    for(i = 0; i < n; i++){
      char c;
      if(either_copyin(&c, user_src, src+i, 1) == -1)
        break;
#if SERVICE_PROCESS_CONFIG
      service_process_write(c);
#else
      uartputc(c);
#endif
    }
  }

  return i;
}

#if SERVICE_PROCESS_CONFIG //服务进程的函数

#define NOT_INIT 0 //未初始化
#define READY 1    //允许进程写入
#define OUTPUT 2   //正在输出或者要被输出，不允许写入
char buf_bitmap[NPROC]; //标识缓冲区的状态

struct process_write_buf{
  uint64 pid;
  uint64 used_byte; //初始为0,指向下一个空位
  char write_buf[4096-16];
};

struct process_write_buf process_write_buf[NPROC]; //每个进程槽位都有一个缓冲区

/**
 * @brief 向服务进程管理的缓冲区写入一个字符，如果缓冲区处于OUTPUT状态就忙等待
 *        直到缓冲区READY为止。现在认为缓冲区就算在输出也会很快输出完
 */
void service_process_write(int c)
{
  int pid = myproc()->pid;
  pid %= NPROC;
  while(1)
  {
    if(buf_bitmap[pid] == READY)
    {
        int pos = process_write_buf[pid].used_byte;
        process_write_buf[pid].write_buf[pos] = c; //写入一个字符
        process_write_buf[pid].used_byte ++; //标识以及写入了一个字符
        // char str[2];str[0] = c;str[1]=0;
        // printf("进程 %d写入一个字节 %s\n",pid,str);
        break;
    }
  }
}

/**
 * @brief 别的进程通知服务进程输出缓冲区内容.在freeproc使用
 * */
void signal_service_process(int pid)
{
  pid %= NPROC;
  buf_bitmap[pid] = OUTPUT;
}

#include "string.h"
/**
 * @brief 服务进程的运行循环
 *        服务进程只在内核态运行，每次运行遍历一次缓冲区，如果有OUTPUT的缓冲区就输出。遍历完后主动调度
 * */
void service_process_loop()
{
  //初始化一下
  for(int i=0; i<NPROC; i++)
  {
    process_write_buf[i].pid = i;
    buf_bitmap[i] = READY;
    process_write_buf[i].used_byte = 0;
    memset(&process_write_buf[i],0,4096);
    // printf("buf_bitmap[%d].state: %d",i,buf_bitmap[i]);
  }

  while(1)
  {
    // printf("循环一次");
    for(int i = 0; i<NPROC; i++)
    {
      if(buf_bitmap[i] == OUTPUT)
      {
        //输出这个缓冲区
        for(int j = 0; j<process_write_buf[i].used_byte; j++)
        {
          consputc(process_write_buf[i].write_buf[j]); //使用consputc会更快，但是多进程时也许会跟其他进程内核的输出冲突
        }
        //标识这个buf为未使用，允许下一次使用
        buf_bitmap[i]=READY;
        process_write_buf[i].used_byte = 0;
        // memset(&process_write_buf[i],0,4096); //清空，似乎没有什么作用
      }
    }
    myproc()->state = RUNNABLE;
    myproc()->main_thread->state = t_RUNNABLE;
    sched();//检查一轮后，让出CPU
  }
}
#endif

int uartgetc(void);   //与sbi效果相同，两个架构都可以
int console_getchar(); // sbi
//
// user read()s from the console go here.
// copy (up to) a whole input line to dst.
// user_dist indicates whether dst is a user
// or kernel address.
//
int
consoleread(int user_dst, uint64 dst, int n)
{
#if SBI //只支持读1个字符
  if (user_dst != 1)
    panic("consoleread传入的fd不是1");
  if(n != 1)
    panic("consoleread传入的n不是1");
	struct proc *p = myproc();
	char str[256];
  int c;
  while((c = uartgetc())== -1)
  {

  }
		str[0] = c;
	copyout(p->pagetable, dst, str, n);
  return n;
#else
  if (user_dst != 1)
    panic("consoleread传入的fd不是1");
  if(n != 1)
    panic("consoleread传入的n不是1");
	struct proc *p = myproc();
	char str[256];
  int c;
  while((c = uartgetc())== -1)
  {

  }
		str[0] = c;
	copyout(p->pagetable, dst, str, n);
  return n;
#endif
  // uint target;
  // int c;
  // char cbuf;

  // target = n;
  // acquire(&cons.lock);
  // while(n > 0){
  //   // wait until interrupt handler has put some
  //   // input into cons.buffer.
  //   while(cons.r == cons.w){
  //     if(killed(myproc())){
  //       release(&cons.lock);
  //       return -1;
  //     }
  //     sleep_on_chan(&cons.r, &cons.lock);
  //   }

  //   c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

  //   if(c == C('D')){  // end-of-file
  //     if(n < target){
  //       // Save ^D for next time, to make sure
  //       // caller gets a 0-byte result.
  //       cons.r--;
  //     }
  //     break;
  //   }

  //   // copy the input byte to the user-space buffer.
  //   cbuf = c;
  //   if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
  //     break;

  //   dst++;
  //   --n;

  //   if(c == '\n'){
  //     // a whole line has arrived, return to
  //     // the user-level read().
  //     break;
  //   }
  // }
  // release(&cons.lock);

  // return target - n;
}

//
// the console input interrupt handler.
// uartintr() calls this for input character.
// do erase/kill processing, append to cons.buf,
// wake up consoleread() if a whole line has arrived.
//
void
consoleintr(int c)
{
  acquire(&cons.lock);

  switch(c){
  case C('P'):  // Print process list.
    procdump();
    break;
  case C('U'):  // Kill line.
    while(cons.e != cons.w &&
          cons.buf[(cons.e-1) % INPUT_BUF_SIZE] != '\n'){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  case C('H'): // Backspace
  case '\x7f': // Delete key
    if(cons.e != cons.w){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  default:
    if(c != 0 && cons.e-cons.r < INPUT_BUF_SIZE){
      c = (c == '\r') ? '\n' : c;

      // echo back to the user.
      consputc(c);

      // store for consumption by consoleread().
      cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

      if(c == '\n' || c == C('D') || cons.e-cons.r == INPUT_BUF_SIZE){
        // wake up consoleread() if a whole line (or end-of-file)
        // has arrived.
        cons.w = cons.e;
        wakeup(&cons.r);
      }
    }
    break;
  }
  
  release(&cons.lock);
}

int
devnullread(int user_dst, uint64 dst, int n)
{
  return 0;
}

int
devnullwrite(int user_src, uint64 src, int n)
{
  return 0;
}

int
devzeroread(int user_src, uint64 src, int n)
{
  if (either_copyout(user_src, src, zeros, n) == -1)
    return 0;
  return n;
}

// FIFO设备实现
#define FIFO_SIZE (17 * 4096)

struct fifo {
  struct spinlock lock;
  char data[FIFO_SIZE];
  uint nread;     // 已读取的字节数
  uint nwrite;    // 已写入的字节数
  int readopen;   // 读端是否打开
  int writeopen;  // 写端是否打开
};

// 全局FIFO实例
static struct fifo fifo_dev;

// 全局变量用于传递非阻塞标志
static int fifo_nonblock_flag = 0;

/**
 * @brief 设置FIFO非阻塞标志
 * 
 * @param nonblock 非阻塞标志
 */
void set_fifo_nonblock(int nonblock) {
  // printf("设置FIFO非阻塞标志: %d\n", nonblock);
  fifo_nonblock_flag = nonblock;
}

/**
 * @brief FIFO设备读取函数
 * 
 * @param user_dst 是否是用户空间地址
 * @param dst 目标地址
 * @param n 要读取的字节数
 * @return int 实际读取的字节数
 */
int
fiforead(int user_dst, uint64 dst, int n)
{
  int i;
  struct proc *pr = myproc();
  char ch;

  acquire(&fifo_dev.lock);
  
  // 等待有数据可读或写端关闭
  while(fifo_dev.nread == fifo_dev.nwrite && fifo_dev.writeopen){
    if(killed(pr)){
      release(&fifo_dev.lock);
      return -1;
    }
    // 检查非阻塞标志
    if(fifo_nonblock_flag){
      release(&fifo_dev.lock);
      return -EAGAIN;
    }
    sleep_on_chan(&fifo_dev.nread, &fifo_dev.lock);
  }
  
  // 读取数据
  for(i = 0; i < n; i++){
    if(fifo_dev.nread == fifo_dev.nwrite)
      break;
    ch = fifo_dev.data[fifo_dev.nread++ % FIFO_SIZE];
    if(either_copyout(user_dst, dst + i, &ch, 1) == -1)
      break;
  }
  
  wakeup(&fifo_dev.nwrite);
  release(&fifo_dev.lock);
  return i;
}

/**
 * @brief FIFO设备写入函数
 * 
 * @param user_src 是否是用户空间地址
 * @param src 源地址
 * @param n 要写入的字节数
 * @return int 实际写入的字节数
 */
int
fifowrite(int user_src, uint64 src, int n)
{
  int i = 0;
  struct proc *pr = myproc();

  acquire(&fifo_dev.lock);
  
  while(i < n){
    // 检查读端是否关闭或进程是否被杀死
    if(fifo_dev.readopen == 0 || killed(pr)){
      release(&fifo_dev.lock);
      return -1;
    }
    
    // 检查FIFO是否已满
    if(fifo_dev.nwrite == fifo_dev.nread + FIFO_SIZE){
      // 如果是非阻塞模式，返回EAGAIN
      if(fifo_nonblock_flag){
        release(&fifo_dev.lock);
        return -EAGAIN;
      }
      // 阻塞模式，等待空间
      wakeup(&fifo_dev.nread);
      sleep_on_chan(&fifo_dev.nwrite, &fifo_dev.lock);
    } else {
      char ch;
      // 从用户空间读取一个字节
      if(either_copyin(&ch, user_src, src + i, 1) == -1)
        break;
      // 写入FIFO
      fifo_dev.data[fifo_dev.nwrite++ % FIFO_SIZE] = ch;
      i++;
    }
  }
  
  wakeup(&fifo_dev.nread);
  release(&fifo_dev.lock);
  return i;
}

void
chardev_init(void)
{
  initlock(&cons.lock, "cons");

  uart_init();

  // connect read and write system calls
  // to consoleread and consolewrite.
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
  /* /dev/null init */
  devsw[DEVNULL].read = devnullread;
  devsw[DEVNULL].write = devnullwrite;
  /* /dev/zero init */
  devsw[DEVZERO].read = devzeroread;
  devsw[DEVZERO].write = devnullwrite;
  /* /dev/fifo init */
  devsw[DEVFIFO].read = fiforead;
  devsw[DEVFIFO].write = fifowrite;
  
  // 初始化FIFO设备
  initlock(&fifo_dev.lock, "fifo");
  fifo_dev.readopen = 1;
  fifo_dev.writeopen = 1;
  fifo_dev.nread = 0;
  fifo_dev.nwrite = 0;
}
