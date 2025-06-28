# SC7物理内存管理

## 概述

SC7物理内存管理模块采用伙伴系统（Buddy System）实现高效的物理页分配与回收，支持动态分配、合并和碎片整理，适用于多种内存分配场景。

---

## 1. 伙伴系统核心结构体

定义于`include/kernel/pmem.h`：

```c
typedef struct buddy_node {
    uint64 addr;           // 块起始地址
    int order;             // 阶数（块大小=2^order页）
    struct list_elem elem; // 链表元素
} buddy_node_t;

typedef struct buddy_system {
    uint64 mem_start;                            // 内存起始地址
    uint64 mem_end;                              // 内存结束地址
    uint64 total_pages;                          // 总页数
    uint64 *bitmap;                              // 位图
    buddy_node_t *nodes;                         // 节点数组
    struct list free_lists[BUDDY_MAX_ORDER + 1]; // 各阶空闲链表
} buddy_system_t;
```
- 伙伴系统将物理内存按2的幂次方分块，支持最大11阶（2048页）。
- 每阶有独立空闲链表，节点记录块地址和阶数。

---

## 2. 初始化与元数据管理

### 初始化流程

`pmem_init()`和`buddy_init()`负责初始化物理内存管理：

```c
int buddy_init(uint64 start, uint64 end) {
    buddy_sys.mem_start = start;
    buddy_sys.mem_end = end;
    buddy_sys.total_pages = (end - start) / PGSIZE;
    // 初始化各阶空闲链表
    for (int i = 0; i <= BUDDY_MAX_ORDER; i++)
        list_init(&buddy_sys.free_lists[i]);
    // 分配并清零bitmap和nodes元数据
    // 标记元数据区为已用
    // 按最大阶分配所有可用块到空闲链表
}
```
- 位图和节点数组用于跟踪每一页的分配状态和块元信息。
- 元数据区本身也用伙伴系统管理并标记为已用。

---

## 3. 伙伴分配与回收算法

### 分配接口

- `void *pmem_alloc_pages(int npages);`  // 分配npages连续物理页
- `void *buddy_alloc(int order);`        // 分配2^order页的块

核心实现：
```c
void *buddy_alloc(int order) {
    // 从目标阶或更高阶空闲链表查找可用块
    // 若高阶分裂为低阶，递归分裂并插入低阶链表
    // 标记分配块为已用，返回块起始地址
}
```
- 分配时优先使用最小满足需求的块，避免碎片。
- 若无合适块，尝试分裂更大块。

### 回收接口

- `void pmem_free_pages(void *ptr, int npages);` // 回收npages连续物理页
- `void buddy_free(void *ptr, int order);`       // 回收2^order页的块

核心实现：
```c
void buddy_free(void *ptr, int order) {
    // 检查相邻伙伴块是否空闲，若空闲则合并为更高阶块
    // 递归合并，直到无法继续
    // 最终插入对应阶空闲链表
}
```
- 回收时自动合并相邻空闲块，减少碎片。

---

## 4. 辅助分配接口

- `void *kalloc(void);`      // 分配单页
- `void kfree(void *ptr);`   // 回收单页
- `void *kmalloc(uint64 size);` // 按字节分配（适合小对象）
- `void *kcalloc(uint n, uint64 size);` // 分配并清零

这些接口底层均基于伙伴系统，适配不同分配需求。

---

## 5. 关键算法与实现片段

### 计算阶数

```c
int get_order(uint64 size) {
    int order = 0;
    uint64 temp = 1;
    while (temp < size && order < BUDDY_MAX_ORDER) {
        temp <<= 1;
        order++;
    }
    return (order >= BUDDY_MAX_ORDER) ? -1 : order;
}
```

### 伙伴地址计算

```c
uint64 get_buddy_addr(uint64 addr, int order) {
    uint64 block_size = PGSIZE << order;
    return addr ^ block_size;
}
```

### 分配与回收流程

- 分配时，从目标阶或更高阶空闲链表查找块，必要时分裂高阶块。
- 回收时，递归合并空闲伙伴块，插入对应阶空闲链表。
- 位图和节点数组用于高效标记和查找块状态。

---

## 6. 诊断与调试

- `buddy_check_integrity()`：检查伙伴系统一致性。
- `buddy_diagnose_fork_issue()`、`buddy_safe_check()`等用于调试和健壮性检查。
- `debug_buddy`全局变量可开启调试输出。

---

## 7. 总结

SC7物理内存管理采用伙伴系统，兼顾分配效率与碎片控制，支持多种分配接口，适合操作系统内核的高并发和动态内存需求。其核心机制为：
- 内存分块与合并，动态适应分配需求
- 位图与节点元数据高效管理
- 多阶空闲链表快速查找与插入
- 代码实现清晰，便于扩展和调试 