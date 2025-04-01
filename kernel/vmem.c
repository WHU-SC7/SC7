#include "vmem.h"
#include "pmem.h"
#include "print.h"
#include "hsai_mem.h"
#include "string.h"
#include "riscv.h"
// #if defined RISCV
//   #include "/home/ly/桌面/os2025/kernel/include/riscv.h"
//   #define PT_LEVEL 3
// #else
//   #include "loongarch.h"
//   #define PT_LEVEL 4
// #endif

pgtbl_t kernel_pagetable; // 内核页表
bool debug_trace_walk =false;

#define UART0 0x10000000L

#define KERNEL_BASE 0x80000000ul
//#define KERNEL_BASE 0x0000000090041000
#define TRAMPOLINE  (MAXVA - PGSIZE) 
extern char KERNEL_DATA;
extern char KERNEL_TEXT;
extern char trampoline; // in trampoline.S 
void vmem_init()
{
    kernel_pagetable = pmem_alloc_pages(1); // 分配一个页,存放内核页表 分配时已清空页面
    memset(kernel_pagetable , 0, PGSIZE);
    LOG("kernel_pagetable address: %p\n",kernel_pagetable);
    //UART映射
    vmem_mappages(kernel_pagetable, UART0 , UART0 , PGSIZE, PTE_R | PTE_X );
    // kernel代码区映射 映射为可读可执行
    vmem_mappages(kernel_pagetable, KERNEL_BASE, KERNEL_BASE, (uint64)&KERNEL_TEXT-KERNEL_BASE, PTE_R | PTE_X );
    LOG("[MAP] KERNEL_BASE:%p ->  KERNEL_TEXT:%p  len: 0x%x\n", KERNEL_BASE, (uint64)&KERNEL_TEXT,(uint64)&KERNEL_TEXT-KERNEL_BASE);
    // kernel数据区映射 映射为可读可写
    vmem_mappages(kernel_pagetable, (uint64)&KERNEL_TEXT, (uint64)&KERNEL_TEXT, (uint64)&KERNEL_DATA-(uint64)&KERNEL_TEXT, PTE_R | PTE_W );
    LOG("[MAP] KERNEL_TEXT :%p ->  KERNEL_DATA:%p  len: 0x%x\n", (uint64)&KERNEL_TEXT ,(uint64)&KERNEL_DATA,(uint64)&KERNEL_DATA-(uint64)&KERNEL_TEXT);
    // trampoline映射
    vmem_mappages(kernel_pagetable, TRAMPOLINE, (uint64)&trampoline, PGSIZE, PTE_R | PTE_X );
    LOG("[MAP] TRAMPOLINE :%p ->  trampoline:%p  len: 0x%x\n", TRAMPOLINE ,(uint64)&trampoline,PGSIZE);


    //w_satp(MAKE_SATP(kernel_pagetable));    
    // flush the TLB
    sfence_vma();
    //配置内核页表前需要正确映射代码区域和数据区域
    //hsai_config_pagetable(kernel_pagetable); 
    printf("config kernel page success\n");
    
}



/**
 * 在页表中遍历虚拟地址对应的页表项（PTE）
 * 
 * @param pt    页表基地址
 * @param va    待查询的虚拟地址
 * @param alloc 是否自动分配缺失的页表层级（1=分配，0=不分配）
 * @return      返回虚拟地址对应的页表项（PTE）指针，若失败返回NULL
 * 
 * 功能说明：
 * 1. 通过多级页表逐级解析虚拟地址va，最终返回对应的PTE。
 * 2. 若alloc=1且中间页表项不存在，函数会动态分配物理页作为下级页表。
 * 3. 若alloc=0且中间页表项缺失，函数返回NULL表示遍历失败。
 * 
 */
pte_t* vmem_walk(pgtbl_t pt, uint64 va, int alloc){
    assert(va < MAXVA ,"va out of range");
    pte_t *pte;
    if ( debug_trace_walk ) printf( "[walk trace] 0x%p:", va );
    for(int level = 3- 1; level > 0; level--){
        pte = &pt[PX(level,va)];
        if ( debug_trace_walk ) printf( "0x%p->", pte );
        if(*pte & PTE_V){   //有效PTE，找下一级页表地址
            pt = (pgtbl_t)PTE2PA(*pte);

        }else if (alloc){   //无效PTE,但是可以分配
            pt = (pgtbl_t)pmem_alloc_pages(1);
            if(pt == NULL) return NULL;
            //将PTE对于的物理地址设置为有效
            *pte = PA2PTE(pt) | PTE_V;

            /// @todo TLB刷新？

        }else{
            return NULL;
        }
    }
    pte = &pt[PX(0, va)];
    if ( debug_trace_walk ) printf( "0x%p\n", pte );
    return pte;
}



/** 
 * 在pt中建立映射 [va, va+len)->[pa, pa+len)
 * @param va :  要映射到的起始虚拟地址
 * @param pa :  需要映射的起始物理地址  输入时需要对齐
 * @param len:  映射长度 
 * @param perm: 权限位 
 * @return   映射成功返回1，失败返回-1
 *
 */
int vmem_mappages(pgtbl_t pt, uint64 va, uint64 pa,uint64 len,int perm){
    assert(va < MAXVA ,"va out of range");
    assert((pa !=0) | (pa % PGSIZE !=0),"pa need be aligned");
    pte_t *pte;
    uint64 begin =  PGROUNDDOWN(va);
    uint64 end   =  PGROUNDDOWN(va+len-1);
    uint64 current = begin;
    for(;;){
        //查找或分配页表项
        if((pte = vmem_walk(pt,current,true)) == NULL){
            assert(0,"pte allock error");
            return -1;
        }
        if(*pte & PTE_V) {
            assert(0,"pte remap!");
            return -1;
        }
        //给页表项写上控制位，置有效
        *pte = PA2PTE(pa) | perm | PTE_V;
        
        /// @todo : 刷新TLB
        if(current == end) break;
        current += PGSIZE;
        pa +=PGSIZE;
    }
    return 1;   
}


/*
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
	uint64 a, last;
	pte_t *pte;

	a = PGROUNDDOWN(va);
	last = PGROUNDDOWN(va + size - 1);
	for (;;) {
		if ((pte = walk(pagetable, a, 1)) == 0) {
			errorf("pte invalid, va = %p", a);
			return -1;
		}
		if (*pte & PTE_V) {
			errorf("remap");
			return -1;
		}
		*pte = PA2PTE(pa) | perm | PTE_V;
		if (a == last)
			break;
		a += PGSIZE;
		pa += PGSIZE;
	}
	return 0;
}
*/