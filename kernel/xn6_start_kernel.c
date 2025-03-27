//complete all data type macro is in type.h

//this program only need these
//wait to be unified in style
#include "types.h"

extern int	  init_main( void ); //see user_init

int main() { while ( 1 ); } //compiler needed, never use this

//these three functions are in "uart.c"  Both architecture has
//提供对底层的直接操作
extern void uart_init();
extern int put_char_sync( uint8 c );
extern void _write_reg( uint8 reg, uint8 data ); 

#include "print.h"

int xn6_start_kernel()
{
	//if ( hsai::get_cpu()->get_cpu_id() == 0 )
		uart_init();
		
		for(int i=65;i<65+26;i++)
		{
			put_char_sync(i);
			put_char_sync('t');
		}

		//char *str="print line";
		print_line("print line\n");
		
		int i = 10;
		printf("printf test: %d",i);

		while(1) ;
	return 0;
}


