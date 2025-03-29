#ifndef __PMEM_H__
#define __PMEM_H__

#define PAGE_NUM  16 * 1024  
#define PAGE_SIZE 4 * 1024

// 对外函数
void pmem_init();
void* pmem_alloc_pages(int npages);
void  pmem_free_pages(void* ptr, int npages);

#endif