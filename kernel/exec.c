#include "elf.h"
#include "defs.h"
#include "types.h"
#include "string.h"
#include "print.h"
#include "virt.h"
#include "vmem.h"
#include "process.h"
#include "cpu.h"
#if defined RISCV
#include "riscv.h"
#include "riscv_memlayout.h"
#else
#include "loongarch.h"
#endif

extern void la_virtio_disk_rw(struct buf *b, int write); //< loongarch磁盘读写
int load_elf_from_disk(uint64 blockno);
int exec(char *path, char **argv, char **env)
{
    intr_on();
    load_elf_from_disk(0);
    return -1;
}

static int flags_to_perm(int flags)
{
    int perm = 0;
    if (flags & 0x01)
        perm |= PTE_X;
    if (flags & 0x02)
        perm |= PTE_W;
    return perm;
}
struct buf b;
struct buf tmpb;
static int loadseg(pgtbl_t pt, uint64 va, program_header_t ph, uint64 offset, uint64 filesz, uint64 elf_start_blockno)
{
    uint64 pa;
    int n;
    assert(va % PGSIZE == 0, "va need be aligned!\n");
    // 逐页加载段内容
    for (int i = 0; i < filesz; i += PGSIZE)
    {
        pa = walkaddr(pt, va + i); // 获取物理地址
        assert(pa != 0, "pa is null!\n");
        n = MIN(filesz - i, PGSIZE);

        tmpb.dev = 1;
        uint64 total_offset = offset + i;
        uint64 start_block = elf_start_blockno + (total_offset / BSIZE);
        uint64 block_offset = total_offset % BSIZE;

        // 分块拷贝数据
        uint64 copied = 0;
        while (copied < n)
        {
            tmpb.blockno = start_block + (block_offset + copied) / BSIZE;
#if defined RISCV
            virtio_rw(&tmpb, 0);
#else
            la_virtio_disk_rw(&tmpb, 0);
#endif

            // 计算本次拷贝参数
            uint64 copy_offset = (block_offset + copied) % BSIZE;
            uint64 copy_size = MIN(BSIZE - copy_offset, n - copied);

            memcpy((void *)((pa + copied) | dmwin_win0), tmpb.data + copy_offset, copy_size);
            copied += copy_size;
        }
    }
    return 0;
}
#define MAX_ELF_SIZE (2 * 1024 * 1024) // 2MB缓冲区

int load_elf_from_disk(uint64 blockno)
{
    b.blockno = blockno; // ELF文件在磁盘上的起始块号
    b.dev = 1;           // 磁盘设备号

// 1. 读取ELF头
#if defined RISCV
    virtio_rw(&b, 0);
#else
    la_virtio_disk_rw(&b, 0);
#endif
    // 2. 解析ELF头
    elf_header_t *ehdr = (elf_header_t *)b.data;

    if (ehdr->magic != ELF_MAGIC)
    {
        printf("错误：不是有效的ELF文件\n");
        return -1;
    }

    // 3. 创建新页表
    proc_t *p = myproc();
    pgtbl_t new_pt = proc_pagetable(p);
    uint64 sz = 0;
    if (new_pt == NULL)
        panic("alloc new_pt\n");

    // 4. 读取程序头表（处理跨块情况）
    uint64 phdr_start_block = blockno + (ehdr->phoff / BSIZE);
    uint64 phdr_end_block = blockno + ((ehdr->phoff + ehdr->phnum * sizeof(program_header_t) + BSIZE - 1) / BSIZE);

    // 为程序头表和elf头分配连续内存
    char phdr_buffer[2048];
    uint64 phdr_size = ehdr->ehsize + ehdr->phnum * sizeof(program_header_t);

    if (phdr_size > sizeof(phdr_buffer))
    {
        printf("错误：程序头表太大\n");
        return -1;
    }

    // 读取所有包含程序头表的块
    for (uint64 blk = phdr_start_block; blk < phdr_end_block; blk++)
    {
        b.blockno = blk;
#if defined RISCV
        virtio_rw(&b, 0);
#else
        la_virtio_disk_rw(&b, 0);
#endif

        // 计算当前块在缓冲区中的位置
        uint64 buf_offset = (blk - phdr_start_block) * BSIZE;
        uint64 copy_size = BSIZE;

        // 处理最后一个块可能的不完整拷贝
        if (buf_offset + copy_size > phdr_size)
        {
            copy_size = phdr_size - buf_offset;
        }

        memcpy(phdr_buffer + buf_offset, b.data, copy_size);
    }

    // 获取程序头表指针（考虑块内偏移）
    program_header_t *phdr = (program_header_t *)(phdr_buffer + (ehdr->phoff % BSIZE));

    int uret, ret;

    // 5. 加载每个PT_LOAD段
    for (int i = 0; i < ehdr->phnum; i++)
    {
        if (phdr[i].type == ELF_PROG_LOAD)
        {
            printf("加载段 %d: 文件偏移 0x%lx, 大小 0x%lx, 虚拟地址 0x%lx\n", i, phdr[i].off, phdr[i].filesz, phdr[i].vaddr);
            program_header_t ph = phdr[i];
            uret = uvm_grow(new_pt, sz, ph.vaddr + ph.memsz, flags_to_perm(ph.flags) | PTE_R);
            if (uret != ph.vaddr + ph.memsz)
                goto bad;
            sz = uret;
            uint64 elf_start_blockno = blockno;
            ret = loadseg(new_pt, ph.vaddr, ph, ph.off, ph.filesz, elf_start_blockno);
            if (ret < 0)
                goto bad;
        }
    }

    // 5. 打印入口点信息
    printf("ELF加载完成，入口点: 0x%lx\n", ehdr->entry);
    p->pagetable = new_pt;
    uint64 program_entry = 0;
    program_entry = ehdr->entry;
    sz = PGROUNDUP(sz);
    uret = uvm_grow(new_pt, sz, sz + 32 * PGSIZE, PTE_USER);
    if (uret == 0)
        goto bad;
    sz = uret;
    uint64 sp = sz;
    uint64 stackbase = sp - PGSIZE;
    // 随机数
    sp -= 16;
    uint64 random[2] = {0x7be6f23c6eb43a7e, 0xb78b3ea1f7c8db96};
    if (sp < stackbase || copyout(new_pt, sp, (char *)(uint64)random, 16) < 0)
        goto bad;

    p->trapframe->a2 = sp;
    sp -= sp % 16;
    p->trapframe->a1 = sp;
    p->sz = sz;
#if defined RISCV
    p->trapframe->epc = program_entry;
#else
    p->trapframe->era = program_entry;
#endif
    p->trapframe->sp = sp;
    return 0;

bad:
    panic("exec error!\n");
    return -1;
}
