
# initcode与init进程
本节解释两个核心问题：initcode 入口点的确定方法，以及用户程序发起系统调用的机制。

## 入口点确定机制
以 RISC-V 架构为例（LoongArch 类似），为了确保系统在无文件系统支持下也能运行初始程序并进行系统调用测试，SC7通过链接脚本强制将initcode的入口函数init_main定位在用户地址空间起始位置（0x0）。这通过在user.c中使用__attribute__((section(".text.user.init")))将init_main函数放入一个专用的代码段.text.user.init来实现。在链接脚本user_initcode.ld中，通过ENTRY(init_main)明确指定入口函数，并通过. = 0x00000000;强制设定起始地址，然后优先放置.text.user.init段。最终编译处理ELF文件生成二进制initcode，确保其在加载到内存后能够从0地址开始执行。

##### 文件user/riscv/user_initcode.ld
    ENTRY(init_main)  /* 明确入口函数 */
    SECTIONS {
    . = 0x00000000;  /* 强制起始地址 */
    .text : {
    (.text.user.init)    /* init_main 优先放置 */
    (.text.syscall_function) /* 系统调用函数 */
    (.text .text.)      /* 其他用户函数 */
    }
    ... /* 数据段 */
    }

// user/riscv/user.c
int init_main(void) attribute((section(".text.user.init")));
int init_main() {
}

## 系统调用发起机制
用户程序发起系统调用采用了一种基于宏的自动化生成机制。核心在于一个汇编宏GEN\_SYSCALL，它接收系统调用名称和调用号作为参数。此宏的作用是生成一个汇编函数入口：将系统调用号加载到a7寄存器（RISC-V ABI规定系统调用号通常通过a7传递），然后执行ecall指令触发系统调用。

##### 文件user/riscv/usys_macro.inc
    .macro GEN_SYSCALL name num
    .global \name
    \name:
    li a7, \num  /* 加载调用号 /
    ecall        / 触发系统调用 */
    ret
    .endm

在user/riscv/usys.S文件中，通过#include "usys_macro.inc"引入此宏，并使用如GEN_SYSCALL fork 300、GEN_SYSCALL sys_openat 56这样的语句，批量自动生成各种系统调用的汇编入口函数。这种设计源自xv6，它使得系统调用的C接口可以处理参数传递，而汇编层统一执行ecall指令，极大地简化了新增系统调用的工作：只需在usys.S中扩展宏声明即可。

## init线程
详情见SC7_start_kernel.c的init_process.从虚拟地址0开始加载initcode数组，然后设置好sp，把epc设置成0，返回用户态即可正常执行用户程序
