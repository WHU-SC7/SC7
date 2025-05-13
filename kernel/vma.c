#include "vma.h"
#include "string.h"
#include "pmem.h"
#include "cpu.h"
#include "vmem.h"
#include "types.h"
#include "vfs_ext4.h"
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
    if (alloc_mmap_vma(p, 0, USER_MMAP_START, 0, 0, 0, 0) == NULL)
    {
        panic("init vma error!");
        return NULL;
    }
    return vma;
};

int get_mmapperms(int prot)
{
    uint64 perm = 0;
#if defined RISCV
    perm = PTE_U;
    if (prot == PROT_NONE)
        return -1;
    if (prot & PROT_READ)
        perm |= PTE_R;
    if (prot & PROT_WRITE)
        perm |= PTE_W;
    if (prot & PROT_EXEC)
        perm |= PTE_X;
#else
    perm = PTE_PLV3 | PTE_MAT | PTE_D | PTE_NR | PTE_W | PTE_NX;
    if (prot & PROT_READ)
        // LA64  架构下，0表示可读
        perm &= ~PTE_NR;
    if (prot & PROT_WRITE)
        // 1 表示可写
        perm |= PTE_W;
    // 表示可以执行
    if (prot & PROT_EXEC)
    {
        perm |= PTE_MAT | PTE_P;
        perm &= ~PTE_NX;
    }
#endif
    return perm;
}

uint64 experm(pgtbl_t pagetable, uint64 va, uint64 perm)
{
    pte_t *pte;
    uint64 pa;
    if (va >= MAXVA)
        return 0;
    pte = walk(pagetable, va, 0);
    if (pte == 0)
        return 0;
    if ((*pte & PTE_V) == 0)
        return 0;
    if ((*pte & PTE_U) == 0)
        return 0;
    *pte |= perm;
    pa = PTE2PA(*pte);
    return pa;
}

uint64 mmap(uint64 start, int len, int prot, int flags, int fd, int offset)
{
    proc_t *p = myproc();
    int perm = get_mmapperms(prot);
    assert(start == 0, "uvm_mmap: 0");
    // assert(flags & MAP_PRIVATE, "uvm_mmap: 1");
    struct file *f = fd == -1 ? NULL : p->ofile[fd];

    if (fd != -1 && f == NULL)
        return -1;
    struct vma *vma = alloc_mmap_vma(p, flags, start, len, perm, fd, offset);
    if (-1 != fd)
    {
        f->f_pos = offset;
        ((ext4_file *)f->f_data.f_vnode.data)->fpos = offset;
    }
    else
    {
        return start;
    }
    if (!(flags & MAP_FIXED))
        start = vma->addr;
    if (vma == NULL)
        return -1;
    assert(len, "len is zero!");
    // /// @todo 逻辑有问题
    uint64 i;
    for (i = 0; i < len; i += PGSIZE)
    {
        uint64 pa = experm(p->pagetable, start + i, perm);
        assert(pa, "pa is null!");
        if (i + PGSIZE < len)
        {
            int bytes = get_file_ops()->read(f, start + i, PGSIZE);
            assert(bytes, "mmap read null!");
        }
        else
        {
            int bytes = get_file_ops()->read(f, start + i, len - i);
            // char buffer[512] = {0};
            // copyinstr(myproc()->pagetable, buffer, start+ i, 512);
            assert(bytes, "mmap read null!");
            // memset((void*)((pa + (len-i)) | dmwin_win0 ), 0, PGSIZE - (len - i));
        }
    }
    get_file_ops()->dup(f);
    return start;
}

