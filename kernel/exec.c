#include "elf.h"
#include "defs.h"
#include "types.h"
#include "string.h"
#include "print.h"
#include "virt.h"
#include "vmem.h"
#include "defs.h"
#include "process.h"
#include "cpu.h"
#include "inode.h"
#include "ops.h"
#include "ext4_oflags.h"
#include "file.h"
#include "vfs_ext4.h"
#include "fs_defs.h"
#include "vma.h"
#if defined RISCV
#include "riscv.h"
#include "riscv_memlayout.h"
#else
#include "loongarch.h"
#endif

static int flags_to_perm(int flags);
static int loadseg(pgtbl_t pt, uint64 va, struct inode *ip, uint offset, uint sz);
void alloc_aux(uint64 *aux, uint64 atid, uint64 value);
int loadaux(pgtbl_t pt, uint64 sp, uint64 stackbase, uint64 *aux);

int exec(char *path, char **argv, char **env)
{
    // load_elf_from_disk(0);
    struct inode *ip;
    if ((ip = namei(path)) == NULL)
    {
        printf("exec: fail to find file %s\n", path);
        return -1;
    }
    elf_header_t ehdr;
    program_header_t ph;
    int ret;
    //int is_dynamic = 0;
    /// @todo : 对ip上锁
    if (ip->i_op->read(ip, 0, (uint64)&ehdr, 0, sizeof(ehdr)) != sizeof(ehdr))
    {
        goto bad;
    }
    if (ehdr.magic != ELF_MAGIC)
    {
        printf("错误：不是有效的ELF文件\n");
        return -1;
    }

    proc_t *p = myproc();
    free_vma_list(p);
    vma_init(p);
    pgtbl_t new_pt = proc_pagetable(p);
    uint64 sz = 0;
    int uret, off;
    if (new_pt == NULL)
        panic("alloc new_pt\n");
    int i;
    for (i = 0, off = ehdr.phoff; i < ehdr.phnum; i++, off += sizeof(ph))
    {
        if (ip->i_op->read(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
            goto bad;
        // if (ph.type == ELF_PROG_INTERP)
        //     is_dynamic = 1;
        if (ph.type != ELF_PROG_LOAD)
            continue;
        if (ph.memsz < ph.filesz)
            goto bad;
#if DEBUG
        printf("加载段 %d: 文件偏移 0x%lx, 大小 0x%lx, 虚拟地址 0x%lx\n", i, ph.off, ph.filesz, ph.vaddr);
#endif
        uret = uvm_grow(new_pt, sz, PGROUNDUP(ph.vaddr + ph.memsz), flags_to_perm(ph.flags));
        if (uret != PGROUNDUP(ph.vaddr + ph.memsz))
            goto bad;
        sz = uret;
        if (loadseg(new_pt, ph.vaddr, ip, ph.off, ph.filesz) < 0)
            goto bad;
    }

// 5. 打印入口点信息
#if DEBUG
    printf("ELF加载完成，入口点: 0x%lx\n", ehdr.entry);
#endif
    uint64 program_entry = 0;
    program_entry = ehdr.entry;
    p->pagetable = new_pt;
    alloc_vma_stack(p);
    uint64 sp = get_proc_sp(p);
    uint64 stackbase = sp - USER_STACK_SIZE;
    /*   开始处理glibc环境           */
    // 随机数
    sp -= 16;
    uint64 random[2] = {0x7be6f23c6eb43a7e, 0xb78b3ea1f7c8db96};
    if (sp < stackbase || copyout(new_pt, sp, (char *)random, 16) < 0)
        goto bad;
    /// auxv
    uint64 aux[MAXARG * 2 + 3] = {0, 0, 0};
    alloc_aux(aux, AT_HWCAP, 0);
    alloc_aux(aux, AT_PAGESZ, PGSIZE);
    alloc_aux(aux, AT_PHDR, ehdr.phoff); // @todo 暂不考虑动态链接
    alloc_aux(aux, AT_PHNUM, ehdr.phnum);
    alloc_aux(aux, AT_PHENT, ehdr.phentsize);
    alloc_aux(aux, AT_BASE, 0); // @todo 暂不考虑动态链接 设置为0
    alloc_aux(aux, AT_UID, 0);
    alloc_aux(aux, AT_EUID, 0);
    alloc_aux(aux, AT_GID, 0);
    alloc_aux(aux, AT_EGID, 0);
    alloc_aux(aux, AT_SECURE, 0);
    alloc_aux(aux, AT_RANDOM, sp);

    /// env
    int envc;
    uint64 estack[NENV];
    if(env){
        for (envc = 0; env[envc]; envc++)
        {
            assert(envc < NENV, "envc out of range!");
            sp -= strlen(env[envc]) + 1;
            sp -= sp % 16;
            assert(sp > stackbase, "sp out of range!");
            ret = copyout(new_pt, sp, (char *)env[envc], strlen(env[envc]) + 1);
            if (ret < 0)
                goto bad;
            estack[envc] = sp;
        }
    }

    /// arg
    int argc;
    uint64 ustack[NARG];
    if(argv){
        for (argc = 0; argv[argc]; argc++)
        {
            assert(argc < NARG, "argc out of range!");
            sp -= strlen(argv[argc]) + 1;
            sp -= sp % 16;
            assert(sp > stackbase, "sp out of range!");
            ret = copyout(new_pt, sp, (char *)argv[argc], strlen(argv[argc]) + 1);
            if (ret < 0)
                goto bad;
            ustack[argc] = sp;
        }
    }


    ustack[argc] = 0;
    if ((sp = loadaux(new_pt, sp, stackbase, aux)) == -1)
    {
        printf("loadaux failed\n");
        goto bad;
    }

    if (env)
    {
        sp -= (envc + 1) * sizeof(uint64);
        if (sp < stackbase)
        {
            goto bad;
        }
        sp -= sp % 16;
        if (copyout(new_pt, sp, (char *)(estack + 1), (envc + 1) * sizeof(uint64)) < 0)
        {
            goto bad;
        }
    }
    sp -= (argc + 2) * sizeof(uint64);
    sp -= sp % 16;
    if (sp < stackbase)
        goto bad;
    if (copyout(new_pt, sp, (char *)(uint64)ustack, (argc + 2) * sizeof(uint64)) < 0)
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

    return argc;

bad:
    panic("exec error!\n");
    return -1;
}
void alloc_aux(uint64 *aux, uint64 atid, uint64 value)
{
    // printf("aux[%d] = %p\n",atid,value);
    uint64 argc = aux[0];
    aux[argc * 2 + 1] = atid;
    aux[argc * 2 + 2] = value;
    aux[argc * 2 + 3] = 0;
    aux[argc * 2 + 4] = 0;
    aux[0]++;
}

int loadaux(pgtbl_t pt, uint64 sp, uint64 stackbase, uint64 *aux)
{
    int argc = aux[0];
    if (!argc)
        return sp;
    sp -= (2 * argc + 2) * sizeof(uint64);
    if (sp < stackbase)
    {
        return - 1;
    }
    aux[0] = 0;
    if (copyout(pt, sp, (char *)(aux + 1), (2 * argc + 2) * sizeof(uint64)) < 0)
        return -1;
    return sp;
}

static int flags_to_perm(int flags)
{
    int perm = 0;
#if defined RISCV
    if (flags & 0x01)
        perm |= PTE_X;
    if (flags & 0x02)
        perm |= PTE_W;
    if (flags & 0x04)
        perm |= PTE_R;
#else

    if (!(flags & 1))
        perm |= PTE_NX;
    if (flags & 2)
        perm |= PTE_W;
    if (!(flags & 4))
        perm |= PTE_NR;

#endif
    return perm;
}

static int loadseg(pgtbl_t pt, uint64 va, struct inode *ip, uint offset, uint sz)
{
    uint64 pa;
    int i, n;
    assert(va % PGSIZE == 0, "va need be aligned!\n");
    for (i = 0; i < sz; i += PGSIZE)
    {
        pa = walkaddr(pt, va + i);
        assert(pa != 0, "pa is null!\n");
        n = MIN(sz - i, PGSIZE);
        if (ip->i_op->read(ip, 0, (uint64)pa, offset + i, n) != n)
            return -1;
    }
    return 0;
}