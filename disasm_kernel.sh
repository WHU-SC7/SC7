#!/bin/bash

# LoongArch 目标文件路径
LOONGARCH_KERNEL="./build/loongarch/kernel-la"
# RISC-V 目标文件路径
RISCV_KERNEL="./build/riscv/kernel-rv"

# 检查 LoongArch 的 kernel.elf 是否存在
if [ -f "$LOONGARCH_KERNEL" ]; then
    echo "检测到 LoongArch 架构内核，开始反汇编..."
    loongarch64-linux-gnu-objdump -D -C "$LOONGARCH_KERNEL" > ./build/kernel.dis
    loongarch64-linux-gnu-objdump -S -C "$LOONGARCH_KERNEL" > ./build/kernel.asm
    echo "LoongArch 反汇编成功！输出文件：./build/kernel.dis 和 ./build/kernel.asm"
# 如果不存在，检查 RISC-V 的 kernel.elf
elif [ -f "$RISCV_KERNEL" ]; then
    echo "未找到 LoongArch 内核，检测到 RISC-V 架构内核，开始反汇编..."
    riscv64-linux-gnu-objdump -D -C "$RISCV_KERNEL" > ./build/kernel.dis
    riscv64-linux-gnu-objdump -S -C "$RISCV_KERNEL" > ./build/kernel.asm
    echo "RISC-V 反汇编成功！输出文件：./build/kernel.dis 和 ./build/kernel.asm"
# 如果都不存在，报错
else
    echo "错误：未找到任何可用的内核文件！"
    echo "请检查以下路径是否存在："
    echo "1. LoongArch 内核: $LOONGARCH_KERNEL"
    echo "2. RISC-V 内核: $RISCV_KERNEL"
    exit 1
fi