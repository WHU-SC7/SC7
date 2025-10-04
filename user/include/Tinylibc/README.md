# 1.开发须知
目前只需关注user/include/Tinylibc下的文件和user/riscv/user.c文件

计划未来的库函数和系统调用包装都在core.h开发，内容增多后才分模块。

# 2.Tlibc介绍
这个条目可能会经常更新
### 项目介绍
Tlibc是RISC-V架构的libc用户库，目标是提供类似Glibc,Musl的库函数
Tlibc是一个以学习为目的的项目，目标是精简地、尽可能多的实现libc的功能
Tlibc目前在SC7内核基础上开发，为了运行，至少需要:
1. 编译器riscv64-linux-gnu-，在ubuntu使用sudo apt install gcc-riscv64-linux-gnu即可，版本无特别要求
2. qemu,使用2025年内核设计赛提供的qemu 9.2.1
3. 一个磁盘镜像放在整个项目(os2025文件夹)的上级目录，使用空磁盘镜像文件也可以
4. xxd命令行工具，linux的系统理论上都有，除了一些docker中可能没有


### 平台支持
现在只支持一种方式: 和SC7项目的用户程序一同编译，然后在qemu中启动内核，运行用户程序并进入Tlibc的测试函数
未来将支持:
1. 在VisionFive2开发版上运行Tlibc程序（理论上很简单，SC7就支持在板上运行
2. 在x86-64架构运行（画个饼，需要额外工作量

### 运行方式
在这个tlibc开发分支，SC7的用户程序初始化后会进入Tlibc的测试函数。
这是最方便的！更改Tlibc文件后，只需要make sbi和make run_sbi,就可以看到我们编写的库函数的运行结果

# 3.你可能关心的

## Tlibc项目结构
### c文件
c文件在主目录下，现在只有两个c文件: core.c和test.c

### 文件夹结构
目前文件夹中都是.h头文件

arch下是架构相关的头文件，目前只有riscv架构的

internal下是Tlibc内部使用的头文件

external是提供给外部的头文件，Tlibc的C文件实现了其中的函数

### 特殊文件
tlibc_commit_log.md汇总记录了项目的详细提交记录

项目计划.md是一阶段计划，已初步完成

Makefile是为了把Tlibc的c文件和SC7的用户程序编译到一起。

## Tlibc开发计划
下一阶段会编写shell和更多用户程序
