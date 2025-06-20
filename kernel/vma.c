#include "vma.h"
#include "string.h"
#include "pmem.h"
#include "cpu.h"
#include "vmem.h"
#include "types.h"
#include "vfs_ext4.h"
#include "ext4_oflags.h"
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

/**
 * @brief 将操作系统内存保护标志转换为硬件页表项权限标志
 *
 * @details 此函数根据传入的内存保护标志 `prot`，生成对应硬件架构的页表项权限位（PTE flags）。
 * 支持两种架构：
 * 1. RISC-V 架构：
 *    - `PROT_READ` → `PTE_R`
 *    - `PROT_WRITE` → `PTE_W`
 *    - `PROT_EXEC` → `PTE_X`
 *    - `PROT_NONE` 视为无效输入，返回 `-1`。
 * 2. 其他架构（如 LA64）：
 *    - `PROT_READ` → 清除不可读标志 `PTE_NR`
 *    - `PROT_WRITE` → 设置可写标志 `PTE_W`
 *    - `PROT_EXEC` → 清除不可执行标志 `PTE_NX` 并设置访问标志 `PTE_MAT | PTE_P`
 *
 * @param prot 内存保护标志，按位组合（`PROT_READ`、`PROT_WRITE`、`PROT_EXEC` 或 `PROT_NONE`）
 * @return 硬件页表项权限值（`uint64` 类型）
 *
 * @note 关键平台差异：
 * - RISC-V 显式设置权限位，其他架构通过修改默认权限位实现。
 * - 其他架构的默认权限包含 `PTE_PLV3 | PTE_MAT | PTE_D | PTE_NR | PTE_W | PTE_NX`。
 * @see PROT_READ, PROT_WRITE, PROT_EXEC, PROT_NONE
 */
