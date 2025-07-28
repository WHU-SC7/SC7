#ifndef __TYPES_H__
#define __TYPES_H__

#include "config.h" //让每个c文件都include配置宏

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

typedef uint64 uint64_t;

typedef uint32 uint32_t;

typedef uint16 uint16_t;

typedef uint8 uint8_t;

typedef int int32_t;

typedef short int16_t;

typedef unsigned long uintptr_t;

typedef long long int64;

typedef long int ssize_t;

typedef uint64 size_t;

// page table entry   4KB / 8B = 512 entry
typedef uint64 pte_t;

// 页表基地址 pagetable可以理解为一个pte数组
typedef uint64 *pgtbl_t;

typedef int pid_t;

// 添加必要的类型定义
typedef uint32_t mode_t;
typedef uint32_t uid_t;
typedef uint32_t gid_t;

#endif ///< __TYPES_H__

#ifndef NULL
#define NULL ((void *)0)
#endif