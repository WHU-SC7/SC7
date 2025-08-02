# 共享内存 (Shared Memory) 详细文档

## 1. 概述

共享内存是一种进程间通信(IPC)机制，允许多个进程访问同一块物理内存区域。这是最高效的IPC方式，因为数据不需要在进程间复制，而是直接在内存中共享。

## 2. 基本概念

### 2.1 共享内存段 (Shared Memory Segment)
- 一块连续的物理内存区域
- 可以被多个进程同时映射到各自的虚拟地址空间
- 每个进程看到的虚拟地址可能不同，但指向相同的物理内存

### 2.2 共享内存标识符 (shmid)
- 系统为每个共享内存段分配的唯一标识符
- 用于标识和管理共享内存段

### 2.3 映射 (Mapping)
- 将共享内存段映射到进程的虚拟地址空间
- 每个进程可以有不同的映射地址

## 3. 系统调用

### 3.1 shmget - 创建或获取共享内存段

```c
int shmget(key_t key, size_t size, int shmflg);
```

**参数说明：**
- `key`: 共享内存段的键值，可以是IPC_PRIVATE或用户定义的键值
- `size`: 共享内存段的大小（字节）
- `shmflg`: 标志位，包括权限和创建选项

**返回值：**
- 成功：返回共享内存段标识符(shmid)
- 失败：返回-1，设置errno

**示例：**
```c
int shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0666);
if (shmid == -1) {
    perror("shmget failed");
    exit(1);
}
```

### 3.2 shmat - 附加共享内存段

```c
void *shmat(int shmid, const void *shmaddr, int shmflg);
```

**参数说明：**
- `shmid`: 共享内存段标识符
- `shmaddr`: 指定映射地址，通常为NULL（让系统自动选择）
- `shmflg`: 标志位，如SHM_RDONLY（只读）

**返回值：**
- 成功：返回映射的虚拟地址
- 失败：返回(void*)-1，设置errno

**示例：**
```c
void *addr = shmat(shmid, NULL, 0);
if (addr == (void*)-1) {
    perror("shmat failed");
    exit(1);
}
```

### 3.3 shmdt - 分离共享内存段

```c
int shmdt(const void *shmaddr);
```

**参数说明：**
- `shmaddr`: 共享内存段的映射地址

**返回值：**
- 成功：返回0
- 失败：返回-1，设置errno

**示例：**
```c
if (shmdt(addr) == -1) {
    perror("shmdt failed");
    exit(1);
}
```

### 3.4 shmctl - 控制共享内存段

```c
int shmctl(int shmid, int cmd, struct shmid_ds *buf);
```

**参数说明：**
- `shmid`: 共享内存段标识符
- `cmd`: 控制命令（IPC_RMID, IPC_STAT, IPC_SET等）
- `buf`: 指向shmid_ds结构的指针

**常用命令：**
- `IPC_RMID`: 删除共享内存段
- `IPC_STAT`: 获取共享内存段状态
- `IPC_SET`: 设置共享内存段属性

**示例：**
```c
// 删除共享内存段
if (shmctl(shmid, IPC_RMID, NULL) == -1) {
    perror("shmctl IPC_RMID failed");
    exit(1);
}
```

## 4. 内核实现

### 4.1 数据结构

#### shmid_kernel 结构体
```c
struct shmid_kernel {
    int shmid;                    // 共享内存段标识符
    int size;                     // 共享内存段大小
    int shm_perm;                 // 权限信息
    struct vma *attaches;         // 附加的VMA链表
    pte_t *shm_pages;            // 物理页面数组
    struct shm_attach *shm_attach_list; // 附加信息链表
};
```

#### shm_attach 结构体
```c
struct shm_attach {
    struct proc *proc;           // 附加的进程
    uint64 addr;                 // 映射的虚拟地址
    struct shm_attach *next;     // 链表指针
};
```

### 4.2 核心函数

#### 4.2.1 newseg - 创建共享内存段
```c
int newseg(int key, int shmflg, int size)
{
    // 1. 分配shmid_kernel结构
    // 2. 初始化共享内存段信息
    // 3. 分配物理页面数组
    // 4. 将段添加到全局数组
}
```

#### 4.2.2 vma_map - 映射共享内存
```c
int vma_map(pgtbl_t old, pgtbl_t new, struct vma *vma)
{
    if (vma->type == SHARE && vma->shm_kernel) {
        // 特殊处理共享内存类型的VMA
        struct shmid_kernel *shp = vma->shm_kernel;
        
        for (int i = 0; i < num_pages; i++) {
            uint64 va = start + i * PGSIZE;
            
            // 检查是否已经映射（避免重复映射）
            pte_t *existing_pte = walk(new, va, 0);
            if (existing_pte && (*existing_pte & PTE_V)) {
                continue; // 跳过已映射的页面
            }
            
            // 映射物理页面
            if (mappages(new, va, pa, PGSIZE, flags) != 1) {
                panic("vma_map: shared memory mapping failed");
            }
        }
    }
}
```

