#include "pmem.h"
#include "hsai_mem.h"
#include "print.h"
#include "string.h"
#include "types.h"
#include <stdbool.h>
#if defined RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

/**
 * @brief 物理内存链表节点
 * 存放指向下一个节点的指针，本身的地址作为分配的物理内存地址
 */
typedef struct listnode
{
    struct listnode *next;
} listnode_t;

/**
 * @brief  内存空闲位图
 * 防止Double Free
 * 为1表示页可分配
 */
static bool page_free[PAGE_NUM];
/**
 * @brief 物理内存空闲链表
 *
 */
struct mem_struct
{
    listnode_t freelist;
} mem;

uint64 _mem_start, _mem_end;
void pmem_init()
{
    uint64 pa;
    listnode_t *node;
    mem.freelist.next = NULL;

    _mem_start = PGROUNDUP(hsai_get_mem_start());
    memset(page_free, 1, sizeof(page_free));
    _mem_end = _mem_start + PAGE_NUM * PGSIZE; ///< 获取物理内存的结束地址，当前为 16K *4K = 64K
    /* 头插法初始化内存链表，倒着循环让freelist按地址升序排列*/
    for (pa = _mem_end - PGSIZE; pa >= _mem_start; pa -= PGSIZE)
    {
        node = (listnode_t *)pa;
        node->next = mem.freelist.next;
        mem.freelist.next = node;
    }
    printf("kernel_mem_start = %p\n", _mem_start);
    printf("kernel_mem_end  = %p\n", _mem_end);
    LOG("pmem init success\n");
}

/**
 * @brief  分配一个物理页
 *
 * @param npages 分配的页面数量，目前只支持1页
 * @return void* 分配成功返回物理页地址，失败返回NULL
 */
void *pmem_alloc_pages(int npages)
{
    assert(npages == 1, "alloc page num should be 1\n");
    listnode_t *node;

    node = mem.freelist.next;
    if (node)
        mem.freelist.next = node->next;
    else
    {
        panic("Error mem alloc");
        mem.freelist.next = NULL;
    }
    if (node)
    {
        uint64 page_idx = ((uint64)node - _mem_start) / PGSIZE;
        assert(page_idx < PAGE_NUM,
               "pmem_alloc_pages: page_idx out of range\n");
        page_free[page_idx] = false; ///< 在内存位图中标记为已分配
    }
    memset(node, 0, PGSIZE); ///< 由于node中默认存储了next指针，分配时需要清空

    return (void *)node;
};

/**
 * @brief 释放指针指向的页面
 *
 * @param ptr 释放页面的首地址，需要对齐
 * @param npages  释放页的数量，目前只支持1页
 */
void pmem_free_pages(void *ptr, int npages)
{
    assert(ptr, "ptr is null\n");
    assert(npages == 1, "free page num should be 1\n");
    assert(((uint64)ptr != 0) || (uint64)ptr % PGSIZE == 0, "ptr align error\n");

    uint64 page_idx = ((uint64)ptr - _mem_start) / PGSIZE;
    assert(page_idx < PAGE_NUM, "pmem_free_pages: page_idx out of range\n");
    /* 防止释放已经释放的页面*/
    if (page_free[page_idx])
    {
        panic("Double free detected");
    }

    page_free[page_idx] = true;

    memset(ptr, 0, PGSIZE);
    listnode_t *node = (listnode_t *)ptr;
    assert((uint64)ptr >= _mem_start && (uint64)ptr <= _mem_end, "Memory Access Out of Bounds\n");
    node->next = mem.freelist.next;
    mem.freelist.next = node;
};

/**
 * @brief 分配一页物理页
 * 
 * @return void* 
 */
void *
kalloc(void)
{
    void *ptr = pmem_alloc_pages(1);
    if (ptr == NULL)
    {
        panic("kmalloc failed");
    }
    return ptr;
}
/**
 * @brief 释放ptr指向的一页物理页
 * 
 * @param ptr 
 */
void
kfree(void *ptr)
{
    pmem_free_pages(ptr, 1);
}

/**
 * TODO: 动态内存分配
 */
void *
kmalloc(uint64 size) 
{
    return kalloc();
}

void *
kcalloc(uint n, uint64 size) 
{
    return kalloc();
}