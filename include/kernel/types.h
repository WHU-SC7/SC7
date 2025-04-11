#ifndef __TYPES_H__
#define __TYPES_H__

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;

typedef unsigned long uintptr_t;

// page table entry   4KB / 8B = 512 entry
typedef uint64 pte_t;

// 页表基地址 pagetable可以理解为一个pte数组
typedef uint64* pgtbl_t;

typedef signed long int64;
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif 