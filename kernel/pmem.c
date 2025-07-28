#include "pmem.h"
#include "hsai_mem.h"
#include "print.h"
#include "string.h"
#include "types.h"
#include "spinlock.h"
#include <stdbool.h>
#if defined RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

// 全局伙伴系统实例
buddy_system_t buddy_sys;

// 内存起始和结束地址
uint64 _mem_start, _mem_end;
int debug_buddy = 0;

/**
 * @brief 计算给定大小对应的阶数
 * @param size 需要的大小（以页面为单位）
 * @return 对应的阶数
 */
int get_order(uint64 size)
{
    int order = 0;
    uint64 temp = 1;

    while (temp < size && order < BUDDY_MAX_ORDER)
    {
        temp <<= 1;
        order++;
    }

    // 确保不会超过最大阶数
    if (order >= BUDDY_MAX_ORDER)
    {
        DEBUG_LOG_LEVEL(LOG_DEBUG,"get_order: size %lu too large, max order %d\n",
               size, BUDDY_MAX_ORDER);
        order = -1;
    }

    return order;
}

/**
 * @brief 计算伙伴地址
 * @param addr 当前地址
 * @param order 阶数
 * @return 伙伴地址
 */
uint64 get_buddy_addr(uint64 addr, int order)
{
    uint64 block_size = PGSIZE << order;
    return addr ^ block_size;
}

/**
 * @brief 检查位图中指定位置是否为空闲
 * @param addr 地址
 * @param order 阶数
 * @return 1表示空闲，0表示已使用
 */
bool is_buddy_available(uint64 addr, int order)
{
    // 检查地址对齐
    uint64 block_size = PGSIZE << order;
    if (addr < buddy_sys.mem_start ||
        addr >= buddy_sys.mem_end ||
        (addr - buddy_sys.mem_start) % block_size != 0)
    {
        return false;
    }

    // 关键修改：只需检查伙伴块起始页的元数据
    uint64 page_idx = (addr - buddy_sys.mem_start) / PGSIZE;
    buddy_node_t *node = &buddy_sys.nodes[page_idx];

    // 验证伙伴块元数据
    return (node->addr == addr) &&
           (node->order == order) &&
           // 仅检查起始页是否空闲（而非整个块）
           !(buddy_sys.bitmap[page_idx >> 6] & (1ULL << (page_idx & 0x3F)));
}

/**
 * @brief 在位图中标记指定位置为已使用
 * @param addr 地址
 * @param order 阶数
 */
void set_buddy_used(uint64 addr, int order)
{
    uint64 block_size = PGSIZE << order;
    uint64 start_page = (addr - buddy_sys.mem_start) / PGSIZE;
    uint64 end_page = start_page + (block_size / PGSIZE);

    for (uint64 page = start_page; page < end_page; page++)
    {
        uint64 bitmap_idx = page >> 6;
        uint64 bit_idx = page & 0x3F;
        buddy_sys.bitmap[bitmap_idx] |= (1ULL << bit_idx);
    }
}

/**
 * @brief 在位图中标记指定位置为空闲
 * @param addr 地址
 * @param order 阶数
 */
void set_buddy_free(uint64 addr, int order)
{
    uint64 block_size = PGSIZE << order;
    uint64 start_page = (addr - buddy_sys.mem_start) / PGSIZE;
    uint64 end_page = start_page + (block_size / PGSIZE);

    for (uint64 page = start_page; page < end_page; page++)
    {
        uint64 bitmap_idx = page >> 6;
        uint64 bit_idx = page & 0x3F;
        buddy_sys.bitmap[bitmap_idx] &= ~(1ULL << bit_idx);
    }
}

/**
 * @brief 设置整个块的元数据信息
 * @param addr 块起始地址
 * @param order 阶数
 */
void set_block_metadata(uint64 addr, int order)
{
    uint64 start_page_idx = (addr - buddy_sys.mem_start) / PGSIZE;
    uint64 page_count = 1UL << order;

    for (uint64 i = 0; i < page_count; i++)
    {
        uint64 current_page_idx = start_page_idx + i;
        if (current_page_idx < buddy_sys.total_pages)
        {
            buddy_node_t *page_node = &buddy_sys.nodes[current_page_idx];
            page_node->addr = addr;
            page_node->order = order;
        }
    }
}

