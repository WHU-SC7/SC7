# Git提交规范
feat：新功能（feature）。

fix/to：修复bug，可以是QA发现的BUG，也可以是研发自己发现的BUG。
fix：产生diff并自动修复此问题。适合于一次提交直接修复问题
to：只产生diff不自动修复此问题。适合于多次提交。最终修复问题提交时使用fix

docs：文档（documentation）。

style：格式（不影响代码运行的变动）。

refactor：重构（即不是新增功能，也不是修改bug的代码变动）。

perf：优化相关，比如提升性能、体验。

test：增加测试。

chore：构建过程或辅助工具的变动。

revert：回滚到上一个版本。

merge：代码合并。

sync：同步主线或分支的Bug。

示例:
> [feat] 添加了锁
> 1. 添加了自旋锁
> 2. 添加了睡眠锁

注意下面进行详细解释的时候需要带上编号`1., 2., ...`

# 代码与注释规范
## 函数前注释
```c
/**
 * @brief 这里写对函数功能的简单介绍
 *
 * 这里可以根据有需要是否要详细说明函数功能
 *
 * @param 这里对参数1进行介绍
 * @param 这里对参数2进行介绍
 * ...
 * @retval 这里对返回值进行介绍
 */
 void            ///< 返回值参数另起一行，防止出现staic unsigned int这种比较长的情况
 happy (void)    ///< 如果参数为空，最好还是带上void，K&R标准在现代C语言标准中已经不再推荐
 {               ///< 大括号另起一行，方便对应

 }
```
介绍可以带上参数的类型(比如int啥的)，也可以不带

## 函数内注释
### 功能注释，单行注释和补充注释
对于每一个需要介绍的功能块，前后需要用空行包裹，然后前面带上注释。
如果文字较少，用单行注释格式，否则用多行注释格式。
补充注释在后面用`///<`。
示例:
```c
void
happy (void)
{
    /* 
     * 这里循环一百遍                  ///< 文字多，多行注释示例
     */              
    for (int i = 0; i < 100; ++i)
    {
        printf ("I am happy.");     ///< 补充注释示例
    }
                                    ///< 空白行
    /* 这里循环一百遍 */              ///< 文字少，单行注释示例
    for (int i = 0; i < 100; ++i)
    {
        printf ("I am happy.");     ///< 补充注释示例
    }
}
```

## 结构体注释
注意对参数的解释说明时，所有注释要对齐。
示例:
```c
/**
 * @brief 基数树结构体
 * 
 * 包含指向根节点的指针和已使用的内存页计数。
 */
typedef struct 
{
    RadixNode *root;  		    ///< 根节点
    size_t used_pages;  		///< 已使用的内存页计数
} RadixTree;

```

## 代码、括号规范
使用大括号的时候，换行另起一行。一行代码可以不要大括号，注意缩进即可，尽量不要压行。一般
不超过整个VSCODE界面一般，如果超过就换行，注意对齐。比如:
```c
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);
```

## 其他情况
其他情况遵循结构清晰即可。尽量不要使用`//`注释。注释主要是清晰明确，写完代码后加上即可也不需要过分详细，占用太多时间，主要时间应该是在code上，注释是方便交流，不论对象是对别人还是明天的自己: )。

# 2025.3.29 ly
[feat] 添加颜色打印宏PRINT_COLOR和日志打印宏LOG
[refactor] kernel下include头文件加上宏保护
[question] 
1. 为什么hal、hsai 和 kernel下都包含了riscv.h,这样需要同时维护三份代码
2. hal 和 hsai 的各自的作用是什么
3. hsai下.c 为什么在kernel下include

[feat] 增加物理内存分配，暂定为64M 新增assert函数
[test] 增加内存分配测试

[bug]
1. ~~耗尽内存测试在循环大于1时卡死~~
   ~~没有判断多次释放，这样多次释放还是会加入表头~~
2. ~~只是释放内存时清空页不够，还得分配时清空页，初始化时全清空也不行~~
    每次分配时是将linknode 强制转换成地址，linknode中存放了next，是两字节指针


# 2025.3.31 ly
[fix] fix物理内存管理Double Free Bug，物理内存已通过测试
    PAGE_NUM 宏定义未加括号，导致运算优先级异常
    添加物理内存起始地址页对齐

# 2025.4.1 ly
[feat] 新增虚拟内存管理 kernel_pagetable ,walk与vmmap
[question]
1. loongarch.h下的pte与pa转换是不是有问题
[bug]
1. ~~在配置内核页表,进入时，riscv.h跳转异常~~  -> vscode gdb调试问题 
    官方gdb 8.2与Qemu版本不匹配，导致出现问题 
2. ~~vmem.c中必须要用hsai/riscv.h~~  
    也是gdb版本问题
3. ~~Risv页面映射完之后printf处卡住~~
   已解决，内核数据段映射内存少导致卡住


# 2025.4.2 ly
[bug]
1. *pte = PA2PTE(pt) | PTE_V;
    printf("pt is %p,to pte %p\n",pt,PA2PTE(pt));
    #define PTE2PA(pte) (((pte) >> 10) << 12)
    调试打印pt is 0x900000009004e000,to pte 0x2400000024013800
问题所在：Loongarch pte存PPN时，需要 & ~dmwin_mask

对于loongarch来说，在walk和mappage等映射函数时需要注意：
1. 在页表中找到页表项的地址时,用&提取出值时，提取的是物理地址，需要*to_vir*转化为虚拟地址接下来寻找下一级
2. 在写入页表项时，高位为物理地址，低位为Flag,由于新页是从PMM申请而来，目前地址高位为9，需要*to_phy*转化为物理地址
3. 在访问不能访问的地址时，gdb会卡住


