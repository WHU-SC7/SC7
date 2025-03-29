//把中断，异常，陷入转发给hal层的trap处理
//
//
#include "types.h"
#include "trap.h"
#include "riscv.h"
extern char uservec[];

// Supervisor Trap-Vector Base Address
// low two bits are mode.
// static inline void w_stvec(uint64 x)
// {
// 	asm volatile("csrw stvec, %0" : : "r"(x));
// }
void hsai_set_usertrap()
{
	#if defined RISCV //trap_init
        w_stvec((uint64)uservec & ~0x3);
    #else

    #endif
}

void hsai_usertrap()
{

}



void hsai_get_arg()//从trapframe获取参数a0-a5
{
    #if defined RISCV

    #else

    #endif
}

extern void swtch(struct context *idle, struct context *p);//把idle和p交换

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

    #endif
}





//为给定的trapframe设置usertrap,在trampoline保存状态后usertrap处理陷入或异常
//这个usertrap地址是固定的
void hsai_set_trapframe_kernel_trap(struct trapframe *trapframe)
{
    #if defined RISCV
        trapframe->kernel_trap=(uint64)usertrap;
    #else

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

    #endif
}

void hsai_set_trapframe_pagetable(struct trapframe *trapframe, uint64 value)//修改页表
{
    #if defined RISCV
        trapframe->kernel_satp=0;
    #else

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

extern struct proc *curr_proc();
extern void *userret(uint64);//trampoline 进入用户态
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

    #endif
}



enum Exception {
	InstructionMisaligned = 0,
	InstructionAccessFault = 1,
	IllegalInstruction = 2,
	Breakpoint = 3,
	LoadMisaligned = 4,
	LoadAccessFault = 5,
	StoreMisaligned = 6,
	StoreAccessFault = 7,
	UserEnvCall = 8,
	SupervisorEnvCall = 9,
	MachineEnvCall = 11,
	InstructionPageFault = 12,
	LoadPageFault = 13,
	StorePageFault = 15,
};
#define SSTATUS_SPP (1L << 8) // Previous mode, 1=Supervisor, 0=User
extern void syscall(); //在kernel中
void printf(char *fmt, ...);
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

	#endif
}