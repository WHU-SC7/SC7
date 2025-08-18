#include "vmem.h"
#include "pmem.h"
#include "print.h"
#include "hsai_mem.h"
#include "string.h"
#include "process.h"
#include "cpu.h"
#include "virt.h"
#include "vma.h"
#include "spinlock.h"
#if defined RISCV
#include "riscv.h"
#include "riscv_memlayout.h"
#else
#include "loongarch.h"
#endif

pgtbl_t kernel_pagetable;
bool debug_trace_walk = false;
extern buddy_system_t buddy_sys;

// 虚拟内存系统锁
struct spinlock vmem_lock;

// 前向声明内部函数
static int mappages_internal(pgtbl_t pt, uint64 va, uint64 pa, uint64 len, uint64 perm);
static pte_t *walk_internal(pgtbl_t pt, uint64 va, int alloc);

extern char KERNEL_TEXT;
extern char KERNEL_DATA;
extern char USER_END;
extern char trampoline;
void vmem_init()
{
    // 初始化虚拟内存锁
    initlock(&vmem_lock, "vmem");
    
    kernel_pagetable = pmem_alloc_pages(1); ///< 分配一个页,存放内核页表 分配时已清空页面
    LOG("kernel_pagetable address: %p\n", kernel_pagetable);
    // RISCV需要将内核映射到外部，LA由于映射窗口，不需要映射
#if defined RISCV
    /*UART映射*/
    mappages(kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);

    /*virtio映射*/
    mappages(kernel_pagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

    /*PLIC映射*/
    mappages(kernel_pagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

    /*kernel代码区映射 映射为可读可执行*/
    mappages(kernel_pagetable, KERNEL_BASE, KERNEL_BASE, (uint64)&KERNEL_TEXT - KERNEL_BASE, PTE_R | PTE_X);
    LOG("[MAP] KERNEL_BASE:%p ->  KERNEL_TEXT:%p  len: 0x%x\n", KERNEL_BASE, (uint64)&KERNEL_TEXT, (uint64)&KERNEL_TEXT - KERNEL_BASE);

    /*kernel数据区映射，映射为可读可写*/
    mappages(kernel_pagetable, (uint64)&KERNEL_TEXT, (uint64)&KERNEL_TEXT, (uint64)&USER_END - (uint64)&KERNEL_TEXT, PTE_R | PTE_W);
    LOG("[MAP] KERNEL_TEXT :%p ->  USER_END:%p  len: 0x%x\n", (uint64)&KERNEL_TEXT, (uint64)&USER_END, (uint64)&USER_END - (uint64)&KERNEL_TEXT);

#endif
    /*trampoline映射为虚拟地址最高位的一个页面*/
    mappages(kernel_pagetable, TRAMPOLINE, (uint64)&trampoline, PGSIZE, PTE_TRAMPOLINE);
    LOG("[MAP] TRAMPOLINE :%p ->  trampoline:%p  len: 0x%x\n", TRAMPOLINE, (uint64)&trampoline, PGSIZE);

    proc_mapstacks(kernel_pagetable);

    hsai_config_pagetable(kernel_pagetable);
    LOG("Kernel page table configuration completed successfully\n");
}

void kvm_init_hart()
{
    hsai_config_pagetable(kernel_pagetable);
}

/**
 * @brief 在pt中建立映射 [va, va+len)->[pa, pa+len)
 * @param va :  要映射到的起始虚拟地址
 * @param pa :  需要映射的起始物理地址  输入时需要对齐
 * @param len:  映射长度
 * @param perm: 权限位 loongarch权限可能超过Int范围,修改为uint64
 * @return   映射成功返回1，失败返回-1
 *
 */
int mappages(pgtbl_t pt, uint64 va, uint64 pa, uint64 len, uint64 perm)
{
    // 获取虚拟内存锁
    acquire(&vmem_lock);
    
    int result = mappages_internal(pt, va, pa, len, perm);
    
    // 释放锁并返回
    release(&vmem_lock);
    return result;
}

/**
 * @brief 在页表中遍历虚拟地址对应的页表项（PTE）
 * 功能说明：
 * 1. 通过多级页表逐级解析虚拟地址va，最终返回对应的PTE。
 * 2. 若alloc=1且中间页表项不存在，函数会动态分配物理页作为下级页表。
 * 3. 若alloc=0且中间页表项缺失，函数返回NULL表示遍历失败。
 *
 * @param pt    页表基地址
 * @param va    待查询的虚拟地址
 * @param alloc 是否自动分配缺失的页表层级（1=分配，0=不分配）
 * @return      返回虚拟地址对应的页表项（PTE）指针，若失败返回NULL
 *
 */
pte_t *walk(pgtbl_t pt, uint64 va, int alloc)
{
    // 获取虚拟内存锁
    acquire(&vmem_lock);
    
    pte_t *result = walk_internal(pt, va, alloc);
    
    // 释放锁并返回
    release(&vmem_lock);
    return result;
}

/**
 * @brief 通过页表将用户空间的虚拟地址（va）转换为物理地址（pa）
 *
 * @param pt  用户进程的页表
 * @param va  虚拟地址
 * @return uint64 成功则返回物理地址，否则返回0
 */
uint64 walkaddr(pgtbl_t pt, uint64 va)
{
    pte_t *pte;
    uint64 pa;
    if (va > MAXVA)
    {
        panic("va:%p out of range", va);
        return 0;
    }
    pte = walk(pt, va, 0);
    if (pte == NULL)
        return 0;
    if ((*pte & PTE_V) == 0)
    {
        // printf("va :%p walkaddr: *pte & PTE_V == 0\n", va);
        return 0;
    }
    if ((*pte & PTE_U) == 0)
    {
        // printf("walkaddr: *pte & PTE_U == 0\n");
        return 0;
    }
    pa = PTE2PA(*pte);
    pa = pa | dmwin_win0;
    return pa;
}

/**
 * @brief 递归释放多级页表及其占用的物理内存
 *
 * @param pt
 */
void freewalk(pgtbl_t pt)
{
    /* 遍历当前页表的所有512个PTE */
    for (int i = 0; i < 512; i++)
    {
        pte_t pte = pt[i];
        if ((pte & PTE_V) && PTE_FLAGS(pte) == PTE_V) ///< 有效且指向下级页表的PTE
        {
            uint64 child = (PTE2PA(pte) | dmwin_win0);
            freewalk((pgtbl_t)child);
            pt[i] = 0;
        }
        else if (pte & PTE_V) ///< 未释放的叶级PTE
        {
            // panic("freewalk: leaf");
            continue;
        }
    }
    pmem_free_pages(pt, 1);
}

/**
 * @brief 在pt中建立映射 [va, va+len)->[pa, pa+len) - 内部版本，不获取锁
 * @param va :  要映射到的起始虚拟地址
 * @param pa :  需要映射的起始物理地址  输入时需要对齐
 * @param len:  映射长度
 * @param perm: 权限位 loongarch权限可能超过Int范围,修改为uint64
 * @return   映射成功返回1，失败返回-1
 *
 */
static int mappages_internal(pgtbl_t pt, uint64 va, uint64 pa, uint64 len, uint64 perm)
{
    assert(va < MAXVA, "va out of range");
    assert((pa % PGSIZE == 0), "pa:%p need be aligned", pa);
    
    pte_t *pte;
    /*将要分配的虚拟地址首地址和尾地址对齐*/
    uint64 begin = PGROUNDDOWN(va);
    uint64 end = PGROUNDDOWN(va + len - 1);
    uint64 current = begin;
    // printf("mappages: pt=%p, va=0x%p -> pa=0x%p, len=0x%p, perm=0x%p\n", pt, va, pa, len, perm);
    for (;;)
    {
        if ((pte = walk_internal(pt, current, true)) == NULL)
        {
            assert(0, "pte allock error");
            return -1;
        }
        if (*pte & PTE_V)
        {
            DEBUG_LOG_LEVEL(LOG_ERROR, "pte remap! va: %p\n", current);
            return -EINVAL;
        }
        /*给页表项写上控制位，置有效*/
        *pte = PA2PTE(pa) | perm | PTE_V;

        /// @todo : 刷新TLB
        if (current == end)
            break;
        current += PGSIZE;
        pa += PGSIZE;
    }
    
    return 1;
}

/**
 * @brief 在页表中遍历虚拟地址对应的页表项（PTE）- 内部版本，不获取锁
 * 功能说明：
 * 1. 通过多级页表逐级解析虚拟地址va，最终返回对应的PTE。
 * 2. 若alloc=1且中间页表项不存在，函数会动态分配物理页作为下级页表。
 * 3. 若alloc=0且中间页表项缺失，函数返回NULL表示遍历失败。
 *
 * @param pt    页表基地址
 * @param va    待查询的虚拟地址
 * @param alloc 是否自动分配缺失的页表层级（1=分配，0=不分配）
 * @return      返回虚拟地址对应的页表项（PTE）指针，若失败返回NULL
 *
 */
static pte_t *walk_internal(pgtbl_t pt, uint64 va, int alloc)
{
    assert(va < MAXVA, "va out of range");

    // 验证页表基地址的有效性
    if (!pt)
    {
        return NULL;
    }

    pte_t *pte;
    if (debug_trace_walk)
        LOG_LEVEL(LOG_DEBUG, "[walk trace] 0x%p:", va);
    /*riscv 三级页表  loongarch 四级页表*/
    for (int level = PT_LEVEL - 1; level > 0; level--)
    {
        /*pt是页表项指针的数组,存放的是物理地址,需要to_vir*/
        pte = &pt[PX(level, va)];

        // 验证PTE指针的有效性
        if (!pte)
        {
            return NULL;
        }

        // pte = to_vir(pte);
        if (debug_trace_walk)
            printf("0x%p->", pte);
        if (*pte & PTE_V)
        {
            uint64 next_pt_pa = PTE2PA(*pte);
            // 验证下一级页表物理地址的有效性
            if (next_pt_pa == 0)
            {
                return NULL;
            }
            pt = ((pgtbl_t)(next_pt_pa | dmwin_win0)); ///< 如果页表项有效，更新pt，找下一级页表
        }
        else if (alloc) ///< 无效且允许分配，则分配一个物理页作为，将物理地址存放在页表项中
        {
            pt = (pgtbl_t)pmem_alloc_pages(1);
            if (pt == NULL)
            {
                return NULL;
            }
            /*写页表的时候，需要把分配的页的物理地址写入pte中*/
            // pt = to_phy(pt);
            *pte = PA2PTE(pt) | PTE_WALK | dmwin_win0;
            if (debug_trace_walk)
                printf("0x%p->", pte);

            /// @todo TLB刷新？
        }
        else
        {
            return NULL;
        }
    }
    pte = &pt[PX(0, va)];

    // 验证最终PTE指针的有效性
    if (!pte)
    {
        return NULL;
    }

    /*最后需要返回PTE的虚拟地址*/
    // pte = to_vir(pte);
    if (debug_trace_walk)
        printf("0x%p\n", pte);
    
    return pte;
}

/**
 * @brief 从虚拟地址 va 开始，解除 npages 个连续页面的映射关系
 *
 * @param pt
 * @param va
 * @param npages
 * @param do_free  是否释放对应物理页
 */
void vmunmap(pgtbl_t pt, uint64 va, uint64 npages, int do_free)
{
    uint64 a;
    pte_t *pte;
    assert((va % PGSIZE) == 0, "va:%p is not aligned", va);

    // 获取虚拟内存锁
    acquire(&vmem_lock);

    // DEBUG_LOG_LEVEL(LOG_DEBUG, "[vmunmap] 开始解除映射: va=%p, npages=%d, do_free=%d\n", va, npages, do_free);

    for (a = va; a < va + npages * PGSIZE; a += PGSIZE)
    {
        if ((pte = walk_internal(pt, a, 0)) == NULL) ///< 确保pte不为空
        {
            // 如果PTE不存在，说明这个页面本来就没有映射，跳过
            continue;
        }
        if ((*pte & PTE_V) == 0) ///< 确保pte有效
            continue;

        if ((PTE_FLAGS(*pte) == PTE_V)) ///< 若 PTE 只有 PTE_V 标志（无其他权限位），说明是中间页表节点
            panic("vmunmap: not a leaf");

        if (do_free)
        {
            uint64 pa = PTE2PA(*pte);
            // 验证物理地址的有效性
            if (pa != 0)
            {
                // 确保物理地址在有效范围内
                if ((pa | dmwin_win0) >= buddy_sys.mem_start && (pa | dmwin_win0) < buddy_sys.mem_end)
                {
                    // DEBUG_LOG_LEVEL(LOG_DEBUG, "[vmunmap] 释放物理页: va=%p, pa=%p\n", a, pa);
                    pmem_free_pages((void *)(pa | dmwin_win0), 1);
                }
                else
                {
                    printf("vmunmap: invalid physical address %p for va %p\n", (void *)pa, (void *)a);
                }
            }
            else
            {
                // DEBUG_LOG_LEVEL(LOG_DEBUG, "[vmunmap] 跳过无效物理地址: va=%p\n", a);
            }
        }
        else
        {
            // DEBUG_LOG_LEVEL(LOG_DEBUG, "[vmunmap] 只清除页表项，不释放物理页: va=%p\n", a);
        }
        
        *pte = 0;
    }
    
    // 释放锁
    release(&vmem_lock);
    
    // DEBUG_LOG_LEVEL(LOG_DEBUG, "[vmunmap] 解除映射完成: va=%p, npages=%d\n", va, npages);
}

void uvmfree(pgtbl_t pagetable, uint64 start, uint64 sz)
{
    if (sz > 0)
        vmunmap(pagetable, start, PGROUNDUP(sz) / PGSIZE, 1);
    freewalk(pagetable);
}

/**
 * @brief  创建用户态页表
 *
 * @return pgtbl_t
 */
pgtbl_t uvmcreate()
{
    pgtbl_t pt;
    pt = pmem_alloc_pages(1);
    if (pt == NULL)
        return NULL;
    return pt;
}
/**
 * @brief  将用户初始代码（src）加载到页表的虚拟地址 0 开始的位置，并映射物理内存。
 *
 * @param pt  用户进程的页表
 * @param src
 * @param sz  需要加载的数据大小，可以不对齐
 *
 * @note mem为局部变量，保存一块地址，确保这块内存不会被其他代码误用
 *       即使 mem 变量被释放，页表仍然持有这个物理页的映射
 *       由于页表已经建立映射，后续访问虚拟地址 i 时，CPU 会自动找到 mem 对应的物理页
 */
void uvminit(proc_t *p, uchar *src, uint sz)
{
    pgtbl_t pt = p->pagetable;
    char *mem;
    uint64 i;
    // assert(sz < PGSIZE, "uvminit: sz is too big");
    for (i = 0; i < sz; i += PGSIZE)
    {
        mem = pmem_alloc_pages(1);
        uint copy_size = (i + PGSIZE <= sz) ? PGSIZE : sz - i;
        mappages(pt, i, (uint64)mem, PGSIZE, PTE_USER);
        memmove(mem, src + i, copy_size);
    }
    // mem = pmem_alloc_pages(1);
    // mappages(pt, i, (uint64)mem, PGSIZE, PTE_USER);

    alloc_vma_stack(p);
}

/**
 * @brief       复制用户态页表,在fork中使用
 *              复制父进程的页表内容到子进程
 * @param old   父进程页表
 * @param new   子进程页表
 * @param sz    需要复制的内存大小（字节）
 * @return int  成功返回0，失败返回-1（并回滚已映射的页）
 */
int uvmcopy(pgtbl_t old, pgtbl_t new, uint64 sz)
{
    // 获取虚拟内存锁
    acquire(&vmem_lock);
    
    pte_t *pte;
    uint flags;
    char *mem;
    uint64 pa, i = PGROUNDDOWN(myproc()->virt_addr);
    int j = 0;
    if (i >= 0x100UL)
    {
        while (j < 0x100UL)
        {
                    if ((pte = walk_internal(old, j, 0)) == NULL) ///< 查找父进程页表中对应的PTE
        {
            j += PGSIZE;
            continue;
        }
        if ((*pte & PTE_V) == 0)
        {
            j += PGSIZE;
            // panic(" uvmcopt: pte is not valid");
            continue;
        }

        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte);
        if ((mem = pmem_alloc_pages(1)) == NULL) ///< 为子进程分配新物理页
            goto err;
        memmove(mem, (void *)(pa | dmwin_win0), PGSIZE);        ///< 复制父进程页内容到子进程页
        if (mappages_internal(new, j, (uint64)mem, PGSIZE, flags) == -1) ///< 将新页映射到子进程页表
        {
            pmem_free_pages(mem, 1);
            goto err;
        }
            j += PGSIZE;
        }
    }
#ifdef RISCV
    for (int i = 0; i < 1; i++)
    {
        pte = walk_internal(old, 0x000000010000036e, 0);
        if (pte == NULL)
            break;
        if ((*pte & PTE_V) == 0)
        {
            break;
            // panic(" uvmcopt: pte is not valid");
        }
        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte);
        if ((mem = pmem_alloc_pages(1)) == NULL) ///< 为子进程分配新物理页
            goto err;
        memmove(mem, (void *)(pa | dmwin_win0), PGSIZE);                         ///< 复制父进程页内容到子进程页
        if (mappages_internal(new, 0x000000010000036e, (uint64)mem, PGSIZE, flags) == -1) ///< 将新页映射到子进程页表
        {
            pmem_free_pages(mem, 1);
            goto err;
        }
    }

#endif

    while (i < sz)
    {
        if ((pte = walk_internal(old, i, 0)) == NULL) ///< 查找父进程页表中对应的PTE
        {
            i += PGSIZE;
            continue;
        }
        if ((*pte & PTE_V) == 0)
        {
            i += PGSIZE;
            // panic(" uvmcopt: pte is not valid");
            continue;
        }

        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte);
        if ((mem = pmem_alloc_pages(1)) == NULL) ///< 为子进程分配新物理页
            goto err;
        memmove(mem, (void *)(pa | dmwin_win0), PGSIZE);        ///< 复制父进程页内容到子进程页
        if (mappages_internal(new, i, (uint64)mem, PGSIZE, flags) == -1) ///< 将新页映射到子进程页表
        {
            pmem_free_pages(mem, 1);
            goto err;
        }
        i += PGSIZE;
    }
    
    // 释放锁并返回成功
    release(&vmem_lock);
    return 0;
