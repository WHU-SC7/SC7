#include "types.h"
#include "print.h"
#include "hsai_mem.h"
#include "string.h"
#include "pmem.h"
#include <stdbool.h>

typedef struct listnode{
    struct listnode* next;
}listnode_t;

static bool page_free[PAGE_NUM];  //新增内存位图，防止Double Free 为1表示页可分配

struct mem_struct{
    listnode_t freelist;
}mem;

//暂定内存空间为64M   16K 页面 * 4K
/*
rv:
kernel_mem_start = 0x0000000080039000
kernel_mem_end  = 0x0000000084039000

la:
kernel_mem_start = 0x900000009003b000
kernel_mem_end  = 0x900000009103b000
*/
uint64 _mem_start,_mem_end ;
void pmem_init(){
    uint64 pa; 
    listnode_t* node;
    mem.freelist.next = NULL; 
    
    _mem_start = PAGE_UP(hsai_get_mem_start());
    memset(page_free, 1, sizeof(page_free));
    _mem_end = _mem_start + PAGE_NUM  * PAGE_SIZE;
    for(pa = _mem_start ; pa < _mem_end ; pa+=PAGE_SIZE){
        node = (listnode_t*) pa;
        node->next = mem.freelist.next;
        mem.freelist.next = node;
    }
    printf("kernel_mem_start = %p\n",_mem_start);
    printf("kernel_mem_end  = %p\n",_mem_end);
    LOG("pmem init success\n");
}

/*
    @param npages :分配页面数，暂定为1
    @return   :返回分配页面的地址
*/
void* pmem_alloc_pages(int npages){
    assert(npages == 1, "alloc page num should be 1\n");
    listnode_t* node;


    node = mem.freelist.next;
    if(node) mem.freelist.next = node->next;
    else {
        assert(0,"mem error");
        mem.freelist.next = NULL;
    }
    if (node) {
        uint64 page_idx = ((uint64)node - _mem_start) / PAGE_SIZE;
        assert(page_idx <PAGE_NUM, "pmem_alloc_pages: page_idx out of range\n");
        page_free[page_idx] = false;  
    }
    memset(node, 0, PAGE_SIZE);             //只有分配时清空node才页面才为空，原因未知

    return (void*)node;
};

/*
    @param ptr : 从ptr指向的地址开始释放，必须保证页对齐
    @param npages : 释放的页面数 暂定为1

*/
void pmem_free_pages(void* ptr, int npages){
    assert(ptr, "ptr is null\n");
    assert(npages == 1, "free page num should be 1\n");
    assert((uint64)ptr % PAGE_SIZE == 0, "ptr align error\n");  //如果未对齐报错

    // 计算页号（ptr到页号的映射）
    uint64 page_idx = ((uint64)ptr - _mem_start) / PAGE_SIZE;
    assert(page_idx < PAGE_NUM, "pmem_free_pages: page_idx out of range\n");
    // 检查是否已释放,如果页不空闲则panic
    if (page_free[page_idx]) {
        assert(0,"Double free detected");
    }

    page_free[page_idx] = true;

    memset(ptr, 0, PAGE_SIZE);
    listnode_t* node = (listnode_t*)ptr;
    assert((uint64)ptr >= _mem_start && (uint64)ptr <=_mem_end,"Memory Access Out of Bounds\n");
    node->next = mem.freelist.next;
    mem.freelist.next = node;

};