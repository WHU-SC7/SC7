#include "vma.h"
#include "string.h"
#include "pmem.h"
#include "cpu.h"
#include "vmem.h"
#include "types.h"
#include "vfs_ext4.h"
#include "ext4_oflags.h"
#include "errno-base.h"
#include "slab.h"
#include "process.h"

// 全局变量定义
int sharemem_start = VM_SHARE_MEMORY_REGION;
struct shmid_kernel *shm_segs[SHMMNI];
int shmid = 1;
#ifdef RISCV
#include "riscv.h"
#include "riscv_memlayout.h"
#else
#include "loongarch.h"
#endif

void shm_init()
{
    for (int i = 0; i < SHMMNI; i++)
    {
        shm_segs[i] = NULL;
    }
}
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
 *    - `PROT_NONE` → 返回0（无权限）
 * 2. 其他架构（如 LA64）：
 *    - `PROT_READ` → 清除不可读标志 `PTE_NR`
 *    - `PROT_WRITE` → 设置可写标志 `PTE_W`
 *    - `PROT_EXEC` → 清除不可执行标志 `PTE_NX` 并设置访问标志 `PTE_MAT | PTE_P`
 *    - `PROT_NONE` → 返回默认权限（不可读不可写不可执行）
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
        return 0; // PROT_NONE: 无任何权限
    if (prot & PROT_READ)
        perm |= PTE_R;
    if (prot & PROT_WRITE)
        perm |= PTE_W;
    if (prot & PROT_EXEC)
        perm |= PTE_X;
#else
    // LoongArch架构：默认权限为不可读不可写不可执行
    perm = PTE_PLV3 | PTE_MAT | PTE_D | PTE_NR | PTE_NX;
    if (prot == PROT_NONE)
        return perm; // PROT_NONE: 保持默认权限（不可读不可写不可执行）
    if (prot & PROT_READ)
        // LA64架构下，0表示可读
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
    return experm(pagetable, va, perm);
}
/**
 * @brief 实现内存映射系统调用
 *
 * @param start     请求的映射起始地址（0表示由内核选择）
 * @param len       映射区域的长度
 * @param prot      内存保护标志（PROT_READ/PROT_WRITE/PROT_EXEC/PROT_NONE）
 * @param flags     映射类型标志（MAP_SHARED/MAP_PRIVATE/MAP_FIXED/MAP_ANONYMOUS）
 * @param fd        文件描述符（MAP_ANONYMOUS时为-1）
 * @param offset    文件偏移量（字节对齐）
 * @return uint64   成功返回映射起始地址，失败返回(uint64)-1
 */