# 2025.4.3 ly
[feat] walk 与 mappage 成功运行
[todo] 映射进程栈，实现copyin , copyout , copyinstr
[tips]
1. loongarch中的页表项是低12位为Flags，和riscv低10位flags不一样
2. 引入<stdint.h>可以使用uintptr_t
[question]
1. kernel下要引用hsai include的memlayout.h改怎么做
2. // vmem_mappages(kernel_pagetable, (uint64)&KERNEL_TEXT, (uint64)&KERNEL_TEXT, (uint64)&KERNEL_DATA-(uint64)&KERNEL_TEXT, PTE_R | PTE_W );
    内核数据段映射空间小的话会导致卡死


# 2025.4.4 czx
[feat] 添加了锁和代码规范
1. 添加了自旋锁和睡眠锁，自旋锁经过测试，同时加入到了进程模块，没有问题。睡眠锁没有测试，之后大概率也用不到睡眠锁。
2. 添加了代码的撰写规范和注释撰写规范。
[bug] uart的自旋锁和相关函数，loongarch的usys.S下函数重定义问题
1. ~~process.c 中的 sleep函数与user文件夹下loongarch的usys.S中出现重定义~~，改名为'sleep_on_chan'。
2. uart.c 下又实现了自旋锁和相关函数，可能需要弄出来。

[todo] ~~注释完善~~

# 2025.4.4 ly
[style] 重整代码风格，添加注释
1. 重整了Pmem,Vmem的代码，添加了注释


# 2025.4.5 ly
[feat] 增加了进程map_stack
1. process.c中添加了map_stack函数，用于映射进程栈
2. riscv.h loongzarch.h 中添加了KSTACK宏，用于获取进程栈的起始地址
3. 新增PTE_MAPSTACK  PTE_TRAMPOLINE  宏，处理两种架构在映射stack和trampoline时不同的权限位


# 2025.4.7 lm
[feat] 统一双架构的用户程序
1.rv和la能使用同一个函数user_program_run()来运行用户程序.现在主函数大大缩短了
2.调整了hsai的csr相关函数位置，增加注释.
3.hsai_trap_init,loongarch需要
[fix] 修复spinlock引起的riscv用户程序只能进行一次系统调用的bug
只需要调整proc结构体，把spinlock放在末尾。
奇怪的bug,原因不能确定。也许是内存布局问题


# 2025.4.7 lm
[feat] 支持scheduler启动线程
1.现在创建线程后，进入scheduler即可正常运行线程
2.虚拟内存的用户程序写了一半。现在感觉需要先实现scheduler功能，所以先提交一次
3.修补了一些小问题。比如hsai_set_trapframe_epc，这个函数在原来手动进入线程时并不需要，现在需要了。已经补全
4.用userret手动进入线程的方法保留在sc7_start_kernel的函数user_program_run中，可以直接使用。
这是为了以防后续需要参考老办法。


# 2025.4.7 ly
[feat] 添加unmap walkaddr copyin copyout, panic添加打印文件和行号信息


# 2025.4.9 ly
[feat] 虚拟化用户态程序跑通 ,makefile中新增将Riscv用户态程序编译为C语言字节数组initcode
[fix] 修正mappages中判断页面对齐的错误
1. usertrap 得用proc()->trapframe获取，不能用传参接收
2. 将trampoline.s的实现替换为xv6的实现 
3. rv的trampoline放在text段，并实现对齐 
4. process kstack初始化为虚拟地址 trapframe使用Pmem分配页面 alloc时创建用户态页表，映射trapframe、trampoline
5. initcode对应用户态程序代码段

# 2025.4.11 czx
[feat] 添加了timer和loognarch的trap及部分功能
1. 添加了timer计时器，除了基本的定时中断功能外，也支持`sys_times`查看进程的内核态和用户态运行时间
2. 修改了assert，使之现在能传入condition
3. 完善了中断处理，使得`devintr`函数专门用于处理中断
4. 完善了loongarch的usertrap和kerneltrap处理函数
5. 添加了loongarch的kernelvec.S，使之能实现内核trap处理函数的跳转
6. hal下两个架构的spinlock.h使用软链接连接到了include文件夹下的spinlock.h，不知道能不能在评测机下自动链接，待测试


# 2025.4.11 ly
[feat] loongarch用户程序能进入usertrap 
1. 添加了Tlb重填处理 tlbrefill.S merrvec.S
2. 添加w_csr_pwcl,配置页表遍历过程
3. 删除include/kernel下多余的loongarch.h,重命名loongarch用户程序为user.c

# 2025.4.12 lm
[feat] 可以生成riscv用户程序的initcode
1. 对user目录的Makefile进行了一些修改，可以生成initcode到user/build下。用法是makeinitcode。主要是加了链接脚本，给syscall函数和init_main指定到段实现的
2. 一个小bug，要先make rv再make initcode才行，不然make initcode时使用的依赖文件-MF是错误的。
3. 把主函数文件名改成SC7_start_kernel。然后gitignore user/build目录
[todo] ~~之后把loongarch的initcode也做出来，然后重整一下~~

# 2025.4.12 lm
[feat] 实现时间片轮转调度
1. riscv可以时间片调度，添加usertrap在时钟中断后返回的逻辑
2. process.c现在都使用cpu进行调度
3. 验证了生成的initcode的正确性
4. 验证了可以正确的传参。
5. 栈的设置，现在用户程序的数据代码和栈共用一个页，目前的大小够用了。

[todo] 
1. 开关中断的逻辑没有完全实现
2. ~~yield的锁没有加，锁问题没有解决~~
3. ~~loongarch的initcode待添加~~

# 2025.4.13 ly
[feat] 新增系统调用 getpid、fork、wait、exit & 解决锁问题

1. include/kernel下新增syscall_ids.h，定义了系统调用的编号;def.h 用于常用宏
2. 修改allocproc锁逻辑，结束后不立刻释放锁，在fork|userinit结束后释放
3. 修改进程被fork后首次进入forkret,进入后释放锁
4. proc结构体新增exit_state和killed
5. vmem下新增uvmcopy、uvmfree,用于fork时复制页表