err:
    vmunmap(new, 0, i / PGSIZE, 1);
    release(&vmem_lock);
    return -1;
}

int fetchaddr(uint64 addr, uint64 *ip)
{
    struct proc *p = myproc();
    // if (addr >= p->sz || addr + sizeof(uint64) > p->sz)
    //     return -1;
    if (copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
        return -1;
    return 0;
}

int fetchstr(uint64 addr, char *buf, int max)
{
    struct proc *p = myproc();
    int err = copyinstr(p->pagetable, buf, addr, max);
    if (err < 0)
        return err;
    return strlen(buf);
}

/**
 * @brief 从用户空间复制数据到内核空间
 *
 * @param pt    用户页表指针
 * @param dst   内核目标地址
 * @param srcva 用户空间源虚拟地址 , 不要求对齐
 * @param len
 * @return int 成功返回0，失败返回-1
 */
int copyin(pgtbl_t pt, char *dst, uint64 srcva, uint64 len)
{
    if (srcva > MAXVA)
        panic("copyin:va:%p > MAXVA", srcva);
    uint64 n, va0, pa0;
    while (len > 0)
    {
        va0 = PGROUNDDOWN(srcva);
        pa0 = walkaddr(pt, va0);
        if (pa0 == 0)
            return -1;
        n = PGSIZE - (srcva - va0);
        if (n > len)
            n = len;
        memmove(dst, (void *)((pa0 + (srcva - va0)) | dmwin_win0), n); ///< 执行复制操作，将用户空间数据复制到内核空间,需要使用dmwin_win0进行地址映射转换

        len -= n;
        dst += n;
        srcva = va0 + PGSIZE;
    }
    return 0;
}

/**
 * @brief 从用户页表虚拟地址复制空终止字符串到内核缓冲区
 *
 * @param pt    用户页表指针
 * @param dst   目标内核缓冲区地址
 * @param srcva 源用户虚拟地址,可不对齐
 * @param max   最大允许复制的字节数
 * @return int  成功返回复制的总字节数（含终止符），失败返回-1
 */
int copyinstr(pgtbl_t pt, char *dst, uint64 srcva, uint64 max)
{
    uint64 n, va0, pa0;
    int got_null = 0, len = 0;

    /*循环直到遇到终止符或达到max限制*/
    while (got_null == 0 && max > 0)
    {
        va0 = PGROUNDDOWN(srcva);
        pa0 = walkaddr(pt, va0);
        if (pa0 == 0)
            return -1;
        n = PGSIZE - (srcva - va0); ///< 计算当前页可复制的字节数（处理非对齐和跨页）
        if (n > max)
            n = max;

#if defined RISCV
        char *p = (char *)(pa0 + (srcva - va0)); ///< 定位源字符串的物理内存位置
#else
        char *p = (char *)((pa0 + (srcva - va0)) | dmwin_win0); // DMWIN_MASK
#endif
        while (n > 0) ///< 逐字节复制
        {
            if (*p == '\0')
            {
                *dst = '\0';
                got_null = 1;
                break;
            }
            else
            {
                *dst = *p;
            }
            --n;
            --max;
            p++;
            dst++;
            len++;
        }

        srcva = va0 + PGSIZE;
    }
    return len;
}

/**
 * @brief  从内核空间复制数据到用户空间
 *
 * @param pt
 * @param dstva 用户空间目标虚拟地址,不要求对齐
 * @param src   内核源地址
 * @param len
 * @return int 成功返回0，失败返回-1
 */
int copyout(pgtbl_t pt, uint64 dstva, char *src, uint64 len)
{
    if (dstva > MAXVA)
        panic("copyout: dstva > MAXVA");

    uint64 n, va0, pa0;

    while (len > 0)
    {
        va0 = PGROUNDDOWN(dstva);
        pa0 = walkaddr(pt, va0);
        if (pa0 == 0)
            return -1;
        n = PGSIZE - (dstva - va0); // n is the remain of the page
        if (n > len)
            n = len;
        memmove((void *)((pa0 + (dstva - va0)) | dmwin_win0), src, n);

        len -= n;
        src += n;
        dstva = va0 + PGSIZE;
    }
    return 0;
}

/**
 * @brief 为进程分配并映射新的物理内存页
 *
 * @param pt
 * @param oldsz 原内存大小
 * @param newsz 新内存大小
 * @param perm
 * @return uint64 成功返回新内存大小，失败返回0
 */
uint64 uvmalloc(pgtbl_t pt, uint64 oldsz, uint64 newsz, int perm)
{
    // 获取虚拟内存锁
    acquire(&vmem_lock);
    
    char *mem;
    uint64 a;
    if (newsz < oldsz)
    {
        release(&vmem_lock);
        return oldsz; ///< 如果新大小小于原大小，直接返回原大小(不处理收缩)
    }
    oldsz = PGROUNDUP(oldsz);
    uint64 npages = (PGROUNDUP(newsz) - oldsz) / PGSIZE;
    // 首先尝试多页分配
    if (npages > 0)
    {
        mem = pmem_alloc_pages(npages);
        if (mem)
        {
            memset(mem, 0, npages * PGSIZE);
            if (mappages_internal(pt, oldsz, (uint64)mem, npages * PGSIZE, perm | PTE_U | PTE_D) == 1)
            {
                release(&vmem_lock);
                return newsz; // 映射成功直接返回
            }
            pmem_free_pages(mem, npages); // 映射失败释放内存
            release(&vmem_lock);
            return -EINVAL;
        }
    }
    // 多页分配失败则逐页分配
    for (a = oldsz; a < newsz; a += PGSIZE)
    {
        mem = pmem_alloc_pages(1);
        if (mem == NULL)
        {
            release(&vmem_lock);
            uvmdealloc(pt, a, oldsz);
            return 0;
        }
        memset(mem, 0, PGSIZE);
        if (mappages_internal(pt, a, (uint64)mem, PGSIZE, perm | PTE_U | PTE_D) != 1)
        {
            pmem_free_pages(mem, 1);
            uvmdealloc(pt, a, oldsz);
            release(&vmem_lock);
            return 0;
        }
    }
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[uvmgrow]:%p -> %p\n", oldsz, newsz);
#endif
    release(&vmem_lock);
    return newsz;
}

uint64 uvmalloc1(pgtbl_t pt, uint64 start, uint64 end, int perm)
{
    // 获取虚拟内存锁
    acquire(&vmem_lock);
    
    char *mem;
    uint64 a;
    assert(start < end, "uvmalloc1:start < end");
    uint64 npages = (PGROUNDUP(end) - start) / PGSIZE;
    // 首先尝试多页分配
    if (npages > 0)
    {
        mem = pmem_alloc_pages(npages);
        if (mem)
        {
            memset(mem, 0, npages * PGSIZE);
            if (mappages_internal(pt, start, (uint64)mem, npages * PGSIZE, perm | PTE_U | PTE_D) == 1)
            {
                release(&vmem_lock);
                return 1; // 映射成功直接返回
            }
            pmem_free_pages(mem, npages); // 映射失败释放内存
        }
    }
    for (a = start; a < end; a += PGSIZE)
    {
        mem = pmem_alloc_pages(1);
        if (mem == NULL)
        {
            uvmdealloc1(pt, start, a);
            panic("pmem alloc error\n");
            release(&vmem_lock);
            return 0;
        }
        memset(mem, 0, PGSIZE);
        if (mappages_internal(pt, a, (uint64)mem, PGSIZE, perm | PTE_U | PTE_D) != 1)
        {
            pmem_free_pages(mem, 1);
            uvmdealloc1(pt, start, a);
            release(&vmem_lock);
            return 0;
        }
    }
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[uvmgrow]:%p -> %p\n", start, end);
#endif
    release(&vmem_lock);
    return 1;
}

/**
 * @brief  释放进程的内存页并解除映射
 *
 * @param pt
 * @param oldsz     原内存大小
 * @param newsz     新内存大小
 * @return uint64   返回调整后的内存大小
 */
uint64 uvmdealloc(pgtbl_t pt, uint64 oldsz, uint64 newsz)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG,"[dealloc]:oldsz:%p newsz:%p",oldsz,newsz);
    if (newsz >= oldsz)
        return oldsz; ///< 如果新大小大于等于原大小，无需操作
    if (PGROUNDUP(newsz) < PGROUNDUP(oldsz))
    {
        int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
        free_vma(myproc(), newsz, oldsz);
        vmunmap(pt, PGROUNDUP(newsz), npages, 1);
    }
    return newsz;
}

