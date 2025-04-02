//参考ucore
//这是hal要向hsai提供的接口
//
#ifndef __TRAP_H__
#define __TRAP_H__
#include "types.h"

void set_usertrap();//设置中断和异常的跳转地址，写csr，架构相关

void usertrap();//处理来自用户态的中断，异常

void usertrapret();//返回用户态

//riscv的异常号列表
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

#if defined RISCV
struct trapframe {//RISCV
    /*   0 */ uint64 kernel_satp;   // kernel page table
    /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
    /*  16 */ uint64 kernel_trap;   // usertrap()
    /*  24 */ uint64 epc;           // saved user program counter
    /*  32 */ uint64 kernel_hartid; // saved kernel tp
    /*  40 */ uint64 ra;
    /*  48 */ uint64 sp;
    /*  56 */ uint64 gp;
    /*  64 */ uint64 tp;
    /*  72 */ uint64 t0;
    /*  80 */ uint64 t1;
    /*  88 */ uint64 t2;
    /*  96 */ uint64 s0;
    /* 104 */ uint64 s1;
    /* 112 */ uint64 a0;
    /* 120 */ uint64 a1;
    /* 128 */ uint64 a2;
    /* 136 */ uint64 a3;
    /* 144 */ uint64 a4;
    /* 152 */ uint64 a5;
    /* 160 */ uint64 a6;
    /* 168 */ uint64 a7;
    /* 176 */ uint64 s2;
    /* 184 */ uint64 s3;
    /* 192 */ uint64 s4;
    /* 200 */ uint64 s5;
    /* 208 */ uint64 s6;
    /* 216 */ uint64 s7;
    /* 224 */ uint64 s8;
    /* 232 */ uint64 s9;
    /* 240 */ uint64 s10;
    /* 248 */ uint64 s11;
    /* 256 */ uint64 t3;
    /* 264 */ uint64 t4;
    /* 272 */ uint64 t5;
    /* 280 */ uint64 t6;
  };
#else
struct trapframe //loongarch
{
    /*   0 */ uint64 ra;
    /*   8 */ uint64 tp;
    /*  16 */ uint64 sp;
    /*  24 */ uint64 a0;
    /*  32 */ uint64 a1;
    /*  40 */ uint64 a2;
    /*  48 */ uint64 a3;
    /*  56 */ uint64 a4;
    /*  64 */ uint64 a5;
    /*  72 */ uint64 a6;
    /*  80 */ uint64 a7;
    /*  88 */ uint64 t0;
    /*  96 */ uint64 t1;
    /* 104 */ uint64 t2;
    /* 112 */ uint64 t3;
    /* 120 */ uint64 t4;
    /* 128 */ uint64 t5;
    /* 136 */ uint64 t6;
    /* 144 */ uint64 t7;
    /* 152 */ uint64 t8;
    /* 160 */ uint64 r21;
    /* 168 */ uint64 fp;
    /* 176 */ uint64 s0;
    /* 184 */ uint64 s1;
    /* 192 */ uint64 s2;
    /* 200 */ uint64 s3;
    /* 208 */ uint64 s4;
    /* 216 */ uint64 s5;
    /* 224 */ uint64 s6;
    /* 232 */ uint64 s7;
    /* 240 */ uint64 s8;
    /* 248 */ uint64 kernel_sp;		// top of process's kernel stack
    /* 256 */ uint64 kernel_trap;	// usertrap()
    /* 264 */ uint64 era;			// saved user program counter
    /* 272 */ uint64 kernel_hartid; // saved kernel tp
    /* 280 */ uint64 kernel_pgdl;	// saved kernel pagetable
};
#endif

#if defined RISCV
struct context {
	uint64 ra;
	uint64 sp;

	// callee-saved
	uint64 s0;
	uint64 s1;
	uint64 s2;
	uint64 s3;
	uint64 s4;
	uint64 s5;
	uint64 s6;
	uint64 s7;
	uint64 s8;
	uint64 s9;
	uint64 s10;
	uint64 s11;
};
#else
struct context
{
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 fp;
};
#endif


enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };
struct proc {
	enum procstate state; // Process state
	int pid; // Process ID
	uint64 ustack; // Virtual address of user stack 现在是物理地址
	uint64 kstack; // Virtual address of kernel stack
	struct trapframe *trapframe; // data page for trampoline.S
	struct context context; // swtch() here to run process
};

#endif 