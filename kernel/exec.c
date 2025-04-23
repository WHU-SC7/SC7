#include "elf.h"
#include "defs.h"
#include "types.h"
#include "string.h"
#include "print.h"
#include "virt.h"
#if defined RISCV
#include "riscv.h"
#include "riscv_memlayout.h"
#else
#include "loongarch.h"
#endif

extern void uservec();
void load_elf_from_disk(uint64 blockno);
void test_exec_elf_from_buffer();
int load_elf_to_kernel_buffer(uint64 blockno);
int exec(char *path, char **argv, char **env)
{
    intr_on();
    load_elf_from_disk(0);
    //load_elf_to_kernel_buffer(0);
    test_exec_elf_from_buffer();
    return -1;
}

#define MAX_ELF_SIZE (2 * 1024 * 1024) // 2MB缓冲区
static uint8 kernel_elf_buffer[MAX_ELF_SIZE];

struct buf b;
void load_elf_from_disk(uint64 blockno)
{
    b.blockno = blockno; // ELF文件在磁盘上的起始块号
    b.dev = 1;           // 磁盘设备号

    // 1. 读取ELF头
    virtio_rw(&b, 0);

    // 2. 解析ELF头
    elf_header_t *ehdr = (elf_header_t *)b.data;

    // 验证ELF魔数
    if (ehdr->magic != ELF_MAGIC)
    {
        printf("错误：不是有效的ELF文件\n");
        return;
    }

    // 3. 读取程序头表（处理跨块情况）
    uint64 phdr_start_block = blockno + (ehdr->phoff / BSIZE);
    uint64 phdr_end_block = blockno + ((ehdr->phoff + ehdr->phnum * sizeof(program_header_t) + BSIZE - 1) / BSIZE);

    // 为程序头表分配连续内存（假设不超过4KB）
    char phdr_buffer[4096];
    uint64 phdr_size = ehdr->phnum * sizeof(program_header_t);

    if (phdr_size > sizeof(phdr_buffer))
    {
        printf("错误：程序头表太大\n");
        return;
    }

    // 读取所有包含程序头表的块
    for (uint64 blk = phdr_start_block; blk < phdr_end_block; blk++)
    {
        b.blockno = blk;
        virtio_rw(&b, 0);

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

    // 4. 加载每个PT_LOAD段
    for (int i = 0; i < ehdr->phnum; i++)
    {
        if (phdr[i].type == ELF_PROG_LOAD)
        {
            printf("加载段 %d: 文件偏移 0x%lx, 大小 0x%lx, 虚拟地址 0x%lx\n",
                   i, phdr[i].off, phdr[i].filesz, phdr[i].vaddr);

            uint64 start_block = phdr[i].off / BSIZE;
            uint64 end_block = (phdr[i].off + phdr[i].filesz + BSIZE - 1) / BSIZE;

            for (uint64 blk = start_block; blk < end_block; blk++)
            {
                b.blockno = blockno + blk;
                virtio_rw(&b, 0);

                // 计算当前块在段中的偏移
                uint64 seg_offset = (blk - start_block) * BSIZE;
                uint64 copy_addr = phdr[i].vaddr + seg_offset;

                // 计算实际拷贝大小（处理最后一个块）
                uint64 copy_size = BSIZE;
                if (seg_offset + copy_size > phdr[i].filesz)
                {
                    copy_size = phdr[i].filesz - seg_offset;
                }

                memcpy((void *)copy_addr, b.data, copy_size);

                // 如果需要，清零.bss区域（memsz > filesz）
                if (seg_offset + copy_size < phdr[i].memsz)
                {
                    uint64 zero_size = phdr[i].memsz - (seg_offset + copy_size);
                    if (zero_size > BSIZE)
                        zero_size = BSIZE;
                    memset((void *)(copy_addr + copy_size), 0, zero_size);
                }
            }
        }
    }

    // 5. 打印入口点信息
    printf("ELF加载完成，入口点: 0x%lx\n", ehdr->entry);

    // 实际使用时这里会跳转到入口点执行:
    //void (*entry)() = (void (*)())ehdr->entry;
    //entry();
}
int load_elf_to_kernel_buffer(uint64 blockno)
{
    b.blockno = blockno;
    b.dev = 1;

    // 读取ELF头
    virtio_rw(&b, 0);
    elf_header_t *ehdr = (elf_header_t *)b.data;

    // 验证elf魔数
    if (ehdr->magic != ELF_MAGIC)
    {
        printf("不是有效的elf文件\n");
        return -1;
    }

    // 检查ELF文件是否过大
    if (ehdr->shoff + (ehdr->shnum * ehdr->shentsize) > MAX_ELF_SIZE)
    {
        printf("ELF文件超过内核缓冲区大小\n");
        return -1;
    }

    // 将整个ELF文件读入内核缓冲区
    uint64 remaining_size = MAX_ELF_SIZE;
    uint64 current_offset = 0;
    uint64 current_block = blockno;

    while (remaining_size > 0)
    {
        b.blockno = current_block;
        virtio_rw(&b, 0);

        uint64 copy_size = (remaining_size > BSIZE) ? BSIZE : remaining_size;
        memcpy(kernel_elf_buffer + current_offset, b.data, copy_size);

        current_offset += copy_size;
        remaining_size -= copy_size;
        current_block++;
    }

    return 0;
}

void test_exec_elf_from_buffer()
{
    elf_header_t *ehdr = (elf_header_t *)kernel_elf_buffer;

    // 1. 验证入口点是否在缓冲区内
    if (ehdr->entry >= MAX_ELF_SIZE)
    {
        printf("无效的入口点\n");
        return;
    }

    // 2. 打印ELF信息用于调试
    printf("ELF入口点: 0x%lx\n", ehdr->entry);
    printf(".text段大小: %lu\n", ehdr->phnum);

    // 3. 测试执行（简单跳转）
    void (*entry_func)(void) = (void (*)(void))(kernel_elf_buffer + ehdr->entry);

    printf("准备跳转到ELF入口点...\n");
    entry_func();
    printf("ELF执行返回\n"); // 通常不会执行到这里
}
