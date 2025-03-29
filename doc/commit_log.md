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