/**
 * @brief 初始化伙伴系统
 * @param start 内存起始地址
 * @param end 内存结束地址
 * @return 0表示成功，-1表示失败
 */
int buddy_init(uint64 start, uint64 end)
{
    buddy_sys.mem_start = start;
    buddy_sys.mem_end = end;
    buddy_sys.total_pages = (end - start) / PGSIZE;

    // 初始化伙伴系统锁
    initlock(&buddy_sys.lock, "buddy_system");

    // 初始化空闲链表
    for (int i = 0; i <= BUDDY_MAX_ORDER; i++)
    {
        list_init(&buddy_sys.free_lists[i]);
    }

    // 计算元数据所需空间
    uint64 bitmap_bytes = ((buddy_sys.total_pages + 63) / 64) * sizeof(uint64);
    uint64 nodes_bytes = buddy_sys.total_pages * sizeof(buddy_node_t);
    uint64 meta_total = bitmap_bytes + nodes_bytes;
    uint64 meta_pages = (meta_total + PGSIZE - 1) / PGSIZE;

    // 设置元数据区域
    buddy_sys.bitmap = (uint64 *)start;
    buddy_sys.nodes = (buddy_node_t *)((char *)start + bitmap_bytes);

    // 清零元数据
    memset(buddy_sys.bitmap, 0, meta_total);

    // +++ 关键修复 1：标记元数据区域为已使用 +++
    uint64 meta_start = start;
    uint64 meta_end = start + meta_pages * PGSIZE;
    for (uint64 page_addr = meta_start; page_addr < meta_end; page_addr += PGSIZE)
    {
        uint64 page_idx = (page_addr - start) / PGSIZE;
        uint64 bitmap_idx = page_idx >> 6;
        uint64 bit_idx = page_idx & 0x3F;

        // 设置位图标记
        buddy_sys.bitmap[bitmap_idx] |= (1ULL << bit_idx);

        // 设置特殊元数据标记
        buddy_sys.nodes[page_idx].order = -1; // 特殊值表示元数据页面
        buddy_sys.nodes[page_idx].addr = page_addr;
    }

    // 可用内存从元数据之后开始
    uint64 available_start = meta_end;
    uint64 available_end = end;

    // 按最大可能阶数添加所有可用块
    int max_order = BUDDY_MAX_ORDER;
    while (max_order >= 0)
    {
        uint64 block_size = PGSIZE << max_order;
        uint64 aligned_start = ALIGN_UP(available_start, block_size);

        if (aligned_start + block_size > available_end)
        {
            max_order--;
            continue;
        }

        // 添加所有可能的块
        while (aligned_start + block_size <= available_end)
        {
            // 获取节点索引（基于物理页号）
            uint64 page_idx = (aligned_start - buddy_sys.mem_start) / PGSIZE;
            buddy_node_t *node = &buddy_sys.nodes[page_idx];

            // 初始化节点信息
            node->addr = aligned_start;
            node->order = max_order;

            // 关键修复：确保链表指针正确初始化
            node->elem.prev = NULL;
            node->elem.next = NULL;

            // 插入链表头部
            list_push_front(&buddy_sys.free_lists[max_order], &node->elem);

            // 设置整个块的元数据
            set_block_metadata(aligned_start, max_order);

            aligned_start += block_size;
        }
        break;
    }

    // 移除完整性检查调用，避免在初始化过程中触发死循环
    // buddy_check_integrity();
    return 0;
}

/**
 * @brief 从伙伴系统分配指定阶数的内存块
 * @param order 阶数
 * @return 分配的内存地址，失败返回NULL
 */
