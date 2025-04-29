#include "vma.h"
#include "string.h"
#include "pmem.h"
#include "vmem.h"
#ifdef RISCV
#include "riscv.h"
#include "riscv_memlayout.h"
#else
#include "loongarch.h"
#endif

struct vma *vma_init(struct proc *p)
{
    struct vma *vma = (struct vma *)pmem_alloc_pages(1);
    if (vma == NULL)
    {
        panic("vma_init: pmem_alloc_pages failed\n");
        return NULL;
    }
    memset(vma, 0, PGSIZE);
    vma->type = NONE;
    vma->prev = vma->next = vma;
    p->vma = vma;
    return vma;
};

uint64 alloc_vma_stack(struct proc *p, int start, int len)
{
    //assert(len == PGSIZE, "user stack size must be PGSIZE");
    char *mem;
    mem = pmem_alloc_pages(1);
    mappages(p->pagetable, start, (uint64)mem, len, PTE_STACK);
    struct vma *find_vma = p->vma->next;
    // stack 放到链表的最后端
    while (find_vma != p->vma && find_vma->next != p->vma)
    {
        find_vma = find_vma->next;
    }
    struct vma *vma = (struct vma *)pmem_alloc_pages(1);
    if (NULL == vma)
    {
        panic("vma kalloc failed\n");
        return -1;
    }
    vma->type = STACK;
    vma->perm = PTE_W;
    vma->addr = start;
    vma->end = start + len;
    vma->size = len;
    vma->flags = 0;
    vma->fd = -1;
    vma->f_off = -1;
    vma->prev = find_vma;
    vma->next = find_vma->next;
    find_vma->next->prev = vma;
    find_vma->next = vma;
    return 0;
}

uint64 get_proc_sp(struct proc *p)
{
    struct vma *vma = p->vma->next;
    while (vma != p->vma)
    {
        if (vma->type == STACK)
        {
            break;
        }
        vma = vma->next;
    }
    assert(vma->type == STACK, "proc don't have stack!");
    return vma->end;
}

struct vma *vma_copy(struct proc *np, struct vma *head)
{
    struct vma *new_vma = (struct vma*)pmem_alloc_pages(1);
    if(new_vma == NULL){
        goto bad;
    }
    new_vma->next = new_vma->prev = new_vma;
    new_vma->type = NONE;
    np->vma = new_vma;
    struct vma *pre = head->next;
    struct vma *nvma = NULL;
    while(pre != head){
        nvma = (struct vma*)pmem_alloc_pages(1);
        if(nvma == NULL) goto bad;
        memmove(nvma,pre,sizeof(struct vma));
        nvma->next = nvma->prev = NULL;
        nvma->prev = new_vma->prev;
        nvma->next = new_vma;
        new_vma->prev->next = nvma;
        new_vma->prev = nvma;
        pre = pre->next; 
    }
    return new_vma;
bad:
    np->vma = NULL;
    pmem_free_pages(new_vma,1);
    panic("vma alloc failed");
    return NULL;
}

int vma_map(pgtbl_t old, pgtbl_t new, struct vma *vma) {
    uint64 start = vma->addr;
    pte_t *pte;
    uint64 pa;
    char *mem;
    long flags;
    while(start < vma->end){
        pte = walk(old,start,0);
        if(pte == NULL){
            panic("pte should exist");
        }
        if((*pte & PTE_V)==0){
            panic("page should present");
        }
        pa = PTE2PA(*pte) | dmwin_win0;
        flags = PTE_FLAGS(*pte);
        mem = (char*)pmem_alloc_pages(1);
        if(mem == NULL)goto bad;
        memmove(mem,(char *)pa,PGSIZE);
        if(mappages(new,start,(uint64)mem,PGSIZE,flags)!=1){
            pmem_free_pages(mem,1);
            goto bad;
        }
        start += PGSIZE;
    }
    pa = walkaddr(new,vma->addr);
    return 0;
bad:
  vmunmap(new, vma->addr, (start - vma->addr) / PGSIZE, 1);
  return -1;

}