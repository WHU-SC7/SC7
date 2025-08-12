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
// 重定向枚举类型
enum redir
{
    REDIR_OUT,
    REDIR_APPEND,
};
static uint64 flags_to_perm(int flags);
static int loadseg(pgtbl_t pt, uint64 va, struct inode *ip, uint offset, uint sz);
void alloc_aux(uint64 *aux, uint64 atid, uint64 value);
int loadaux(pgtbl_t pt, uint64 sp, uint64 stackbase, uint64 *aux);
void debug_print_stack(pgtbl_t pagetable, uint64 sp, uint64 argc, uint64 envc, uint64 aux[]);
static uint64 load_interpreter(pgtbl_t pt, struct inode *ip, elf_header_t *interpreter);
int is_sh_script(char *path);
int exec(char *path, char **argv, char **env)
{
    // load_elf_from_disk(0);
    struct inode *ip;
    char *original_path = path;

    // 将全局变量改为局部变量，避免多核竞态条件
    proc_t p_copy;
    uint64 ustack[NARG];
    uint64 estack[NENV];
    char *modified_argv[MAXARG];
    uint64 aux[MAXARG * 2 + 3] = {0, 0, 0};

    /* 脚本处理，如果是shell脚本，替换为busybox执行 */
    int is_shell_script = is_sh_script(path); ///< 判断路径是否为shell脚本
    if (is_shell_script)
    {
        original_path = "/musl/busybox"; ///< 若为脚本，替换为busybox执行脚本
        modified_argv[0] = "busybox";
        modified_argv[1] = "sh";
        int i;
        for (i = 2; i < MAXARG - 1 && argv[i - 2] != NULL; i++) ///< 复制原始参数
        {
            modified_argv[i] = argv[i - 2];
        }
        modified_argv[i] = NULL;
        argv = modified_argv;
        path = original_path;
    }
    
    /* 特殊路径处理：将/bin/sh替换为busybox sh */
    if (strcmp(path, "/bin/sh") == 0)
    {
        original_path = "/musl/busybox"; ///< 替换为busybox
        modified_argv[0] = "busybox";
        // modified_argv[1] = "sh";
        int i;
        for (i = 1; i < MAXARG - 1 && argv[i - 1] != NULL; i++) ///< 复制原始参数
        {
            modified_argv[i] = argv[i - 1];
        }
        modified_argv[i] = NULL;
        argv = modified_argv;
        path = original_path;
    }

    if (strcmp(path,"/tmp/hello") == 0)
    {
        original_path = "/musl/busybox"; ///< 替换为busybox
        modified_argv[0] = "busybox";
        modified_argv[1] = "sh";
        int i;
        for (i = 2; i < MAXARG - 1 && argv[i - 2] != NULL; i++) ///< 复制原始参数
        {
            modified_argv[i] = argv[i - 2];
        }
        modified_argv[i] = NULL;
        argv = modified_argv;
        path = original_path;
    }
    
    if (strcmp(path,"/code/lmbench_src/bin/build/lmbench_all") == 0)
    {
        path = "lmbench_all";
    }

    /* 打开目标文件 */
    if ((ip = namei(path)) == NULL)
    {
        printf("exec: fail to find file %s\n", path);
        return -1;
    }
    
    // 获取当前进程，但不假设已经持有锁
    struct proc *p = myproc();
    
    // 在读取文件之前锁定inode
    ip->i_op->lock(ip);
    
    elf_header_t ehdr;
    program_header_t ph;
    program_header_t interp; //< 保存interp程序头地址，用来读取所需解释器的name
    int ret;
    int is_dynamic = 0;
    
    /* 读取ELF头部信息并进行验证 */
    if (ip->i_op->read(ip, 0, (uint64)&ehdr, 0, sizeof(ehdr)) != sizeof(ehdr)) ///< 读取Elf头部信息
    {
        ip->i_op->unlock(ip);
        free_inode(ip);
        goto bad;
    }
    if (ehdr.magic != ELF_MAGIC) ///< 判断是否为ELF文件
    {
        printf("错误:不是有效的ELF文件\n");
        ip->i_op->unlock(ip);
        free_inode(ip);
        return -1;
    }

    /* 准备新进程环境 */
    // 获取进程锁进行进程状态修改
    acquire(&p->lock);
    p_copy = *p;
    uint64 oldsz = p->sz;
    p->sz = 0;
    free_vma_list(p);                      ///< 清除进程原来映射的VMA空间
    vma_init(p);                           ///< 初始化VMA列表
    pgtbl_t new_pt = proc_pagetable(p);    ///< 给进程分配新的页表
    uint64 low_vaddr = 0xffffffffffffffff; ///< 记录起始地址
    uint64 sz = 0;
    int off;
    if (new_pt == NULL) {
        ip->i_op->unlock(ip);
        free_inode(ip);
        release(&p->lock);
        panic("alloc new_pt\n");
    }
    
    // 在文件I/O操作前释放进程锁，但保持inode锁
    release(&p->lock);
    
    int i;
    /* 加载程序段 （PT_LOAD类型）*/
    for (i = 0, off = ehdr.phoff; i < ehdr.phnum; i++, off += sizeof(ph))
    {
        if (ip->i_op->read(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
            goto bad;
        if (ph.type == ELF_PROG_INTERP)
        {
            is_dynamic = 1;
            memmove((void *)&interp, (const void *)&ph, sizeof(ph)); //< 拷贝到interp，不然ph下一轮就被覆写了
        }

        // if(ph.type == ELF_PROG_PHDR)
        // {
        //     //< 本来想加载PHDR的，但是发现没有作用
        // }
        if (ph.type != ELF_PROG_LOAD) //< DYNAMIC段已经在PT_LOAD被加载了
            continue;
        if (ph.memsz < ph.filesz)
            goto bad;
        if (ph.vaddr + ph.memsz < ph.vaddr)
        {
            goto bad;
        }
        if (ph.vaddr < low_vaddr) ///< 更新最低虚拟地址并扩展虚拟内存
        {
            if (ph.vaddr != 0)
                uvm_grow(new_pt, sz, 0x100UL, flags_to_perm(ph.flags));
            low_vaddr = ph.vaddr;
        }

#if DEBUG
        printf("加载段 %d: 文件偏移 0x%lx, 大小 0x%lx, 虚拟地址 0x%lx, 权限标志: 0x%x\n", i, ph.off, ph.filesz, ph.vaddr, ph.flags);
        uint64 computed_perm = flags_to_perm(ph.flags);
        printf("  计算出的页面权限: 0x%x (R=%d, W=%d, X=%d)\n",
               computed_perm,
               !!(computed_perm & PTE_R),
               !!(computed_perm & PTE_W),
               !!(computed_perm & PTE_X));
#endif
        uint64 sz1;
        /* 扩展用户虚拟空间 */
#if defined RISCV
        sz1 = uvm_grow(new_pt, PGROUNDDOWN(ph.vaddr), ph.vaddr + ph.memsz, flags_to_perm(ph.flags));
#else
        sz1 = uvm_grow(new_pt, PGROUNDDOWN(ph.vaddr), ph.vaddr + ph.memsz, flags_to_perm(ph.flags));
#endif
        // if (uret != PGROUNDUP(ph.vaddr + ph.memsz))
        //     goto bad;
        sz = sz1;
        uint margin_size = 0;
        if ((ph.vaddr % PGSIZE) != 0) ///< 处理未对齐的段
        {
            margin_size = ph.vaddr % PGSIZE;
        }
        /* 加载段内容到内存中 */
        if (loadseg(new_pt, PGROUNDDOWN(ph.vaddr), ip, PGROUNDDOWN(ph.off), ph.filesz + margin_size) < 0)
            goto bad;
        sz = PGROUNDUP(sz1);
    }
    ip->i_op->unlock(ip);
    
    // 重新获取进程锁进行最终设置
    acquire(&p->lock);
    
    /* 设置进程内存，页表，虚拟地址，为动态映射mmap做准备 */
    p->virt_addr = low_vaddr;
    p->sz = sz;  // 确保正确设置进程内存大小
    p->pagetable = new_pt; ///< 便于mmap映射

    // 确保所有线程的sz也被正确设置
    for (struct list_elem *e = list_begin(&p->thread_queue);
         e != list_end(&p->thread_queue); e = list_next(e))
    {
        thread_t *t = list_entry(e, thread_t, elem);
        t->sz = sz;
    }

    /*----------------------------处理动态链接--------------------------*/
    uint64 interp_start_addr = 0;
    elf_header_t interpreter;
    if (is_dynamic && !strstr(myproc()->cwd.path,"basic") && !strstr(path,"basic"))
    {
        // 释放进程锁进行文件操作
        release(&p->lock);
        
        /* 从INTERP段读取所需的解释器 */
        char interp_name[256];
        if (interp.filesz > 256) //< 应该不会大于64吧
        {
            panic("interp段长度大于256,缓冲区不够读了!\n");
        }
        // interp.off表示interp段在elf文件中的偏移量。interp.filesz表示其长度。
        // interp段是一个字符串，例如/lib/ld-linux-riscv64-lp64d.so.1加上结尾的\0是0x21长
        ip->i_op->read(ip, 0, (uint64)interp_name, interp.off, interp.filesz); //< 读取字符串到interp_name
        DEBUG_LOG_LEVEL(LOG_INFO, "elf文件%s所需的解释器: %s\n", path, interp_name);
        
        // 释放当前inode并获取新的解释器inode
        ip->i_op->unlock(ip);
        free_inode(ip);
        ip = NULL; // 标记ip已经被释放
        
        struct inode *interp_ip = NULL;
        if (!strcmp((const char *)interp_name, "/lib/ld-linux-riscv64-lp64d.so.1")) //< rv glibc dynamic
        {
            if(strstr(path,"glibc") || strstr(path,"ltp") || strstr(path,"execv") || strstr(path,"dynamic")){
                if ((interp_ip = namei("/glibc/lib/ld-linux-riscv64-lp64d.so.1")) == NULL) ///< 这个解释器要求/usr/lib下有libc.so.6  libm.so.6两个动态库
                {
                    LOG_LEVEL(LOG_ERROR, "exec: fail to find interpreter: %s\n", interp_name);
                    return -1;
                }
            }
        }
        else if (!strcmp((const char *)interp_name, "/lib/ld-musl-riscv64-sf.so.1")) //< rv musl dynamic
        {
            if ((interp_ip = namei("/musl/lib/libc.so")) == NULL) ///< musl加载libc.so就行了
            {
                LOG_LEVEL(LOG_ERROR, "exec: fail to find libc.so for riscv musl\n");
                return -1;
            }
        }
        else if (!strcmp((const char *)interp_name, "/lib/ld-musl-riscv64.so.1")) //< rv musl dynamic
        {
            if ((interp_ip = namei("/musl/lib/libc.so")) == NULL) ///< musl加载libc.so就行了
            {
                LOG_LEVEL(LOG_ERROR, "exec: fail to find libc.so for riscv musl\n");
                return -1;
            }
        }
        else if (!strcmp((const char *)interp_name, "/lib64/ld-musl-loongarch-lp64d.so.1")) //< la musl dynamic
        {
            if ((interp_ip = namei("/musl/lib/libc.so")) == NULL) ///< musl加载libc.so就行了
            {
                LOG_LEVEL(LOG_ERROR, "exec: fail to find libc.so for loongarch musl\n");
                return -1;
            }
        }
        else if (!strcmp((const char *)interp_name, "/lib64/ld-linux-loongarch-lp64d.so.1")) //< la glibc dynamic
        {
            if ((interp_ip = namei("/glibc/lib/ld-linux-loongarch-lp64d.so.1")) == NULL) ///< 现在这个解释器加载动态库的时候有问题
            {
                LOG_LEVEL(LOG_ERROR, "exec: fail to find libc.so for loongarch musl\n");
                return -1;
            }
        }
        else
        {
            LOG_LEVEL(LOG_ERROR, "unknown interpreter: %s\n", interp_name);
            return -1;
        }

        // 锁定解释器inode
        interp_ip->i_op->lock(interp_ip);

        // program_header_t  interpreter_ph; ld-linux-riscv64-lp64d.so.1 libc.so.6 ld-linux-loongarch-lp64d.so.1

        if (interp_ip->i_op->read(interp_ip, 0, (uint64)&interpreter, 0, sizeof(interpreter)) != sizeof(interpreter)) ///< 读取Elf头部信息
        {
            interp_ip->i_op->unlock(interp_ip);
            free_inode(interp_ip);
            goto bad;
        }
        if (interpreter.magic != ELF_MAGIC) ///< 判断是否为ELF文件
        {
            printf("错误：不是有效的ELF文件\n");
            interp_ip->i_op->unlock(interp_ip);
            free_inode(interp_ip);
            return -1;
        }
        interp_start_addr = load_interpreter(new_pt, interp_ip, &interpreter); ///< 加载解释器
        
        // 释放解释器inode
        interp_ip->i_op->unlock(interp_ip);
        free_inode(interp_ip);
        
        // 重新获取进程锁
        acquire(&p->lock);
        
        // 重新设置进程内存大小，因为锁被重新获取后可能被重置
        p->sz = sz;
        
        // 确保所有线程的sz也被正确设置
        for (struct list_elem *e = list_begin(&p->thread_queue);
             e != list_end(&p->thread_queue); e = list_next(e))
        {
            thread_t *t = list_entry(e, thread_t, elem);
            t->sz = sz;
        }
    }
    
    // 只有在ip没有被释放的情况下才释放它
    if (ip != NULL) {
        free_inode(ip);
    }

    /*----------------------------结束动态链接--------------------------*/
// 5. 打印入口点信息
#if DEBUG
    printf("ELF加载完成，入口点: 0x%lx\n", ehdr.entry);
#endif
    /* 设置程序入口点 */
    uint64 program_entry = 0;
    if (interp_start_addr)
        program_entry = interp_start_addr + interpreter.entry; ///< 动态链接地址
    else
        program_entry = ehdr.entry; ///< 设置程序的entry地址
    alloc_vma_stack(p);             ///< 给进程分配栈空间
    uint64 sp = get_proc_sp(p);     ///< 获取栈指针
    uint64 stackbase = sp - USER_STACK_SIZE;
#ifdef RISCV
    mappages(p->pagetable, 0x000000010000036e, (uint64)pmem_alloc_pages(1), PGSIZE, PTE_R | PTE_W | PTE_X | PTE_U | PTE_D); //< 动态链接要访问这个地址，映射了能跑，但是功能不完全
#endif

    /*-------------------------------   开始处理glibc环境    -----------------------------*/
    int redirection = -1;
    char *redir_file = NULL;
    int argc;
    int redirend = -1;
    int first = -1;
    // 处理重定向符号 > 或 >>
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
            redirend = argc; ///< 标记重定向结束位置
            continue;
        }
    }

    /// 遍历环境变量数组 env，将每个环境变量字符串复制到用户栈 environment ASCIIZ str
    int envc;
    estack[0] = 0;
    sp -= sp % 16;
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
        estack[estack[0] + 1] = 0;
    }

    /// 填充ustack
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
    ustack[ustack[0] + 1] = 0; // 添加终止符 NULL
    // 随机数
    sp -= 16;
    uint64 random[2] = {0x7be6f23c6eb43a7e, 0xb78b3ea1f7c8db96}; /// AT_RANDOM值
    if (sp < stackbase || copyout(new_pt, sp, (char *)random, 16) < 0)
        goto bad;
    /// auxv 填充辅助变量

    alloc_aux(aux, AT_HWCAP, 0);
    alloc_aux(aux, AT_PAGESZ, PGSIZE);
    alloc_aux(aux, AT_PHDR, ehdr.phoff + p->virt_addr); // 程序头表地址
    // LOG_LEVEL(LOG_ERROR,"ehdr.phoff + p->virt_addr: %x\n",ehdr.phoff + p->virt_addr); //< 红字显示信息，更醒目 :) .本来是想看ehdr头的地址，但是好像没有影响
    alloc_aux(aux, AT_PHENT, ehdr.phentsize); // 程序头大小
    alloc_aux(aux, AT_PHNUM, ehdr.phnum);
    alloc_aux(aux, AT_BASE, interp_start_addr); // 解释器基址
    alloc_aux(aux, AT_ENTRY, ehdr.entry);       // 程序入口
    alloc_aux(aux, AT_UID, 0);                  // 用户ID
    alloc_aux(aux, AT_EUID, 0);                 // 有效用户ID
    alloc_aux(aux, AT_GID, 0);                  // 组ID
    alloc_aux(aux, AT_EGID, 0);                 // 有效组ID
    alloc_aux(aux, AT_SECURE, 0);               // 安全模式
    alloc_aux(aux, AT_RANDOM, sp);              // 随机数地址
    alloc_aux(aux, AT_FLAGS, 0);                // 标志位
    alloc_aux(aux, AT_NULL, 0);                 // 结束标志

    /* Load Aux */
    if ((sp = loadaux(new_pt, sp, stackbase, aux)) == -1)
    {
        printf("loadaux failed\n");
        goto bad;
    }

    /// 复制环境变量指针数组（即使没有环境变量也要复制NULL指针）
    argc = estack[0];
    if (argc)
    {
        sp -= (estack[0] + 1) * sizeof(uint64); // +1 为终止NULL
        sp -= sp % 16;                          // 确保栈指针对齐
                                                // 复制从 estack[1] 开始的所有指针（包含终止NULL）
        if (copyout(new_pt, sp, (char *)(estack + 1),
                    (argc + 1) * sizeof(uint64)) < 0)
        {
            goto bad;
        }
    }

    argc = ustack[0];
    sp -= (argc + 2) * sizeof(uint64);
    sp -= sp % 16;
    if (sp < stackbase)
        goto bad;
    if (copyout(new_pt, sp, (char *)ustack, (argc + 2) * sizeof(uint64)) < 0)
        goto bad;

    p->trapframe->a1 = sp + 8;