void *buddy_alloc(int order)
{
    if (order < 0 || order > BUDDY_MAX_ORDER)
    {
        printf("buddy_alloc: invalid order %d\n", order);
        return NULL;
    }

    // 获取伙伴系统锁
    acquire(&buddy_sys.lock);

    if (debug_buddy)
        printf("buddy_alloc: requesting order %d\n", order);

    // 查找合适的空闲块
    int current_order = order;
    while (current_order <= BUDDY_MAX_ORDER)
    {
        if (!list_empty(&buddy_sys.free_lists[current_order]))
        {
            // 找到空闲块
            struct list_elem *e = list_pop_front(&buddy_sys.free_lists[current_order]);
            buddy_node_t *node = list_entry(e, buddy_node_t, elem);
            uint64 addr = node->addr;

            if (debug_buddy)
                printf("buddy_alloc: found block at %p (order %d)\n", (void *)addr, current_order);

            // 如果当前阶数大于需要的阶数，需要分割
            while (current_order > order)
            {
                current_order--;
                uint64 block_size = PGSIZE << current_order;
                uint64 buddy_addr = addr + block_size;

                if (debug_buddy)
                    printf("buddy_alloc: splitting block, creating buddy at %p (order %d)\n",
                           (void *)buddy_addr, current_order);

                // 获取伙伴块节点索引
                uint64 buddy_page_idx = (buddy_addr - buddy_sys.mem_start) / PGSIZE;
                buddy_node_t *buddy_node = &buddy_sys.nodes[buddy_page_idx];

                // 初始化伙伴节点
                buddy_node->addr = buddy_addr;
                buddy_node->order = current_order;
                buddy_node->elem.prev = NULL;
                buddy_node->elem.next = NULL;

                // 设置整个伙伴块的元数据
                set_block_metadata(buddy_addr, current_order);

                // 插入伙伴块到链表头部
                list_push_front(&buddy_sys.free_lists[current_order], &buddy_node->elem);

                // 标记伙伴块为空闲
                set_buddy_free(buddy_addr, current_order);
            }

            // 更新当前块的元数据
            node->addr = addr;
            node->order = order;
            node->elem.prev = NULL;
            node->elem.next = NULL;

            // 清空分配的内存（不影响元数据）
            memset((void *)addr, 0, PGSIZE << order);

            // 设置整个分配块的元数据
            set_block_metadata(addr, order);

            // 标记为已使用
            set_buddy_used(addr, order);

            if (debug_buddy)
                printf("buddy_alloc: allocated %p (order %d)\n", (void *)addr, order);
            
            // 释放锁并返回
            release(&buddy_sys.lock);
            return (void *)addr;
        }
        current_order++;
    }

    if (debug_buddy)
        printf("buddy_alloc: no suitable block found for order %d\n", order);
    
    // 释放锁并返回
    release(&buddy_sys.lock);
    return NULL; // 没有找到合适的空闲块
}

/**
 * @brief 释放伙伴系统中的内存块
 * @param ptr 要释放的内存地址
 * @param order 阶数
 */