int munmap(uint64 start, int len)
{
    proc_t *p = myproc();
    struct vma *vma = p->vma->next; // 从链表头部开始遍历
    uint64 end = PGROUNDUP(start + len);
    start = PGROUNDDOWN(start);
    int found = 0;

    // 参数合法性检查（需页对齐）
    if (start != PGROUNDDOWN(start) || len <= 0)
    {
        return -1; // EINVAL
    }

    // 遍历所有VMA
    while (vma != p->vma)
    {
        struct vma *next_vma = vma->next;
        uint64 vma_start = vma->addr;
        uint64 vma_end = vma->end;

        // 判断是否与当前VMA重叠
        if (vma_end > start && vma_start < end)
        {
            found = 1;
            // 情况1：当前VMA完全在解除范围内
            if (vma_start >= start && vma_end <= end)
            {
                // 释放物理内存和页表项
                vmunmap(p->pagetable, vma_start, (vma_end - vma_start) / PGSIZE, 1);
                // 处理文件引用（若关联了文件）
                if (vma->fd != -1)
                {
                    struct file *f = p->ofile[vma->fd];
                    if (f)
                    {
                        get_file_ops()->close(f);
                    }
                }
                // 从链表中移除VMA
                vma->prev->next = vma->next;
                vma->next->prev = vma->prev;
                pmem_free_pages(vma, 1); // 释放VMA结构体
            }
            // 情况2：仅部分重叠（需分割VMA）
            else if (vma_start < start || vma_end > end)
            {
                // 分割为前段和后段，中间部分解除映射
                if (vma_start < start)
                {
                    // 创建前段VMA（保留start之前的区域）
                    struct vma *new_front = alloc_vma(p, vma->type, vma_start,
                                                      start - vma_start, vma->perm, 0, 0);
                    new_front->fd = vma->fd;
                    new_front->f_off = vma->f_off;
                }
                if (vma_end > end)
                {
                    // 创建后段VMA（保留end之后的区域）
                    struct vma *new_back = alloc_vma(p, vma->type, end,
                                                     vma_end - end, vma->perm, 0, 0);
                    new_back->fd = vma->fd;
                    new_back->f_off = vma->f_off + (end - vma_start);
                }
                // 解除重叠部分
                uint64 unmap_start = (vma_start > start) ? vma_start : start;
                uint64 unmap_end = (vma_end < end) ? vma_end : end;
                vmunmap(p->pagetable, unmap_start, (unmap_end - unmap_start) / PGSIZE, 1);
                // 移除原VMA
                vma->prev->next = vma->next;
                vma->next->prev = vma->prev;
                pmem_free_pages(vma, 1);
            }
        }
        vma = next_vma;
    }

    return found ? 0 : -1; // 返回成功或失败
}

struct vma *alloc_mmap_vma(struct proc *p, int flags, uint64 start, int len, int perm, int fd, int offset)
{
    struct vma *vma = NULL;
    struct vma *find_vma = find_mmap_vma(p->vma);
    if (start == 0 && len < find_vma->addr)
        start = PGROUNDUP(find_vma->addr - len);

    vma = alloc_vma(p, MMAP, start, len, perm, 1, 0);
    if (vma == NULL)
    {
        panic("alloc_mmap_vma");
        return NULL;
    }
    vma->fd = fd;
    vma->f_off = offset;
    return vma;
}

struct vma *alloc_vma(struct proc *p, enum segtype type, uint64 addr, uint64 sz, int perm, int alloc, uint64 pa)
{
    uint64 start = PGROUNDDOWN(addr);
    uint64 end = PGROUNDUP(addr + sz);
    struct vma *find_vma = p->vma->next;
    while (find_vma != p->vma)
    {
        if (end <= find_vma->addr)
            break;
        else if (start >= find_vma->end)
            find_vma = find_vma->next;
        else if (start >= find_vma->addr && end <= find_vma->end)
        {
            return find_vma;
        }
        else
        {
            panic("vma address overflow\n");
            return NULL;
        }
    }
    struct vma *vma = (struct vma *)pmem_alloc_pages(1);
    if (vma == NULL)
    {
        panic("vma alloc failed\n");
        return NULL;
    }
    if (sz != 0)
    {
        if (alloc)
        {
            if (!uvmalloc1(p->pagetable, start, end, perm))
            {
                panic("uvmalloc1 failed\n");
                return NULL;
            }
        }
        else if (pa != 0)
        {
            if (mappages(p->pagetable, start, end, pa, perm) == -1)
            {
                panic("mappages failed\n");
                return NULL;
            }
        }
    }
    vma->addr = start;
    vma->size = sz;
    vma->perm = perm;
    vma->end = end;
    vma->fd = -1;
    vma->f_off = 0;
    vma->type = type;
    vma->prev = find_vma->prev;
    vma->next = find_vma;
    find_vma->prev->next = vma;
    find_vma->prev = vma;
    return vma;
}

