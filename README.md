支持riscv和loongarch的虚拟内存和系统调用，还有时钟中断

已经重整了结构。之后要做适配初赛rv使用opensbi的版本

loongarch要使用virtio-blk-pcie设备，似乎挺麻烦的

分支是SC7。
解释一下为什么是SC7: SC7 is SmartCore7 

# 如何运行
make 生成loongarch镜像 (ls2k)

./run.sh 运行loongarch系统。最下面是一个参考

make docker_la 编译初赛docker的virt机器的镜像

make docker_la_qemu 在初赛docker中运行

make rv 生成riscv镜像

make rv_qemu 运行riscv系统

make user 生成用户程序的init_code，并输出汇编代码到文件

这个命令本机和初赛镜像都可以 //注意！今天(3月30日)testsuit更新了全部测例，现在可以在更新前的镜像运行。更新后工具版本似乎没有变，但是还没有测试过

# riscv工具链
riscv64-unknown-elf-gcc --version
riscv64-unknown-elf-gcc (SiFive GCC 10.1.0-2020.08.2) 10.1.0
Copyright (C) 2020 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

riscv64-unknown-elf-ld --version
GNU ld (SiFive Binutils 2.35.0-2020.08.2) 2.35
Copyright (C) 2020 Free Software Foundation, Inc.

qemu-system-riscv64 --version ！！要使用高版本的qemu,推荐9.2.1
QEMU emulator version 9.2.1
Copyright (c) 2003-2024 Fabrice Bellard and the QEMU Project developers

可以参照这个，安装riscv64-unknown-elf工具链和qemu。或者初赛的镜像环境也可以

https://learningos.cn/uCore-Tutorial-Guide-2024S/chapter0/1setup-devel-env.html


# loongarch工具链
loongarch64-linux-gnu-gcc --version
loongarch64-linux-gnu-gcc (GCC) 13.2.0
Copyright © 2023 Free Software Foundation, Inc.
本程序是自由软件；请参看源代码的版权声明。本软件没有任何担保；
包括没有适销性和某一专用目的下的适用性担保。

qemu-system-loongarch64 --version
QEMU emulator version 3.1.0 (-lvz1-dirty)
Copyright (c) 2003-2018 Fabrice Bellard and the QEMU Project developers

loongarch64-linux-gnu-gdb --version
GNU gdb (GDB) 12.0.50.20220221-git
Copyright (C) 2022 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.

这两个网站下载：

https://github.com/LoongsonLab/oscomp-toolchains-for-oskernel/releases

https://github.com/LoongsonLab/2k1000-materials/releases

可能要装一些动态链接库


# run.sh示例
#通常情况下该文件应当放在项目的根目录下

RUNENV_PREFIX=/home/lu/Downloads/qemu/
KERNEL_PREFIX=`pwd`

cd $RUNENV_PREFIX

./bin/qemu-system-loongarch64 \
	-M ls2k \
	-serial stdio \
	-k ./share/qemu/keymaps/en-us \
	-kernel ${KERNEL_PREFIX}/build/loongarch/kernel-la \
	-serial vc \
	-m 1G \
	-net nic \
	-net user,net=10.0.2.0/24,tftp=/srv/tftp \
	-vnc :0 \
	-hda ${KERNEL_PREFIX}/tmp/disk \
	-hdb ~/Desktop/sdcard-loongarch.img 