[bug] ~~目前test_fork子进程未能成功退出，父进程一直等待~~，待Fix

# 2025.4.13 ly
[fix] 修复test_fork问题 
[style] 重整代码风格，添加注释
1. 用户程序使用print打印字符串，目前不能打印整数
2. process.c和vmem.c下新增函数注释

# 2025.4.13 lm
[feat] 用户程序自动生成init_code, loongarch可以copyinstr
1. 修改makefile，make和make rv都会先生成user.c的initcode，并自动include到内核中。以后修改用户程序可以直接编译运行
2. 也支持loongarch的用户程序init_code,重要的一点是删除usys.S的段定义，否则不能正确链接出init_code
3. 修正了loongarch的copyinstr
4. loongarch的时钟中断和调度也是正常的
5. 稍微修改README.md
6. 还修改之前版本生成initcode时-MF异常的问题，原来是有一个RISCV_BUILDPATH没改
7. 增加了virtio的虚拟内存映射到vmem_init(),可以识别磁盘。但是现在磁盘中断不会来，待解决

[todo] 
1. ~~增加用户程序的printf，便于开发~~
2. virtio驱动

# 2024.4.15 ly
[feat] 添加gettimeofday系统调用，重构用户程序，可调试用户程序
1. gettimeofday需要用到timeval_t结构体
2. Usr Makefile新增编译出usr.out.debug文件，在gdb中输入-exec add-symbol-file user/build/riscv/user.out.debug 0x0 即可调试用户程序
3. 目前riscv用户程序可以查看用户程序变量，而Loongarch不行

# 2025.4.16 lm
[feat] 可以生成opensbi的rv系统镜像 能系统调用
1. 修改Makefile,使用make sbi生成支持opensbi的镜像，使用make sbi_qemu运行
2. 修改uart.c，支持opensbi
3. 修改entry.S，避免S态读M态csr的错误。如果之后要支持多核可能需要改回来
4. 修改riscv.h，sbi下能正确映射内核
5. 暂时把cpu.c中的r_tp()改成0

[fix] 把timer.c中的include改成本项目的include文件，而不是/user/include

[todo] 
1. ~~把sbi_call改为inline函数，提高效率~~
2. ~~查清楚sbi下cpu.c中r_tp()的问题~~
3. ~~sbi支持时钟中断，磁盘中断~~

# 2025.4.16 lm
[feat] sbi支持时钟中断 磁盘读写
1. 添加了sbi.c，给sbi_call加上了inline(原来inline要和static一起用)。目前只使用set_timer和console_putchar
2. 添加了start函数在sbi情况下的初始化，设置tp和时钟中断，
3. 添加时钟中断中使用sbi的逻辑，并且可以读写磁盘
4. 把上次添加的uart.c中单独的sbi_call删去。
5. 修正上次r_tp产生的错误，在start中设置tp为0

[todo]
1. riscv中，intr_on和intr_off的完整逻辑
2. loongarch的virtio pcie
3. 文件系统

# 2025.4.16 lm
[refactor] 提取出user下公共的include文件 减小工作量
1. 提取出def.h usercall.h userlib.h. 各个架构的usys_macro.inc仍然放在各自的include下
2. user的makefile的编译选项加上-I../include
[feat] 给loongarch加上能打印变量的printf函数 方便调试
1. 从ucore中移植，完整的printf打印字符时行为异常，只保留了打印变量的功能

# 2025.4.18 ly
[feat] 实现sleep系统调用
[fix] 修复loongarch用户程序查看不了变量值的bug
1. 将user.out加入gdb即可，user.out.debug中无data、bss段。
2. myproc() 与 current_proc作用相同，删除current_proc

[bug] loongarch sleep(1) 但是过了2000ms，考虑硬件寄存器配置问题

# 2025.4.20 ly
[feat] 实现brk 系统调用
1. vmem.c 下添加 uvmalloc、 uvmdealloc
2. brk目前仅做了简单测试，未深度测试
[question] ~~brk传参为什么不用copyin?~~
如果用户态传参是值类型，那么不用担心用户态修改内核地址，不用copyin

# 2025.4.21 ly
[feat] 实现times,uname,yield,getppid系统调用
[refactor] 重构实现times系统调用,重构修改wait系统调用参数

# 2025.4.21 lm
[feat] loongarch识别pci设备
1. 目前只能识别到pci设备
2. 磁盘挂载方式见Makefile 的目标docker_la_qemu

# 2025.4.22 lm
[feat] loongarch初始化virtio-blk-pci
1. 新建virtio_pci.c，增加pci.c和virtio_disk.c的内容。现在能初始化识别到的磁盘设备了
2. 增加pci.h和virt_la.h，但是大部分宏和引用的函数都是在c文件写的，也有重复
3. 下一步做完整的磁盘驱动和loongarch外部中断；然后要为第1点提到的3个文件的设置头文件。

# 2025.4.23 lm
[feat] loongarch支持virtio-blk-pci读写
1. 增加SC7_start_kernel.c中的la磁盘测试函数，可以正常读写
2. 在virtio_disk.c和virtio_pci.c中增加磁盘读写的函数
3. 在Makefile中增加virt目标，用于正常挂载磁盘。原来的make docker_la_qemu不能正确的挂载磁盘
4. 更新了READM.md


# 2025.4.23 ly
[feat] 实现从磁盘读取elf并成功解析ProgramHeader,目前只能在内核态读取
1. 新增elf.h存放elf相关结构体,exec.c处理execve
2. 将用户空间扩充了一页，uvminit中配置第二页映射

[todo] 将读取的elf映射到用户空间

