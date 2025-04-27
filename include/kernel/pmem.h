#ifndef __PMEM_H__
#define __PMEM_H__
#include "types.h"
// 对外函数
void pmem_init();
void *pmem_alloc_pages(int npages);
void pmem_free_pages(void *ptr, int npages);
void *kmalloc(uint64 size);
void *kcalloc(uint n, uint64 size);
void kfree(void *ptr);
void * kalloc(void);
#endif