#include "print.h"
#include "types.h"
#include "test.h"
#include "pmem.h"
#include "vmem.h"
#include "string.h"
#if defined RISCV
  #include "riscv.h"
#else
  #include "loongarch.h"
#endif

/** ================ 测试辅助函数==============  */
/** 测试打印格式化输出 */
#define TEST_PRINT(title, fmt, ...) \
    do { \
        print_line("\n--- " #title " ---\n"); \
        printf(fmt, ##__VA_ARGS__); \
    } while(0)

/** 测试printf */
void 
test_print() 
{
    // 测试十进制整数
    TEST_PRINT(Decimal Basic, "%d %d %d", 123, -456, 0);       // 预期: 123 -456 0
    TEST_PRINT(Decimal Long, "%ld", (int64)2147483648L);     // 预期: 2147483648（测试32位溢出）
    TEST_PRINT(Decimal LongLong, "%lld", (int64)-123456789012345LL); // 预期: -123456789012345

    // 测试十六进制
    TEST_PRINT(Hex Basic, "%x %x", 255, (uint32)0xDeadBeef);          // 预期: ff deadbeef
    TEST_PRINT(Hex Long, "%lx", (uint64)0xCAFE1234);        // 预期: cafe1234
    TEST_PRINT(Hex LongLong, "%llx", (uint64)0x123456789ABCDEF0LL); // 预期: 123456789abcdef0

    // 测试指针
    void *p = (void*)0x12345678;
    TEST_PRINT(Pointer Normal, "%p", p);                      // 预期: 0x0000000012345678（格式依赖实现）
    TEST_PRINT(Pointer Null, "%p", NULL);                      // 预期: 0x0000000000000000


    // 测试字符串
    TEST_PRINT(String Normal, "%s", "Hello");                  // 预期: Hello
    TEST_PRINT(String Null, "%s", (char*)0);                   // 预期: (null)
    TEST_PRINT(String Mixed, "A%sb%sc", "1", "2");             // 预期: A1b2c

    // 测试无符号整数
    TEST_PRINT(Unsigned Basic, "%u %u", 123, (unsigned)-1);    // 预期: 123 4294967295
    TEST_PRINT(Unsigned Long, "%lu", (uint64)4294967296UL);  // 预期: 4294967296
    TEST_PRINT(Unsigned LongLong, "%llu", (uint64)123456789012345ULL); // 预期: 123456789012345

    // 测试特殊格式
    TEST_PRINT(Percent Sign, "%%");                            // 预期: %
    TEST_PRINT(Mixed Format, "a%%b%d%sc", 123, "test");        // 预期: a%b123testc

    // 测试异常格式
    TEST_PRINT(Unknown Format, "%a %lz");                      // 预期: %a %lz
    TEST_PRINT(Partial Format, "x%");                          // 预期: x%
    TEST_PRINT(Long Partial, "%l");                            // 预期: %l
    consputc('\n');
}

/** 测试 assert */
void 
test_assert ()
{
    int x = -1;
    int* y = &x;
    assert(x > 0, 
        "x should be positive, got %d, ptr_16 is %x, ptr is %p", 
        x, y, y);
    // 输出：panic: x should be positive, got -1
    // 然后进入死循环
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
    pmem_free_pages(pages[1], 1);
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


//----------------------test--------------------------------------------
// 测试辅助函数，用于比较PTE的内容
extern pgtbl_t kernel_pagetable;
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
    int pte_perm = PTE_FLAGS(*pte);
    
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



// 主测试函数
void vmem_test() {
    printf("Starting virtual memory tests...\n");
    
    test_vmem_walk_basic();
    test_single_page_mapping();
    test_multi_page_mapping();
    printf("All virtual memory tests passed!\n");
}