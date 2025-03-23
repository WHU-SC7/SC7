make 生成loongarch镜像
./run.sh 运行loongarch系统
make docker_qemu 在初赛docker中运行

make riscv 生成riscv镜像
make riscv_qemu 运行riscv系统


#riscv工具链
riscv64-unknown-elf-gcc --version
riscv64-unknown-elf-gcc (SiFive GCC 10.1.0-2020.08.2) 10.1.0
Copyright (C) 2020 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

riscv64-unknown-elf-ld --version
GNU ld (SiFive Binutils 2.35.0-2020.08.2) 2.35
Copyright (C) 2020 Free Software Foundation, Inc.


可以参照这个，安装riscv64-unknown-elf工具链和qemu。或者初赛的镜像环境也可以
https://learningos.cn/uCore-Tutorial-Guide-2024S/chapter0/1setup-devel-env.html
