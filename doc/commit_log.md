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
1. 耗尽内存测试在循环大于1时卡死
    没有判断多次释放，这样多次释放还是会加入表头
2. 只是释放内存时清空页不够，还得分配时清空页，初始化时全清空也不行
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
1. 在配置内核页表,进入时，riscv.h跳转异常  -> vscode gdb调试问题 
    官方gdb 8.2与Qemu版本不匹配，导致出现问题 
2. vmem.c中必须要用hsai/riscv.h  
    也是gdb版本问题
3. Risv页面映射完之后printf处卡住
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
1. process.c 中的 sleep函数与user文件夹下loongarch的usys.S中出现重定义，改名为'sleep_on_chan'。
2. uart.c 下又实现了自旋锁和相关函数，可能需要弄出来。
[todo] 注释完善

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
[todo] 之后把loongarch的initcode也做出来，然后重整一下

# 2025.4.12 lm
[feat] 实现时间片轮转调度
1. riscv可以时间片调度，添加usertrap在时钟中断后返回的逻辑
2. process.c现在都使用cpu进行调度
3. 验证了生成的initcode的正确性
4. 验证了可以正确的传参。
5. 栈的设置，现在用户程序的数据代码和栈共用一个页，目前的大小够用了。

[todo] 
1. 开关中断的逻辑没有完全实现
2. yield的锁没有加，锁问题没有解决
3. loongarch的initcode待添加

# 2025.4.13 ly
[feat] 新增系统调用 getpid、fork、wait、exit & 解决锁问题

1. include/kernel下新增syscall_ids.h，定义了系统调用的编号;def.h 用于常用宏
2. 修改allocproc锁逻辑，结束后不立刻释放锁，在fork|userinit结束后释放
3. 修改进程被fork后首次进入forkret,进入后释放锁
4. proc结构体新增exit_state和killed
5. vmem下新增uvmcopy、uvmfree,用于fork时复制页表
[bug] 目前test_fork子进程未能成功退出，父进程一直等待，待Fix

# 2025.4.13 ly
[fix] 修复test_fork问题 
[style] 重整代码风格，添加注释
1. 用户程序使用print打印字符串，目前不能打印整数
2. process.c和vmem.c下新增函数注释

