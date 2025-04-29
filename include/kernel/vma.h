#ifndef __VMA_H__
#define __VMA_H__

#include "types.h"
#include "process.h"
enum segtype {NONE,MMAP,STACK};
struct proc;
struct vma {
    enum segtype type;
    int perm;    ///< vma的权限
    uint64 addr; ///< 起始地址
    uint64 size;
    uint64 end;  ///< 结束地址
    int flags;   ///< vma进程的权限
    int fd;
    uint64 f_off;
    struct vma *prev;
    struct vma *next;
};

struct vma *vma_init(struct proc *p);
uint64 alloc_vma_stack(struct proc *p,int start,int len);
uint64 get_proc_sp(struct proc *p);
struct vma *vma_copy(struct proc *np, struct vma *head);
int vma_map(pgtbl_t old, pgtbl_t new, struct vma *vma);

#endif