# SC7
初赛设计文档请查看./doc/SC7初赛设计文档.pdf
汇报ppt和视频在网盘，链接为: https://pan.baidu.com/s/1Sv-GNPEq07DWsxFS-EtMKw?pwd=w3gy

## SC7简介
SC7（SmartCore7）是采用 C 语言开发的，基于 MIT XV6 操作系统开发的教学用操作系统，支持 RISC-V和 LoongArch 双架构。系统已通过全国大学生计算机系统能力大赛初赛的 Basic、Busybox、Libctest 和 Libcbench 测例。

## 如何运行
make all        一次生成两种架构的镜像

#### riscv
make sbi        生成riscv架构使用opensbi的镜像
make sbi_qemu   启动riscv镜像,调试模式
make run_sbi    直接启动riscv镜像

#### loongarch
make docker_la  生成loongarch -M virt的镜像
make virt       启动loongarch镜像,调试模式
make run        直接启动loongarch镜像

## 工具链
可以使用初赛镜像，在镜像中make all即可编译。
或者按下面信息在本地安装gcc和qemu。

#### riscv工具链
riscv64-linux-gnu-gcc --version
riscv64-linux-gnu-gcc (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0
Copyright (C) 2023 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

qemu-system-riscv64 --version
QEMU emulator version 9.2.1
Copyright (c) 2003-2024 Fabrice Bellard and the QEMU Project developers

riscv64-unknown-elf-gdb --version
GNU gdb (GDB) 13.2
Copyright (C) 2023 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.

#### loongarch工具链
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
