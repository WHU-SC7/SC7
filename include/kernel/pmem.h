#ifndef __PMEM_H__
#define __PMEM_H__
#include "types.h"
#include "list.h"

// 伙伴系统相关定义
#define MAX_ORDER 11 // 最大阶数，支持2^11 = 2048个页面
#define BUDDY_MAX_ORDER 11

// 伙伴系统空闲链表节点
// 修改 buddy_node_t 结构
typedef struct buddy_node
{
    uint64 addr;           // 块起始地址
    int order;             // 阶数
    struct list_elem elem; // 链表元素
} buddy_node_t;
// 伙伴系统管理结构
typedef struct buddy_system
{
    uint64 mem_start;                            // 内存起始地址
    uint64 mem_end;                              // 内存结束地址
    uint64 total_pages;                          // 总页面数
    uint64 *bitmap;                              // 位图
    buddy_node_t *nodes;                         // 元数据节点数组
    struct list free_lists[BUDDY_MAX_ORDER + 1]; // 空闲链表数组
} buddy_system_t;
// 对外函数
void pmem_init();
void *pmem_alloc_pages(int npages);
void pmem_free_pages(void *ptr, int npages);
void *kmalloc(uint64 size);
void *kcalloc(uint n, uint64 size);
void kfree(void *ptr);
void *kalloc(void);

// 伙伴系统内部函数
int buddy_init(uint64 start, uint64 end);
void *buddy_alloc(int order);
void buddy_free(void *ptr, int order);
int get_order(uint64 size);
uint64 get_buddy_addr(uint64 addr, int order);
int is_buddy_free(uint64 addr, int order);
void set_buddy_used(uint64 addr, int order);
void set_buddy_free(uint64 addr, int order);
void buddy_check_integrity();
void buddy_diagnose_fork_issue();
void buddy_safe_check();
void buddy_cleanup_and_rebuild();
void buddy_free_tracked(void *ptr, int order, int caller_pid);

extern int debug_buddy;

#endif