/*
1. MAP_SHARED (共享映射)
    对映射区域的修改会同步到底层文件（或共享内存）
    多个进程映射同一文件时，内存变更互相可见
    适用于进程间通信(IPC)
2. MAP_PRIVATE (私有映射)
    修改采用写时复制(COW) 机制
    进程修改的数据不会同步到文件
    不同进程的修改相互隔离
3. MAP_ANONYMOUS (匿名映射)
    不关联任何文件 (fd=-1)
    本质是纯内存分配
*/
uint64 mmap(uint64 start, int64 len, int prot, int flags, int fd, int offset)
{
    proc_t *p = myproc();

    // MAP_ANONYMOUS 优先处理：强制 fd = -1
    if (flags & MAP_ANONYMOUS)
        fd = -1;

    if (!(flags & (MAP_PRIVATE | MAP_SHARED | MAP_SHARED_VALIDATE)))
    {
        return -EINVAL;
    }
    // 当未指定 MAP_ANONYMOUS  fd 必须是有效的文件描述符
    if (!(flags & MAP_ANONYMOUS))
    {
        if (fd < 0)
        {
            return -EBADF;
        }
    }

    if (len == 0)
    {
        return -EINVAL;
    }
    // 权限检查条件修正：只有在非匿名映射且 fd != -1 时才进行文件权限检查
    if (!(flags & MAP_ANONYMOUS) && fd != -1)
    {
        struct file *f = p->ofile[fd];
        if (f == NULL)
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "mmap: invalid file descriptor %d\n", fd);
            return -EBADF;
        }

        // 检查文件是否以只写模式打开（O_WRONLY）
        if (f->f_flags & O_WRONLY && !(f->f_flags & O_RDWR))
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "mmap: file descriptor %d opened with O_WRONLY, no read permission\n", fd);
            return -EACCES;
        }

        // 检查文件是否可读
        if (!get_file_ops()->readable(f))
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "mmap: file descriptor %d is not readable\n", fd);
            return -EACCES;
        }
    }

    // 长度参数为 0 时的 EINVAL 错误
    if (len == 0)
    {
        DEBUG_LOG_LEVEL(LOG_WARNING, "mmap: length is 0, returning EINVAL\n");
        return -EINVAL;
    }

    // 无效标志组合时的 EINVAL 错误
    // 检查 flags 中是否包含有效的映射类型标志
    int valid_flags = flags & (MAP_PRIVATE | MAP_SHARED);
    if (valid_flags == 0)
    {
        DEBUG_LOG_LEVEL(LOG_WARNING, "mmap: invalid flags 0x%x, missing MAP_PRIVATE or MAP_SHARED\n", flags);
        return -EINVAL;
    }

    int perm = get_mmapperms(prot);
    struct file *f = fd == -1 ? NULL : p->ofile[fd];

    /* 文件描述符有效性检查：只有在非匿名映射且 fd != -1 时才检查 */
    if (!(flags & MAP_ANONYMOUS) && fd != -1 && f == NULL)
        return -1;

    /* 分配并初始化VMA结构 */
    struct vma *vma = alloc_mmap_vma(p, flags, start, len, perm, fd, offset);
    if (!(flags & MAP_FIXED))
        start = vma->addr; // 若无MAP_FIXED标志，使用内核选择的地址

    // 保存原始权限信息
    vma->orig_prot = prot;

    // 对于PROT_NONE权限，不立即分配物理页面，等待访问时按需处理
    if (prot == PROT_NONE)
    {
        if (vma == NULL)
            return -1;
        return start;
    }

    /********************* MAP_SHARED 处理 *********************/
    if (flags & MAP_SHARED)
    {
        DEBUG_LOG_LEVEL(LOG_DEBUG, "MAP_SHARED: creating shared memory mapping, fd=%d, offset=%d\n", fd, offset);

        // 为共享映射创建共享内存段
        int shmid = newseg(0, IPC_CREAT | 0666, len);
        if (shmid == -1)
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "MAP_SHARED: failed to create shared memory segment\n");
            return -1;
        }

        // 获取共享内存段
        struct shmid_kernel *shp = shm_segs[shmid];
        if (!shp)
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "MAP_SHARED: failed to get shared memory segment\n");
            return -1;
        }

        // 设置VMA的共享内存属性
        vma->type = SHARE;
        vma->shm_kernel = shp;

        // 将VMA添加到共享内存段的附加列表
        shp->attaches = vma;
        if(f){
            struct kstat stat;
            int ret = vfs_ext4_fstat(f, &stat);
            if (ret < 0)
            {
                return ret;
            }
            int fsize = stat.st_size;
            len = MIN(fsize,len);
            vma->fsize = fsize;
        }

        // 分配物理页面并建立映射
        for (uint64 i = 0; i < len; i += PGSIZE)
        {
            void *pa = pmem_alloc_pages(1);
            if (!pa)
            {
                DEBUG_LOG_LEVEL(LOG_WARNING, "MAP_SHARED: failed to allocate physical page\n");
                return -1;
            }

            // 清零物理页面
            memset(pa, 0, PGSIZE);

            // 建立映射
            if (mappages(p->pagetable, start + i, (uint64)pa, PGSIZE, perm | PTE_U | PTE_R) < 0)
            {
                pmem_free_pages(pa, 1);
                DEBUG_LOG_LEVEL(LOG_WARNING, "MAP_SHARED: failed to map page\n");
                return -1;
            }

            // 保存物理页面地址到共享内存段
            int idx = i / PGSIZE;
            shp->shm_pages[idx] = (pte_t)pa;

            DEBUG_LOG_LEVEL(LOG_DEBUG, "MAP_SHARED: mapped va=%p to pa=%p, idx=%d\n",
                            start + i, pa, idx);
        }

        // 同步共享内存段
        sync_shared_memory(shp);

        /* 文件内容读取（如果是文件映射） */
        if (fd != -1)
        {
            struct kstat stat;
            int ret = vfs_ext4_fstat(f, &stat);
            if (ret < 0)
            {
                return ret;
            }
            int fsize = stat.st_size;
            vma->fsize = fsize;
            uint64 orig_pos = f->f_pos;
            ret = vfs_ext4_lseek(f, offset, SEEK_SET);
            if (ret < 0)
            {
                DEBUG_LOG_LEVEL(LOG_WARNING, "MAP_SHARED: lseek failed, ret=%d\n", ret);
                return ret;
            }
            len = MIN(len, fsize);
            // 读取文件内容到共享内存
            for (uint64 i = 0; i < len; i += PGSIZE)
            {
                uint64 pa = experm(p->pagetable, start + i, perm);
                int remaining = len - i;
                int to_read = (remaining > PGSIZE) ? PGSIZE : remaining;

                if (to_read > 0)
                {
                    int bytes_read = get_file_ops()->read(f, start + i, to_read);
                    if (bytes_read < 0)
                    {
                        DEBUG_LOG_LEVEL(LOG_WARNING, "MAP_SHARED: read failed\n");
                        return -1;
                    }

                    // 文件内容不足时，填充零
                    if (bytes_read < to_read)
                    {
                        memset((void *)((pa + bytes_read) | dmwin_win0), 0, to_read - bytes_read);
                    }
                }

                // 页面剩余部分清零
                if (to_read < PGSIZE)
                {
                    memset((void *)((pa + to_read) | dmwin_win0), 0, PGSIZE - to_read);
                }
            }
            vfs_ext4_lseek(f, orig_pos, SEEK_SET);
        }
        if (f != NULL)
            get_file_ops()->dup(f); // 增加文件引用计数
        return start;
    }

    /********************* 匿名映射处理（MAP_ANONYMOUS）*********************/
    if (flags & MAP_ANONYMOUS || fd == -1)
    {
        // 对于MAP_ANONYMOUS映射，需要分配物理页面但不读取文件内容
        if (vma == NULL)
            return -1;

        // 分配物理页面并清零
        for (uint64 i = 0; i < len; i += PGSIZE)
        {
            uint64 pa = experm(p->pagetable, start + i, perm);
            if (pa == 0)
            {
                // 如果页面还没有分配，分配一个
                void *new_pa = pmem_alloc_pages(1);
                if (!new_pa)
                {
                    return -1;
                }
                memset(new_pa, 0, PGSIZE);

                // 建立映射
                if (mappages(p->pagetable, start + i, (uint64)new_pa, PGSIZE, perm) < 0)
                {
                    pmem_free_pages(new_pa, 1);
                    return -1;
                }
            }

            // 对于MAP_PRIVATE映射，设置写时复制标志
            if (flags & MAP_PRIVATE && (prot & PROT_WRITE))
            {
                pte_t *pte = walk(p->pagetable, start + i, 0);
                if (pte && (*pte & PTE_V))
                {
                    // 对于MAP_PRIVATE且需要写权限，清除写权限以启用写时复制
                    // 但保留读权限
                    *pte &= ~PTE_W;
                    // 在VMA中记录这是私有映射
                    vma->flags |= MAP_PRIVATE;
                    DEBUG_LOG_LEVEL(LOG_DEBUG, "MAP_PRIVATE: va=%p, cleared write permission\n", start + i);
                }
            }
        }

        return start;
    }

    /********************* 文件映射处理（非匿名映射）*********************/
    {
        int ret = vfs_ext4_lseek(f, offset, SEEK_SET); //< 设置文件位置指针到指定偏移量
        if (ret < 0)
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "lseek in pread failed!, ret is %d\n", ret);
            return ret;
        }
    }

    if (vma == NULL)
        return -1;
    assert(len, "len is zero!");

    uint64 i;
    uint64 orig_pos = f->f_pos;
    for (i = 0; i < len; i += PGSIZE) //< 从offset开始读len字节
    {
        uint64 pa = experm(p->pagetable, start + i, perm); //< 检查是否可以访问start + i，如果可以就返回start + i所在页的物理地址

        int remaining = len - i;
        int to_read = (remaining > PGSIZE) ? PGSIZE : remaining;

        // 读取文件内容（如果 to_read > 0）
        int bytes_read = 0;
        if (to_read > 0)
        {
            bytes_read = get_file_ops()->read(f, start + i, to_read);
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

        // 对于MAP_PRIVATE映射，设置写时复制标志
        if (flags & MAP_PRIVATE && (prot & PROT_WRITE))
        {
            pte_t *pte = walk(p->pagetable, start + i, 0);
            if (pte && (*pte & PTE_V))
            {
                // 对于MAP_PRIVATE且需要写权限，清除写权限以启用写时复制
                // 但保留读权限
                *pte &= ~PTE_W;
                // 在VMA中记录这是私有映射
                vma->flags |= MAP_PRIVATE;
                DEBUG_LOG_LEVEL(LOG_DEBUG, "MAP_PRIVATE: va=%p, cleared write permission\n", start + i);
            }
        }
    }
    vfs_ext4_lseek(f, orig_pos, SEEK_SET);
    if (f != NULL)
        get_file_ops()->dup(f);
    return start;
}

