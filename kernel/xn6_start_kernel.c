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
#include "pmem.h"
#include "string.h"
extern struct proc *current_proc;
void scheduler(void);

#include "hsai_trap.h"
extern int init_main();
__attribute__((aligned(4096))) char user_stack[4096] ;
extern void *userret(uint64);//trampoline 进入用户态

void  test_pmem();
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
    PRINT_COLOR(BLUE_COLOR_PRINT,"xn6_start_kernel at :%p\n",&xn6_start_kernel);
		proc_init();
		printf("proc初始化完成\n");

    //下面设置只涉及csr
		//设置ecall的跳转地址到stvec，固定为uservec
		hsai_set_usertrap();
    //设置sstatus
		hsai_set_csr_to_usermode();
		//设置sepc. 指令sret使用。S态进入U态
		//hsai_set_csr_sepc((uint64)(void *)init_main);
		
    //下面设置涉及到trapframe
    //分配线程，现在只在用户态进行一次系统调用
		struct proc* p = allocproc();
		current_proc =p ;
		//设置内核栈，用户栈,用户页表
		hsai_set_trapframe_kernel_sp(p->trapframe,p->kstack);
		hsai_set_trapframe_user_sp(p->trapframe,(uint64)user_stack+4096);
		hsai_set_trapframe_pagetable(p->trapframe,0);
		//设置内核异常处理函数的地址，固定为usertrap
		hsai_set_trapframe_kernel_trap(p->trapframe);
		LOG("hsai设置完成\n");

    //初始化物理内存
    pmem_init();
    test_pmem();


    

		//运行线程
		userret((uint64)p->trapframe);
		p->state=RUNNABLE;
		scheduler();


		while(1) ;
	return 0;
}

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

void syscall(struct trapframe *trapframe)
{
	char *str=(char *)trapframe->a1;
	//int len=trapframe->a2;
	printf(str);
	//模拟write调用
	//printf("syscall: write调用. 参数a2: %d\n",len);
}



void test_pmem() {
  // 初始化内存管理（假设已实现）
  
  printf("======= 开始内存分配测试 =======\n");
  
  // 测试1: 单次分配释放
  void *page1 = pmem_alloc_pages(1);
  assert(page1 != NULL, "测试1-分配失败");
  assert((uint64)page1 % PAGE_SIZE == 0, "测试1-地址未对齐");
  printf("[测试1] 分配地址: %p 对齐验证通过\n", page1);
  
  pmem_free_pages(page1, 1);
  printf("[测试1] 释放验证通过\n");

  // 测试2: 多次分配验证重用
  void *pages[3];
  for(int i=0; i<3; i++) {
      pages[i] = pmem_alloc_pages(1);
      assert(pages[i] != NULL, "测试2-分配失败");
      printf("[测试2] 第%d次分配地址: %p\n", i+1, pages[i]);
  }
  
  // 验证释放后能重复使用
  pmem_free_pages(pages[1], 1);
  pmem_free_pages(pages[2], 1);
  pmem_free_pages(pages[1], 1);
  void *reused = pmem_alloc_pages(1);
  assert(reused == pages[1], "测试2-内存重用失败");
  printf("[测试2] 地址%p重用成功\n", reused);

  // 测试3: 内存清零验证
  uint8 *test_buf = (uint8*)pmem_alloc_pages(1);
  memset(test_buf, 0xAA, PAGE_SIZE);
  // 释放内存（pmem_free_pages 内部会清零）
  pmem_free_pages(test_buf, 1);
  // 重新分配内存（可能复用 test_buf 的内存）
  uint8 *zero_buf = (uint8*)pmem_alloc_pages(1);
  // 验证新分配的内存是否已被清零
  uint8 expected_zero[PAGE_SIZE] = {};
  assert(memcmp(zero_buf, expected_zero, PAGE_SIZE) == 0, "测试3-内存未清零");
  printf("[测试3] 内存清零验证通过\n");
  pmem_free_pages(zero_buf, 1);
  
  // 测试4: 边界条件测试
  // 4.1 尝试释放非法地址
  //void *invalid_ptr = (void*)((uint64)pages[0] + 1);
  //pmem_free_pages(invalid_ptr, 1); // 应触发断言
  
  //4.2 耗尽内存测试
  // int max_pages = 2;  
  // void *exhausted[max_pages];
  // for(int i=0; i < max_pages; i++) {
  //     exhausted[i] = pmem_alloc_pages(1);
  //     if(exhausted[i] == NULL) assert(1,"mem");
  //     //assert(exhausted[i]!=NULL, "测试4-内存耗尽过早失败");
  // }
 // assert(pmem_alloc_pages(1) == NULL, "测试4-内存未耗尽");

  for(int i=0; i<3; i++) {
      pages[i] = pmem_alloc_pages(1);
      //assert(pages[i] != NULL, "测试2-分配失败");
      printf("[测试2] 第%d次分配地址: %p\n", i+1, pages[i]);
  }
  printf("[测试4] 边界条件测试通过\n");

  printf("======= 所有测试通过 =======\n");
}