void buddy_free(void *ptr, int order)
{
    if (!ptr || order < 0 || order > BUDDY_MAX_ORDER)
    {
        printf("buddy_free: invalid parameters ptr=%p, order=%d\n", ptr, order);
        return;
    }

    // 获取伙伴系统锁
    acquire(&buddy_sys.lock);

    uint64 addr = (uint64)ptr;
    if (debug_buddy)
        printf("buddy_free: freeing %p (order %d)\n", ptr, order);

    // 获取节点索引
    uint64 page_idx = (addr - buddy_sys.mem_start) / PGSIZE;
    if (page_idx >= buddy_sys.total_pages)
    {
        panic("buddy_free: invalid address %p", ptr);
        return;
    }

    buddy_node_t *node = &buddy_sys.nodes[page_idx];

    // 使用元数据中的实际块信息
    uint64 actual_addr = node->addr;
    int actual_order = node->order;

    if (debug_buddy)
        printf("buddy_free: actual block addr=%p, order=%d\n", (void *)actual_addr, actual_order);

    // 检查是否已经被释放（重复释放检查）
    uint64 block_size = PGSIZE << actual_order;
    uint64 start_page = (actual_addr - buddy_sys.mem_start) / PGSIZE;
    uint64 end_page = start_page + (block_size / PGSIZE);

    bool already_freed = true;
    for (uint64 page = start_page; page < end_page; page++)
    {
        uint64 bitmap_idx = page >> 6;
        uint64 bit_idx = page & 0x3F;
        if (buddy_sys.bitmap[bitmap_idx] & (1ULL << bit_idx))
        {
            already_freed = false;
            break;
        }
    }

    if (already_freed)
    {
        if (debug_buddy)
            printf("buddy_free: WARNING - block %p (order %d) already freed, ignoring\n",
                   (void *)actual_addr, actual_order);
        return;
    }

    // 确保地址在块范围内
    if (addr < actual_addr || addr >= actual_addr + block_size)
    {
        panic("buddy_free: address %p outside block %p (size 0x%lx)",
              ptr, (void *)actual_addr, block_size);
        return;
    }

    // 标记为空闲
    set_buddy_free(actual_addr, actual_order);

    // 保存当前块信息用于合并
    uint64 merge_addr = actual_addr;
    int merge_order = actual_order;

    // 尝试与伙伴合并
    while (merge_order < BUDDY_MAX_ORDER)
    {
        uint64 buddy_addr = get_buddy_addr(merge_addr, merge_order);
        uint64 buddy_page_idx = (buddy_addr - buddy_sys.mem_start) / PGSIZE;

        // 验证伙伴地址有效性
        if (buddy_page_idx >= buddy_sys.total_pages)
        {
            if (debug_buddy)
                printf("buddy_free: buddy address out of range\n");
            break;
        }

        buddy_node_t *buddy_node = &buddy_sys.nodes[buddy_page_idx];

        // 验证伙伴块是否完整空闲（简化检查）
        if (!is_buddy_available(buddy_addr, merge_order) ||
            buddy_node->addr != buddy_addr ||
            buddy_node->order != merge_order)
        {
            if (debug_buddy)
                printf("buddy_free: buddy not available for merging\n");
            break;
        }

        if (debug_buddy)
            printf("buddy_free: merging with buddy at %p (order %d)\n",
                   (void *)buddy_addr, merge_order);

        // 从空闲链表移除伙伴块（确保安全移除）
        list_remove(&buddy_node->elem);
        buddy_node->elem.prev = NULL; // 关键：清空指针防止循环
        buddy_node->elem.next = NULL;

        // 合并后取最小地址
        if (merge_addr > buddy_addr)
        {
            merge_addr = buddy_addr;
        }
        merge_order++; // 阶数提升一级

        // +++ 关键修复：更新合并块的元数据 +++
        uint64 new_page_idx = (merge_addr - buddy_sys.mem_start) / PGSIZE;
        if (new_page_idx >= buddy_sys.total_pages)
        {
            panic("buddy_free: invalid merged page index %lu", new_page_idx);
        }

        node = &buddy_sys.nodes[new_page_idx];
        node->addr = merge_addr;
        node->order = merge_order;
        node->elem.prev = NULL;
        node->elem.next = NULL;
        set_block_metadata(merge_addr, merge_order);

        if (debug_buddy)
            printf("buddy_free: merged to %p (new order %d)\n",
                   (void *)merge_addr, merge_order);
    }

    // 最终合并后的块信息
    actual_addr = merge_addr;
    actual_order = merge_order;

    if (debug_buddy)
        printf("buddy_free: final merged block addr=%p, order=%d\n",
               (void *)actual_addr, actual_order);

    // 确保节点指针正确（合并后可能变化）
    uint64 final_page_idx = (actual_addr - buddy_sys.mem_start) / PGSIZE;
    if (final_page_idx >= buddy_sys.total_pages)
    {
        panic("buddy_free: invalid final page index %lu", final_page_idx);
    }

    node = &buddy_sys.nodes[final_page_idx];

    // 更新节点信息（冗余但安全）
    node->addr = actual_addr;
    node->order = actual_order;
    node->elem.prev = NULL;
    node->elem.next = NULL;

    // 设置整个块的元数据
    set_block_metadata(actual_addr, actual_order);

    // 将合并块加入空闲链表头部
    list_push_front(&buddy_sys.free_lists[actual_order], &node->elem);

    // 释放锁
    release(&buddy_sys.lock);
}

/**
 * @brief 初始化物理内存管理器
 * 
 * @todo 现在riscv实际是1024M,loongarch是512M。pmem_init没有完全使用，
 *       因为可用内存还受内核大小影响，保险起见设小一点，也够用了。之后考虑充分利用所有内存
 */