/**
 * @brief 处理写时复制（Copy-on-Write）
 * @details 当MAP_PRIVATE映射发生写访问时，需要复制页面并设置写权限
 *
 * @param p 当前进程
 * @param va 发生写访问的虚拟地址
 * @return 0表示成功，-1表示失败
 */
int handle_cow_write(proc_t *p, uint64 va)
{
    // 查找对应的VMA
    struct vma *vma = p->vma->next;
    while (vma != p->vma)
    {
        if (va >= vma->addr && va < vma->end)
        {
            // 检查是否是私有映射
            if (vma->flags & MAP_PRIVATE)
            {
                pte_t *pte = walk(p->pagetable, va, 0);
                if (pte && (*pte & PTE_V))
                {
                    // 分配新的物理页面
                    void *new_pa = pmem_alloc_pages(1);
                    if (!new_pa)
                    {
                        return -1;
                    }

                    // 复制原页面内容
                    uint64 old_pa = PTE2PA(*pte) | dmwin_win0;
                    memmove(new_pa, (void *)old_pa, PGSIZE);

                    // 更新页表项，设置写权限
                    uint64 flags = PTE_FLAGS(*pte) | PTE_W;
                    *pte = PA2PTE((uint64)new_pa) | flags | PTE_V;

                    // DEBUG_LOG_LEVEL(LOG_DEBUG, "COW: va=%p, old_pa=%p, new_pa=%p\n",va, old_pa, new_pa);

                    return 0;
                }
            }
            break;
        }
        vma = vma->next;
    }
    return -1;
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
        struct vma *next_vma = vma->next; // 在修改链表之前就保存下一个节点
        uint64 vma_start = vma->addr;
        uint64 vma_end = vma->end;

        // 判断是否与当前VMA重叠
        if (vma_end > start && vma_start <= end)
        {
            found = 1;
            // 情况1：当前VMA完全在解除范围内
            if (vma_start >= start && vma_end <= end)
            {
                // 特殊处理共享内存类型的VMA
                if (vma->type == SHARE && vma->shm_kernel)
                {
                    // 对于共享内存，只解除映射，不释放物理页面
                    vmunmap(p->pagetable, vma_start, (vma_end - vma_start) / PGSIZE, 0);

                    // 从共享内存段的附加列表中移除
                    struct shmid_kernel *shp = vma->shm_kernel;

                    // 安全检查：确保shp和attaches有效
                    if (shp && shp->attaches)
                    {
                        // 如果attaches指向当前VMA，直接设置为NULL
                        if (shp->attaches == vma)
                        {
                            shp->attaches = NULL;
                        }
                        else
                        {
                            // 遍历attaches链表查找当前VMA
                            struct vma *current = shp->attaches;
                            while (current && current->next != vma)
                            {
                                current = current->next;
                                // 防止无限循环
                                if (current == shp->attaches)
                                    break;
                            }

                            if (current && current->next == vma)
                            {
                                current->next = vma->next;
                            }
                        }
                    }

                    DEBUG_LOG_LEVEL(LOG_DEBUG, "[munmap] SHARE: detached vma from shmid=%d\n", shp->shmid);
                }
                else
                {
                    // 对于非共享内存，释放物理内存和页表项
                    vmunmap(p->pagetable, vma_start, (vma_end - vma_start) / PGSIZE, 1);
                }

                // 从链表中移除VMA
                vma->prev->next = vma->next;
                vma->next->prev = vma->prev;
                pmem_free_pages(vma, 1); // 释放VMA结构体
            }
            // 情况2：仅部分重叠（需分割VMA）
            else if (vma_start < start || vma_end > end)
            {
                // 保存共享内存信息，因为原VMA将被释放
                struct shmid_kernel *shm_kernel_backup = NULL;
                if (vma->type == SHARE && vma->shm_kernel)
                {
                    shm_kernel_backup = vma->shm_kernel;
                }

                // 先移除原VMA，避免在创建新VMA时干扰链表结构
                vma->prev->next = vma->next;
                vma->next->prev = vma->prev;

                // 分割为前段和后段，中间部分解除映射
                if (vma_start < start)
                {
                    // 创建前段VMA（保留start之前的区域）
                    struct vma *new_front = (struct vma *)pmem_alloc_pages(1);
                    if (new_front == NULL)
                    {
                        panic("munmap: failed to allocate front VMA");
                        return -1;
                    }

                    // 复制原VMA的属性
                    *new_front = *vma;
                    new_front->addr = vma_start;
                    new_front->end = start;
                    new_front->size = start - vma_start;

                    // 插入到原VMA的位置
                    new_front->prev = vma->prev;
                    new_front->next = vma->next;
                    vma->prev->next = new_front;
                    vma->next->prev = new_front;
                }

                if (vma_end > end)
                {
                    // 创建后段VMA（保留end之后的区域）
                    struct vma *new_back = (struct vma *)pmem_alloc_pages(1);
                    if (new_back == NULL)
                    {
                        panic("munmap: failed to allocate back VMA");
                        return -1;
                    }

                    // 复制原VMA的属性
                    *new_back = *vma;
                    new_back->addr = end;
                    new_back->end = vma_end;
                    new_back->size = vma_end - end;
                    new_back->f_off = vma->f_off + (end - vma_start);

                    // 插入到链表中
                    if (vma_start < start)
                    {
                        // 如果前段存在，插入到前段之后
                        struct vma *front = vma->prev->next; // 新创建的前段
                        new_back->prev = front;
                        new_back->next = front->next;
                        front->next->prev = new_back;
                        front->next = new_back;
                    }
                    else
                    {
                        // 如果前段不存在，插入到原位置
                        new_back->prev = vma->prev;
                        new_back->next = vma->next;
                        vma->prev->next = new_back;
                        vma->next->prev = new_back;
                    }
                }

                // 解除重叠部分的映射
                uint64 unmap_start = (vma_start > start) ? vma_start : start;
                uint64 unmap_end = (vma_end < end) ? vma_end : end;
                if (unmap_end > unmap_start)
                {
                    // 特殊处理共享内存类型的VMA
                    if (shm_kernel_backup)
                    {
                        // 对于共享内存，只解除映射，不释放物理页面
                        vmunmap(p->pagetable, unmap_start, (unmap_end - unmap_start) / PGSIZE, 0);
                        DEBUG_LOG_LEVEL(LOG_DEBUG, "[munmap] SHARE: partial unmapped va=%p to %p\n", unmap_start, unmap_end);

                        // 注意：对于部分重叠的共享内存VMA，我们不应该从attaches链表中移除
                        // 因为VMA仍然存在，只是大小被调整了
                    }
                    else
                    {
                        // 对于非共享内存，释放物理内存和页表项
                        vmunmap(p->pagetable, unmap_start, (unmap_end - unmap_start) / PGSIZE, 1);
                    }
                }

                // 释放原VMA
                pmem_free_pages(vma, 1);
            }
        }

        // 移动到下一个VMA (使用之前保存的next_vma)
        vma = next_vma;
    }

    return found ? 0 : -1; // 返回成功或失败
}