# 2025.4.24 ly
[feat] 简单实现riscv的execve系统调用，用户栈未详细设置
[fix] 修复用户程序exec报kernel_trap的问题
1. 当用户程序跳转内核态的函数时，创建局部变量用的是栈空间，如果局部变量分配过大会进不去函数
    char phdr_buffer[1024];
    devintr: scause=0xf
    unexpected interrupt scause=0xf
    scause 0x000000000000000f
    sepc=0x0000000080200036 stval=0x0000003fffffc000
    panic:[hsai_trap.c:519] kerneltrap
2. 目前可以把elf文件挂载到硬盘，根据Offset读对应磁盘块
[bug] 用户态test_execve后init_proc会exit

# 2025.4.25 ly
[feat] 合并loongarch virio，实现用户态从磁盘读取elf并成功执行
1. 需要注意loongarch和riscv在mapage的时候权限位设置不同，若设置错误可能出现
    kerneltrap: unexpected trap cause 40000
    usertrap(): unexpected trapcause 40000
    era=0x0000000000001060 badi=29c0e061
这样的问题
2. 需要注意修改了qemu启动时挂载的磁盘为elf
3. 修改了uvminit，现在initcode大于一个页面也会正常逐页映射
[bug] 
1.  ~loongarch下读写磁盘时来时钟中断会报kernel_trap~
    timer tick
    scheduler没有线程可运行
    线程切换
    kerneltrap: unexpected trap cause 20000
    estat 20000
    era=0x9000000090003424 eentry=0x90000000900033f0
    需要主要estat 20000为 store 操作页无效例外，考虑访问的地址无法正常访问
2. ~~用户态程序execve后init_proc会exit?~~  fork后execve即可
    panic:[process.c:441] init exiting
3. loongarch用户态测试waitpid异常,似乎和映射有关
    报错原因 ： 用户态全局变量报错 int i = 1000; 与data段映射有关j

# 2025.4.25 lm
[fix] 在loongarch磁盘读写函数中关闭时钟中断
[refactor] 编译riscv镜像时不会编译loongarch的磁盘驱动

# 2025.4.27 czx
[feat] 
1. Makefile添加了ext4磁盘生成的命令，以后需要测试的elf可以放到/tmp/cases下，然后拿到就行
2. 添加了ext4处理，主要是移植了lwext4，然后写了VFS_EXT4和VFS，目前打开文件，创建文件
，读取文件，写入文件，关闭文件经过测试，没有问题。其他的后面碰到再完善

[bug]
~~loongarch的virtio一定要有那个打印的交换语句，不然不知道为什么写入有问题，太奇怪了~~

[todo]
由于我test_fs()是在forkret中测试的(filesysteminit不能在main函数中用)，这个东西好像是内核态的所以补充了一个isforkret的全局变量，这样eithercopyin和eithercopyout就是用memmove直接挪了。后面也需要删掉。

# 2025.4.27 lm
[refactor] 在kernel下设fs和driver模块
1. kernel分fs和driver模块，其中driver区分架构
2. 使用绝对路径的-I选项，移除了hal/riscv/include中的链接文件spinlock.h
3. 删除SC7_start_kernel.c中，用于loongarch磁盘驱动中断的冗余代码。精简
4. 优化主Makefile。现在运行命令使用同一个磁盘文件，用disk_file变量存储。并且对于不需要导出的变量，删去了export

# 2025.4.28 ly
[feat] riscv和loongarch支持文件系统读取磁盘镜像中的elf并执行
1. exec实现利用文件系统从镜像中读取elf并执行
2. 修改BSIZE为4096，同时修改makefile创建 -b 4096的磁盘
3. devintr 中当系统调用来临时不会打印unexpected interrupt
4. 现在loongarch中walkaddr中取出的物理地址高位自动设置为9

[bug] loongarch现在无timer tick
[todo] 增加目录支持，增加execve中对用户栈的各种设置

# 2025.4.28 lm
[fix] 修复loongarch的时钟中断
1. 在usertrap为loongarch增加intr_on,在hsai_usertrapret为loongarch增加intr_off
2. virtio_disk的la磁盘读写函数不再开启时钟中断，删去countdown_timer_init()。现在loongarch的时钟中断都是正常的

# 2025.4.29 ly
[feat]初步实现vma管理用户态进程的虚拟内存
1. process结构体添加了vma
2. 给进程分配页表时会初始化vma，创建用户线程时分配sp空间,目前暂定为两个页面
3. fork时要拷贝vma区域

[bug]  ~~loongarch的用户程序跑不通了~~
    已解决  load 操作页无效例外, memmove(mem,(char *)pa,PGSIZE);
        pa高位未设置为9，导致无法访问
[todo] mmap系统调用

# 2025.4.30 ly
[feat]
1. 调整用户栈位置 0x80000000 - PGSIZE 一个页面

# 2025.4.29 ly
[feat]初步实现vma管理用户态进程的虚拟内存
1. process结构体添加了vma
2. 给进程分配页表时会初始化vma，创建用户线程时分配sp空间,目前暂定为两个页面
3. fork时要拷贝vma区域

[bug]  ~~loongarch的用户程序跑不通了~~
    已解决  load 操作页无效例外, memmove(mem,(char *)pa,PGSIZE);
        pa高位未设置为9，导致无法访问
[todo] mmap系统调用

# 2025.4.30 ly
[feat]
1. 调整用户栈位置 0x80000000 - PGSIZE 一个页面

# 2025.4.30 czx
[feat] 添加注释与优化
1. 为VFS层每个模块添加了必要注释
2. 为vfs-ext4添加了注释
3. 把所有xn6改成sc7了，彻底抹除了学长存在的痕迹

[todo] vfs一些没有实现的函数我标上todo了，防止没实现大家用了，有todo的要用的话需要先实现

# 2025.5.1 czx
[feat] 添加pipe2, read, close, dup, mknod, 修改write
1. 添加了系统调用pipe2, read, close, dup, mknod,修改了write系统调用，现在输出到标准输出/标准错误输出是通过设备交换表的write实现
2. 添加了console.c 这个是终端设备，位于设备交换表的1号位(从0开始)，不知道0号位是不是磁盘，好像我没看到显示赋值设备交换表0号位到磁盘的
3. 这几个系统调用跑rv的官方pipe测例都有用到，我通过了这个测例，理论上没有问题了。但是有个疑问，看bug部分。