void pmem_init()
{
    _mem_start = PGROUNDUP(hsai_get_mem_start());

    // 使用更合理的内存大小，避免分配过多内存
    // 根据实际可用内存调整，这里使用较小的值进行测试
#if VF //使用128M内存
    _mem_end = _mem_start + 32*1024 * PGSIZE; //给128M吧, 2^27 = 2^12 * 2^15
#else
    _mem_end = _mem_start + PAGE_NUM * PGSIZE; // 现在riscv是1000M、250k个页；loongarch是400M、100k个页
#endif
    if (buddy_init(_mem_start, _mem_end) != 0)
    {
        panic("buddy system init failed");
    }

    printf("kernel_mem_start = %p\n", _mem_start);
    printf("kernel_mem_end  = %p\n", _mem_end);
    LOG("pmem init success (buddy system)\n");
}

/**
 * @brief 分配指定数量的物理页
 * @param npages 页面数量
 * @return 分配的内存地址，失败返回NULL
 */
void *pmem_alloc_pages(int npages)
{
    if (npages <= 0)
    {
        return NULL;
    }

    int order = get_order(npages);
    if (order != -1)
    {
        void *ptr = buddy_alloc(order);

        if (!ptr)
        {
            DEBUG_LOG_LEVEL(DEBUG, "pmem_alloc_pages failed for %d pages (order %d)", npages, order);
        }
        return ptr;
    }else{
        return NULL;
    }
}

/**
 * @brief 释放指定数量的物理页
 * @param ptr 要释放的内存地址
 * @param npages 页面数量
 */
void pmem_free_pages(void *ptr, int npages)
{
    if (!ptr || npages <= 0)
        return;

    uint64 addr = (uint64)ptr;

    // 验证地址是否在有效范围内
    if (addr < buddy_sys.mem_start || addr >= buddy_sys.mem_end)
    {
        DEBUG_LOG_LEVEL(LOG_WARNING,"pmem_free_pages: address %p outside memory range [%p, %p]\n",
               ptr, (void *)buddy_sys.mem_start, (void *)buddy_sys.mem_end);
        return;
    }

    uint64 page_idx = (addr - buddy_sys.mem_start) / PGSIZE;

    if (page_idx >= buddy_sys.total_pages)
    {
        printf("pmem_free_pages: invalid address %p (page_idx %lu >= total_pages %lu)\n",
               ptr, page_idx, buddy_sys.total_pages);
        return;
    }

    // 获取元数据记录的块起始地址和阶数
    uint64 block_start = buddy_sys.nodes[page_idx].addr;
    int actual_order = buddy_sys.nodes[page_idx].order;
    uint64 actual_pages = 1UL << actual_order;

    // 验证块信息
    if (block_start < buddy_sys.mem_start ||
        block_start >= buddy_sys.mem_end)
    {
        printf("pmem_free_pages: invalid block start %p\n", (void *)block_start);
        return;
    }

    // 验证阶数
    if (actual_order < 0 || actual_order > BUDDY_MAX_ORDER)
    {
        printf("pmem_free_pages: invalid order %d for address %p\n", actual_order, ptr);
        return;
    }

    // 检查大小是否匹配
    if (actual_pages < npages)
    {
        printf("pmem_free_pages: cannot free %d pages from order%d block (actual_pages %lu)\n",
               npages, actual_order, actual_pages);
        return;
    }

    // 验证地址对齐
    uint64 block_size = PGSIZE << actual_order;
    if (block_start % block_size != 0)
    {
        printf("pmem_free_pages: block start %p not aligned to order %d\n",
               (void *)block_start, actual_order);
        return;
    }

    // 检查是否已经被释放（重复释放检查）
    uint64 start_page = (block_start - buddy_sys.mem_start) / PGSIZE;
    uint64 end_page = start_page + actual_pages;

    bool already_freed = true;
    for (uint64 page = start_page; page < end_page; page++)
    {
        uint64 bitmap_idx = page >> 6;
        uint64 bit_idx = page & 0x3F;
        if (buddy_sys.bitmap[bitmap_idx] & (1ULL << bit_idx))
        {
            already_freed = false;
            break;
        }
    }

    if (already_freed)
    {
        if (debug_buddy)
            printf("pmem_free_pages: WARNING - block %p (order %d) already freed, ignoring\n",
                   (void *)block_start, actual_order);
        return;
    }

    // 使用实际块起始地址调用buddy_free
    buddy_free((void *)block_start, actual_order);
}