struct vma *alloc_mmap_vma(struct proc *p, int flags, uint64 start, int64 len, int perm, int fd, int offset)
{
    struct vma *vma = NULL;
    struct vma *find_vma = find_mmap_vma(p->vma);

    // 检查find_mmap_vma是否返回NULL
    if (find_vma == NULL)
    {
        // 如果没有找到MMAP类型的VMA，使用链表头作为插入点
        find_vma = p->vma;
    }

    if (start == 0 && len < find_vma->addr)
        start = PGROUNDDOWN(find_vma->addr - len);

    int isalloc = 0;
    if ((flags & MAP_ALLOC) || ((fd != -1) && perm))
        isalloc = 1;
    if (flags & 0x01)
        isalloc = 0;
    vma = alloc_vma(p, MMAP, start, len, perm, isalloc, 0);
    if (vma == NULL)
    {
        panic("alloc_mmap_vma");
        return NULL;
    }
    vma->fd = fd;
    vma->f_off = offset;
    vma->orig_prot = 0; // 初始化为0，在mmap中会被正确设置
    return vma;
}

struct vma *alloc_vma(struct proc *p, enum segtype type, uint64 addr, int64 sz, int perm, int alloc, uint64 pa)
{
    // 添加空指针检查
    if (p == NULL || p->vma == NULL)
    {
        panic("alloc_vma: invalid process or VMA list");
        return NULL;
    }

