#ifndef __VMA_H__
#define __VMA_H__

#include "types.h"
#include "process.h"

// for mmap
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define PROT_GROWSDOWN 0X01000000
#define PROT_GROWSUP 0X02000000

#define MAP_FILE 0
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0X02
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void *)-1)

#define USER_STACK_SIZE 25 * PGSIZE

enum segtype
{
    NONE,
    MMAP,
    STACK
};
struct proc;
struct vma
{
    enum segtype type;
    int perm;    ///< vma的权限
    uint64 addr; ///< 起始地址
    uint64 size;
    uint64 end; ///< 结束地址
    int flags;  ///< vma进程的权限
    int fd;
    uint64 f_off;
    struct vma *prev;
    struct vma *next;
};

struct vma *vma_init(struct proc *p);
uint64 alloc_vma_stack(struct proc *p);
int get_mmapperms(int prot);
uint64 experm(pgtbl_t pagetable, uint64 va, uint64 perm);
uint64 get_proc_sp(struct proc *p);
struct vma *vma_copy(struct proc *np, struct vma *head);
int vma_map(pgtbl_t old, pgtbl_t new, struct vma *vma);
int free_vma_list(struct proc *p);
uint64 mmap(uint64 start, int len, int prot, int flags, int fd, int offset);
int munmap(uint64 start, int len);
int get_mmapperms(int prot);
struct vma *find_mmap_vma(struct vma *head);
struct vma *alloc_mmap_vma(struct proc *p, int flags, uint64 start, int len, int perm, int fd, int offset);
struct vma *alloc_vma(struct proc *p, enum segtype type, uint64 addr, uint64 sz, int perm, int alloc, uint64 pa);
#endif