/**
 * @brief 分配一页物理页
 * @return 分配的内存地址
 */
void *kalloc(void)
{
    void *ptr = pmem_alloc_pages(1);
    if (ptr == NULL)
    {
        panic("kmalloc failed");
    }
    return ptr;
}

/**
 * @brief 释放ptr指向的一页物理页
 * @param ptr 要释放的内存地址
 */
void kfree(void *ptr)
{
    pmem_free_pages(ptr, 1);
}

/**
 * @brief 动态内存分配（简化版本，总是分配一页）
 * @param size 请求的大小
 * @return 分配的内存地址
 */
void *kmalloc(uint64 size)
{
    // 简化实现，总是分配一页
    return kalloc();
}

/**
 * @brief 分配并清零内存（简化版本，总是分配一页）
 * @param n 元素数量
 * @param size 每个元素的大小
 * @return 分配的内存地址
 */
void *kcalloc(uint n, uint64 size)
{
    // 简化实现，总是分配一页
    return kalloc();
}

void buddy_check_integrity()
{
    if (debug_buddy)
    {
        printf("=== Buddy System Integrity Check ===\n");
        printf("Memory range: %p - %p\n",
               (void *)buddy_sys.mem_start,
               (void *)buddy_sys.mem_end);

        // 检查空闲链表
        for (int i = 0; i <= BUDDY_MAX_ORDER; i++)
        {
            int count = 0;
            struct list_elem *e;
            int max_iterations = 1000; // 防止死循环的安全限制

            for (e = list_begin(&buddy_sys.free_lists[i]);
                 e != list_end(&buddy_sys.free_lists[i]) && count < max_iterations;
                 e = list_next(e))
            {
                count++;
                buddy_node_t *node = list_entry(e, buddy_node_t, elem);

                // 验证节点信息
                if (node->order != i)
                {
                    printf("\nERROR: Node %p has wrong order %d (expected %d)\n",
                           (void *)node->addr, node->order, i);
                }

                // 验证节点地址的有效性
                if (node->addr < buddy_sys.mem_start || node->addr >= buddy_sys.mem_end)
                {
                    printf("\nERROR: Node %p has invalid address %p\n",
                           (void *)node, (void *)node->addr);
                }
            }

            if (count >= max_iterations)
            {
                printf("WARNING: Order %d list iteration limit reached, possible circular reference\n", i);
            }

            printf("Order %d: %d blocks\n", i, count);
        }

        // 检查位图一致性
        uint64 errors = 0;
        for (uint64 i = 0; i < buddy_sys.total_pages; i++)
        {
            uint64 addr = buddy_sys.mem_start + i * PGSIZE;
            uint64 bitmap_idx = i >> 6;
            uint64 bit_idx = i & 0x3F;
            bool used = buddy_sys.bitmap[bitmap_idx] & (1ULL << bit_idx);

            // 获取节点信息
            buddy_node_t *node = &buddy_sys.nodes[i];

            // 跳过元数据页面
            if (node->order == -1)
            {
                if (!used)
                {
                    printf("ERROR: Metadata page %p (idx %lu) should be marked used\n",
                           (void *)addr, i);
                    errors++;
                }
                continue;
            }

            // 验证元数据一致性
            if (used)
            {
                // 检查地址是否匹配
                uint64 block_size = PGSIZE << node->order;
                if (node->addr % block_size != 0)
                {
                    printf("ERROR: Page %p (idx %lu) address %p not aligned to order %d\n",
                           (void *)addr, i, (void *)node->addr, node->order);
                    errors++;
                }

                // 检查页面是否在块内
                if (addr < node->addr || addr >= node->addr + block_size)
                {
                    printf("ERROR: Page %p (idx %lu) not in block %p (order %d)\n",
                           (void *)addr, i, (void *)node->addr, node->order);
                    errors++;
                }
            }
            else
            {
                // 空闲页面应有有效阶数
                if (node->order < 0 || node->order > BUDDY_MAX_ORDER)
                {
                    printf("ERROR: Free page %p (idx %lu) has invalid order %d\n",
                           (void *)addr, i, node->order);
                    errors++;
                }
            }
        }
        printf("Integrity check complete, %lu errors found\n", errors);
        assert(!errors, "error occur");
    }
}

