#include "pmem.h"
#include "hsai_mem.h"
#include "print.h"
#include "string.h"
#include "types.h"
#include <stdbool.h>
#if defined RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

// 全局伙伴系统实例
static buddy_system_t buddy_sys;

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
        printf("get_order: size %lu too large, max order %d\n",
               size, BUDDY_MAX_ORDER);
        order = BUDDY_MAX_ORDER - 1;
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

            // 插入链表头部
            list_push_front(&buddy_sys.free_lists[max_order], &node->elem);

            // 设置整个块的元数据
            set_block_metadata(aligned_start, max_order);

            aligned_start += block_size;
        }
        break;
    }

    buddy_check_integrity();
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

            // 从链表中移除
            list_remove(&node->elem);

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

                // 设置整个伙伴块的元数据
                set_block_metadata(buddy_addr, current_order);

                // 插入伙伴块到链表头部
                list_push_front(&buddy_sys.free_lists[current_order], &buddy_node->elem);

                // 标记伙伴块为空闲
                set_buddy_free(buddy_addr, current_order);
            }

            // 清空分配的内存（不影响元数据）
            memset((void *)addr, 0, PGSIZE << order);

            // 设置整个分配块的元数据
            set_block_metadata(addr, order);

            // 标记为已使用
            set_buddy_used(addr, order);

            if (debug_buddy)
                printf("buddy_alloc: allocated %p (order %d)\n", (void *)addr, order);
            buddy_check_integrity();
            return (void *)addr;
        }
        current_order++;
    }

    if (debug_buddy)
        printf("buddy_alloc: no suitable block found for order %d\n", order);
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

    // 确保地址在块范围内
    uint64 block_size = PGSIZE << actual_order;
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
            printf("buddy_free: buddy address out of range\n");
            break;
        }

        buddy_node_t *buddy_node = &buddy_sys.nodes[buddy_page_idx];

        // 验证伙伴块是否完整空闲且在空闲链表中
        if (!is_buddy_available(buddy_addr, merge_order) ||
            buddy_node->addr != buddy_addr ||
            buddy_node->order != merge_order)
        {
            if (debug_buddy)
                printf("buddy_free: buddy not available for merging\n");
            break;
        }

        // 检查伙伴块是否在空闲链表中
        bool in_free_list = false;
        struct list_elem *e;
        for (e = list_begin(&buddy_sys.free_lists[merge_order]);
             e != list_end(&buddy_sys.free_lists[merge_order]);
             e = list_next(e))
        {
            buddy_node_t *cur_node = list_entry(e, buddy_node_t, elem);
            if (cur_node == buddy_node)
            {
                in_free_list = true;
                break;
            }
        }

        if (!in_free_list)
        {
            printf("buddy_free: buddy not in free list\n");
            break;
        }

        if (debug_buddy)
            printf("buddy_free: merging with buddy at %p (order %d)\n", (void *)buddy_addr, merge_order);

        // 从空闲链表移除伙伴块
        list_remove(&buddy_node->elem);

        // 合并后取最小地址
        if (merge_addr > buddy_addr)
        {
            merge_addr = buddy_addr;
        }
        merge_order++; // 阶数提升一级

        // 获取新合并块的起始页索引
        uint64 new_page_idx = (merge_addr - buddy_sys.mem_start) / PGSIZE;
        node = &buddy_sys.nodes[new_page_idx];
    }

    // 最终合并后的块信息
    actual_addr = merge_addr;
    actual_order = merge_order;

    if (debug_buddy)
        printf("buddy_free: final merged block addr=%p, order=%d\n", (void *)actual_addr, actual_order);

    // 更新节点信息
    node->addr = actual_addr;
    node->order = actual_order;

    // 设置整个块的元数据
    set_block_metadata(actual_addr, actual_order);
    // 将合并块加入空闲链表头部
    list_push_front(&buddy_sys.free_lists[actual_order], &node->elem);

    buddy_check_integrity();
}

/**
 * @brief 初始化物理内存管理器
 */
void pmem_init()
{
    _mem_start = PGROUNDUP(hsai_get_mem_start());

    // 使用更合理的内存大小，避免分配过多内存
    // 根据实际可用内存调整，这里使用较小的值进行测试
    _mem_end = _mem_start + PAGE_NUM * PGSIZE;

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
    void *ptr = buddy_alloc(order);

    if (!ptr)
    {
        DEBUG_LOG_LEVEL(DEBUG, "pmem_alloc_pages failed for %d pages (order %d)", npages, order);
    }

    return ptr;
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
    uint64 page_idx = (addr - buddy_sys.mem_start) / PGSIZE;

    if (page_idx >= buddy_sys.total_pages)
    {
        printf("pmem_free_pages: invalid address %p\n", ptr);
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

    // 检查大小是否匹配
    if (actual_pages < npages)
    {
        printf("pmem_free_pages: cannot free %d pages from order%d block\n",
               npages, actual_order);
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
        // 修复：添加阶数循环
        for (int i = 0; i <= BUDDY_MAX_ORDER; i++)
        {
            int count = 0;
            struct list_elem *e;
            for (e = list_begin(&buddy_sys.free_lists[i]);
                 e != list_end(&buddy_sys.free_lists[i]);
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