#### 4.2.3 sync_shared_memory - 同步共享内存
```c
void sync_shared_memory(struct shmid_kernel *shp)
{
    // 确保所有附加进程的映射保持一致
    struct vma *vma = shp->attaches;
    while (vma != NULL) {
        // 同步映射信息
        vma = vma->next;
    }
}
```

## 5. 使用示例

### 5.1 生产者-消费者模式

**生产者进程：**
```c
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <string.h>

int main() {
    // 创建共享内存段
    int shmid = shmget(IPC_PRIVATE, 1024, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget failed");
        return 1;
    }
    
    // 附加共享内存段
    char *shared_mem = shmat(shmid, NULL, 0);
    if (shared_mem == (char*)-1) {
        perror("shmat failed");
        return 1;
    }
    
    // 写入数据
    strcpy(shared_mem, "Hello from producer!");
    printf("Producer: wrote data to shared memory\n");
    
    // 分离共享内存段
    shmdt(shared_mem);
    
    return 0;
}
```

**消费者进程：**
```c
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>

int main() {
    int shmid; // 需要从生产者获取shmid
    
    // 附加共享内存段
    char *shared_mem = shmat(shmid, NULL, 0);
    if (shared_mem == (char*)-1) {
        perror("shmat failed");
        return 1;
    }
    
    // 读取数据
    printf("Consumer: read '%s' from shared memory\n", shared_mem);
    
    // 分离共享内存段
    shmdt(shared_mem);
    
    // 删除共享内存段
    shmctl(shmid, IPC_RMID, NULL);
    
    return 0;
}
```

### 5.2 多进程共享计数器

```c
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    // 创建共享内存段存储计数器
    int shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    int *counter = shmat(shmid, NULL, 0);
    *counter = 0;
    
    // 创建多个子进程
    for (int i = 0; i < 5; i++) {
        if (fork() == 0) {
            // 子进程：增加计数器
            for (int j = 0; j < 1000; j++) {
                (*counter)++;
            }
            shmdt(counter);
            exit(0);
        }
    }
    
    // 等待所有子进程完成
    for (int i = 0; i < 5; i++) {
        wait(NULL);
    }
    
    printf("Final counter value: %d\n", *counter);
    
    // 清理
    shmdt(counter);
    shmctl(shmid, IPC_RMID, NULL);
    
    return 0;
}
```

## 6. 注意事项和最佳实践

### 6.1 同步问题
- 共享内存本身不提供同步机制
- 需要使用信号量、互斥锁等同步原语
- 避免竞态条件

### 6.2 内存对齐
- 确保共享内存段按页面边界对齐
- 考虑CPU缓存行对齐以提高性能

### 6.3 错误处理
```c
// 检查系统调用返回值
if (shmid == -1) {
    perror("shmget failed");
    // 处理错误
}

// 检查映射地址
if (addr == (void*)-1) {
    perror("shmat failed");
    // 处理错误
}
```

### 6.4 资源清理
- 及时分离共享内存段
- 最后一个使用进程负责删除共享内存段
- 避免内存泄漏

### 6.5 权限控制
```c
// 设置适当的权限
int shmid = shmget(key, size, IPC_CREAT | 0644);
```

## 7. 性能考虑

### 7.1 优势
- 零拷贝：数据不需要在进程间复制
- 低延迟：直接内存访问
- 高带宽：适合大量数据传输

### 7.2 限制
- 需要手动同步
- 调试困难
- 可能产生内存碎片

### 7.3 优化建议
- 使用大页面（如果支持）
- 合理设置共享内存段大小
- 避免频繁的映射/分离操作

## 8. 调试和故障排除

### 8.1 常见问题

#### 重复映射问题
```c
// 错误：页表项重复映射
[ERROR][vmem.c:210] pte remap! va: 0x000000006fea5000
panic:[vma.c:971] vma_map: shared memory mapping failed
```

**解决方案：**
在映射前检查页表项是否已存在：
```c
pte_t *existing_pte = walk(new, va, 0);
if (existing_pte && (*existing_pte & PTE_V)) {
    continue; // 跳过已映射的页面
}
```

#### 权限问题
```c
// 错误：权限不足
[ERROR][hsai_trap.c:259] address:aligned_addr:0x000000006fea5000 is already mapped!,not enough permission
```

**解决方案：**
确保正确设置共享内存段权限和映射权限。

### 8.2 调试工具
- `ipcs -m`: 显示共享内存段信息
- `ipcrm -m shmid`: 删除指定的共享内存段
- 内核日志：查看映射和同步信息

## 9. 总结

共享内存是一种高效的进程间通信机制，特别适合需要频繁数据交换的场景。正确使用共享内存需要：

1. 理解其工作原理和限制
2. 正确处理同步问题
3. 遵循最佳实践
4. 做好错误处理和资源清理

通过合理的设计和实现，共享内存可以显著提高系统性能，但同时也需要更多的编程经验和调试技巧。 