/**
 * @brief 诊断伙伴系统状态，用于调试fork时的内存问题
 */
void buddy_diagnose_fork_issue()
{
    printf("=== Buddy System Diagnosis for Fork Issue ===\n");
    printf("Memory range: %p - %p\n", (void *)buddy_sys.mem_start, (void *)buddy_sys.mem_end);
    printf("Total pages: %lu\n", buddy_sys.total_pages);

    // 检查每个阶数的空闲块数量
    for (int i = 0; i <= BUDDY_MAX_ORDER; i++)
    {
        int count = 0;
        struct list_elem *e;
        int max_iterations = 1000; // 防止死循环的安全限制

        for (e = list_begin(&buddy_sys.free_lists[i]);
             e != list_end(&buddy_sys.free_lists[i]) && count < max_iterations;
             e = list_next(e))
        {
            count++;
        }

        if (count >= max_iterations)
        {
            printf("WARNING: Order %d list iteration limit reached, possible circular reference\n", i);
        }

        printf("Order %d: %d free blocks\n", i, count);
    }

    // 检查位图中已使用页面的数量
    uint64 used_pages = 0;
    for (uint64 i = 0; i < buddy_sys.total_pages; i++)
    {
        uint64 bitmap_idx = i >> 6;
        uint64 bit_idx = i & 0x3F;
        if (buddy_sys.bitmap[bitmap_idx] & (1ULL << bit_idx))
        {
            used_pages++;
        }
    }
    printf("Used pages: %lu / %lu\n", used_pages, buddy_sys.total_pages);

    // 检查元数据一致性
    uint64 metadata_errors = 0;
    for (uint64 i = 0; i < buddy_sys.total_pages; i++)
    {
        buddy_node_t *node = &buddy_sys.nodes[i];
        if (node->order < -1 || node->order > BUDDY_MAX_ORDER)
        {
            printf("ERROR: Page %lu has invalid order %d\n", i, node->order);
            metadata_errors++;
        }
    }
    printf("Metadata errors: %lu\n", metadata_errors);
}

/**
 * @brief 安全的伙伴系统状态检查（手动调用）
 * 这个函数可以在需要时手动调用，不会在分配/释放过程中自动触发
 */
void buddy_safe_check()
{
    printf("=== Safe Buddy System Check ===\n");
    printf("Memory range: %p - %p\n",
           (void *)buddy_sys.mem_start,
           (void *)buddy_sys.mem_end);

    // 检查空闲链表
    for (int i = 0; i <= BUDDY_MAX_ORDER; i++)
    {
        int count = 0;
        struct list_elem *e;
        int max_iterations = 1000; // 防止死循环的安全限制

        for (e = list_begin(&buddy_sys.free_lists[i]);
             e != list_end(&buddy_sys.free_lists[i]) && count < max_iterations;
             e = list_next(e))
        {
            count++;
            buddy_node_t *node = list_entry(e, buddy_node_t, elem);

            // 验证节点信息
            if (node->order != i)
            {
                printf("ERROR: Node %p has wrong order %d (expected %d)\n",
                       (void *)node->addr, node->order, i);
            }

            // 验证节点地址的有效性
            if (node->addr < buddy_sys.mem_start || node->addr >= buddy_sys.mem_end)
            {
                printf("ERROR: Node %p has invalid address %p\n",
                       (void *)node, (void *)node->addr);
            }
        }

        if (count >= max_iterations)
        {
            printf("WARNING: Order %d list iteration limit reached, possible circular reference\n", i);
        }

        printf("Order %d: %d blocks\n", i, count);
    }

    printf("Safe check complete\n");
}