    uint64 start = PGROUNDUP(addr);
    uint64 end = PGROUNDUP(addr + sz);

    // uint64 start = PGROUNDUP(p->sz);
    // uint64 end = PGROUNDUP(p->sz + sz);
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[allocvma] : start:%p,end:%p,sz:%p,perm:%x\n", start, start + sz, sz, perm);
#endif
    // p->sz += sz;
    // p->sz = PGROUNDUP(p->sz);
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
    vma->fsize = 0; // 初始化为0，文件映射时会设置
    vma->type = type;
    vma->shm_kernel = NULL; // 初始化为NULL，由调用者设置
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
        if ((vma->type == MMAP) || (vma->type == SHARE))
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
    vma->perm = PTE_R | PTE_W;
    vma->addr = start;
    vma->end = end;
    vma->size = USER_STACK_SIZE;
    vma->flags = 0;
    vma->fd = -1;
    vma->f_off = -1;
    vma->fsize = 0; // 栈VMA不需要文件大小
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
        if (nvma->type == MMAP && nvma->fd != -1)
        {
            struct file *f = np->ofile[nvma->fd];
            if (f)
            {
                get_file_ops()->dup(f);
            }
        }
        // 共享内存段不需要复制，只需要保持指针引用
        // 但需要验证shm_kernel指针的有效性
        if (nvma->type == SHARE && nvma->shm_kernel)
        {
            // 验证共享内存段是否仍然有效
            if (nvma->shm_kernel->is_deleted || !nvma->shm_kernel->shm_pages)
            {
                // 如果共享内存段已被标记删除或shm_pages为NULL，清除引用
                DEBUG_LOG_LEVEL(LOG_WARNING, "[vma_copy] cleared reference to invalid shm_kernel (deleted=%d, shm_pages=%p)\n",
                                nvma->shm_kernel->is_deleted, nvma->shm_kernel->shm_pages);
                nvma->shm_kernel = NULL;
            }
        }
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
    // 特殊处理共享内存类型的VMA
    if (vma->type == SHARE && vma->shm_kernel)
    {
        // 对于共享内存，我们需要确保子进程能够访问相同的共享内存段
        struct shmid_kernel *shp = vma->shm_kernel;

        // 安全检查：确保共享内存段和页表数组有效
        if (!shp || !shp->shm_pages)
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "[vma_map] SHARE: invalid shm_kernel or shm_pages, skipping this VMA\n");
            return 0; // 跳过这个VMA，而不是返回错误
        }

        uint64 start = vma->addr;

        DEBUG_LOG_LEVEL(LOG_DEBUG, "[vma_map] SHARE vma: addr=%p, size=%p, shmid=%d\n",
                        vma->addr, vma->size, shp->shmid);

        // 遍历共享内存段的所有页面
        int num_pages = (vma->size + PGSIZE - 1) / PGSIZE;
        int shm_num_pages = (shp->size + PGSIZE - 1) / PGSIZE;

        for (int i = 0; i < num_pages; i++)
        {
            uint64 va = start + i * PGSIZE;

            // 首先检查目标页表是否已经存在映射
            pte_t *existing_pte = walk(new, va, 0);
            if (existing_pte && (*existing_pte & PTE_V))
            {
                // 目标页表已经存在映射，跳过这个页面
                DEBUG_LOG_LEVEL(LOG_DEBUG, "[vma_map] SHARE: va=%p already mapped, skipping\n", va);
                continue;
            }

            // 检查共享内存段中是否已经分配了物理页面
            if (i < shm_num_pages && shp->shm_pages[i] != 0)
            {
                // 共享内存段已经有物理页面，直接映射
                uint64 pa = (uint64)shp->shm_pages[i];
                uint64 flags = vma->perm | PTE_U;

                if (mappages(new, va, pa, PGSIZE, flags) != 1)
                {
                    panic("vma_map: shared memory mapping failed");
                }

                DEBUG_LOG_LEVEL(LOG_DEBUG, "[vma_map] SHARE: mapped va=%p to existing pa=%p\n", va, pa);
            }
            else
            {
                // 共享内存段还没有物理页面，检查父进程是否已经映射
                pte_t *pte = walk(old, va, 0);
                if (pte && (*pte & PTE_V))
                {
                    // 父进程已经映射了这个页面，子进程也应该映射相同的物理页面
                    uint64 pa = PTE2PA(*pte);
                    uint64 flags = PTE_FLAGS(*pte);

                    // 直接映射相同的物理页面，而不是分配新的
                    if (mappages(new, va, pa, PGSIZE, flags) != 1)
                    {
                        panic("vma_map: shared memory mapping failed");
                    }

                    // 保存物理页面地址到共享内存段（确保索引在范围内）
                    if (i < shm_num_pages)
                    {
                        shp->shm_pages[i] = (pte_t)pa;
                    }

                    DEBUG_LOG_LEVEL(LOG_DEBUG, "[vma_map] SHARE: mapped va=%p to inherited pa=%p\n", va, pa);
                }
            }

            // 对于RISC-V，添加内存屏障确保映射生效
#ifdef RISCV
            sfence_vma();
#endif
        }

        // 同步共享内存段
        sync_shared_memory(shp);

        return 0;
    }

    // 对于其他类型的VMA，使用原来的复制逻辑
    uint64 start = vma->addr;
    pte_t *pte, *new_pte;
    uint64 pa;
    char *mem;
    long flags;
    while (start < vma->end)
    {
        pte = walk(old, start, 0);
        if (pte == NULL)
        {
            // LazyLoad VMA: 页面可能还没有分配，跳过
            start += PGSIZE;
            continue;
        }
        if ((*pte & PTE_V) == 0)
        {
            // LazyLoad VMA: 页面无效，跳过
            start += PGSIZE;
            continue;
        }

        // 检查目标页表项是否已经存在（避免重复映射）
        new_pte = walk(new, start, 0);
        if (new_pte != NULL && (*new_pte & PTE_V))
        {
            // 目标页表已经存在映射，跳过这个页面
            start += PGSIZE;
            continue;
        }

        pa = PTE2PA(*pte) | dmwin_win0;
        flags = PTE_FLAGS(*pte);

        if ((mem = pmem_alloc_pages(1)) == 0)
        {
            panic("vma_map: pmem_alloc_pages failed");
        }

        memmove(mem, (char *)pa, PGSIZE);
        if (mappages(new, start, (uint64)mem, PGSIZE, flags) != 1)
        {
            pmem_free_pages(mem, 1);
            panic("vma_map: mappages failed");
        }

        start += PGSIZE;
    }

    return 0;
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

        // 检查是否为共享内存VMA
        int is_shared_memory = (vma->type == SHARE);

        for (a = vma->addr; a < vma->end; a += PGSIZE)
        {
            if ((pte = walk(p->pagetable, a, 0)) == NULL)
                continue;
            if ((*pte & PTE_V) == 0)
                continue;
            if (PTE_FLAGS(*pte) == PTE_V)
                continue;

            if (is_shared_memory)
            {
                // 对于共享内存，只清除PTE，不释放物理内存
                *pte = 0;
            }
            else
            {
                // 对于普通内存，释放物理内存
                uint64 pa = PTE2PA(*pte) | dmwin_win0;
                pmem_free_pages((void *)pa, 1);
                *pte = 0;
            }
        }
        vma = vma->next;
        pmem_free_pages(vma->prev, 1);
    }
    pmem_free_pages(vma, 1);
    p->vma = NULL;
    return 1;
}

