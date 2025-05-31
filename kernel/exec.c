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

enum redir
{
    REDIR_OUT,
    REDIR_APPEND,
};
static int flags_to_perm(int flags);
static int loadseg(pgtbl_t pt, uint64 va, struct inode *ip, uint offset, uint sz);
void alloc_aux(uint64 *aux, uint64 atid, uint64 value);
int loadaux(pgtbl_t pt, uint64 sp, uint64 stackbase, uint64 *aux);
proc_t p_copy;
uint64 ustack[NARG];
uint64 estack[NENV];
char *modified_argv[MAXARG];
uint64 aux[MAXARG * 2 + 3] = {0, 0, 0};
int is_sh_script(char *path);
int exec(char *path, char **argv, char **env)
{
    // load_elf_from_disk(0);
    struct inode *ip;
    //char *original_path = path;


    // int is_shell_script = is_sh_script(path);
    // if (is_shell_script)
    // {
    //     original_path = "/musl/busybox"; 
    //     modified_argv[0] = "busybox";
    //     modified_argv[1] = "sh";
    //     modified_argv[2] = path;
    //     int i;
    //     for (i = 3; i < MAXARG - 1 && argv[i - 3] != NULL; i++) {
    //         modified_argv[i] = argv[i - 3];
    //     }
    //     modified_argv[i] = NULL;
    //     argv = modified_argv;
    //     path = original_path;
    // }

    if ((ip = namei(path)) == NULL)
    {
        printf("exec: fail to find file %s\n", path);
        return -1;
    }
    elf_header_t ehdr;
    program_header_t ph;
    int ret;
    // int is_dynamic = 0;
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
    p_copy = *p;
    free_vma_list(p);
    vma_init(p);
    pgtbl_t new_pt = proc_pagetable(p);
    uint64 low_vaddr = 0xffffffffffffffff;
    uint64 sz = 0;
    int off;
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
        if (ph.vaddr + ph.memsz < ph.vaddr)
        {
            goto bad;
        }
        if (ph.vaddr < low_vaddr)
        {
            if (ph.vaddr != 0)
                uvm_grow(new_pt, sz, 0x100UL, flags_to_perm(ph.flags));
            low_vaddr = ph.vaddr;
        }

#if DEBUG
        printf("加载段 %d: 文件偏移 0x%lx, 大小 0x%lx, 虚拟地址 0x%lx\n", i, ph.off, ph.filesz, ph.vaddr);
#endif
        uint64 sz1;
#if defined RISCV
        sz1 = uvm_grow(new_pt, PGROUNDDOWN(ph.vaddr), ph.vaddr + ph.memsz, flags_to_perm(ph.flags));
#else
        sz1 = uvm_grow(new_pt, PGROUNDDOWN(ph.vaddr), ph.vaddr + ph.memsz, flags_to_perm(ph.flags));
#endif
        // if (uret != PGROUNDUP(ph.vaddr + ph.memsz))
        //     goto bad;
        sz = sz1;
        uint margin_size = 0;
        if ((ph.vaddr % PGSIZE) != 0)
        {
            margin_size = ph.vaddr % PGSIZE;
        }
        if (loadseg(new_pt, PGROUNDDOWN(ph.vaddr), ip, PGROUNDDOWN(ph.off), ph.filesz + margin_size) < 0)
            goto bad;
        sz = PGROUNDUP(sz1);
    }
    p->virt_addr = low_vaddr;
// 5. 打印入口点信息
#if DEBUG
    printf("ELF加载完成，入口点: 0x%lx\n", ehdr.entry);
