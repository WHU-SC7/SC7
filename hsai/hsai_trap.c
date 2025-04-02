//处理两个架构的异常。为kernel提供架构无关的接口
//
//
#include "types.h"
#include "trap.h"
#include "print.h"
#if defined RISCV
	#include <riscv.h>
#else
	#include "loongarch.h"
#endif
//两个架构的trampoline函数名称一致
extern char uservec[];//trampoline 用户态异常，陷入。hsai_set_usertrap使用
extern void *userret(uint64);//trampoline 进入用户态。hsai_usertrapret使用

//usertrap()需要这两个
#define SSTATUS_SPP (1L << 8) // Previous mode, 1=Supervisor, 0=User
extern void syscall(); //在kernel中

//hsai_set_trapframe_kernel_sp需要这个
extern struct proc *curr_proc();

//把idle和p交换.再swtch.S中，目前没有使用
extern void swtch(struct context *idle, struct context *p);

// Supervisor Trap-Vector Base Address
// low two bits are mode.
void hsai_set_usertrap()
{
	#if defined RISCV //trap_init
        w_stvec((uint64)uservec & ~0x3);
    #else

    #endif
}

uint64 hsai_get_arg(struct trapframe *trapframe, uint64 register_num)//从trapframe获取参数a0-a7
{
    switch (register_num)//从0开始编号，0返回a0
		{
		case 0:
			return trapframe->a0;
			break;
		case 1:
			return trapframe->a1;
			break;
		case 2:
			return trapframe->a2;
			break;
		case 3:
			return trapframe->a3;
			break;
		case 4:
			return trapframe->a4;
			break;
		case 5:
			return trapframe->a5;
			break;
		case 6:
			return trapframe->a6;
			break;
		case 7:
			return trapframe->a7;
			break;
		default:
			printf("无效的regisrer_num: %d",register_num);
			return -1;
			break;
		}
		return 0;
}

void hsai_swtch(struct context *idle, struct context *p)
{
    #if defined RISCV
        swtch(idle,p);
    #else
        swtch(idle,p);
    #endif
}

void hsai_set_trapframe_kernel_sp(struct trapframe *trapframe, uint64 value)//修改线程内核栈
{
    #if defined RISCV
        trapframe->kernel_sp=value;
    #else
		trapframe->kernel_sp=value;
    #endif
}

//为给定的trapframe设置usertrap,在trampoline保存状态后usertrap处理陷入或异常
//这个usertrap地址是固定的
void hsai_set_trapframe_kernel_trap(struct trapframe *trapframe)
{
    #if defined RISCV
        trapframe->kernel_trap=(uint64)usertrap;
    #else
		trapframe->kernel_trap=(uint64)usertrap;
    #endif
}

void hsai_set_trapframe_epc(struct trapframe *trapframe, uint64 value)//修改返回地址，loongarch的Trapframe为era,意义相同
{
    #if defined RISCV
        trapframe->epc=value;
    #else

    #endif
}

void hsai_set_trapframe_user_sp(struct trapframe *trapframe, uint64 value)//修改用户态的栈
{
    #if defined RISCV
        trapframe->sp=value;
    #else
		trapframe->sp=value;
    #endif
}

void hsai_set_trapframe_pagetable(struct trapframe *trapframe, uint64 value)//修改页表
{
    #if defined RISCV
        trapframe->kernel_satp=0;
    #else
		trapframe->kernel_pgdl=0;
    #endif
}

void hsai_set_csr_to_usermode() //设置好csr寄存器，准备进入U态
{
	#if defined RISCV
		// set S Previous Privilege mode to User.
		uint64 x = r_sstatus();
		x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
		x |= SSTATUS_SPIE; // enable interrupts in user mode
		w_sstatus(x);
	#else

	#endif
}

void hsai_set_csr_sepc(uint64 addr) //设置sepc, sret时跳转
{
	#if defined RISCV
		w_sepc(addr);
    #else

    #endif
}

//如果是第一次进入用户程序，调用usertrapret之前，还要初始化trapframe->sp
void hsai_usertrapret(struct trapframe *trapframe)
{
	
	#if defined RISCV
		hsai_set_trapframe_pagetable(trapframe,0);
		//设置内核栈.第一次设置成功后，kernel_sp的值应该不会被修改，但每次ret还是设置。
		hsai_set_trapframe_kernel_sp(trapframe,curr_proc()->kstack);
		hsai_set_trapframe_kernel_trap(trapframe);
		//set_hartid
		
		hsai_set_csr_sepc(trapframe->epc);
		hsai_set_csr_to_usermode();

		userret((uint64)trapframe);
    #else
		// printf("到达loongarch hsai_usertrapret,停止");
		// while(1) ;

		// set Previous Privilege mode to User Privilege3.
		uint32 x = r_csr_prmd();
		x |= PRMD_PPLV; // set PPLV to 3 for user mode
		x |= PRMD_PIE; // enable interrupts in user mode
		w_csr_prmd(x);

		//设置ertn的返回地址
		w_csr_era(trapframe->era);
		userret((uint64)trapframe);
    #endif
}

//其实xv6-loongarch从uservec进入usertrap时，a0也是trapframe.只不过xv6-loongarch声明为usertrap(void)。我们是可以用a0当trapframe的
void usertrap(struct trapframe *trapframe) 
{
	#if defined RISCV
    	if ((r_sstatus() & SSTATUS_SPP) != 0)
			{printf("usertrap: not from user mode"); while(1) ;}

		uint64 cause = r_scause();
		if (cause == UserEnvCall) {
			trapframe->epc += 4;
			syscall(trapframe);
			hsai_usertrapret(trapframe);
		}
		switch (cause) {
		case StoreMisaligned:
		case StorePageFault:
		case LoadMisaligned:
		case LoadPageFault:
		case InstructionMisaligned:
		case InstructionPageFault:
			printf("%d in application, bad addr = %p, bad instruction = %p, core "
			       "dumped.",
			       cause, r_stval(), trapframe->epc);
			break;
		case IllegalInstruction:
			printf("IllegalInstruction in application, epc = %p, core dumped.",
			       trapframe->epc);
			break;
		default:
			printf("unknown trap: %p, stval = %p sepc = %p", r_scause(),
			       r_stval(), r_sepc());
			break;
		}
	#else
	//我真的服了，xv6-loongarch的trampoline不写入era，要在usertrap保存。riscv都是在trampoline保存的。就这样，在usertrap保存era,不在trampoline保存了
	trapframe->era = r_csr_era();
		if((r_csr_prmd() & PRMD_PPLV) == 0)
    		{printf("usertrap: not from user mode");
			while(1) ;
			}
		if( ((r_csr_estat() & CSR_ESTAT_ECODE) >> 16) == 0xb){
			// system call
			trapframe->era += 4;
			syscall(trapframe);
		} else {
			printf("usertrap(): unexpected trapcause %x\n", r_csr_estat());
			printf("            era=%p badi=%x\n", r_csr_era(), r_csr_badi());
			while(1) ;
		}
		hsai_usertrapret(trapframe);
	#endif
}