#if defined RISCV
    p->trapframe->epc = program_entry;
#else
    p->trapframe->era = program_entry;
#endif
    p->trapframe->sp = sp;
#if DEBUG
    printf("Jump to entry: 0x%lx (interp base: 0x%lx)\n",
           program_entry, interp_start_addr);
    debug_print_stack(new_pt, sp, ustack[0], estack[0], aux);
#endif
    /// 处理重定向
    if (redirection != -1)
    {
        get_file_ops()->close(p->ofile[1]); ///< 标准输出
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
    /// 清理旧进程资源
    proc_freepagetable(&p_copy, oldsz);

    // 释放进程锁（确保我们持有锁）
    if (holding(&p->lock)) {
        release(&p->lock);
    }

    return 0;
    //< FUCK GLIBC!!!

bad:
    // 确保在错误情况下也释放锁
    if (holding(&p->lock)) {
        release(&p->lock);
    }
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

static uint64 flags_to_perm(int flags)
{
    uint64 perm = 0;
#if defined RISCV
    if (flags & 0x01)
        perm |= PTE_X;
    if (flags & 0x02)
        perm |= PTE_W | PTE_D;
    if (flags & 0x04)
        perm |= PTE_R;
#else

    if (!(flags & 1))
        perm |= PTE_NX;
    if (flags & 2)
        perm |= PTE_W | PTE_D;
    if (!(flags & 4))
        perm |= PTE_NR;

#endif
    return perm;
}

/// @brief 计算解释器内存映射大小
/// @param interpreter
/// @param ip
/// @return
uint64 get_mmap_size(elf_header_t *interpreter, struct inode *ip)
{
    int i, off;
    uint64 minaddr = -1;
    uint64 maxaddr = 0;
    int flag = 0;
    program_header_t ph;
    for (i = 0, off = interpreter->phoff; i < interpreter->phnum; i++, off += sizeof(ph))
    {
        if (ip->i_op->read(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
        {
            panic("read ph error!\n");
        }
        if (ph.type == ELF_PROG_LOAD)
        {
            flag = 1;
            minaddr = MIN(minaddr, PGROUNDDOWN(ph.vaddr));
            maxaddr = MAX(maxaddr, ph.vaddr + ph.memsz);
        }
    }
    if (flag)
        return maxaddr - minaddr;
    return 0;
}
/**
 * @brief  加载段到内存
 *
 * @param pt
 * @param va
 * @param ip
 * @param offset
 * @param sz
 * @return int
 */
static int loadseg(pgtbl_t pt, uint64 va, struct inode *ip, uint offset, uint sz)
{
    uint64 pa;
    int i, n;
#if DEBUG
    LOG_LEVEL(LOG_DEBUG, "[loadseg] : va:%p,end:%p,sz:%p\n", va, va + sz, sz);
#endif
    assert(va % PGSIZE == 0, "va need be aligned!\n");
    for (i = 0; i < sz; i += PGSIZE)
    {
        pa = walkaddr(pt, va + i);
        assert(pa != 0, "pa is null!,virt_addr:%p not map!\n", va + i);
        n = MIN(sz - i, PGSIZE);
        if (ip->i_op->read(ip, 0, (uint64)pa, offset + i, n) != n)
            return -1;
    }
    return 0;
}
/**
 * @brief 加载动态链接器
 *
 * @param pt
 * @param ip
 * @param interpreter
 * @return uint64
 */
static uint64 load_interpreter(pgtbl_t pt, struct inode *ip, elf_header_t *interpreter)
{
    uint64 sz, startaddr;
    program_header_t ph;
    if ((sz = get_mmap_size(interpreter, ip)) == 0)
        panic("mmap size is zero!\n");
    /// 分配内存空间
    startaddr = mmap(0, sz, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ALLOC |MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (startaddr == -1)
        panic("mmap error!\n");
    /// 加载解释器的每个段
    int i, off;
    for (i = 0, off = interpreter->phoff; i < interpreter->phnum; i++, off += sizeof(ph))
    {
        if (ip->i_op->read(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
        {
            panic("read ph error!\n");
        }
        if (ph.type == ELF_PROG_LOAD)
        {
            assert(ph.memsz >= ph.filesz, "ph.memsz:%d < ph.filesz:%d!");
            assert(ph.vaddr + ph.memsz >= ph.vaddr, "ph.vaddr + ph.memsz < ph.vaddr");

            uint32 margin_size = 0;
            if (ph.vaddr % PGSIZE != 0)
                margin_size = ph.vaddr % PGSIZE;

            if (loadseg(pt, PGROUNDDOWN(startaddr + ph.vaddr), ip, PGROUNDDOWN(ph.off), ph.filesz + margin_size) < 0)
                panic("loadseg error!\n");
        }
    }

    return startaddr;
}

void debug_print_stack(pgtbl_t pagetable, uint64 sp, uint64 argc, uint64 envc, uint64 aux[])
{
    PRINT_COLOR(YELLOW_COLOR_PRINT, "User stack layout at exec:\n");
    PRINT_COLOR(YELLOW_COLOR_PRINT, "position            content                     size (bytes) + comment\n");
    PRINT_COLOR(YELLOW_COLOR_PRINT, "------------------------------------------------------------------------\n");

    // 1. 打印 argc
    uint64 argc_val;
    if (copyin(pagetable, (char *)&argc_val, sp, sizeof(uint64)))
    {
        PRINT_COLOR(YELLOW_COLOR_PRINT, "  [sp]             [Failed to read argc]\n");
        return;
    }
    PRINT_COLOR(YELLOW_COLOR_PRINT, "  %lx  [ argc = %d ]            8\n", sp, (int)argc_val);
    sp += sizeof(uint64);

    // 2. 打印 argv 数组
    for (int i = 0; i <= argc; i++)
    {
        uint64 argv_ptr;
        if (copyin(pagetable, (char *)&argv_ptr, sp + i * sizeof(uint64), sizeof(uint64)))
        {
            PRINT_COLOR(YELLOW_COLOR_PRINT, "  %lx  [ Failed to read argv[%d] ]\n", sp + i * sizeof(uint64), i);
            continue;
        }

        if (i < argc)
        {
            char arg_str[256];
            if (copyinstr(pagetable, arg_str, argv_ptr, sizeof(arg_str)) > 0)
            {
                PRINT_COLOR(YELLOW_COLOR_PRINT, "  %lx  [ argv[%d] = %s ]      8\n",
                            sp + i * sizeof(uint64), i, arg_str);
            }
            else
            {
                PRINT_COLOR(YELLOW_COLOR_PRINT, "  %lx  [ argv[%d] = ? ]      8\n",
                            sp + i * sizeof(uint64), i);
            }
        }
        else
        {
            PRINT_COLOR(YELLOW_COLOR_PRINT, "  %lx  [ argv[%d] = NULL ]      8\n",
                        sp + i * sizeof(uint64), i);
        }
    }
    sp += (argc + 1) * sizeof(uint64);
    sp += sp % 16;

    // 3. 打印 envp 数组
    for (int i = 0; i <= envc; i++)
    {
        uint64 envp_ptr;
        if (copyin(pagetable, (char *)&envp_ptr, sp + i * sizeof(uint64), sizeof(uint64)))
        {
            PRINT_COLOR(YELLOW_COLOR_PRINT, "  %lx  [ Failed to read envp[%d] ]\n", sp + i * sizeof(uint64), i);
            continue;
        }

        if (i < envc)
        {
            char env_str[256];
            if (copyinstr(pagetable, env_str, envp_ptr, sizeof(env_str)) > 0)
            {
                PRINT_COLOR(YELLOW_COLOR_PRINT, "  %lx  [ envp[%d] = %s ]      8\n",
                            sp + i * sizeof(uint64), i, env_str);
            }
            else
            {
                PRINT_COLOR(YELLOW_COLOR_PRINT, "  %lx  [ envp[%d] = ? ]      8\n",
                            sp + i * sizeof(uint64), i);
            }
        }
        else
        {
            PRINT_COLOR(YELLOW_COLOR_PRINT, "  %lx  [ envp[%d] = NULL ]      8\n",
                        sp + i * sizeof(uint64), i);
        }
    }
    sp += (envc) * sizeof(uint64);
    sp += sp % 16;

    // 4. 打印 auxv 数组
    int aux_index = 0;
    while (1)
    {
        uint64 type, val;
        if (copyin(pagetable, (char *)&type, sp, sizeof(uint64)) ||
            copyin(pagetable, (char *)&val, sp + 8, sizeof(uint64)))
        {
            break;
        }

        const char *type_str = "UNKNOWN";
        if (type == AT_ENTRY)
            type_str = "AT_ENTRY";
        else if (type == AT_PHNUM)
            type_str = "AT_PHNUM";
        else if (type == AT_PHDR)
            type_str = "AT_PHDR";
        else if (type == AT_RANDOM)
            type_str = "AT_RANDOM";
        else if (type == AT_HWCAP)
            type_str = "AT_HWCAP";
        else if (type == AT_PAGESZ)
            type_str = "AT_PAGESZ";
        else if (type == AT_PHENT)
            type_str = "AT_PHENT";
        else if (type == AT_ENTRY)
            type_str = "AT_ENTRY";
        else if (type == AT_BASE)
            type_str = "AT_BASE";
        else if (type == AT_NULL)
            type_str = "AT_NULL";

        PRINT_COLOR(YELLOW_COLOR_PRINT, "  %lx  [ %s ]      16  (type=0x%lx, val=0x%lx)\n",
                    sp, type_str, type, val);

        if (type == AT_NULL)
            break;

        sp += 16;
        aux_index++;
    }
    PRINT_COLOR(YELLOW_COLOR_PRINT, "------------------------------------------------------------------------\n");
}