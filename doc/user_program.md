# SC7用户程序

## 概述

SC7支持用户空间程序运行，兼容RISC-V和LoongArch架构，用户程序可通过系统调用访问内核服务，支持C标准库、shell、测试程序、第三方工具等多种用户态应用。

---

## 1. 用户程序入口与初始化

以RISC-V为例，用户程序入口在`user/riscv/user.c`：

```c
int init_main(void) __attribute__((section(".text.user.init")));

int init_main()
{
    if (openat(AT_FDCWD, "/dev/tty", O_RDWR) < 0)
    {
        sys_mknod("/dev/tty", CONSOLE, 0);
        openat(AT_FDCWD, "/dev/tty", O_RDWR);
    }
    sys_dup(0); // stdout
    sys_dup(0); // stderr
    run_all();
    shutdown();
    while (1);
    return 0;
}
```
- 用户程序启动时自动初始化标准输入输出、运行测试用例或shell。
- 支持多种测试集（如libc、busybox、iozone、lmbench等）。

---

## 2. 用户态库与API

用户程序可调用用户态库函数（见`user/include/userlib.h`）：

```c
int open(const char *path, int flags);
int openat(int dirfd, const char *path, int flags);
int mkdir(const char *path, uint16 mode);
int wait(int *code);
int sleep(unsigned long long time);
int get_time(void);
#define WEXITSTATUS(s) (((s) & 0xff00) >> 8)
```
- 用户库对系统调用进行简单封装，便于直接使用。
- 提供常用的文件、进程、时间等操作。

---

## 3. 系统调用封装

用户程序通过`user/include/usercall.h`中的API发起系统调用：

```c
extern int write(int fd, const void *buf, int len);
extern int getpid(void);
extern int fork(void);
extern int clone(...);
extern int waitpid(int pid, int *code, int options);
extern int exit(int exit_status);
extern int sys_execve(const char *name, char *const argv[], char *const argp[]);
extern int sys_openat(int fd, const char *upath, int flags, uint16 mode);
extern int sys_mmap(void *start, int len, int prot, int flags, int fd, int off);
// ... 详见源码
```
- 所有系统调用均通过软中断/陷入指令进入内核。
- 支持多参数、指针安全校验。

---

## 4. 用户程序支持特性

- 支持静态/动态链接C库（如musl、glibc），可运行标准C程序。
- 支持shell、busybox、iozone、lmbench、lua等第三方工具和测试集。
- 用户程序可通过fork/exec/clone等机制创建新进程或线程。
- 支持mmap、文件IO、信号、管道、socket等高级功能。

---

## 5. 运行与调试机制

- 用户程序通过ELF格式加载，内核负责分配虚拟内存、建立页表、初始化栈和参数。
- 用户程序异常（如非法访问、缺页、系统调用）由内核trap统一处理。
- 支持GDB远程调试、QEMU仿真等多种开发环境。

---

SC7用户程序模块为操作系统提供了丰富的用户空间生态，支持多种开发、测试和应用场景，便于教学、实验和实际部署。 