int free_vma(struct proc *p, uint64 start, uint64 end)
{
    struct vma *vma_head = p->vma;
    if (!vma_head || !vma_head->next)
        return -1;

    struct vma *vma = vma_head->next;
    while (vma != vma_head)
    {
        struct vma *next_vma = vma->next;

        // 检查是否有重叠
        if (vma->end > start && vma->addr < end)
        {
            // 情况1：完全在释放范围内
            if (vma->addr >= start && vma->end <= end)
            {
                // 从链表移除
                vma->prev->next = vma->next;
                vma->next->prev = vma->prev;
                pmem_free_pages(vma, 1);
            }
            // 情况2：部分重叠（需要拆分）
            else
            {
                // 左侧非重叠部分
                if (vma->addr < start)
                {
                    // 创建新的左侧 VMA
                    struct vma *left = (struct vma *)pmem_alloc_pages(1);
                    if (!left)
                        panic("free_vma: pmem_alloc_pages failed");

                    memcpy(left, vma, sizeof(struct vma));
                    left->size = start - vma->addr;
                    left->end = start;

                    // 插入链表
                    left->prev = vma->prev;
                    left->next = vma;
                    vma->prev->next = left;
                    vma->prev = left;
                }

                // 右侧非重叠部分
                if (vma->end > end)
                {
                    // 创建新的右侧 VMA
                    struct vma *right = (struct vma *)pmem_alloc_pages(1);
                    if (!right)
                        panic("free_vma: pmem_alloc_pages failed");

                    memcpy(right, vma, sizeof(struct vma));
                    right->addr = end;
                    right->size = vma->end - end;
                    right->f_off = vma->f_off + (end - vma->addr); // 调整文件偏移

                    // 插入链表
                    right->prev = vma;
                    right->next = vma->next;
                    vma->next->prev = right;
                    vma->next = right;
                }

                // 移除当前 VMA（重叠部分）
                vma->prev->next = vma->next;
                vma->next->prev = vma->prev;
                pmem_free_pages(vma, 1);
            }
        }
        vma = next_vma;
    }
    return 1;
}

