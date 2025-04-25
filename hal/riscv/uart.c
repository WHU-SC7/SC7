//
// low-level driver routines for 16550a UART.
//
//type.h
typedef unsigned int   uint;
typedef unsigned char  uchar;
typedef unsigned long uint64;
//param.h
#define NCPU          8  // maximum number of CPUs
#define NOFILE       16  // open files per process
//memlayout.h
#define UART0 0x10000000L
//riscv.h
typedef uint64 *pagetable_t; // 512 PTEs
#include "spinlock.h"

// the UART control registers are memory-mapped
// at address UART0. this macro returns the
// address of one of the registers.
#define Reg(reg) ((volatile unsigned char *)(UART0 + reg))

// the UART control registers.
// some have different meanings for
// read vs write.
// see http://byterunner.com/16550.html
#define RHR 0                 // receive holding register (for input bytes)
#define THR 0                 // transmit holding register (for output bytes)
#define IER 1                 // interrupt enable register
#define IER_RX_ENABLE (1<<0)
#define IER_TX_ENABLE (1<<1)
#define FCR 2                 // FIFO control register
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1) // clear the content of the two FIFOs
#define ISR 2                 // interrupt status register
#define LCR 3                 // line control register
#define LCR_EIGHT_BITS (3<<0)
#define LCR_BAUD_LATCH (1<<7) // special mode to set baud rate
#define LSR 5                 // line status register
#define LSR_RX_READY (1<<0)   // input is waiting to be read from RHR
#define LSR_TX_IDLE (1<<5)    // THR can accept another character to send

#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

// the transmit output buffer.
struct spinlock uart_tx_lock;
#define UART_TX_BUF_SIZE 32
char uart_tx_buf[UART_TX_BUF_SIZE];
uint64 uart_tx_w; // write next to uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE]
uint64 uart_tx_r; // read next from uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE]

extern volatile int panicked; // from printf.c

void uartstart();

void
uart_init(void)
{
  #if defined SBI
    /*使用opensbi,uart_init不做任何事情*/
  #else
  // disable interrupts.
  WriteReg(IER, 0x00);

  // special mode to set baud rate.
  WriteReg(LCR, LCR_BAUD_LATCH);

  // LSB for baud rate of 38.4K.
  WriteReg(0, 0x03);

  // MSB for baud rate of 38.4K.
  WriteReg(1, 0x00);

  // leave set-baud mode,
  // and set word length to 8 bits, no parity.
  WriteReg(LCR, LCR_EIGHT_BITS);

  // reset and enable FIFOs.
  WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

  // enable transmit and receive interrupts.
  WriteReg(IER, IER_TX_ENABLE | IER_RX_ENABLE);
  #endif
}

#if defined SBI
extern void console_putchar(int c);
#endif

// add a character to the output buffer and tell the
// UART to start sending if it isn't already.
// blocks if the output buffer is full.
// because it may block, it can't be called
// from interrupts; it's only suitable for use
// by write().
int
put_char_sync(int c)
{
<<<<<<< HEAD
  push_off();

  if(panicked){
    for(;;)
      ;
  }

=======
  #if defined SBI //< 使用sbi的方式输出字符
  console_putchar(c);
  return 0;
  #else
>>>>>>> master
  while ((ReadReg(LSR) & LSR_TX_IDLE) == 0);

  WriteReg(THR, c);

  pop_off();
  return 0;
  #endif
}

#define uint8 unsigned char
void _write_reg( uint8 reg, uint8 data ) //< 这个函数在别的地方没有使用
{
    WriteReg(reg,data);
}