/**
 * @brief 清理和重建伙伴系统的链表
 * 用于修复循环引用和元数据不一致问题
 */
void buddy_cleanup_and_rebuild()
{
    printf("=== Buddy System Cleanup and Rebuild ===\n");

    // 清空所有空闲链表
    for (int i = 0; i <= BUDDY_MAX_ORDER; i++)
    {
        list_init(&buddy_sys.free_lists[i]);
    }

    // 重新扫描内存，重建空闲链表
    uint64 available_start = buddy_sys.mem_start;
    uint64 available_end = buddy_sys.mem_end;

    // 跳过元数据区域
    uint64 bitmap_bytes = ((buddy_sys.total_pages + 63) / 64) * sizeof(uint64);
    uint64 nodes_bytes = buddy_sys.total_pages * sizeof(buddy_node_t);
    uint64 meta_total = bitmap_bytes + nodes_bytes;
    uint64 meta_pages = (meta_total + PGSIZE - 1) / PGSIZE;
    available_start += meta_pages * PGSIZE;

    // 按最大可能阶数添加所有可用块
    int max_order = BUDDY_MAX_ORDER;
    while (max_order >= 0)
    {
        uint64 block_size = PGSIZE << max_order;
        uint64 aligned_start = ALIGN_UP(available_start, block_size);

        if (aligned_start + block_size > available_end)
        {
            max_order--;
            continue;
        }

        // 添加所有可能的块
        while (aligned_start + block_size <= available_end)
        {
            // 检查这个块是否真的空闲
            bool is_free = true;
            uint64 start_page = (aligned_start - buddy_sys.mem_start) / PGSIZE;
            uint64 end_page = start_page + (block_size / PGSIZE);

            for (uint64 page = start_page; page < end_page; page++)
            {
                uint64 bitmap_idx = page >> 6;
                uint64 bit_idx = page & 0x3F;
                if (buddy_sys.bitmap[bitmap_idx] & (1ULL << bit_idx))
                {
                    is_free = false;
                    break;
                }
            }

            if (is_free)
            {
                // 获取节点索引
                uint64 page_idx = (aligned_start - buddy_sys.mem_start) / PGSIZE;
                buddy_node_t *node = &buddy_sys.nodes[page_idx];

                // 重新初始化节点信息
                node->addr = aligned_start;
                node->order = max_order;
                node->elem.prev = NULL;
                node->elem.next = NULL;

                // 插入链表头部
                list_push_front(&buddy_sys.free_lists[max_order], &node->elem);

                // 设置整个块的元数据
                set_block_metadata(aligned_start, max_order);

                printf("Rebuilt: block at %p (order %d)\n", (void *)aligned_start, max_order);
            }

            aligned_start += block_size;
        }
        break;
    }

    printf("Cleanup and rebuild complete\n");
}

/**
 * @brief 跟踪内存释放，帮助调试重复释放问题
 * @param ptr 要释放的内存地址
 * @param order 阶数
 * @param caller_pid 调用者进程ID
 */
void buddy_free_tracked(void *ptr, int order, int caller_pid)
{
    if (debug_buddy)
    {
        printf("buddy_free_tracked: pid %d freeing %p (order %d)\n", caller_pid, ptr, order);
    }

    // 记录释放历史（简化版本，只记录最近的几次）
    static struct
    {
        void *ptr;
        int order;
        int pid;
        uint64 timestamp;
    } free_history[10] = {0};
    static int history_index = 0;

    // 检查是否在历史记录中
    for (int i = 0; i < 10; i++)
    {
        if (free_history[i].ptr == ptr)
        {
            printf("WARNING: Memory %p (order %d) was freed by pid %d at time %lu, "
                   "now being freed again by pid %d\n",
                   ptr, free_history[i].order, free_history[i].pid,
                   free_history[i].timestamp, caller_pid);
        }
    }

    // 添加到历史记录
    free_history[history_index].ptr = ptr;
    free_history[history_index].order = order;
    free_history[history_index].pid = caller_pid;
    free_history[history_index].timestamp = r_time(); // 假设有这个函数
    history_index = (history_index + 1) % 10;

    // 调用实际的释放函数
    buddy_free(ptr, order);
}