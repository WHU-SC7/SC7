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