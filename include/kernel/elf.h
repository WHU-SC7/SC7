#ifndef __ELF_H__
#define __ELF_H__

#define ELF_MAGIC 0x464C457FU
#include "types.h"

typedef struct elf_header {
    uint32 magic;
    uint8 elf[12];
    uint16 type;
    uint16 machine;
    uint32 version;
    uint64 entry;
    uint64 phoff;
    uint64 shoff;
    uint32 flags;
    uint16 ehsize;
    uint16 phentsize;
    uint16 phnum;
    uint16 shentsize;
    uint16 shnum;
    uint16 shstrndx;

}elf_header_t;

typedef struct program_header {
    uint32 type;
    uint32 flags;
    uint64 off;
    uint64 vaddr;
    uint64 paddr;
    uint64 filesz;
    uint64 memsz;
    uint64 align;

}program_header_t;


// Values for Proghdr type
#define ELF_PROG_LOAD           1
#define ELF_PROG_INTERP         3
#define ELF_PROG_PHDR           6

#define NENV   8                  // exec接受的最大环境变量数量
#define NARG   16                 // exec接受的最大参数数量


#define AT_NULL		0   ///< 辅助向量的终止标记，表示auxv数组结束
#define AT_IGNORE	1
#define AT_EXECFD	2
#define AT_PHDR		3   ///< 程序头表（Program Header Table）的地址
#define AT_PHENT	4   ///< 单个程序头表条目的大小（字节数）
#define AT_PHNUM	5   ///< 程序头表中的条目数量
#define AT_PAGESZ	6   ///< 系统页大小
#define AT_BASE		7   ///< 动态链接器（ld.so）的加载地址
#define AT_FLAGS	8
#define AT_ENTRY	9   ///< 程序入口点地址
#define AT_NOTELF	10
#define AT_UID		11  ///< 进程的实际用户ID
#define AT_EUID		12  ///< 进程的有效用户ID
#define AT_GID		13  ///< 进程的实际组ID
#define AT_EGID		14  ///< 进程的有效组ID
#define AT_PLATFORM	15  ///< 平台标识字符串的地址
#define AT_HWCAP	16  ///< 硬件能力标志
#define AT_CLKTCK	17  
#define AT_FPUCW	18
#define AT_DCACHEBSIZE	19
#define AT_ICACHEBSIZE	20
#define AT_UCACHEBSIZE	21
#define AT_IGNOREPPC	22
#define	AT_SECURE	23
#define AT_BASE_PLATFORM 24
#define AT_RANDOM	25   ///< 随机数种子地址（16字节的安全随机值，用于栈保护等）
#define AT_HWCAP2	26
#define AT_EXECFN	31
#define AT_SYSINFO	32
#define AT_SYSINFO_EHDR	33

#endif