uint64 uvmdealloc1(pgtbl_t pt, uint64 start, uint64 end)
{

    assert(start < end, "uvmdealloc1:start < end");
    if (PGROUNDUP(start) <= PGROUNDUP(end))
    {
        int npages = (PGROUNDUP(end) - PGROUNDUP(start)) / PGSIZE;
        vmunmap(pt, PGROUNDUP(start), npages, 1);
    }
    return 0;
}

uint64 uvm_grow(pgtbl_t pagetable, uint64 oldsz, uint64 newsz, uint64 xperm)
{
    if (newsz <= oldsz)
        return oldsz;
    char *mem;
    oldsz = PGROUNDUP(oldsz);
    uint64 npages = (PGROUNDUP(newsz) - oldsz) / PGSIZE;
    // 首先尝试多页分配
    if (npages > 0)
    {
        mem = pmem_alloc_pages(npages);
        if (mem)
        {
            memset(mem, 0, npages * PGSIZE);
            if (mappages(pagetable, oldsz, (uint64)mem, npages * PGSIZE, xperm | PTE_U | PTE_D) == 1)
            {
                return newsz; // 映射成功直接返回
            }
            pmem_free_pages(mem, npages); // 映射失败释放内存
        }
    }
    for (uint64 cur_page = oldsz; cur_page < newsz; cur_page += PGSIZE)
    {
        mem = pmem_alloc_pages(1);
        if (mem == NULL)
        {
            uvmdealloc(pagetable, cur_page, oldsz);
            return 0;
        }
        memset(mem, 0, PGSIZE);
        if (mappages(pagetable, cur_page, (uint64)mem, PGSIZE, xperm | PTE_U | PTE_D) != 1)
        {
            pmem_free_pages(mem, 1);
            uvmdealloc(pagetable, cur_page, oldsz);
            return 0;
        }
    }
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[uvmgrow]:%p -> %p\n", oldsz, newsz);
#endif
    return newsz;
}

