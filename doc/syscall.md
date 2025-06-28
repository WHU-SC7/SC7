# SC7系统调用

## 概述

SC7系统调用模块为用户程序提供访问内核服务的标准接口，支持进程管理、文件操作、内存管理、时钟、信号、网络等多类系统调用，兼容POSIX标准。

---

## 1. 系统调用号与接口

系统调用号定义于`include/kernel/syscall_ids.h`，如：

```c
#define SYS_write   64
#define SYS_getpid  172
#define SYS_fork    300
#define SYS_clone   220
#define SYS_exit    93
#define SYS_wait    260
#define SYS_execve  221
#define SYS_openat  56
#define SYS_read    63
#define SYS_mmap    222
#define SYS_munmap  215
// ... 详见源码
```

- 每个系统调用有唯一编号，用户态通过软中断/陷入指令（如ecall）进入内核。
- `get_syscall_name(int num)`可根据编号返回系统调用名称。

---

## 2. 系统调用分发机制

系统调用统一由`syscall(struct trapframe *trapframe)`处理，流程如下：

1. 用户程序通过陷入指令触发trap，CPU切换到内核态。
2. 内核trap处理函数识别为系统调用，调用`syscall()`。
3. `syscall()`根据trapframe中的系统调用号和参数，分发到对应的内核实现函数。
4. 系统调用返回值写回trapframe，返回用户态。

### 关键实现片段（`kernel/syscall.c`）
```c
void syscall(struct trapframe *trapframe) {
    int num = trapframe->a7; // 系统调用号
    uint64 a[6] = {trapframe->a0, trapframe->a1, ...}; // 参数
    switch(num) {
        case SYS_write: ret = sys_write(a[0], a[1], a[2]); break;
        case SYS_getpid: ret = sys_getpid(); break;
        // ... 其他系统调用
    }
    trapframe->a0 = ret; // 返回值
}
```
- 支持最多6个参数，参数通过寄存器传递。
- 分发表可自动生成或手动维护。

---

## 3. 参数传递与返回

- 用户态参数通过trapframe寄存器（a0~a5）传递到内核。
- 指针参数需通过`copyin`/`copyinstr`等函数从用户空间拷贝到内核空间，防止非法访问。
- 返回值写回trapframe->a0，用户程序可直接获取。

### 关键实现片段
```c
int sys_write(int fd, uint64 va, int len) {
    struct file *f = myproc()->ofile[fd];
    if (!f) return -ENOENT;
    return get_file_ops()->write(f, va, len);
}

int sys_openat(int fd, const char *upath, int flags, uint16 mode) {
    char path[MAXPATH];
    copyinstr(myproc()->pagetable, path, (uint64)upath, MAXPATH);
    // ... 路径解析、文件分配、VFS分发
}
```

---

## 4. 典型系统调用实现

### 进程管理
- `sys_fork()`：创建子进程，复制父进程资源。
- `sys_exit()`：进程退出，释放资源。
- `sys_wait()`：等待子进程退出。
- `sys_execve()`：加载新程序映像。

### 文件操作
- `sys_openat()`：打开文件，支持多文件系统。
- `sys_read()`/`sys_write()`：读写文件。
- `sys_close()`：关闭文件描述符。
- `sys_mkdirat()`/`sys_unlinkat()`：创建/删除目录或文件。

### 内存管理
- `sys_mmap()`/`sys_munmap()`：用户空间内存映射与解除。
- `sys_brk()`：调整进程数据段末尾。

### 时钟与定时
- `sys_gettimeofday()`/`sys_clock_gettime()`：获取系统时间。
- `sys_sleep()`：进程休眠。

### 信号与同步
- `sys_kill()`/`sys_rt_sigaction()`/`sys_futex()`：信号发送、处理与用户态同步。

### 网络与设备
- `sys_socket()`/`sys_bind()`/`sys_listen()`/`sys_accept()`/`sys_sendto()`/`sys_recvfrom()`：套接字操作。

---

## 5. 机制与特性总结

- 支持多参数、指针安全校验、错误码返回。
- 兼容POSIX标准，支持glibc、busybox等主流用户程序。
- 系统调用表可扩展，便于新增内核服务。
- 通过trapframe和寄存器实现高效参数传递与返回。

---

SC7系统调用模块为用户程序与内核之间提供了安全、高效、标准化的接口，是操作系统内核与应用交互的桥梁。 