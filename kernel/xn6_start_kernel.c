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
#include "process.h"
extern struct proc *current_proc;
void scheduler(void);

#include "hsai_trap.h"
extern int init_main();
__attribute__((aligned(4096))) char user_stack[4096] ;
extern void *userret(uint64);//trampoline 进入用户态


#include "riscv.h"
int xn6_start_kernel()
{
	//if ( hsai::get_cpu()->get_cpu_id() == 0 )
		uart_init();
		
		for(int i=65;i<65+26;i++)
		{
			put_char_sync(i);
			put_char_sync('t');
		}
		put_char_sync('\n');

		proc_init();
		printf("proc初始化完成\n");

		hsai_set_usertrap();
		
		//分配线程，现在只在用户态进行一次系统调用
		struct proc* p = allocproc();
		//设置用户态入口
		uint64 init_main_addr=(uint64)(void *)init_main;
		hsai_set_trapframe_epc(p->trapframe,init_main_addr);//这个没有用

		w_sepc(init_main_addr);
		// set S Previous Privilege mode to User.
		uint64 x = r_sstatus();
		x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
		x |= SSTATUS_SPIE; // enable interrupts in user mode
		w_sstatus(x);
		//设置内核栈，用户栈
		hsai_set_trapframe_kernel_sp(p->trapframe,p->kstack);
		hsai_set_trapframe_user_sp(p->trapframe,(uint64)user_stack+4096);
		hsai_set_trapframe_pagetable(p->trapframe,0);
		//设置异常处理
		hsai_set_trapframe_kernel_trap(p->trapframe);
		printf("准备好进入用户态");
		//运行线程
		userret((uint64)p->trapframe);

		p->state=RUNNABLE;
		scheduler();


		while(1) ;
	return 0;
}

void syscall()
{
	//模拟write调用
	printf("syscall: write调用\n");
}