#endif
    uint64 program_entry = 0;
    program_entry = ehdr.entry;
    p->pagetable = new_pt;
    alloc_vma_stack(p);
    uint64 oldsz = p->sz;
    uint64 sp = get_proc_sp(p);
    uint64 stackbase = sp - USER_STACK_SIZE;
    /*   开始处理glibc环境           */
    // 随机数
    sp -= 16;
    uint64 random[2] = {0x7be6f23c6eb43a7e, 0xb78b3ea1f7c8db96};
    if (sp < stackbase || copyout(new_pt, sp, (char *)random, 16) < 0)
        goto bad;
    /// auxv

    alloc_aux(aux, AT_HWCAP, 0);
    alloc_aux(aux, AT_PAGESZ, PGSIZE);
    alloc_aux(aux, AT_PHDR, ehdr.phoff); // @todo 暂不考虑动态链接
    alloc_aux(aux, AT_PHNUM, ehdr.phnum);
    alloc_aux(aux, AT_PHENT, ehdr.phentsize);
    alloc_aux(aux, AT_ENTRY, ehdr.entry);
    alloc_aux(aux, AT_BASE, 0); // @todo 暂不考虑动态链接 设置为0
    alloc_aux(aux, AT_UID, 0);
    alloc_aux(aux, AT_EUID, 0);
    alloc_aux(aux, AT_GID, 0);
    alloc_aux(aux, AT_EGID, 0);
    alloc_aux(aux, AT_SECURE, 0);
    alloc_aux(aux, AT_RANDOM, sp);
    alloc_aux(aux, AT_NULL, 0);
    // /// 处理重定向

    int redirection = -1;
    char *redir_file = NULL;

    int argc;
    int redirend = -1;
    int first = -1;
    for (argc = 0; argv[argc]; argc++)
    {
        if (strlen(argv[argc]) == 1 && strncmp(argv[argc], ">", 1) == 0)
        {
            redirection = REDIR_OUT;
        }
        else if (strlen(argv[argc]) == 2 && strncmp(argv[argc], ">>", 2) == 0)
        {
            redirection = REDIR_APPEND;
        }
        if (redirection != -1 && first == -1)
        {
            redir_file = argv[argc + 1];
            first = 1;
            redirend = argc;
            continue;
        }
    }

    /// 遍历环境变量数组 env，将每个环境变量字符串复制到用户栈 environment ASCIIZ str
    int envc;
    if (env)
    {
        for (envc = 0; env[envc]; envc++)
        {
            uint64 index = ++estack[0];
            assert(envc < NENV, "envc out of range!");
            sp -= strlen(env[envc]) + 1;
            sp -= sp % 16;
            assert(sp > stackbase, "sp out of range!");
            ret = copyout(new_pt, sp, (char *)env[envc], strlen(env[envc]) + 1);
            if (ret < 0)
                goto bad;
            estack[index] = sp;
            estack[index + 1] = 0;
        }
    }

    /// arg
    ustack[0] = 0;
    if (argv)
    {
        for (argc = 0; argv[argc] && argc != redirend; argc++)
        {
            uint64 index = ++ustack[0];
            assert(argc < NARG, "argc out of range!");
            sp -= strlen(argv[argc]) + 1;
            sp -= sp % 16;
            assert(sp > stackbase, "sp out of range!");
            ret = copyout(new_pt, sp, (char *)argv[argc], strlen(argv[argc]) + 1);
            if (ret < 0)
                goto bad;
            ustack[index] = sp;
            ustack[index + 1] = 0;
        }
    }
    /// 将 aux 数组中的辅助向量复制到用户栈
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

    argc = ustack[0];
    sp -= (argc + 2) * sizeof(uint64);
    sp -= sp % 16;
    if (sp < stackbase)
        goto bad;
    if (copyout(new_pt, sp, (char *)(uint64)ustack, (argc + 2) * sizeof(uint64)) < 0)
        goto bad;

    p->trapframe->a1 = sp + 8;
    p->sz = sz;
#if defined RISCV
    p->trapframe->epc = program_entry;
#else
    p->trapframe->era = program_entry;
#endif
    p->trapframe->sp = sp;

    if (redirection != -1)
    {
        get_file_ops()->close(p->ofile[1]);
        myproc()->ofile[1] = 0;
        const char *dirpath = myproc()->cwd.path;
        if (redirection == REDIR_OUT)
        {
            vfs_ext4_open(redir_file, dirpath, O_WRONLY);
        }
        else if (redirection == REDIR_APPEND)
        {
            vfs_ext4_open(redir_file, dirpath, O_WRONLY | O_APPEND);
        }
    }

    proc_freepagetable(&p_copy, oldsz);

    return 0;
    //< FUCK GLIBC!!!

bad:
    panic("exec error!\n");
    return -1;
}

int is_sh_script(char *path)
{
    int len = strlen(path);
    if (len < 3)
    {
        return 0;
    }
    if (path[len - 1] == 'h' && path[len - 2] == 's' && path[len - 3] == '.')
    {
        return 1;
    }
    return 0;
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
        return -1;
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

// #### OS COMP TEST GROUP START basic-musl-glibc ####
//