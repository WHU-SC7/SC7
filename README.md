支持riscv和loongarch的虚拟内存和系统调用，还有时钟中断,以及磁盘读写

下一个目标是，文件系统！

解释一下为什么是SC7: SC7 is SmartCore7 

# 如何运行
make sbi 生成riscv架构使用opensbi的镜像

make sbi_qemu 启动riscv镜像

make docker_la 生成loongarch -M virt的镜像

make virt 启动生成loongarch镜像

# riscv工具链
riscv64-linux-gnu-gcc --version
riscv64-linux-gnu-gcc (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0
Copyright (C) 2023 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

riscv64-linux-gnu-ld --version
GNU ld (GNU Binutils for Ubuntu) 2.42
Copyright (C) 2024 Free Software Foundation, Inc.
这个程序是自由软件；您可以遵循GNU 通用公共授权版本 3 或
(您自行选择的) 稍后版本以再次散布它。
这个程序完全没有任何担保。

qemu-system-riscv64 --version ！！要使用高版本的qemu,推荐9.2.1
QEMU emulator version 9.2.1
Copyright (c) 2003-2024 Fabrice Bellard and the QEMU Project developers

riscv64-unknown-elf-gdb --version
GNU gdb (GDB) 13.2
Copyright (C) 2023 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.

# loongarch工具链
loongarch64-linux-gnu-gcc --version
loongarch64-linux-gnu-gcc (GCC) 13.2.0
Copyright © 2023 Free Software Foundation, Inc.
本程序是自由软件；请参看源代码的版权声明。本软件没有任何担保；
包括没有适销性和某一专用目的下的适用性担保。

qemu-system-loongarch64 --version
QEMU emulator version 9.2.1
Copyright (c) 2003-2024 Fabrice Bellard and the QEMU Project developers


loongarch64-linux-gnu-gdb --version
GNU gdb (GDB) 12.0.50.20220221-git
Copyright (C) 2022 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.

qemu-system-loongarch使用初赛镜像的版本，不使用ls2k的qemu
这两个网站下载gcc和gdb：

https://github.com/LoongsonLab/oscomp-toolchains-for-oskernel/releases

https://github.com/LoongsonLab/2k1000-materials/releases

可能要装一些动态链接库

# 如何运行(old version)
这些选项可能不再支持！
make 生成loongarch镜像 (ls2k)

./run.sh 运行loongarch系统。最下面是一个参考

make docker_la 编译初赛docker的virt机器的镜像

make docker_la_qemu 在初赛docker中运行

make rv 生成riscv镜像

make rv_qemu 运行riscv系统

make user 生成用户程序的init_code，并输出汇编代码到文件

这个命令本机和初赛镜像都可以 //注意！今天(3月30日)testsuit更新了全部测例，现在可以在更新前的镜像运行。更新后工具版本似乎没有变，但是还没有测试过