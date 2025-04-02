#ifndef __VMEM_H__
#define __VMEM_H__

#include "types.h"
#include <stdbool.h>


void vmem_init();
pte_t* vmem_walk(pgtbl_t pt, uint64 va, int alloc);
int vmem_mappages(pgtbl_t pt, uint64 va, uint64 pa,uint64 len,int perm);

#endif