struct vma *find_mmap_vma(struct vma *head)
{
    struct vma *vma = head->next;
    while (vma != head)
    {
        // vma 映射类型是： MMAP
        if (MMAP == vma->type)
            return vma;
        vma = vma->next;
    }
    return NULL;
}

uint64 alloc_vma_stack(struct proc *p)
{
    // assert(len == PGSIZE, "user stack size must be PGSIZE");
    uint64 end = USER_STACK_TOP;
    uint64 start = end - USER_STACK_SIZE;
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
    if (uvmalloc1(p->pagetable, start, end, PTE_STACK) != 1)
    {
        panic("user stack vma alloc failed\n");
        return -1;
    }
    vma->type = STACK;
    vma->perm = PTE_W;
    vma->addr = start;
    vma->end = end;
    vma->size = USER_STACK_SIZE;
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
    struct vma *new_vma = (struct vma *)pmem_alloc_pages(1);
    if (new_vma == NULL)
    {
        goto bad;
    }
    new_vma->next = new_vma->prev = new_vma;
    new_vma->type = NONE;
    np->vma = new_vma;
    struct vma *pre = head->next;
    struct vma *nvma = NULL;
    while (pre != head)
    {
        nvma = (struct vma *)pmem_alloc_pages(1);
        if (nvma == NULL)
            goto bad;
        memmove(nvma, pre, sizeof(struct vma));
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
    pmem_free_pages(new_vma, 1);
    panic("vma alloc failed");
    return NULL;
}

int vma_map(pgtbl_t old, pgtbl_t new, struct vma *vma)
{
    uint64 start = vma->addr;
    pte_t *pte;
    uint64 pa;
    char *mem;
    long flags;
    while (start < vma->end)
    {
        pte = walk(old, start, 0);
        if (pte == NULL)
        {
            panic("pte should exist");
        }
        if ((*pte & PTE_V) == 0)
        {
            panic("page should present");
        }
        pa = PTE2PA(*pte) | dmwin_win0;
        flags = PTE_FLAGS(*pte);
        mem = (char *)pmem_alloc_pages(1);
        if (mem == NULL)
            goto bad;
        memmove(mem, (char *)pa, PGSIZE);
        if (mappages(new, start, (uint64)mem, PGSIZE, flags) != 1)
        {
            pmem_free_pages(mem, 1);
            goto bad;
        }
        start += PGSIZE;
    }
    pa = walkaddr(new, vma->addr);
    return 0;
bad:
    vmunmap(new, vma->addr, (start - vma->addr) / PGSIZE, 1);
    return -1;
}

int free_vma_list(struct proc *p)
{
    struct vma *vma_head = p->vma;
    if (vma_head == NULL)
    {
        return 1;
    }
    struct vma *vma = vma_head->next;
    while (vma != vma_head)
    {
        uint64 a;
        pte_t *pte;
        for (a = vma->addr; a < vma->end; a += PGSIZE)
        {
            if ((pte = walk(p->pagetable, a, 0)) == NULL)
                continue;
            if ((*pte & PTE_V) == 0)
                continue;
            if (PTE_FLAGS(*pte) == PTE_V)
                continue;
            uint64 pa = PTE2PA(*pte) | dmwin_win0;
            pmem_free_pages((void *)pa, 1);
            *pte = 0;
        }
        vma = vma->next;
        pmem_free_pages(vma->prev, 1);
    }
    pmem_free_pages(vma, 1);
    p->vma = NULL;
    return 1;
}