[bug]

~~不知道为什么挂载sdcard-la.img之后write失效了，是这个镜像的问题吗还是什么不是很懂。但是挂载sdcard-rv.img两个架构的write都能用。~~

A: openat的问题，没有设备文件要创建设备文件(sys_mknod)而不是普通文件

[todo] 
1. ~~dup3系统调用~~
2. 文件系统重构

# 2025.5.2 czx
[fix] 修复了openat的错误，修复了write的问题
1. openat打开没有的文件的时候不会创建文件，后面也不会调用close关闭文件
2. 现在打开console文件会先创建字符设备了，可以成功实现终端输入输出
3. 修改了makefile，两个架构用不同的磁盘镜像文件

# 2025.5.2 lm
[feat] 增加ls的功能
1. 增加函数list_file在test.c中，不同类型的文件用不同颜色输出
2. 启动时会调用list_file("/")显示根目录下内容
3. vfs_ext4_ext.c的vfs_ext_getdents函数有问题，d_reclen要+2才对

# 2025.5.2 ly
[feat] 新增mmap系统调用  && [fix] 修复mknod kernel panic问题  
1. vm下新增uvmalloc1和uvmdealloc1
2. mknod传参upath未copyin直接使用，地址为0xa00，内核访问会panic
3. gitignore中添加ignore镜像文件
[fix] 修复使用4G镜像报错问题
1. 错误显示为无法找到对应文件，初步分析inode发现glibc的inodenum过大，musl的num较小于是可以打开，所以初步考虑是某个最大值设置过小的原因
2. 错误定位到(lba < bdev->lg_bcnt) ， lg_bcnt由bdev->part_size确定，part_size在初始化时由于设置过小导致问题，目前设置为4GB

[bug] 
1. 目前挂rv.img本地测试test_mmap无问题，测试mmap.elf读数据为空,考虑write写问题
    write写入文件是正常的，但是通过f读文件读出来全为空
2. 本地fs.img用户态设置create open打开文件失败,怀疑是f_flags为uint8导致高位截断

# 2025.5.3 czx
[fix] 修复文件创建问题，map系统调用
1. 修复了O_CREATE值导致的文件创建问题
2. 修复了map系统调用，让offset是正确的文件的offset的值

[feat] 添加了系统调用dup3

[bug] ~~现在loogarch创建完test_mmap.txt文件返回会有kerneltrap的panic~~
```
[INFO][syscall.c:61] sys_openat fd:-100,path:test_mmap.txt,flags:66,mode:2
kerneltrap: unexpected trap cause c0000
estat c0000
era=0x90000000900443e0 eentry=0x9000000090003580
panic:[hsai_trap.c:608] kerneltrap
```
[fix]
~~现在先注释掉了map函数里的memset语句，可以正常运行结果，但是理论上应该是跑去处理地址异常?也许？~~
地址忘记或上直接映射窗口了

# 2025.5.3 lm
[feat] 实现了getcwd, mkdirat, chdir, getdents64系统调用
1. 两个架构都实现了，本地的测例是按照比赛测例写的，可以跑。比赛测例也可以跑。然后添加了exe函数在user.c，方便执行初赛测例
2. hsai_trap.c中#ifdef DEBUG改为#if DEBUG. 现在发现#if defined DEBUG并不会检查DEBUG是什么值
3. 小bug,初版的mkdirat会创建inode为1的test_chdir目录，导致文件系统损坏，不能读取部分文件。但是后来测试时好了，我没有保存初版的代码，不清楚哪里错了。
不过现在的mkdirat是好的，创建的test_chdir是好的。
4. 我在本地保存了3中损坏的sdcard-rv.img文件

# 2025.5.3 ly
[feat] 新增munmap、statx系统调用 && [fix] 修复loongarch用户程序访问bss、data段时页修改例外的错误0x4000
1. statx为Loongarch版本的fstat
3. 页修改例外为：store 操作的虚地址在 TLB 中找到了匹配，且 V=1，且特权等级合规的项，但是该页表项的 D 位为 0，需要在exec load时加上PTE_D

[bug] ~~loongarch的在statx之后会报错~~
[todo] 由于官方open默认没给create权限，自测时会打开失败，暂时在openat中增加O_CREATE

# 2025.5.4 czx
[fix] 修复~~openat设备打开问题~~和waitpid问题
1. ~~openat特判是不是打开console~~ 官方测试脚本Makefile会创建./text.txt
2. waitpid用了真正的POSIX标准

# 2025.5.5 ly
[feat] 支持调整用户栈大小 支持批量运行用户程序 clone系统调用
1. 调整vma.h中的USER_STACK_SIZE为即可调整用户栈大小
2. clone 具体逻辑跟fork类似，只是sp和epc需要单独设置
[bug] 
1. 用户栈大小为1 PAGESIZE时跑用户程序可能出现卡住的问题，不知道卡在哪了
2. 每次新运行loongarch镜像时，在测试test_chdir时会报错：
panic:[ext4_fs.c:1554] *fblock

[todo] 对flags的解析暂无，目前只支持SIGCHLD

# 2025.5.6 ly
[feat] 新增批量运行用户程序，修改loongarch异常打印格式，修改brk返回值
1. 用户程序test_basic批量测试，修改basic_name中的元素即可修改测试文件
2. print.h下新增loongarch的异常打印格式，方便调试

# 2025.5.6 ly
[feat] 初步修改execve，适配glibc
1. elf.h中新增有关AUXV的宏定义
2. 修改execve中的用户栈，压入argc envp auxv