int newseg(int key, int shmflg, int size)
{
    if (key != IPC_PRIVATE)
    {
        if (findshm(key) >= 0)
        {
            return -EEXIST; // 键值已存在
        }
    }
    struct shmid_kernel *shp;
    shp = slab_alloc(sizeof(struct shmid_kernel));
    if (!shp)
    {
        return -ENOMEM;
    }
    // 分配系统 ID
    int sysid = allocshmid();
    if (sysid < 0)
    {
        slab_free((uint64)shp);
        return -ENOSPC;
    }

    // 获取当前进程信息
    struct proc *p = myproc();

    shp->shm_key = key;
    shp->shmid = sysid;
    shp->size = size;
    shp->flag = shmflg;
    shp->attach_count = 0;
    shp->is_deleted = 0;
    shp->attaches = NULL;

    // 初始化 shmid_ds 相关字段
    shp->shm_perm.key = key;
    shp->shm_perm.uid = p ? p->euid : 0;  // 所有者用户ID
    shp->shm_perm.gid = p ? p->egid : 0;  // 所有者组ID
    shp->shm_perm.cuid = p ? p->euid : 0; // 创建者用户ID
    shp->shm_perm.cgid = p ? p->egid : 0; // 创建者组ID
    shp->shm_perm.mode = shmflg & 0777;   // 权限模式
    shp->shm_perm.__seq = 0;
    shp->shm_perm.__pad2 = 0;
    shp->shm_perm.__glibc_reserved1 = 0;
    shp->shm_perm.__glibc_reserved2 = 0;

    shp->shm_segsz = size;
    shp->shm_atime = 0;             // 最后附加时间，暂时设为0
    shp->shm_dtime = 0;             // 最后分离时间，暂时设为0
    shp->shm_ctime = 0;             // 最后修改时间，暂时设为0
    shp->shm_cpid = p ? p->pid : 0; // 创建者PID
    shp->shm_lpid = p ? p->pid : 0; // 最后操作的PID
    shp->shm_nattch = 0;            // 当前附加数

    // 初始化附加记录链表
    shp->shm_attach_list = NULL;

    // 分配共享内存页表项数组
    int num_pages = (size + PGSIZE - 1) / PGSIZE; // 向上取整
    shp->shm_pages = slab_alloc(num_pages * sizeof(pte_t));
    if (!shp->shm_pages)
    {
        slab_free((uint64)shp);
        return -ENOMEM;
    }

    // 初始化页表项数组为0（表示未分配物理页）
    memset(shp->shm_pages, 0, num_pages * sizeof(pte_t));

    // 将共享内存段添加到全局数组
    shm_segs[shp->shmid] = shp;

    return shp->shmid;
}

int allocshmid()
{
    for (int i = 0; i < SHMMNI; i++)
    {
        if (shm_segs[i] == NULL)
        {
            return i;
        }
    }
    return -1;
}

