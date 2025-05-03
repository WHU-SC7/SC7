#include "print.h"
#include "types.h"
#include "test.h"
#include "pmem.h"
#include "vmem.h"
#include "string.h"
#include "spinlock.h"
#include "cpu.h"
#if defined RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

#include "ext4_oflags.h"
#include "file.h"
#include "vfs_ext4_ext.h"

/** ================ 测试辅助函数==============  */
/** 测试打印格式化输出 */
#define TEST_PRINT(title, fmt, ...)           \
    do                                        \
    {                                         \
        print_line("\n--- " #title " ---\n"); \
        printf(fmt, ##__VA_ARGS__);           \
    } while (0)

/** ======== 1. 测试printf ============ */
void test_print()
{
    // 测试十进制整数
    TEST_PRINT(Decimal Basic, "%d %d %d", 123, -456, 0);             // 预期: 123 -456 0
    TEST_PRINT(Decimal Long, "%ld", (int64)2147483648L);             // 预期: 2147483648（测试32位溢出）
    TEST_PRINT(Decimal LongLong, "%lld", (int64)-123456789012345LL); // 预期: -123456789012345

    // 测试十六进制
    TEST_PRINT(Hex Basic, "%x %x", 255, (uint32)0xDeadBeef);        // 预期: ff deadbeef
    TEST_PRINT(Hex Long, "%lx", (uint64)0xCAFE1234);                // 预期: cafe1234
    TEST_PRINT(Hex LongLong, "%llx", (uint64)0x123456789ABCDEF0LL); // 预期: 123456789abcdef0

    // 测试指针
    void *p = (void *)0x12345678;
    TEST_PRINT(Pointer Normal, "%p", p);  // 预期: 0x0000000012345678（格式依赖实现）
    TEST_PRINT(Pointer Null, "%p", NULL); // 预期: 0x0000000000000000

    // 测试字符串
    TEST_PRINT(String Normal, "%s", "Hello");      // 预期: Hello
    TEST_PRINT(String Null, "%s", (char *)0);      // 预期: (null)
    TEST_PRINT(String Mixed, "A%sb%sc", "1", "2"); // 预期: A1b2c

    // 测试无符号整数
    TEST_PRINT(Unsigned Basic, "%u %u", 123, (unsigned)-1);            // 预期: 123 4294967295
    TEST_PRINT(Unsigned Long, "%lu", (uint64)4294967296UL);            // 预期: 4294967296
    TEST_PRINT(Unsigned LongLong, "%llu", (uint64)123456789012345ULL); // 预期: 123456789012345

    // 测试特殊格式
    TEST_PRINT(Percent Sign, "%%");                     // 预期: %
    TEST_PRINT(Mixed Format, "a%%b%d%sc", 123, "test"); // 预期: a%b123testc

    // 测试异常格式
    TEST_PRINT(Unknown Format, "%a %lz"); // 预期: %a %lz
    TEST_PRINT(Partial Format, "x%");     // 预期: x%
    TEST_PRINT(Long Partial, "%l");       // 预期: %l
    consputc('\n');
}

/** 测试 assert */
void test_assert()
{
    int x = -1;
    int *y = &x;
    assert(x > 0,
           "x should be positive, got %d, ptr_16 is %x, ptr is %p",
           x, y, y);
    // 输出：panic: x should be positive, got -1
    // 然后进入死循环
}

void test_pmem()
{
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
    for (int i = 0; i < 3; i++)
    {
        pages[i] = pmem_alloc_pages(1);
        assert(pages[i] != NULL, "测试2-分配失败");
        printf("[测试2] 第%d次分配地址: %p\n", i + 1, pages[i]);
    }

    // 验证释放后能重复使用
    pmem_free_pages(pages[1], 1);
    pmem_free_pages(pages[2], 1);
    pmem_free_pages(pages[1], 1);
    void *reused = pmem_alloc_pages(1);
    assert(reused == pages[1], "测试2-内存重用失败");
    printf("[测试2] 地址%p重用成功\n", reused);

    // 测试3: 内存清零验证
    uint8 *test_buf = (uint8 *)pmem_alloc_pages(1);
    memset(test_buf, 0xAA, PGSIZE);
    // 释放内存（pmem_free_pages 内部会清零）
    pmem_free_pages(test_buf, 1);
    // 重新分配内存（可能复用 test_buf 的内存）
    uint8 *zero_buf = (uint8 *)pmem_alloc_pages(1);
    // 验证新分配的内存是否已被清零
    uint8 expected_zero[PGSIZE] = {};
    assert(memcmp(zero_buf, expected_zero, PGSIZE) == 0, "测试3-内存未清零");
    printf("[测试3] 内存清零验证通过\n");
    pmem_free_pages(zero_buf, 1);

    // 测试4: 边界条件测试
    // 4.1 尝试释放非法地址
    // void *invalid_ptr = (void*)((uint64)pages[0] + 1);
    // pmem_free_pages(invalid_ptr, 1); // 应触发断言

    // 4.2 耗尽内存测试
    //  int max_pages = 2;
    //  void *exhausted[max_pages];
    //  for(int i=0; i < max_pages; i++) {
    //      exhausted[i] = pmem_alloc_pages(1);
    //      if(exhausted[i] == NULL) assert(1,"mem");
    //      //assert(exhausted[i]!=NULL, "测试4-内存耗尽过早失败");
    //  }
    // assert(pmem_alloc_pages(1) == NULL, "测试4-内存未耗尽");

    for (int i = 0; i < 3; i++)
    {
        pages[i] = pmem_alloc_pages(1);
        // assert(pages[i] != NULL, "测试2-分配失败");
        printf("[测试2] 第%d次分配地址: %p\n", i + 1, pages[i]);
    }
    printf("[测试4] 边界条件测试通过\n");

    printf("======= 所有测试通过 =======\n");
}

//----------------------test--------------------------------------------
// 测试辅助函数，用于比较PTE的内容
extern pgtbl_t kernel_pagetable;
static int check_pte(pgtbl_t pt, uint64 va, uint64 expected_pa, int expected_perm)
{
    pte_t *pte = walk(pt, va, 0);
    if (pte == NULL)
    {
        printf("PTE for 0x%lx not found\n", va);
        return -1;
    }
    if (!(*pte & PTE_V))
    {
        printf("PTE for 0x%lx not valid\n", va);
        return -2;
    }

    uint64 pte_pa = PTE2PA(*pte);
    int pte_perm = PTE_FLAGS(*pte);

    if (pte_pa != expected_pa)
    {
        printf("PTE PA mismatch: 0x%p vs 0x%p\n", pte_pa, expected_pa);
        return -3;
    }
    if ((pte_perm & expected_perm) != expected_perm)
    {
        printf("PTE perm mismatch: 0x%p vs 0x%p\n", pte_perm, expected_perm);
        return -4;
    }
    return 0;
}

// 测试walk基本功能
void test_walk_basic()
{
    printf("=== Running walk tests ===\n");

    // 创建测试页表
    pgtbl_t pt = pmem_alloc_pages(1);
    memset(pt, 0, PGSIZE);

    // 测试1：未分配时返回NULL
    pte_t *pte = walk(pt, 0x1000, 0);
    assert(pte == NULL, "Test1: Unallocated walk should return NULL");

    // 测试2：启用alloc后应分配页表
    pte = walk(pt, 0x2000, 1);
    assert(pte != NULL, "Test2: Failed to allocate page tables");
    assert(*pte == 0, "Test2: PTE should be initially zero");

    printf("walk basic tests passed!\n\n");
}

// 测试单页映射
void test_single_page_mapping()
{
    printf("=== Running single page mapping tests ===\n");

    pgtbl_t pt = pmem_alloc_pages(1);
    memset(pt, 0, PGSIZE);

    // 测试合法映射
    uint64 va = 0x3000;
    uint64 pa = 0x4000;
    mappages(pt, va, pa, PGSIZE, PTE_R | PTE_W);

    // 验证PTE内容
    int ret = check_pte(pt, va, pa, PTE_R | PTE_W | PTE_V);
    assert(ret == 0, "Test1: Single page mapping failed");

    // 测试相邻地址未映射
    ret = check_pte(pt, va + PGSIZE, 0, 0);
    assert(ret != 0, "Test2: Adjacent address should not be mapped");

    printf("Single page mapping tests passed!\n\n");
}

// 测试多页跨越映射
void test_multi_page_mapping()
{
    printf("=== Running multi-page mapping tests ===\n");

    pgtbl_t pt = pmem_alloc_pages(1);
    memset(pt, 0, PGSIZE);

    // 映射3个页面
    uint64 va = 0x5000;
    uint64 pa = 0x6000;
    uint64 len = 3 * PGSIZE - 100; // 故意使用非对齐长度
    mappages(pt, va, pa, len, PTE_R);

    // 验证三个页面映射
    for (int i = 0; i < 3; i++)
    {
        uint64 curr_va = va + i * PGSIZE;
        uint64 curr_pa = pa + i * PGSIZE;
        int ret = check_pte(pt, curr_va, curr_pa, PTE_R | PTE_V);
        assert(ret == 0, "Test1: Page %d mapping failed", i);
    }

    // 验证第四个页面未映射
    int ret = check_pte(pt, va + 3 * PGSIZE, 0, 0);
    assert(ret != 0, "Test2: Extra page should not be mapped");

    printf("Multi-page mapping tests passed!\n\n");
}
void test_walkaddr_valid() {
    printf("=== Testing walkaddr valid translation ===\n");
    pgtbl_t pt = pmem_alloc_pages(1);
    memset(pt, 0, PGSIZE);

    uint64 va = 0x3000;
    uint64 pa = 0x4000;
    mappages(pt, va, pa, PGSIZE, PTE_R | PTE_W | PTE_U);

    uint64 result = walkaddr(pt, va);
    assert(result == pa, "Test1: walkaddr failed to translate valid VA to PA");
    printf("Test1 passed: VA 0x%lx -> PA 0x%lx\n", va, result);

    pmem_free_pages(pt, 1);
}


void test_walkaddr_permission() {
    printf("=== Testing walkaddr permission check (PTE_U) ===\n");
    pgtbl_t pt = pmem_alloc_pages(1);
    memset(pt, 0, PGSIZE);

    uint64 va = 0x5000;
    uint64 pa = 0x6000;
    mappages(pt, va, pa, PGSIZE, PTE_R | PTE_W); // 未设置PTE_U

    uint64 result = walkaddr(pt, va);
    assert(result == 0, "Test3: walkaddr should return 0 when PTE_U is not set");
    printf("Test3 passed: walkaddr blocked access without PTE_U\n");

    pmem_free_pages(pt, 1);
}

void test_vmunmap_keep_memory() {
    printf("=== Testing vmunmap with do_free=0 ===\n");
    pgtbl_t pt = pmem_alloc_pages(1);
    memset(pt, 0, PGSIZE);

    uint64 va = 0x7000;
    uint64 pa = 0x8000;
    mappages(pt, va, pa, PGSIZE, PTE_R | PTE_U);

    // 解除映射但不释放内存
    vmunmap(pt, va, 1, 0);

    // 检查PTE是否清除
    pte_t *pte = walk(pt, va, 0);
    assert(pte != NULL && (*pte & PTE_V) == 0, "Test1: PTE not cleared");

    // 物理内存应保留，可重新映射
    mappages(pt, va, pa, PGSIZE, PTE_R | PTE_U);
    assert(check_pte(pt, va, pa, PTE_R | PTE_U | PTE_V) == 0, "Test1: Remapping failed");
    printf("Test1 passed: Memory retained after vmunmap\n");

    pmem_free_pages(pt, 1);
}
void test_vmunmap_free_memory() {
    printf("=== Testing vmunmap with do_free=1 ===\n");
    pgtbl_t pt = pmem_alloc_pages(1);
    memset(pt, 0, PGSIZE);

    uint64 va = 0x9000;
    uint64 pa = (uint64)pmem_alloc_pages(1); // 动态分配物理页
    mappages(pt, va, pa, PGSIZE, PTE_R | PTE_U);

    vmunmap(pt, va, 1, 1);

    // 尝试访问释放的内存应失败
    pte_t *pte = walk(pt, va, 0);
    assert(pte != NULL && (*pte & PTE_V) == 0, "Test2: PTE not cleared");

    // 尝试重新分配同一物理地址（应成功）
    uint64 new_pa = (uint64)pmem_alloc_pages(1);
    assert(new_pa == pa, "Test2: Freed physical page not reused");
    printf("Test2 passed: Physical memory freed\n");

    pmem_free_pages(pt, 1);
}
// // 测试 uvmcreate 基本功能
// void test_uvmcreate_basic() {
//     printf("=== Testing uvmcreate basic functionality ===\n");
//     pgtbl_t pt = uvmcreate();
//     assert(pt != NULL, "Test1: Failed to create user page table");
//     assert(((uint64)pt % PGSIZE) == 0, "Test1: Page table not aligned");
//     pmem_free_pages(pt, 1); // 确保释放避免内存泄漏
//     printf("Test1 passed: Page table creation succeeded\n");
// }

// 测试 uvminit 单页映射
// void test_uvminit_single_page() {
//     printf("=== Testing uvminit single page ===\n");
//     pgtbl_t pt = uvmcreate();
//     uchar src[PGSIZE];
//     memset(src, 0xAA, PGSIZE); // 填充测试模式
    
//     uvminit(pt, src, PGSIZE);
    
//     // 验证第一个页的映射
//     uint64 pa = walkaddr(pt, 0);
//     assert(pa != 0, "Test1: No physical page mapped");
//     assert(memcmp((void*)(pa | dmwin_win0) , src, PGSIZE) == 0, "Test1: Data copy failed");
    
//     pmem_free_pages(pt, 1);
//     printf("Test1 passed: Single page initialization correct\n");
// }

// 测试 uvminit 部分页映射
// void test_uvminit_partial_page() {
//     printf("=== Testing uvminit partial page ===\n");
//     pgtbl_t pt = uvmcreate();
//     uint sz = 500;
//     uchar src[sz];
//     memset(src, 0xBB, sz);
    
//     uvminit(pt, src, sz);
    
//     // 验证部分页的映射
//     uint64 pa = walkaddr(pt, 0);
//     assert(pa != 0, "Test1: Physical page not mapped");
//     assert(memcmp((void*)(pa | dmwin_win0), src, sz) == 0, "Test1: Partial data mismatch");
    
//     pmem_free_pages(pt, 1);
//     printf("Test1 passed: Partial page initialization correct\n");
// }

// 测试 copyin 基础功能
// void test_copyin_basic() {
//     printf("=== Testing copyin basic ===\n");
//     pgtbl_t pt = uvmcreate();
//     uchar src[PGSIZE];
//     uchar dst[PGSIZE];
//     memset(src, 0xCC, PGSIZE);
    
//     // 设置映射
//     uvminit(pt, src, PGSIZE);
    
//     // 执行复制
//     int ret = copyin(pt, (char*)dst, 0, PGSIZE);
//     assert(ret == 0, "Test1: Copyin failed");
//     assert(memcmp((char*)dst, src, PGSIZE) == 0, "Test1: Data mismatch");
    
//     pmem_free_pages(pt, 1);
//     printf("Test1 passed: Basic copyin succeeded\n");
// }

// 测试 copyin 跨页访问
// void test_copyin_cross_page() {
//     printf("=== Testing copyin cross-page ===\n");
//     pgtbl_t pt = uvmcreate();
//     uint sz = 2*PGSIZE;
//     uchar src[sz];
//     uchar dst[sz];
//     memset(src, 0xDD, sz);
    
//     // 设置两页映射
//     uvminit(pt, src, sz);
    
//     // 从中间位置复制跨越两页
//     int ret = copyin(pt, (char*)dst, PGSIZE-100, 200);
//     assert(ret == 0, "Test1: Cross-page copy failed");
//     assert(memcmp(dst, src+(PGSIZE-100), 200) == 0, "Test1: Cross-page data mismatch");
    
//     pmem_free_pages(pt, 1);
//     printf("Test1 passed: Cross-page copyin succeeded\n");
// }

// 测试 copyout 基础功能
// void test_copyout_basic() {
//     printf("=== Testing copyout basic ===\n");
//     pgtbl_t pt = uvmcreate();
//     uchar src[PGSIZE];
//     uchar dst[PGSIZE] = {0};
//     memset(src, 0xEE, PGSIZE);
    
//     // 分配目标物理页
//     uvminit(pt, dst, PGSIZE);
    
//     // 执行复制
//     int ret = copyout(pt, 0, (char*)src, PGSIZE);
//     assert(ret == 0, "Test1: Copyout failed");
    
//     // 验证数据
//     uint64 pa = walkaddr(pt, 0);
//     assert(memcmp((void*)(pa|dmwin_win0), src, PGSIZE) == 0, "Test1: Copyout data mismatch");
    
//     pmem_free_pages(pt, 1);
//     printf("Test1 passed: Basic copyout succeeded\n");
// }


// 主测试函数
void vmem_test()
{
    printf("Starting virtual memory tests...\n");

    // test_walk_basic();
    // test_single_page_mapping();
    // test_multi_page_mapping();

    // // 新增测试 walkaddr unmap
    // test_walkaddr_valid();
    // test_walkaddr_permission();
    // test_vmunmap_keep_memory();
    // test_vmunmap_free_memory();


    // 新增测试 copyin copyout 
    // test_uvmcreate_basic();
    // //test_uvminit_single_page();
    // test_uvminit_partial_page();
    // test_copyin_basic();
    // test_copyin_cross_page();
    //test_copyout_basic();

    printf("All virtual memory tests passed!\n");
}

/**============================= 测试自旋锁================================== */
/** 定义一个共享变量，用于测试互斥性 */
static int shared_counter = 0;
static spinlock_t test_lock;

/**
 * @brief 测试用的线程函数（模拟多 CPU 或多线程）
 */
void test_thread(int thread_id)
{
    int i;
    for (i = 0; i < 10000; i++)
    {
        acquire(&test_lock); ///< 获取锁
        int temp = shared_counter;
        temp++; ///< 对共享变量进行操作
        shared_counter = temp;
        release(&test_lock); ///< 释放锁
    }
    printf("Thread %d finished, counter = %d\n", thread_id, shared_counter);
}

/**
 * @brief 测试 push_off 和 pop_off 的嵌套功能
 */
void test_interrupt_nesting(void)
{
    printf("Testing interrupt nesting...\n");

    int old_intr = intr_get();
    printf("Initial interrupt state: %d\n", old_intr);

    push_off(); ///< 第一次关闭中断
    printf("After 1st push_off, noff = %d\n", mycpu()->noff);

    push_off(); ///< 第二次关闭中断
    printf("After 2nd push_off, noff = %d\n", mycpu()->noff);

    pop_off(); ///< 第一次弹出
    printf("After 1st pop_off, noff = %d\n", mycpu()->noff);

    pop_off(); ///< 第二次弹出
    printf("After 2nd pop_off, noff = %d, intr = %d\n", mycpu()->noff, intr_get());
}

/**
 * @brief 主测试自旋锁的函数
 */
void test_spinlock(void)
{
    /* 初始化锁 */
    initlock(&test_lock, "test_lock");
    printf("Spinlock initialized: locked = %d, cpu = %p\n", test_lock.locked, test_lock.cpu);

    /* 测试单线程锁功能 */
    printf("Testing single-thread locking...\n");
    acquire(&test_lock);
    printf("Lock acquired, holding = %d\n", holding(&test_lock));
    release(&test_lock);
    printf("Lock released, holding = %d\n", holding(&test_lock));

    /* 测试中断嵌套 */
    test_interrupt_nesting();

    /* 测试多线程互斥性（假设有多个 CPU 或线程）*/
    printf("Testing multi-thread locking...\n");
    shared_counter = 0;

    /* 模拟两个线程（需要你的环境支持多线程或多 CPU）*/
    test_thread(1); ///< 线程 1
    test_thread(2); ///< 线程 2

    /* 检查最终结果 */
    printf("Final shared_counter = %d (expected 20000)\n", shared_counter);
}

/* =================== 文件系统测试工具集 =================== */
// 共用工具函数
void print_file_content(const char *path) {
    struct file *f = filealloc();
    if (!f) {
        printf("文件分配失败: %s\n", path);
        return;
    }

    strcpy(f->f_path, path);
    f->f_flags = O_RDONLY;
    f->f_type = FD_REG;

    // 打开文件
    if (vfs_ext_openat(f) < 0) {
        printf("无法打开文件: %s\n", path);
        get_fops()->close(f);
        return;
    }

    // 读取内容
    char buffer[512] = {0};
    int bytes = get_fops()->read(f, (uint64)buffer, sizeof(buffer)-1);
    
    if (bytes > 0) {
        printf("▬▬▬▬▬ [文件内容] %s ▬▬▬▬▬\n", path);
        printf("内容 %s\n", buffer);
        printf("▬▬▬▬▬ 共 %d 字节 ▬▬▬▬▬\n", bytes);
    } else {
        printf("空文件或读取失败: %s\n", path);
    }

    get_fops()->close(f);
}

// 创建并写入新文件
int create_file(const char *path, const char *content, int flags) {
    struct file *f = filealloc();
    if (!f) return -1;

    strcpy(f->f_path, path);
    f->f_flags = flags | O_WRONLY | O_CREAT;
    f->f_type = FD_REG;

    // 创建文件
    int ret = vfs_ext_openat(f);
    if (ret < 0) {
        printf("创建失败: %s (错误码: %d)\n", path, ret);
        get_fops()->close(f);
        return ret;
    }

    // 写入数据
    int len = strlen(content);
    int written = get_fops()->write(f, (uint64)content, len);
    
    // 提交并关闭
    get_fops()->close(f);

    if (written != len) {
        printf("写入不完全: %d/%d 字节\n", written, len);
        return -2;
    }
    return 0;
}

#define LS_BUF_SIZE 4096*4 //< 缓冲区大小
char ls_buf[LS_BUF_SIZE];
// struct linux_dirent64 {
//     uint64 d_ino; //0
//     int64 d_off;  //8
//     unsigned short d_reclen; //16
//     unsigned char d_type;    //18
//     char d_name[0];          //19
// };

//< d_type的取值:
#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device
#define T_CHR     4   // 字符设备
#define T_BLK     5
#define T_UNKNOWN 6
/*
输出格式
-------------------------
index  inode  type  name
-------------------------
struct linux_dirent64的成员中:
    d_ino表示inode号
    d_off表示index，即遍历的顺序
    d_reclen表示当前项的长度
    d_type表示文件类型
    d_name是文件名
*/
void printf_ls_buf(struct linux_dirent64 *buf)
{
    struct linux_dirent64 *data =buf;
    printf("index\tinode\ttype\tname\t\n");
    while(data->d_off!=0) //< 检查不严谨，但是考虑到每次list_file会清空ls_buf为0,这样是可以的
    {
        //printf("%d\t%d\t%d\t%s\n",data->d_off,data->d_ino,data->d_type,data->d_name);
        printf("%d\t",data->d_off);
        printf("%d\t",data->d_ino);
        switch (data->d_type)
        {
        case T_DIR: //< 目录，蓝色
            PRINT_COLOR(BLUE_COLOR_PRINT,"DIR\t");
            PRINT_COLOR(BLUE_COLOR_PRINT,"%s\t",data->d_name);
            break;
        case T_FILE: //< 普通文件，白色
            printf("FILE\t");
            printf("%s\t",data->d_name);
            break;
        case T_DEVICE: //< 设备，目前不知道什么文件是这个，用红色
            PRINT_COLOR(RED_COLOR_PRINT,"CHA\t");
            PRINT_COLOR(RED_COLOR_PRINT,"%s\t",data->d_name);
            break;
        case T_CHR: //< 字符设备，如console，黄色 
            PRINT_COLOR(YELLOW_COLOR_PRINT,"CHA\t");
            PRINT_COLOR(YELLOW_COLOR_PRINT,"%s\t",data->d_name);
            break;
        case T_BLK: //< 块设备，黄色 
            PRINT_COLOR(YELLOW_COLOR_PRINT,"BLK\t");
            PRINT_COLOR(YELLOW_COLOR_PRINT,"%s\t",data->d_name);
            break;
        default: //< 默认，白色
            printf("%d\t",data->d_type);
            printf("%s\t",data->d_name);
            break;
        }
        
        printf("\n");
        // char *s=(char*)data; //<调试时逐个字节显示
        // for(int i=0;i<data->d_reclen;i++)
        // {
        //     printf("%d ",*s++);
        // }
        // printf("\n");
        data=(struct linux_dirent64 *)((char *)data+data->d_reclen); //< 遍历
    }
}

void list_file(const char *path)
{
    printf("------------------------------\n");
    printf("正在显示该目录: %s\n",path);
    struct file *f = filealloc();
    if (!f) {
        printf("文件分配失败: %s\n", path);
        return;
    }

    strcpy(f->f_path, path);
    f->f_flags = O_RDONLY| O_CREAT | O_RDWR; //< 我不清楚有什么作用，可能要改
    f->f_type = FD_REG;

    // 打开文件
    int ret;
    if ((ret=vfs_ext_openat(f)) < 0) {
        //printf("vfs_ext_openat返回值: %d",ret);
        printf("无法打开文件: %s\n", path);
        get_fops()->close(f);
        return;
    }

    memset((void *)ls_buf,0,LS_BUF_SIZE);
    int count =vfs_ext_getdents(f,(struct linux_dirent64 *)ls_buf,4096); //< 遍历目录，输出内容到缓冲区ls_buf
    printf("count: %d\n",count);
    printf_ls_buf((struct linux_dirent64 *)ls_buf); //< 格式化输出缓冲区中的内容

    // char*s=(char *)ls_buf; //<调试时显示内容
    // for(int i=0;i<1024;i++)
    // {
    //     printf("%d ",*s++);
    // }
}

/* =================== 测试用例 =================== */
void test_fs(void) {
    const char *test_path = "/text2";
    const char *test_content = "I love Mujica.";

    // 阶段1：创建新文件
    printf("\n>>> 正在创建文件: %s\n", test_path);
    if (create_file(test_path, test_content, O_EXCL) == 0) {
        printf("✓ 文件创建成功\n");
    }

    // 阶段2：验证新文件
    printf("\n>>> 验证新文件内容\n");
    print_file_content(test_path);

    // 阶段3：保留原有测试（示例文件）
    printf("\n>>> 原始测试文件验证\n");
    print_file_content("/text");
}