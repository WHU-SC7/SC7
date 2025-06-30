# SC7 🚀

## 🌟 文档中心 🌟

*   **初赛代码:** 🚀 请查看 `preliminary_contest` 分支
*   **初赛设计文档:** 📄 [SC7初赛设计文档](./doc/SC7初赛设计文档.pdf)
*   **汇报资料:** 🎬 [PPT和视频](https://pan.baidu.com/s/1Sv-GNPEq07DWsxFS-EtMKw?pwd=w3gy) 提取码: `w3gy`

## 💡 SC7 简介 💡

**SC7 (martCore7)** 是一款基于 MIT XV6 操作系统的教学用操作系统，使用 C 语言开发。🌟 它同时支持 **RISC-V** 和 **LoongArch** 双架构！

我们的系统已成功通过全国大学生计算机系统能力大赛初赛的以下测例：
*   Basic ✅
*   Busybox ✅
*   Libctest ✅
*   Libcbench ✅
*   lua ✅

## 🛠️ 如何运行 🛠️

### 🚀 通用构建 🚀

```bash
make all      # 一次性生成两种架构的镜像
```

### 🎯 RISC-V 架构 🎯

*   `make sbi`        生成使用 OpenSBI 的 RISC-V 架构镜像
*   `make sbi_qemu`   在调试模式下启动 RISC-V 镜像
*   `make run_sbi`    直接启动 RISC-V 镜像

### 🐉 LoongArch 架构 🐉

*   `make docker_la`  生成 LoongArch `-M virt` 的镜像
*   `make virt`       在调试模式下启动 LoongArch 镜像
*   `make run`        直接启动 LoongArch 镜像

## 🔧 工具链指南 🔧

您可以使用初赛提供的镜像，在镜像内部执行 `make all` 来进行编译。或者，您也可以按照以下信息在本地安装所需的 GCC 和 QEMU。

### 🌐 RISC-V 工具链 🌐

```bash
# GCC 版本
riscv64-linux-gnu-gcc --version
# GCC (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0

# QEMU 版本
qemu-system-riscv64 --version
# QEMU emulator version 9.2.1

# GDB 版本
riscv64-unknown-elf-gdb --version
# GNU gdb (GDB) 13.2
```

### 🐲 LoongArch 工具链 🐲

```bash
# GCC 版本
loongarch64-linux-gnu-gcc --version
# GCC (GCC) 13.2.0

# QEMU 版本
qemu-system-loongarch64 --version
# QEMU emulator version 9.2.1
# 注意：请使用初赛镜像中的 QEMU 版本，不要使用 ls2k 的 QEMU。

# GDB 版本
loongarch64-linux-gnu-gdb --version
# GNU gdb (GDB) 12.0.50.20220221-git
```

您可以在以下两个网站下载 GCC 和 GDB：
*   [LoongsonLab/oscomp-toolchains-for-oskernel](https://github.com/LoongsonLab/oscomp-toolchains-for-oskernel/releases)
*   [LoongsonLab/2k1000-materials](https://github.com/LoongsonLab/2k1000-materials/releases)

**提示:** 安装过程中可能需要安装一些动态链接库。