int get_mmapperms(int prot)
{
    uint64 perm = 0;
#if defined RISCV
    perm = PTE_U;
    if (prot == PROT_NONE)
        return 0; //< 这个地方先设成可读可写,似乎只可读和只可写也能通过. 不，设置成0也可以通过
    //< 如果return -1,会在glibc dynamic程序结束时报错panic:[pmem.c:103] pmem_free_pages: page_idx out of range
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
/**
 * @brief 修改页表项权限并返回对应的物理地址
 * @details 该函数在指定的页表中查找虚拟地址对应的页表项（PTE），进行安全校验后，添加指定的权限位，最后返回映射的物理地址。
 * 主要流程：
 * 1. 检查虚拟地址范围有效性（< MAXVA）
 * 2. 通过 `walk()` 获取页表项指针
 * 3. 验证页表项有效性（PTE_V）和用户态权限（PTE_U）
 * 4. 添加权限位（`*pte |= perm`）
 * 5. 转换页表项为物理地址（PTE2PA）
 *
 * @param pagetable [in] 页表根节点指针（pgtbl_t 类型）
 * @param va        [in] 目标虚拟地址（64位）
 * @param perm      [in] 待添加的权限标志位（如 PTE_W, PTE_X）
 * @return 物理地址（成功时）或 0（失败时）
 *
 * @retval >0  操作成功，返回虚拟地址对应的物理地址
 * @retval 0   操作失败（地址无效、PTE无效或权限不足）
 *
 * @note 关键安全约束：
 * - 仅允许修改用户态页表项（`PTE_U` 必须置位）
 * - 权限修改是叠加操作（`|=`），非覆盖（需先清除旧权限应额外处理）
 * - 调用方需确保 TLB 刷新（如 RISCV 的 `sfence_vma`）
 *
 * @warning 非原子操作！并发场景需加锁保护页表访问
 * @see walk(), PTE2PA(), PTE_V, PTE_U
 */
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

int vm_protect(pgtbl_t pagetable, uint64 va, uint64 addr, uint64 perm)
{
    return experm(pagetable,va, perm);
}

uint64 mmap(uint64 start, int len, int prot, int flags, int fd, int offset)
{
    proc_t *p = myproc();
    int perm = get_mmapperms(prot);
    // assert(start == 0, "uvm_mmap: 0");
    //  assert(flags & MAP_PRIVATE, "uvm_mmap: 1");
    //len += PGSIZE;
    struct file *f = fd == -1 ? NULL : p->ofile[fd];

    if (fd != -1 && f == NULL)
        return -1;
    struct vma *vma = alloc_mmap_vma(p, flags, start, len, perm, fd, offset);
    if (!(flags & MAP_FIXED))
        start = vma->addr;
    if (-1 != fd)
    {
        int ret = vfs_ext4_lseek(f, offset, SEEK_SET); //< 设置文件位置指针到指定偏移量
        if (ret < 0)
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "lseek in pread failed!, ret is %d\n", ret);
            return ret;
        }
    }
    else
    {
        return start;
    }
    if (vma == NULL)
        return -1;
    assert(len, "len is zero!");
    // /// @todo 逻辑有问题
    uint64 i;

    //< 特殊处理一下，如果len大于文件大小，就把len减小到文件大小
    //< !把下面处理len的代码块注释掉，也是可以跑的!
    // uint64 file_size = 0;
    // if (fd != -1)
    // {
    //     struct ext4_file *efile = (struct ext4_file *)f->f_data.f_vnode.data;
    //     file_size = efile->fsize; // 实际文件大小
    // }

    // // 新增：调整映射长度（核心修复）
    // size_t aligned_len = PGROUNDUP(len);
    // if (fd != -1 && len > file_size)
    // {
    //     // 文件映射：禁止超过文件实际大小
    //     len = PGROUNDUP(file_size); // 对齐到页边界
    //     DEBUG_LOG_LEVEL(LOG_DEBUG, "Truncate mmap len to file size: 0x%x\n", len);
    // }

    for (i = 0; i < len; i += PGSIZE) //< 从offset开始读len字节  //< ?为什么la glibc一进来i就是0x8c000
    {
        // LOG_LEVEL(LOG_ERROR,"[mmap] i=%x",i);
        uint64 pa = experm(p->pagetable, start + i, perm); //< 检查是否可以访问start + i，如果可以就返回start + i所在页的物理地址
        assert(pa != 0, "pa is null!,va:%p", start + i);

        int remaining = len - i;
        int to_read = (remaining > PGSIZE) ? PGSIZE : remaining;

        // 读取文件内容（如果 to_read > 0）
        int bytes_read = 0;
        if (to_read > 0)
        {
            // uint64 orig_pos = f->f_pos;
            // int ret = vfs_ext4_lseek(f,start + i, SEEK_SET); //< 设置文件位置指针到指定偏移量
            // if (ret < 0)
            // {
            //     DEBUG_LOG_LEVEL(LOG_WARNING, "lseek in pread failed!, ret is %d\n", ret);
            //     return ret;
            // }
            bytes_read = get_file_ops()->read(f, start + i, to_read);
            // vfs_ext4_lseek(f, orig_pos, SEEK_SET);
            // bytes_read = vfs_ext4_readat(f,0,pa,to_read,offset+i); //< read比vfs_ext4_readat好，vfs_ext4_readat如果offset大于size会panic。之后删掉这行吧
            if (bytes_read < 0)
            {
                // 错误处理（如取消映射并返回）
                panic("bytes_read null");
                return -1;
            }
        }

        // 文件内容不足时，填充零
        if (bytes_read < to_read)
        {
            memset((void *)((pa + bytes_read) | dmwin_win0), 0, to_read - bytes_read);
        }

        // 页面剩余部分清零
        if (to_read < PGSIZE)
        {
            memset((void *)((pa + to_read) | dmwin_win0), 0, PGSIZE - to_read);
        }
    }
    // if (aligned_len > len)
    // {
    //     size_t extra_len = aligned_len - len;
    //     uint64 prot_start = start + len;
    //     DEBUG_LOG_LEVEL(LOG_DEBUG, "Set PROT_NONE for extra pages: 0x%llx-0x%llx\n",
    //                     prot_start, prot_start + extra_len);
    //     vm_protect(p->pagetable, prot_start, extra_len, PROT_NONE);
    // }
    get_file_ops()->dup(f);
    return start;
}

int munmap(uint64 start, int len)
{
    proc_t *p = myproc();
    struct vma *vma = p->vma->next; // 从链表头部开始遍历
    uint64 end = PGROUNDDOWN(start + len);
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
    //uint64 start = PGROUNDUP(addr);
    //uint64 end = PGROUNDUP(addr+sz);

    uint64 start = PGROUNDDOWN(p->sz);
    uint64 end = PGROUNDUP(p->sz + sz);
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[allocvma] : start:%p,end:%p,sz:%p\n", start, start + sz, sz);
#endif
    p->sz += sz;
    p->sz = PGROUNDUP(p->sz);
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