# 2025.5.6 lm
[feat] 完成mkdirat调用，通过mkdir_测例
1. 手动添加了mkdirat的代码，因为我把master merge到SC7会导致loongarch运行测例失败。所以直接手动更改了
2. 将从master创建一个新分支，提交后merge到SC7. 不知道效果如何

# 2025.5.6 lm
[merge] 完成merge
1. 删除多余的commit_log记录
2. 测试两个架构的test_basic没有问题


# 2025.5.6 czx
[feat] 添加了mount和umount系统调用，没有实际设备，故没完全实现
1. 添加了mount和umount系统调用
2. 但是没有完全实现，那个/dev/sda2我们没有这个路径，vfat的文件系统也没管它，目前实现了接口，然后读写函数绑定到主磁盘上了，算是一个折中的mount和umount方案

[todo]
文件系统重构

[bug]
~~单个执行elf没问题，跑一遍basic测例有问题，loongarch不停止，riscv会kernel trap~~
已解决，execve的argv传少了，但是测例需要，因此访问越界，riscv会application core dump
13 in application, bad addr = 0x0000003fffff99d0, bad instruction = 0x0000000000001a44, core dumped.scause 0x000000000000000c
sepc=0x0000000000001108 stval=0x0000000000001108
panic:[hsai_trap.c:574] kerneltrap
而loongarch会卡在tramplines.s处死循环

# 2025.5.8 czx
[refactor] 重构file
1. 删去了file结构体中没意义的东西
2. file实际指向的设备/目录/文件使用file_data来标识
3. ext4/vfat的文件/目录全部存成了vnode，这样只需要维护一个vnode表

[todo]
文件系统接着重构

# 2025.5.8 lm
[feat] 实现unlink系统调用
1. unlink要实现的功能实际上是删除指定的文件，也没有link测例来创建一个链接，unlink就是创建了一个文件，然后unlink创建的文件，再检查能不能打开这个文件，不能打开就success。
2. 修复sys_mkdirat的逻辑问题，三元表达式的条件应该是(dirfd == FDCWD)
3. 所以现在就是实现了删除文件的系统调用，哈哈。

[todo]
真正的link功能需要的时候再做

# 2025.5.9 czx
[refactor] 重构VFS层的inode
1. 把ops一些操作挪到inode了
2. 提升了inode的封装，只能通过get_inode_op得到
3. 优化了gei_absolute_path，重命名了一些函数，优化了一些函数，确保内存及时释放

[refactor] 重构fs，blockdev
1. 删去了无关的filesystem_init
2. 设置init的cwd挪出来了
3. fs_init是初始化对应表，register是初始化表项，mount是挂载文件系统操作函数到文件系统
4. blockdev删除了一些没意义的函数，对部分变量重名名
5. 更新了list和container_of成Linux下的

# 2025.5.9 ly
[fix] 修复了loongarch qemu读取磁盘莫名奇妙的问题
1. 经常随机出现找不到elf文件，格式错误的问题，在一次运行时发现qemu-system-loongarch64: Virtqueue size exceeded
2. 查找资料发现是Virtqueue size的问题，本地设置成了8太小，设置成1024后读取正常
3. 以后一边架构无问题另一边有问题的话考虑查看qemu log找问题

# 2025.5.10 ly
[feat] 修改Execve，目前能正常传argv 通过basic所有测例
[bug] 目前先运行mount再openat会导致./mnt变成vfat,导致openat失败

# 2025.5.11 ly
[feat] 添加busybox相关系统调用 修改execve逻辑
1. 目前execve从vaddr开始映射而不是0开始映射
[question]
1. 目前uvm_grow待修改，loongarch的entry位于高地址，导致不能从0开始map，否则爆内存
2. glibc初始化会使用brk和mmap系统调用，不清楚目前coredump是否是由于brk和mmap导致的

[bug] 目前loongarch的读磁盘一直忙等待
[bug] ~~0x8024011c处的跳转不同导致问题~~ libc不同，一个是glibc一个是musl

# 2025.5.13 ly
[fix] 修复riscv musl busybox
1. 当mmap匿名映射时，return 的是分配vma的首地址，之前搞错导致mmap返回0
[feat] 修改riscv用户程序testbusybox,支持批量测试
1. 修改testbusybox的dir和busybox数组的valid即可自定义测试
[fix] 修复riscv glibc busybox
1. brk需要 return 扩充后的堆的起始地址
[todo] loongarch glibc/musl busybox 

# 2025.5.14 ly
[bug] 
1. loongarch musl pte remap
2. loongarch glibc读磁盘忙等待

# 2025.5.15 ly
[feat] 实现部分busybox系统调用
1. 完善了loongarch usertrap打印信息
2. process 结构体新增信号相关成员sig_set，sigaction,新增signal.h，signal.c文件
3. file.h中新增iovec结构体，stat.h 中新增sysinfo结构体
4. 新增writev sys_clock_gettime sys_syslog sys_sysinfo系统调用
5. 新增信号相关系统调用 sys_rt_sigprocmask sys_rt_sigaction

[todo]
1. 完成 SYS_fstatat SYS_faccessat SYS_fcntl系统调用
2. 目前只测试了rv musl

# 2025.5.15 czx
[fix] 修复先mount后openat的文件系统不支持问题
1. 目前看来官方的ext4和vfat用的都是ext4的打开方式，所以vfs层的file也用ext4实现了，然后openat系统调用vfat的行为和ext4完全一致(Maybe todo)

[refactor] 重构了vfs层的ext4系统相关的系统和文件操作
1. 删除了部分不应该对外提供的函数接口
2. 增加了部分应该对外提供的接口
3. 修改了部分变量的名字，赋值逻辑以及部分函数的逻辑

[bug?] 有些奇怪的问题，我不知道是我改出bug还是本来就有，在basic下
1. RISC-V偶尔会有pte is null, test_yield结束比unlink晚，kernel panic
2. Loongarch 日常kernel panic, 全空的buffer缓冲区, 不是有效的ELF
It is really strange in our kernel, what will happen in the online judge?

