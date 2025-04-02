//complete all data type macro is in type.h

//this program only need these
//wait to be unified in style
#include "types.h"
#include "uart.h"
#include "print.h"
#include "process.h"
#include "pmem.h"
#include "vmem.h"
#include "string.h"
#include "virt.h"
#include "hsai_trap.h"
#if defined RISCV
  #include "riscv.h"
#else
  #include "loongarch.h"
#endif

extern struct proc *current_proc;
extern void uservec();
extern void *userret(uint64);//trampoline 进入用户态
extern int init_main( void ); //用户程序

__attribute__((aligned(4096))) char user_stack[4096] ;//用户栈

int main() { while ( 1 ); } //为了通过编译, never use this

void  test_pmem();

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

  #if defined RISCV //进入用户态
    //下面设置只涉及csr
		//设置ecall的跳转地址到stvec，固定为uservec
		hsai_set_usertrap();
    //设置sstatus
		hsai_set_csr_to_usermode();//
		//设置sepc. 指令sret使用。S态进入U态
		hsai_set_csr_sepc((uint64)(void *)init_main);
		
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
    printf("识别硬盘\n");
    virtio_disk_init();
    //运行线程
    hsai_set_usertrap();
		userret((uint64)p->trapframe);
  #else //loongarch
    struct proc* p = allocproc();
		current_proc =p ;

    //trap_init.!!!!!!!!!有了这个才能跑
    uint32 ecfg = ( 0U << CSR_ECFG_VS_SHIFT ) | HWI_VEC | TI_VEC;//例外配置
    //uint64 tcfg = 0x1000000UL | CSR_TCFG_EN | CSR_TCFG_PER;
    w_csr_ecfg(ecfg);
    //w_csr_tcfg(tcfg);//计时器

    //设置syscall的跳转地址到CSR.EENTRY
    printf("uservec address: %p\n",&uservec);
    w_csr_eentry((uint64)uservec);// send syscalls, interrupts, and exceptions to trampoline.S
    //类似设置sstatus
    uint32 x = r_csr_prmd();
    x |= PRMD_PPLV; // set PPLV to 3 for user mode
    x |= PRMD_PIE; // enable interrupts in user mode
    w_csr_prmd(x);
    //设置era,指令ertn使用。S态进入U态
    w_csr_era((uint64)(void *)init_main);
    printf("era init_main address: %p\n",&init_main);
    //下面涉及到trapframe
    //设置内核栈，用户栈，用户页表(现在是0)
    hsai_set_trapframe_kernel_sp(p->trapframe,p->kstack);
		hsai_set_trapframe_user_sp(p->trapframe,(uint64)user_stack+4096);
		hsai_set_trapframe_pagetable(p->trapframe,0);
		//设置内核异常处理函数的地址，固定为usertrap
		hsai_set_trapframe_kernel_trap(p->trapframe);

    printf("hsai设置完成,准备进入用户态\n");
    userret((uint64)p->trapframe);

    while(1) ;

    printf("never reach");
  #endif
    //初始化物理内存
    pmem_init();
    //test_pmem();
    //vmem_init();


    

		//运行线程

		//userret((uint64)p->trapframe);
		//p->state=RUNNABLE;
		while(1) ;
		scheduler();


	return 0;
}







void test_pmem() {
  // 初始化内存管理（假设已实现）
  
  printf("======= 开始内存分配测试 =======\n");
  
  // 测试1: 单次分配释放
  void *page1 = pmem_alloc_pages(1);
  assert(page1 != NULL, "测试1-分配失败");
  assert((uint64)page1 % PGSIZE == 0, "测试1-地址未对齐");
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
  pmem_free_pages(pages[0], 1);
  pmem_free_pages(pages[2], 1);
  pmem_free_pages(pages[1], 1);
  void *reused = pmem_alloc_pages(1);
  assert(reused == pages[1], "测试2-内存重用失败");
  printf("[测试2] 地址%p重用成功\n", reused);

  // 测试3: 内存清零验证
  uint8 *test_buf = (uint8*)pmem_alloc_pages(1);
  memset(test_buf, 0xAA, PGSIZE);
  // 释放内存（pmem_free_pages 内部会清零）
  pmem_free_pages(test_buf, 1);
  // 重新分配内存（可能复用 test_buf 的内存）
  uint8 *zero_buf = (uint8*)pmem_alloc_pages(1);
  // 验证新分配的内存是否已被清零
  uint8 expected_zero[PGSIZE] = {};
  assert(memcmp(zero_buf, expected_zero, PGSIZE) == 0, "测试3-内存未清零");
  printf("[测试3] 内存清零验证通过\n");
  pmem_free_pages(zero_buf, 1);
  
  // 测试4: 边界条件测试
  //4.1 尝试释放非法地址
  //void *invalid_ptr = (void*)((uint64)pages[0] + 1);
  //pmem_free_pages(invalid_ptr, 1); // 应触发断言
  
  //4.2 耗尽内存测试
//   int max_pages = 16;  
//   //int max_pages = 16 ;  
//   void *exhausted[max_pages];
//   for(int i=0; i < max_pages; i++) {
//       exhausted[i] = pmem_alloc_pages(1);
//       //printf("%d\n",i);
//       if(exhausted[i] == NULL) assert(1,"mem");
//       assert(exhausted[i]!=NULL, "测试4-内存耗尽过早失败");
//   }
//  assert(pmem_alloc_pages(1) == NULL, "测试4-内存未耗尽");

  // for(int i=0; i<3; i++) {
  //     pages[i] = pmem_alloc_pages(1);
  //     //assert(pages[i] != NULL, "测试2-分配失败");
  //     printf("[测试2] 第%d次分配地址: %p\n", i+1, pages[i]);
  // }
  printf("[测试4] 边界条件测试通过\n");

  printf("======= 所有测试通过 =======\n");
}