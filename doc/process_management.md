# SC7进程管理

## 概述

SC7进程管理模块实现了进程和线程的创建、调度、销毁、进程间通信等核心功能，支持多进程并发和多线程编程。

## 进程管理架构

```
┌──────────────────────────────────────────────┐
│                用户空间                      │
│   ┌─────┐  ┌─────┐  ┌─────┐                 │
│   │进程A│  │进程B│  │进程C│                 │
│   └─────┘  └─────┘  └─────┘                 │
├──────────────────────────────────────────────┤
│              系统调用接口                    │
│   fork/exec/exit/wait/sched/yield            │
├──────────────────────────────────────────────┤
│              进程调度与管理                  │
│   ┌────────────┐  ┌────────────┐             │
│   │进程控制块PCB│  │线程控制块TCB│             │
│   └────────────┘  └────────────┘             │
│   就绪队列/等待队列/调度算法                 │
└──────────────────────────────────────────────┘
```

## 核心数据结构

### 1. 进程控制块（PCB）

进程的所有状态和资源由`proc_t`结构体统一管理，定义于`include/kernel/process.h`：

```c
typedef struct proc {
    spinlock_t lock;         // 进程锁，保证并发安全
    void *chan;              // 睡眠通道
    struct proc *parent;     // 父进程指针
    thread_t *main_thread;   // 主线程
    struct list thread_queue;// 线程链表
    enum procstate state;    // 进程状态（UNUSED, RUNNABLE, RUNNING等）
    int exit_state;          // 退出状态
    int killed;              // 被杀标志
    int pid;                 // 进程ID
    int uid, gid;            // 用户/组ID
    uint64 virt_addr;        // 虚拟地址
    uint64 sz;               // 进程内存大小
    uint64 kstack;           // 内核栈虚拟地址
    struct trapframe *trapframe; // trap帧
    struct context context;  // 上下文切换信息
    pgtbl_t pagetable;       // 用户页表
    int utime, ktime;        // 用户/内核态运行时间
    int thread_num;          // 线程数量
    struct vma *vma;         // 虚拟内存区域链表
    struct file *ofile[NOFILE]; // 打开文件表
    struct file_vnode cwd;   // 当前工作目录
    struct rlimit ofn;       // 打开文件数限制
    __sigset_t sig_set;      // 信号掩码
    sigaction sigaction[SIGRTMAX + 1]; // 信号处理
    __sigset_t sig_pending;  // 待处理信号
} proc_t;
```
- 进程支持多线程（thread_queue），每个进程有主线程（main_thread）。
- 进程资源包括内存、文件、信号、虚拟内存区域（vma）等。

### 2. 进程状态
```c
enum procstate {
    UNUSED,     // 未使用
    USED,       // 已分配
    SLEEPING,   // 睡眠
    RUNNABLE,   // 可运行
    RUNNING,    // 正在运行
    ZOMBIE      // 僵尸
};
```

### 3. 线程控制块（TCB）
```c
struct thread {
    struct proc *proc;          // 所属进程
    uint64 tid;                 // 线程ID
    char *kstack;               // 内核栈
    struct context context;     // 线程上下文
    int state;                  // 线程状态
    // ... 其他字段 ...
};
```

## 进程生命周期

### 1. 创建（fork/clone）
- `fork()`：复制父进程的内存空间、文件描述符、页表等，分配新的PCB，设置为RUNNABLE。
- `clone()`：支持线程/进程的细粒度资源共享（如CLONE_VM等标志），用于实现多线程。

关键实现（`kernel/process.c`）：
```c
struct proc *allocproc(void) {
    // 遍历进程池，找到UNUSED项
    // 初始化锁、状态、分配trapframe、页表、主线程等
    // 进程主线程加入线程队列
}
```
- 进程池 `pool[NPROC]` 预分配，避免动态分配带来的碎片和复杂性。

### 2. 执行（exec）
- 用新程序替换当前进程的代码和数据段。
- 重新初始化用户栈和入口地址。

### 3. 退出（exit/freeproc）
- `exit()`：释放进程资源，关闭文件，释放内存，设置为ZOMBIE，等待父进程回收。
- `freeproc()`：实际释放PCB、主线程、trapframe等资源。

### 4. 等待（wait）
- `wait()`：父进程阻塞，等待子进程退出，回收资源。

## 进程调度

### 1. 调度器
- 采用循环遍历所有进程池，选择RUNNABLE进程，切换到其上下文执行。
- 支持多线程调度，遍历线程队列。

关键实现（`kernel/process.c`）：
```c
void scheduler(void) {
    for (;;) {
        for (p = pool; p < &pool[NPROC]; p++) {
            acquire(&p->lock);
            if (p->state == RUNNABLE) {
                // 切换到进程上下文
                p->state = RUNNING;
                swtch(&cpu->context, &p->context);
                // 进程返回后恢复
            }
            release(&p->lock);
        }
    }
}
```
- 上下文切换通过 `swtch()` 汇编实现，保存/恢复寄存器和栈指针。

## 进程间通信

### 1. 管道（Pipe）
- 通过文件描述符实现父子进程间的字节流通信。
- 管道结构体和操作在文件系统层实现，进程通过 `ofile` 访问。

### 2. 信号（Signal）
- 进程结构体内有信号掩码、信号处理表、待处理信号集合。
- `kill(pid, sig)` 向目标进程发送信号，信号处理由调度器或trap处理流程分发。

## 线程管理

### 1. 线程创建
- 线程通过 `clone_thread()` 创建，加入进程的 `thread_queue`。
- 线程调度与进程调度类似，支持同一进程内多线程并发。

### 2. 线程同步
- 支持互斥锁、自旋锁等同步机制。

## 典型流程示意

1. 用户程序调用 `fork()`，内核分配新PCB，复制父进程资源，加入就绪队列。
2. 调度器选择RUNNABLE进程，切换到其上下文执行。
3. 进程可通过 `exec()` 加载新程序，或通过 `clone()` 创建线程。
4. 进程结束时调用 `exit()`，父进程通过 `wait()` 回收资源。
5. 多线程进程通过 `thread_queue` 管理所有线程，支持并发和同步。

---

SC7进程管理模块通过PCB/TCB、调度器、上下文切换等机制，实现了高效的多进程多线程管理，支持进程间通信和同步，保证系统的并发性和稳定性。 