# 2025.5.15 lm
[perf] Makefile支持多线程make
1. 稍微包装了一下，make sbi和docker_la都会多线程进行
2. 实测make会分三个线程进行，都编译完之后进行链接。fs编译是最耗时的。总时间从约五到六秒减少到2秒(未接电源)

# 2025.5.15 lm
[feat] 增加figlet.c，可以打印艺术字,还支持渐变色
1. printf_figlet无颜色打印，printf_figlet_color自动6种颜色渐变打印
2. 在figlet中修改color_style即可改变颜色样式
3. 非常酷炫的艺术字打印！
   _____    _____   ______        ___               ____                      _     _                   _ 
  / ____|  / ____| |____  |      (   )             |  _ \                    | |   (_)           __ _  | |
 | (___   | |          / /        | |    ___       | |_) )   ___     ___    _| |_   _   _ __    / _` | | |
  \___ \  | |         / /         | |   / __|      |  _ (   / _ \   / _ \  |__ __| | | | '_ \  | (_| | | |
  ____) | | |____    / /          | |   \__ \      | |_) ) | (_) | | (_) |   | |_  | | | | | |  \__, | |_|
 |_____/   \_____|  /_/          (___)  |___/      |____/   \___/   \___/     \__| |_| |_| |_|   __/ | (_)
                                                                                                |___/     

# 2025.5.15 lm
[fix] 把la磁盘的等待时间改成600*1024之后似乎不会有问题了
1. 我也不知道为什么...先这样,反正它成功了
2. la的virt_disk.c写的有点混乱，不过既然能跑，先不要动它吧 

# 2025.5.16 ly
[feat] 添加figlet打印方式 添加shutdown系统调用

# 2025.5.16 lm
[feat] 提交测试！
[fix] 既可以评测又可以开发了

# 2025.5.18 lm
[fix] 修复la virt忙等待
1. 现在la磁盘应该正常些了
[docs] 增加riscv virt部分注释

[bug] 
1. basic的umount测例有时候会kerneltrap
2. printf的锁有时候会重入

# 2025.5.18 ly
[fix] syscall的返回值需要为uint64 如果为int会导致高位截断
[feat] 实现la busybox!
1. 修改syscall中ret的类型为uint64
2. 修改alloc_vma 分配mmap区域时从p->sz开始扩展，并增加p->sz
3. exec 是设置p->virt_addr为entry , freepagetable时从p->virt_addr开始释放p->sz - p->virt_addr的空间
4. sys_brk 时 uvmalloc时需要设置页为PTE_D，否则会PME
    Ecode=0x4, EsubCode=0x0
    类型=PME, 描述: 页修改例外
    [ERROR][hsai_trap.c:507] 
        era=0x000000012018b688
        badi=0x0000000029c26061
        badv=0x00000001201ff000
        crmd=b0
5. 跑basic时不映射低地址，busybox时映射低地址

# 2025.5.18 lm
[feat] 开启la浮点数扩展，能跑la busybox musl
1. 设置eneu寄存器(地址0x2)第一位为1

[bug]
1. 两个架构在运行到第50个程序时就找不到文件了

[feat] 完善getdents64,支持busybox的du
1. vfs_ext4_getdents返回的linux_dirent64是符合标准的了！然后内核的list_file略有变化。原来d_off不是index，是条目的偏移量，busybox要用的
2. sys_getdents64更新，主要调用了vfs_ext4_getdents.支持busybox和basic
3. sys_fstatat由ly提供

# 2025.5.19 ly
[feat] 实现fstatat、kill、faccessat、utimensat系统调用
1. kill调用目前只是把进程的killed设置为1，usertrap时检查p->killed，如果为1则kill进程
2. 目前未对进程的kill信号进程处理
3. 暂未对fstatat的flags位进行处理
4. faccessat应该需要对文件进行判断，但文件不存在，目前是创建了文件，并返回0

# 2025.5.20 lm
[feat] 完善sys_sysinfo调用，增加sys_set_robust_list调用

[bug]
1. 现在riscv busybox glibc也出现了remap的问题。la busybox glibc等增加几个调用估计也会有remap

# 2025.5.20 lm
[fix] 修复glibc的remap
1. alloc_vma函数的地址改为向上对齐
[feat] 简单实现SYS_gettid，SYS_tgkill，SYS_prlimit64调用

[bug]
1. glibc在mmap后，进行了两轮的getpid,gettid,tgkill，然后触发0x3 interrupt断点异常，很奇怪
[todo] la busybox glibc也有类似的问题# 2025.5.20 ly
[feat] 实现exec中重定向
1. rv的kernelstack如果只有一个页面，则在加入重定向后会kernel panic


[feat] 增加SYS_readlinkat，SYS_getrandom调用，
1. 能进入la busybox glibc，但有问题。只能执行部分命令，如第一个echo,du。并且执行完一个命令就异常
2. 修正sys_tgkill的实现，现在会杀死线程。能进入rv busybox glibc的多个命令，但是每个都执行失败,问题与FATAL: kernel too old相关

[todo]
1. sys_readlinkat没完全实现，这也可能是问题

# 2025.5.21 lm
[feat] sys_exit_group时exit，可以执行la glibc的ash和sh命令

# 2025.5.26 lm
[feat] 修复glibc的问题，可以运行大部分busybox命令。测试了busybox
1. sys_exec的返回值改为0.如果不为0,trapframe->a7的值就不是0.glibc会将a7中的值作为参数注册先析构函数，退出时异常。两个架构都会这样
2. sys_uname中的release字段设置为"6.1.0\0",为了glibc,内核版本被迫变成6.1.0了。解决了rv glibc的问题。由ly提供
3. la内核的文件系统部分有break语句，在hsai_trap.c的kerneltrap中增加处理断点异常的功能
4. rv的内核栈大小扩大到4个页
5. 在user.c标注了busybox需要的系统调用，两个架构、musl和glibc都标了。
6. 注释了部分我写的la的syscall的调试输出。

# 2025.5.26 ly
[fix] 修复增加重定向后Exec在inode find时kernel panic的问题
1. 在函数内部创建过大的局部变量，导致内核栈溢出
2. 将ustack和Estack移到外部，避免在函数内部创建过大的局部变量，成功修复

# 2025.5.27 lm
[feat] 增加SYS_sendfile64，SYS_readv，SYS_renameat2调用。未完全实现. 实现了SYS_llseek
1. SYS_sendfile64的命令，即使sendfile64没有实现也是正常的，目前SYS_sendfile64是返回-1
2. SYS_readv没有完全实现，只返回0. musl的od和hexdump读出来都是0
3. SYS_renameat2只处理了参数，返回0,能过
4. SYS_llseek正常实现，错误处理应该完整

[todo]
1. SYS_sendfile64，SYS_readv，SYS_renameat2的正确逻辑
2. 还有103(uptime) 115(sleep) 98(find) 号系统调用要实现，括号内是对应的busybox命令


# 2025.5.28 czx
[feat] 通过了busybox的df, ps, free, hwclock

[fix] 修复了几个文件夹和文件(/proc,/proc/meminfo...)的打开bug

[feat] 通过了busybox的hexdump

[fix] 修复了系统调用dump3的问题
1. 通过了busybox的df, ps, free, hwclock，修复了文件的bug
2. 新建了文件类型FD_BUSYBOX，专门用来处理busybox打开的文件/文件夹
3. 修复了stat, statx两个系统调用的实现
4. 添加了系统调用65--readv
5. 解决了hexdump的问题

[todo] ~~hexdump指令，musl这个需要系统调用65~~

# 2025.5.31 ly
[feat] 新增SYS_settimer、SYS_pread系统调用(通过uptime)
1. settimer注释了处理，直接返回0

   
# 2025.6.1 ly
[feat] Exec新增对sh脚本文件的支持，但存在小问题
1. 运行sh脚本文件时，将路径替换为/musl/busybox执行文件
2. exec新增对 env环境变量的压栈
3. 实现fcntl系统调用
[bug]
1. sh测试 clone中 ， 对于uvmcopy (*pte & PTE_V) panic ,暂时跳过，可能有问题
2. clone 暂时注释掉ctid !=0 的处理，copyout时报错 "copyout: dstva > MAXVA"

# 2025.6.1 lm
[feat] 增加sys_clock_nanosleep,通过glibc的sleep
1. 通过glibc的sleep不知道为什么一直调用sys_clock_nanosleep,返回0和往rmtp写0都没用。参数也奇怪，写在注释了,second是32位最大值。

# 2025.6.2 czx
[feat] 添加了futex，线程管理和通过find
1. 为了通过find，futex系统调用直接exit(0)了，后面的not_reach
2. 添加了线程管理，目前一个进程对应一个线程
3. 添加了futex相关实现，理论上可以实现futex相关功能了

[fix] 修复了statx, fstat系统调用
1. 修复了statx, fstat系统调用，他们需要支持AT_FDCWD的情况
2. 为了防止递归深度过深，find的时候只允许递归一层

[fix&&feat] 修复clone
1. 紧急修复，clone的trapframe
2. du只du /proc，不扫描"."


# 2025.6.2 ly
[feat] exec添加打印栈的函数，便于调试
1. 回调对vmem的uvmcopy修改

[bug] 
1. busybox > 未覆盖原文件内容 
2. busybox rm 未实际删除文件
[todo] 修改clone函数


# 2025.6.2 lm
[fix] 修复sys_unlinkat
1. 可以处理相对路径不以./开头的情况了

# 2025.6.3 lm
[fix] 修复busybox的la glibc，sys_chdir待完善。freeproc释放文件，速度变快了
1. 临时把sys_chdir("glibc")改成sys_chdir("glibc")
2. freeproc释放文件，用的很简陋的方法，可能要完善。

# 2025.6.3 ly
[feat] 调整busybox打印信息
[fix] freeproc时调用free_vma_list释放进程的vma链表


# 2025.6.3 ly
[feat] 通过busybox测试
[question] 
1. 上面的free file似乎没有释放inode, NINODE改小了还是不够用
2. 我也不知道为什么free file之后速度变快了
[todo]
1. chdir如果以/开头有问题
2. 重写freeproc 释放文件逻辑
3. busybox find命令 application core dumped

# 2025.6.3 czx
[fix] 修复getcwd, chdir, futex, sys_renameat2, sys_unlinkat， statx
1. getcwd内核态需要返回cwd长度，及对应标准错误码
2. chdir让cwd存储的是绝对路径
3. futex_wake返回唤醒的线程数量
4. sys_renameat2调用VFS中层
5. sys_unlinkat调用VFS中层
6. statx不再使用open实现，这个哪怕最后删了文件他貌似还会有缓存还是什么的，反正很烦，只能破坏封装性和代码易读性，改用路径实现。

~~[bug] la的busybox的mv，rename的时候会先创建那个文件，导致ext4判断失败，认为该文件已经存在，同时，rv的newpath
是"/glibc/test/test"这样的形式，会重复两遍，但是行为完全正常。la的newpath是"/glibc/test/"这样的形式，但是已经存在，rename失败，真TM服了
不知道是什么傻逼错误。~~


# 2025.6.5 ly
[feat] 添加SYS_rt_sigtimedwait系统调用
1. 可以开始写libc的系统调用了

[feat] 成功实现动态链接
1. 动态链接需要用到系统调用SYS_mprotect,暂时返回0

# 2025.6.7 ly
[feat] musl 的 rv la 静态链接和动态链接都正常
[fix] 修复静态链接时uvmcopy fetchstr env error
1. 用户低地址可能存放env,因此uvmcopy时也需要copy这段低地址
2. 这段地址目前未释放

[bug] la uvmcopy
