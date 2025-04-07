//处理两个架构的异常。为kernel提供架构无关的接口
//
//
#include "types.h"
#include "trap.h"
#include "print.h"
#include "virt.h"
#include "plic.h"
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

/**
 * @brief 只对loongarch设置ecfg,应该只在初始化时设置一次。开启外部中断和时钟中断
 * 
 * Riscv对应的是设置sie，但是已经在start.c中设置了，而loongarch没有M态的初始化。
 * 这里只对loongarch执行操作，riscv什么都不做
*/
void hsai_trap_init()
{
	#if defined RISCV 
		//w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
    #else
		uint32 ecfg = ( 0U << CSR_ECFG_VS_SHIFT ) | HWI_VEC | TI_VEC;//例外配置
		w_csr_ecfg(ecfg);
    #endif
}

/**
 * @brief 设置异常处理函数到uservec,对于U态的异常
 */
void hsai_set_usertrap()
{
	#if defined RISCV //trap_init
        w_stvec((uint64)uservec & ~0x3);
    #else
		w_csr_eentry((uint64)uservec & ~0x3);// 
    #endif
}

/**
 * @brief 设置好sstatus或prmd,准备进入U态
 */
void hsai_set_csr_to_usermode() //设置好csr寄存器，准备进入U态
{
	#if defined RISCV
		// set S Previous Privilege mode to User.
		uint64 x = r_sstatus();
		x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
		x |= SSTATUS_SPIE; // enable interrupts in user mode
		x |= SSTATUS_SIE; //委托给S态处理中断，要使能
		w_sstatus(x);
	#else
		//类似设置sstatus
		uint32 x = r_csr_prmd();
		x |= PRMD_PPLV; // set PPLV to 3 for user mode
		x |= PRMD_PIE; // enable interrupts in user mode
		w_csr_prmd(x);
	#endif
}

/**
 * @brief 设置sepc或era,返回用户态时跳转到用户程序
 */
void hsai_set_csr_sepc(uint64 addr) //设置sepc, sret时跳转
{
	#if defined RISCV
		w_sepc(addr);
    #else
		//设置era,指令ertn使用。S态进入U态
		w_csr_era((uint64)(void *)addr);
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
		trapframe->era=value;
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
        trapframe->kernel_satp=value;
    #else
		trapframe->kernel_pgdl=value;
    #endif
}

//如果是第一次进入用户程序，调用usertrapret之前，还要初始化trapframe->sp
void hsai_usertrapret()
{
	struct trapframe *trapframe=curr_proc()->trapframe;
	hsai_set_usertrap();
	hsai_set_csr_to_usermode();

	hsai_set_trapframe_kernel_sp(trapframe,curr_proc()->kstack+PGSIZE);
	hsai_set_trapframe_pagetable(curr_proc()->trapframe,0); ///< 待修改
	hsai_set_trapframe_kernel_trap(curr_proc()->trapframe);
	#if defined RISCV ///< 后续系统调用，只需要下面的代码
		hsai_set_csr_sepc(trapframe->epc);
		userret((uint64)trapframe);
    #else
		//设置ertn的返回地址
		hsai_set_csr_sepc(trapframe->era);
		userret((uint64)trapframe);
    #endif
}

///< 如果已经进入了U态，每次系统调用完成后返回时只需要如下就可以（不考虑虚拟内存
//如果是第一次进入用户程序，调用usertrapret之前，还要初始化trapframe->sp
// void minium_usertrap(struct trapframe *trapframe)
// {
// 	#if defined RISCV
// 		hsai_set_csr_sepc(trapframe->epc);
// 		userret((uint64)trapframe);
//     #else
// 		hsai_set_csr_sepc(trapframe->era);
// 		userret((uint64)trapframe);
//     #endif
// }

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
			hsai_usertrapret();
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
		hsai_usertrapret();
	#endif
}


#define VIRTIO0_IRQ 1
int devintr()
{
	#if defined RISCV
	printf("devintr处理中断\n");
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } 
//处理时钟中断的部分，现在不需要
//   else if(scause == 0x8000000000000001L){
//     // software interrupt from a machine-mode timer interrupt,
//     // forwarded by timervec in kernelvec.S.

//     if(cpuid() == 0){
//       clockintr();
//     }
    
//     // acknowledge the software interrupt by clearing
//     // the SSIP bit in sip.
//     w_sip(r_sip() & ~2);

//     return 2;
//   } 
  else {//没检测到外部中断，错误，返回0
    return 0;
  }
  	#else
  	return 1;
	#endif
}

void kerneltrap()
{
	#if defined RISCV
	printf("kerneltrap! \n");
	//while(1) ;
	int which_dev = 0;
	uint64 sepc = r_sepc();
	uint64 sstatus = r_sstatus();
	uint64 scause = r_scause();
	
	if((sstatus & SSTATUS_SPP) == 0)
	  panic("kerneltrap: not from supervisor mode");
	if(intr_get() != 0)
	  panic("kerneltrap: interrupts enabled");
  
	if((which_dev = devintr()) == 0){
	  printf("scause %p\n", scause);
	  printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
	  panic("kerneltrap");
	}
	//这里删去了时钟中断的代码，时钟中断使用yield
	//give up the CPU if this is a timer interrupt.
	
	// the yield() may have caused some traps to occur,
	// so restore trap registers for use by kernelvec.S's sepc instruction.
	w_sepc(sepc);
	w_sstatus(sstatus);
	#else

	#endif
}