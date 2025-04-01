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
2. vmem.c中必须要用hsai/riscv.h