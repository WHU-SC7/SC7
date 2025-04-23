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










#endif