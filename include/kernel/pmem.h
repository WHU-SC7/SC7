#ifndef __PMEM_H__
#define __PMEM_H__


// 对外函数
void pmem_init();
void* pmem_alloc_pages(int npages);
void  pmem_free_pages(void* ptr, int npages);

#endif