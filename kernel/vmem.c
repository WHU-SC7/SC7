#include "vmem.h"
#include "pmem.h"
#include "print.h"
#include "hsai_mem.h"
#include "string.h"
#if defined RISCV
  #include "riscv.h"
  #define PT_LEVEL 3
  #define KERNEL_BASE 0x80000000ul
#else
  #include "loongarch.h"
  #define PT_LEVEL 4
  #define KERNEL_BASE 0x0000000090041000
#endif

pgtbl_t kernel_pagetable; // 内核页表
bool debug_trace_walk =false;

#define UART0 0x10000000L


#define TRAMPOLINE  (MAXVA - PGSIZE) 
extern char KERNEL_DATA;
extern char KERNEL_TEXT;
extern char trampoline; // in trampoline.S 
void  vmem_test();
void vmem_init()
{
    kernel_pagetable = pmem_alloc_pages(1); // 分配一个页,存放内核页表 分配时已清空页面
    memset(kernel_pagetable , 0, PGSIZE);
    LOG("kernel_pagetable address: %p\n",kernel_pagetable);
    //RISCV需要将内核映射到外部，LA由于映射窗口，不需要映射
    #if defined RISCV
        //UART映射
        vmem_mappages(kernel_pagetable, UART0 , UART0 , PGSIZE, PTE_R | PTE_W) ;
        // kernel代码区映射 映射为可读可执行
        vmem_mappages(kernel_pagetable, KERNEL_BASE, KERNEL_BASE, (uint64)&KERNEL_TEXT-KERNEL_BASE, PTE_R | PTE_X );
        LOG("[MAP] KERNEL_BASE:%p ->  KERNEL_TEXT:%p  len: 0x%x\n", KERNEL_BASE, (uint64)&KERNEL_TEXT,(uint64)&KERNEL_TEXT-KERNEL_BASE);
        // kernel数据区映射 映射为可读可写
        vmem_mappages(kernel_pagetable, (uint64)&KERNEL_TEXT, (uint64)&KERNEL_TEXT, (uint64)&KERNEL_DATA-(uint64)&KERNEL_TEXT, PTE_R | PTE_W );
        LOG("[MAP] KERNEL_TEXT :%p ->  KERNEL_DATA:%p  len: 0x%x\n", (uint64)&KERNEL_TEXT ,(uint64)&KERNEL_DATA,(uint64)&KERNEL_DATA-(uint64)&KERNEL_TEXT);
        // trampoline映射
        vmem_mappages(kernel_pagetable, TRAMPOLINE, (uint64)&trampoline, PGSIZE, PTE_R | PTE_X );
        LOG("[MAP] TRAMPOLINE :%p ->  trampoline:%p  len: 0x%x\n", TRAMPOLINE ,(uint64)&trampoline,PGSIZE);
    #endif

    //配置内核页表前需要正确映射代码区域和数据区域
    hsai_config_pagetable(kernel_pagetable); 
    printf("config kernel page success\n");
    printf("Begin vmem_test\n");
    vmem_test();
    
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
    for(int level = PT_LEVEL - 1; level > 0; level--){
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



//----------------------test--------------------------------------------
// 测试辅助函数，用于比较PTE的内容
static int check_pte(pgtbl_t pt, uint64 va, uint64 expected_pa, int expected_perm) {
    pte_t *pte = vmem_walk(pt, va, 0);
    if (pte == NULL) {
        printf("PTE for 0x%lx not found\n", va);
        return -1;
    }
    if (!(*pte & PTE_V)) {
        printf("PTE for 0x%lx not valid\n", va);
        return -2;
    }
    
    uint64 pte_pa = PTE2PA(*pte);
    int pte_perm = *pte & ~PTE_V;
    
    if (pte_pa != expected_pa) {
        printf("PTE PA mismatch: 0x%p vs 0x%p\n", pte_pa, expected_pa);
        return -3;
    }
    if ((pte_perm & expected_perm) != expected_perm) {
        printf("PTE perm mismatch: 0x%p vs 0x%p\n", pte_perm, expected_perm);
        return -4;
    }
    return 0;
}

// 测试vmem_walk基本功能
void test_vmem_walk_basic() {
    printf("=== Running vmem_walk tests ===\n");
    
    // 创建测试页表
    pgtbl_t pt = pmem_alloc_pages(1);
    memset(pt, 0, PGSIZE);

    // 测试1：未分配时返回NULL
    pte_t *pte = vmem_walk(pt, 0x1000, 0);
    assert(pte == NULL, "Test1: Unallocated walk should return NULL");

    // 测试2：启用alloc后应分配页表
    pte = vmem_walk(pt, 0x2000, 1);
    assert(pte != NULL, "Test2: Failed to allocate page tables");
    assert(*pte == 0, "Test2: PTE should be initially zero");

    printf("vmem_walk basic tests passed!\n\n");
}

// 测试单页映射
void test_single_page_mapping() {
    printf("=== Running single page mapping tests ===\n");
    
    pgtbl_t pt = pmem_alloc_pages(1);
    memset(pt, 0, PGSIZE);

    // 测试合法映射
    uint64 va = 0x3000;
    uint64 pa = 0x4000;
    vmem_mappages(pt, va, pa, PGSIZE, PTE_R | PTE_W);
    
    // 验证PTE内容
    int ret = check_pte(pt, va, pa, PTE_R | PTE_W | PTE_V);
    assert(ret == 0, "Test1: Single page mapping failed");

    // 测试相邻地址未映射
    ret = check_pte(pt, va + PGSIZE, 0, 0);
    assert(ret != 0, "Test2: Adjacent address should not be mapped");

    printf("Single page mapping tests passed!\n\n");
}

// 测试多页跨越映射
void test_multi_page_mapping() {
    printf("=== Running multi-page mapping tests ===\n");
    
    pgtbl_t pt = pmem_alloc_pages(1);
    memset(pt, 0, PGSIZE);

    // 映射3个页面
    uint64 va = 0x5000;
    uint64 pa = 0x6000;
    uint64 len = 3*PGSIZE - 100; // 故意使用非对齐长度
    vmem_mappages(pt, va, pa, len, PTE_R);

    // 验证三个页面映射
    for (int i = 0; i < 3; i++) {
        uint64 curr_va = va + i*PGSIZE;
        uint64 curr_pa = pa + i*PGSIZE;
        int ret = check_pte(pt, curr_va, curr_pa, PTE_R | PTE_V);
        assert(ret == 0, "Test1: Page %d mapping failed", i);
    }

    // 验证第四个页面未映射
    int ret = check_pte(pt, va + 3*PGSIZE, 0, 0);
    assert(ret != 0, "Test2: Extra page should not be mapped");

    printf("Multi-page mapping tests passed!\n\n");
}

// 测试内核页表初始化
void test_kernel_pagetable() {
    printf("=== Running kernel pagetable tests ===\n");
    
    // 初始化内核页表
    vmem_init();

// RISCV特定测试
#if defined(RISCV)
    // 验证UART映射
    int ret = check_pte(kernel_pagetable, UART0, UART0, PTE_R | PTE_W);
    assert(ret == 0, "UART mapping check failed");

    // 验证TRAMPOLINE映射
    uint64 trampoline_pa = (uint64)&trampoline;
    ret = check_pte(kernel_pagetable, TRAMPOLINE, trampoline_pa, PTE_R | PTE_X);
    assert(ret == 0, "Trampoline mapping check failed");

    // 验证内核代码段（示例检查第一个指令页）
    ret = check_pte(kernel_pagetable, KERNEL_BASE, KERNEL_BASE, PTE_R | PTE_X);
    assert(ret == 0, "Kernel code mapping check failed");

    // 验证内核数据段（示例检查数据段起始）
    uint64 data_va = (uint64)&KERNEL_TEXT;
    ret = check_pte(kernel_pagetable, data_va, data_va, PTE_R | PTE_W);
    assert(ret == 0, "Kernel data mapping check failed");
#endif

    printf("Kernel pagetable tests passed!\n\n");
}

// 主测试函数
void vmem_test() {
    printf("Starting virtual memory tests...\n");
    
    test_vmem_walk_basic();
    //test_single_page_mapping();
    test_multi_page_mapping();
    test_kernel_pagetable();

    printf("All virtual memory tests passed!\n");
}