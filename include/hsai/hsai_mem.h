#ifndef __HSAI_MEM_H__
#define __HSAI_MEM_H__
#include "types.h"

uint64 hsai_get_mem_start();
void   hsai_config_pagetable(pgtbl_t kernel_pagetable);

#endif