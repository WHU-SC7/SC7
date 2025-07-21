#include "spinlock.h"
#include "process.h"

#define uint64 unsigned long
#define uint8 unsigned char

typedef enum 
		{
			/* General Register Set */

			RHR = 0,				// Receiver Holding Register
			THR = 0,				// Transmitter Holding Register
			IER = 1,				// Interrupt Enable Register
			FCR = 2,				// FIFO Control Register
			ISR = 2,				// Interrupt Status Regsiter
			LCR = 3,				// Line Control Register
			MCR = 4,				// Modem Control Register
			LSR = 5,				// Line Status Register
			MSR = 6,				// Modem Status Regsiter
			SPR = 7,				// Scratch Pad Register

			/* Division Register Set */

			DLL = 0,				// Divisor Latch, Least significant byte
			DLM = 1,				// Divisor Latch, Most significant byte
			PSD = 5,				// PreScaler Division
		} RegOffset;

		struct regIER
		{
			uint8 data_ready : 1;
			uint8 thr_empty : 1;
			uint8 lsr_int : 1;
			uint8 modem_int : 1;
			uint8 _rev : 2;
			uint8 dma_rx : 1;
			uint8 dma_tx : 1;
		};
		// static_assert( sizeof( regIER ) == 1 );
		struct regFCR
		{
			uint8 enable : 1;
			uint8 rx_fifo_reset : 1;
			uint8 tx_fifo_reset : 1;
			uint8 dma_mode : 1;
			uint8 dma_end_enable : 1;
			uint8 _rev : 1;
			uint8 rx_fifo_level : 2;
		};
		
		struct regLCR
		{
			enum {
				bits_5 = 0,
				bits_6 = 1,
				bits_7 = 2,
				bits_8 = 3
			} WordLen;
			uint8 word_length : 2;
			uint8 stop_bits : 1;
			uint8 parity_enbale : 1;
			uint8 even_parity : 1;
			uint8 force_parity : 1;
			uint8 set_break : 1;
			uint8 divisor_access : 1;
		};
		struct regLSR
		{
			uint8 data_ready : 1;
			uint8 overrun_error : 1;
			uint8 parity_error : 1;
			uint8 framing_error : 1;
			uint8 break_intr : 1;
			uint8 thr_empty : 1;
			uint8 transmit_empty : 1;
			uint8 fifo_data_error : 1;
		};

#define _REG_BASE ( (uint64) uart0 | (uint64) win_1 )

#if defined QEMU_VIRT 
	#define uart0  0x1fe001e0UL //loongarch的 -M virt要使用这个地址，在评测机上
#else
	#define uart0  0x1fe20000UL //这是 ls2k的地址
#endif

// the transmit output buffer.
struct spinlock uart_tx_lock;
#define UART_TX_BUF_SIZE 32
char uart_tx_buf[UART_TX_BUF_SIZE];
uint64 uart_tx_w; // write next to uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE]
uint64 uart_tx_r; // read next from uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE]
extern volatile int panicked; // from printf.c

//
#define win_1 0x8UL << 60
		
uint64 _reg_base = ( (uint64) uart0 | (uint64) win_1 );

void _write_reg( RegOffset reg, uint8 data ) { ( ( volatile uint8* ) ( (uint64) uart0 | (uint64) win_1 ) )[ ( int ) reg ] = data; }
uint8 _read_reg( RegOffset reg ) { return ( ( volatile uint8* ) ( (uint64) uart0 | (uint64) win_1 ) )[ ( int ) reg ]; }

void uart_init()
{
	// disable interrrupt
	_write_reg( IER, 0x00 );
	
	// set baudrate
	struct regLCR * lcr = (struct  regLCR* ) ( _reg_base + LCR );
	lcr->divisor_access = 1;
		//the line below will output 'D'
	//_write_reg( DLL, 68 );
	_write_reg( DLM, 0x00 );
	lcr->divisor_access = 0;

	// word length 8 bits
	lcr->word_length = 3;

	// reset and enable FIFO
	struct regFCR * fcr = (struct  regFCR* ) ( _reg_base + FCR );
	fcr->tx_fifo_reset = fcr->rx_fifo_reset = 1;
	fcr->enable = 1;
	fcr->rx_fifo_level = 0;

	// enable interrupt
	struct regIER * ier = (struct  regIER* ) ( _reg_base + IER );
	ier->data_ready = 1;
	// ier->thr_empty = 1;

	//_lock.init( "UART" );
	initlock(&uart_tx_lock, "uart");
}

int put_char_sync( uint8 c )//目前的效果和_write_reg( )一样
{
	volatile struct regLSR * lsr = ( volatile struct regLSR* ) ( _reg_base + LSR );
	while ( lsr->thr_empty == 0 )
		;
	_write_reg( THR, ( uint8 ) c );
	return 0;
}

int get_char_sync( uint8 * c )
{
	volatile struct regLSR * lsr = ( volatile struct regLSR * ) ( _reg_base + LSR );
	while ( lsr->data_ready == 0 );
	*c = _read_reg( THR );
	return 0;
}
// if the UART is idle, and a character is waiting
// in the transmit buffer, send it.
// caller must hold uart_tx_lock.
// called from both the top- and bottom-half.
void 
uartstart(void) 
{
    while(1) {
        if(uart_tx_w == uart_tx_r) return;
        
        volatile struct regLSR *lsr = (volatile struct regLSR*)(_reg_base + LSR);
        if (!(lsr->thr_empty)) return; // 发送寄存器未就绪
        
        int c = uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE];
        uart_tx_r++;
      
        wakeup(&uart_tx_r);
		_write_reg(THR, ( uint8 )  c);
    }
}
// add a character to the output buffer and tell the
// UART to start sending if it isn't already.
// blocks if the output buffer is full.
// because it may block, it can't be called
// from interrupts; it's only suitable for use
// by write().
void
uartputc(int c)
{
  acquire(&uart_tx_lock);

  if(panicked){
    for(;;)
      ;
  }
  while(uart_tx_w == uart_tx_r + UART_TX_BUF_SIZE){
    // buffer is full.
    // wait for uartstart() to open up space in the buffer.
    sleep_on_chan(&uart_tx_r, &uart_tx_lock);
  }
  uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE] = c;
  uart_tx_w += 1;
  uartstart();
  release(&uart_tx_lock);
}

#define Reg(reg) ((volatile unsigned char *)( ( (uint64) uart0 | (uint64) win_1 ) + reg))
#define ReadReg(reg) (*(Reg(reg)))
int
uartgetc(void)
{
  if(ReadReg(LSR) & 0x01){
    // input data is ready.
    return ReadReg(RHR);
  } else {
    return -1;
  }
}