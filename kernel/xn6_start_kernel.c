//complete all data type macro is in type.h

//this program only need these
//wait to be unified in style
#define uint64 unsigned long
#define uint8 unsigned char
#define THR 0 //loongarch, output reg address

extern int	  init_main( void ); //see user_init

int main() { while ( 1 ); } //compiler needed, never use this


extern void uart_init();
extern int put_char_sync( uint8 c );
extern void _write_reg( uint8 reg, uint8 data ); //loongarch version

int xn6_start_kernel()
{
	//if ( hsai::get_cpu()->get_cpu_id() == 0 )
		uart_init();
		
		for(int i=65;i<65+26;i++)
			put_char_sync(i);
		_write_reg( THR, '\n' );
		while(1) ;
	return 0;
}