/**
 * @brief 验证用户空间地址的有效性（类似Linux的access_ok）
 * 
 * @param type 访问类型（VERIFY_READ/VERIFY_WRITE）
 * @param addr 用户空间地址
 * @param size 访问大小
 * @return int 1表示有效，0表示无效
 */

 int access_ok(int type, uint64 addr, uint64 size)
 {
     // 检查地址范围
     if (addr > MAXVA || addr + size > MAXVA || addr + size < addr)
         return 0;
     
     // 对于写操作，需要检查页表项是否存在且可写
     if (type == VERIFY_WRITE) {
         proc_t *p = myproc();
         if (!p || !p->pagetable)
             return 0;
         
         // 检查整个地址范围的所有页
         uint64 end_addr = addr + size;
         uint64 current_addr = PGROUNDDOWN(addr);
         
         while (current_addr < end_addr) {
             pte_t *pte = walk(p->pagetable, current_addr, 0);
             
             // 检查页表项是否存在且有效
             if (!pte || (*pte & PTE_V) == 0) {
                 return 0;  // 页表项不存在或无效
             }
             
             // 检查用户权限
             if ((*pte & PTE_U) == 0) {
                 return 0;  // 不是用户页
             }
             
             // 对于写操作，检查写权限
 #ifdef RISCV
             if (type == VERIFY_WRITE && (*pte & PTE_W) == 0) {
                 return 0;  // 没有写权限
             }
 #else
             if (type == VERIFY_WRITE && (*pte & PTE_W) == 0) {
                 return 0;  // 没有写权限
             }
 #endif
             
             current_addr += PGSIZE;
         }
     }
 
     // 对于读操作，需要检查页表项是否存在且可读
     if (type == VERIFY_READ) {
         proc_t *p = myproc();
         if (!p || !p->pagetable)
             return 0;
         
         // 检查整个地址范围的所有页
         uint64 end_addr = addr + size;
         uint64 current_addr = PGROUNDDOWN(addr);
         
         while (current_addr < end_addr) {
             pte_t *pte = walk(p->pagetable, current_addr, 0);
             
             // 检查页表项是否存在且有效
             if (!pte || (*pte & PTE_V) == 0) {
                 return 0;  // 页表项不存在或无效
             }
             
             // 检查用户权限
             if ((*pte & PTE_U) == 0) {
                 return 0;  // 不是用户页
             }
             
             // 对于读操作，检查读权限
 #ifdef RISCV
             if ((*pte & PTE_R) == 0) {
                 return 0;  // 没有读权限
             }
 #else         
             if ((*pte & PTE_NR) != 0) {
                 return 0;  // 没有读权限
             }
 #endif
             current_addr += PGSIZE;
         }
     }
     
     return 1;  // 所有检查通过
 }

