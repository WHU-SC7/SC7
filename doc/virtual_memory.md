# SC7虚拟内存管理

## 概述

SC7虚拟内存管理模块为每个进程提供独立的虚拟地址空间，支持多级页表映射、虚拟内存区域（VMA）链表管理、内存保护、mmap等功能，兼容RISC-V和LoongArch架构。

---

## 1. 虚拟内存空间布局

以RISC-V为例，虚拟内存空间布局如下（见`riscv_memlayout.h`）：

- 用户空间：从0地址开始，包含代码段、数据段、堆、用户栈、mmap区等
- 内核空间：高地址部分，包含内核代码、数据、内核栈、trampoline等
- 关键常量：
  - `TRAMPOLINE`：最高地址的trampoline页
  - `TRAPFRAME`：trampoline下方的trapframe页
  - `KSTACK(p)`：每个进程的内核栈

---

## 2. 多级页表机制

SC7支持RISC-V三/四级页表和LoongArch四级页表，页表项结构和权限位兼容主流架构。

### 主要接口（见`vmem.h`）
- `pte_t *walk(pgtbl_t pt, uint64 va, int alloc);`  // 多级页表遍历/分配
- `int mappages(pgtbl_t pt, uint64 va, uint64 pa, uint64 len, uint64 perm);` // 建立虚实映射
- `uint64 walkaddr(pgtbl_t pt, uint64 va);` // 虚拟地址转物理地址
- `void vmunmap(pgtbl_t pt, uint64 va, uint64 npages, int do_free);` // 解除映射
- `uint64 uvmalloc(pgtbl_t pt, uint64 oldsz, uint64 newsz, int perm);` // 用户空间扩展
- `void uvmfree(pgtbl_t pagetable, uint64 start, uint64 sz);` // 释放页表
- `int uvmcopy(pgtbl_t old, pgtbl_t new, uint64 sz);` // 拷贝页表

### 关键实现片段

```c
pte_t *walk(pgtbl_t pt, uint64 va, int alloc) {
    // 多级页表遍历，必要时分配中间页表
    // 返回va对应的PTE指针
}

int mappages(pgtbl_t pt, uint64 va, uint64 pa, uint64 len, uint64 perm) {
    // 建立虚拟地址到物理地址的映射，设置权限
}

uint64 walkaddr(pgtbl_t pt, uint64 va) {
    // 查找va对应的物理地址，校验权限
}
```

---

## 3. 虚拟内存区域（VMA）链表

每个进程维护一条VMA链表，描述所有虚拟内存段，包括mmap、栈、堆等。

### 结构体定义（见`vma.h`）
```c
struct vma {
    enum segtype type;   // 区段类型（MMAP/STACK等）
    int perm;            // 权限
    uint64 addr;         // 起始地址
    uint64 size;         // 区段大小
    uint64 end;          // 结束地址
    int flags;           // 标志
    int fd;              // 文件映射时的文件描述符
    uint64 f_off;        // 文件偏移
    struct vma *prev, *next; // 链表指针
};
```

### 主要接口
- `struct vma *vma_init(struct proc *p);` // 初始化VMA链表
- `struct vma *alloc_mmap_vma(...)` // 分配mmap区
- `uint64 alloc_vma_stack(struct proc *p);` // 分配用户栈
- `int free_vma_list(struct proc *p);` // 释放全部VMA
- `int free_vma(struct proc *p, uint64 start, uint64 end);` // 释放指定区间

### 关键实现片段

```c
struct vma *vma_init(struct proc *p) {
    struct vma *vma = (struct vma *)pmem_alloc_pages(1);
    memset(vma, 0, PGSIZE);
    vma->type = NONE;
    vma->prev = vma->next = vma;
    p->vma = vma;
    alloc_mmap_vma(p, 0, USER_MMAP_START, 0, 0, 0, 0);
    return vma;
}
```

---

## 4. mmap与munmap机制

支持用户空间动态内存映射（mmap）和解除映射（munmap），可用于文件映射、匿名内存、共享/私有映射等。

### 主要接口
- `uint64 mmap(uint64 start, int64 len, int prot, int flags, int fd, int offset);`
- `int munmap(uint64 start, int len);`

### 关键实现片段

```c
uint64 mmap(uint64 start, int64 len, int prot, int flags, int fd, int offset) {
    proc_t *p = myproc();
    int perm = get_mmapperms(prot);
    struct file *f = fd == -1 ? NULL : p->ofile[fd];
    struct vma *vma = alloc_mmap_vma(p, flags, start, len, perm, fd, offset);
    if (!(flags & MAP_FIXED))
        start = vma->addr;
    // 文件映射时读取文件内容到物理页
    for (i = 0; i < len; i += PGSIZE) {
        uint64 pa = experm(p->pagetable, start + i, perm);
        // 读取文件内容到物理页...
    }
    return start;
}
```
- 支持懒加载和立即分配（MAP_ALLOC）。
- munmap遍历VMA链表，释放指定区间并解除映射。

---

## 5. 权限与保护

- 支持PROT_READ/WRITE/EXEC/NONE等权限，映射到硬件页表权限位。
- `int get_mmapperms(int prot);` // 权限转换
- `uint64 experm(pgtbl_t pagetable, uint64 va, uint64 perm);` // 修改页表项权限
- `int vm_protect(pgtbl_t pagetable, uint64 va, uint64 addr, uint64 perm);` // 区间权限修改

---

## 6. 典型流程与机制总结

1. 进程创建时初始化页表和VMA链表，分配用户栈、代码、数据段。
2. 进程运行时可通过mmap动态映射文件或匿名内存，VMA链表自动维护区段。
3. 发生缺页异常时，内核根据VMA链表和页表分配物理页并建立映射。
4. 进程退出时，释放所有VMA和页表，回收物理内存。

---

SC7虚拟内存管理模块通过多级页表和VMA链表，实现了高效、灵活的虚拟内存分配、映射和保护，支持多种内存管理需求，兼容主流硬件架构，便于扩展和调试。 