int findshm(int key)
{
    for (int i = 0; i < SHMMNI; i++)
    {
        if (shm_segs[i] != NULL && shm_segs[i]->shm_key == key)
        {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 同步共享内存段，确保所有进程能看到最新的内存状态
 * @param shp 共享内存段指针
 */
void sync_shared_memory(struct shmid_kernel *shp)
{
    if (!shp)
        return;

    DEBUG_LOG_LEVEL(LOG_DEBUG, "[sync_shared_memory] syncing shmid=%d\n", shp->shmid);

#ifdef RISCV
    // RISC-V特定的内存同步操作
    // 刷新TLB
    sfence_vma();

    // 内存屏障确保所有内存操作对其他进程可见
    asm volatile("fence rw, rw" ::: "memory");

    // 指令缓存同步
    asm volatile("fence.i" ::: "memory");
#endif

    // 遍历所有附加到该共享内存段的进程，确保它们能看到最新的内存状态
    struct vma *vma = shp->attaches;
    if (vma) // 确保attaches不为NULL
    {
        struct vma *start_vma = vma;
        do
        {
            // 这里可以添加进程特定的同步逻辑
            // 目前主要通过内存屏障来确保一致性
            vma = vma->next;
        } while (vma && vma != start_vma); // 防止循环和空指针
    }
}

/**
 * @brief 内存同步系统调用实现
 *
 * @param addr  要同步的内存区域起始地址
 * @param len   要同步的区域长度
 * @param flags 同步标志（MS_ASYNC, MS_SYNC, MS_INVALIDATE）
 * @return int  成功返回0，失败返回-1
 */
int msync(uint64 addr, uint64 len, int flags)
{
    DEBUG_LOG_LEVEL(LOG_DEBUG, "[msync] addr: %p, len: %lu, flags: %d\n", addr, len, flags);

    struct proc *p = myproc();
    struct vma *vma = p->vma->next;
    int found = 0;

    // 查找地址对应的VMA
    while (vma != p->vma)
    {
        if (addr >= vma->addr && addr < vma->end)
        {
            found = 1;
            break;
        }
        vma = vma->next;
    }

    if (!found)
    {
        DEBUG_LOG_LEVEL(LOG_WARNING, "[msync] address %p not found in any VMA\n", addr);
        return -1;
    }

    // // 对于共享内存，执行同步操作
    // if (vma->type == SHARE && vma->shm_kernel)
    // {
    //     sync_shared_memory(vma->shm_kernel);
    // }

    return 0;
}

/**
 * @brief 检查当前进程是否为root用户
 * @return 1表示是root用户，0表示不是
 */
int is_root_user(void)
{
    struct proc *p = myproc();
    return (p && p->euid == 0);
}

/**
 * @brief 检查进程是否有指定的共享内存权限
 * @param shp 共享内存段
 * @param perm 请求的权限（SHM_R, SHM_W等）
 * @return 1表示有权限，0表示没有权限
 */
int has_shm_permission(struct shmid_kernel *shp, int perm)
{
    if (!shp)
        return 0;

    struct proc *p = myproc();
    if (!p)
        return 0;

    // root用户拥有所有权限
    if (is_root_user())
        return 1;

    // 检查所有者权限
    if (p->euid == shp->shm_perm.uid)
    {
        if (perm == SHM_R && (shp->shm_perm.mode & S_IRUSR))
            return 1;
        if (perm == SHM_W && (shp->shm_perm.mode & S_IWUSR))
            return 1;
        if (perm == (SHM_R | SHM_W) && (shp->shm_perm.mode & (S_IRUSR | S_IWUSR)) == (S_IRUSR | S_IWUSR))
            return 1;
    }

    // 检查组权限
    if (p->egid == shp->shm_perm.gid)
    {
        if (perm == SHM_R && (shp->shm_perm.mode & S_IRGRP))
            return 1;
        if (perm == SHM_W && (shp->shm_perm.mode & S_IWGRP))
            return 1;
        if (perm == (SHM_R | SHM_W) && (shp->shm_perm.mode & (S_IRGRP | S_IWGRP)) == (S_IRGRP | S_IWGRP))
            return 1;
    }

    // 检查其他用户权限
    if (perm == SHM_R && (shp->shm_perm.mode & S_IROTH))
        return 1;
    if (perm == SHM_W && (shp->shm_perm.mode & S_IWOTH))
        return 1;
    if (perm == (SHM_R | SHM_W) && (shp->shm_perm.mode & (S_IROTH | S_IWOTH)) == (S_IROTH | S_IWOTH))
        return 1;

    return 0;
}

/**
 * @brief 检查共享内存段的权限
 * @param shp 共享内存段
 * @param requested_perms 请求的权限
 * @return 0表示有权限，-EACCES表示权限不足
 */
int check_shm_permissions(struct shmid_kernel *shp, int requested_perms)
{
    if (!shp)
        return -EINVAL;

    // 检查读权限
    if (requested_perms & SHM_R)
    {
        if (!has_shm_permission(shp, SHM_R))
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "[check_shm_permissions] no read permission for shmid %d\n", shp->shmid);
            return -EACCES;
        }
    }

    // 检查写权限
    if (requested_perms & SHM_W)
    {
        if (!has_shm_permission(shp, SHM_W))
        {
            DEBUG_LOG_LEVEL(LOG_WARNING, "[check_shm_permissions] no write permission for shmid %d\n", shp->shmid);
            return -EACCES;
        }
    }

    return 0;
}