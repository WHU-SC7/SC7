# SC7时钟管理

## 概述

SC7时钟管理模块负责系统时钟初始化、定时中断、进程计时、时间获取等功能，支持RISC-V和LoongArch架构，保障内核调度、定时任务和用户程序时间相关操作。

---

## 1. 时钟初始化与架构支持

- 头文件`include/kernel/timer.h`定义了时钟频率、时间结构体、主要接口。
- 支持RISC-V（`set_next_timeout`）和LoongArch（`countdown_timer_init`）两种架构的定时器初始化。

### 关键实现片段
```c
void timer_init(void) {
    initlock(&tickslock, "time");
    ticks = 0;
    boot_time += (uint64)read_rtc();
#ifdef RISCV
    // 启用定时中断，设置下一个中断时间
    w_sie(r_sie() | SIE_STIE);
    set_timer(r_time() + INTERVAL);
#else
    countdown_timer_init();
#endif
}
```
- `read_rtc()`读取硬件RTC时间，初始化系统启动时间。
- RISC-V通过SBI或寄存器设置定时中断，LoongArch使用倒计时定时器。

---

## 2. 时钟中断与tick管理

- 每次时钟中断触发`timer_tick()`，递增全局tick计数`ticks`，并唤醒等待的进程。

```c
void timer_tick(void) {
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
#ifdef RISCV
    set_next_timeout();
#endif
}
```
- 支持多核并发安全，tick为系统调度、定时任务等提供基础。

---

## 3. 时间获取与定时接口

- 提供多种时间结构体：`timeval_t`（秒/微秒）、`timespec_t`（秒/纳秒）、`tms_t`（进程计时）。
- 主要接口：
  - `timer_get_time()`：获取当前系统时间（秒/微秒）
  - `timer_get_ntime()`：获取当前系统时间（秒/纳秒）
  - `get_times(uint64 utms)`：获取当前进程及其子进程的用户/内核态运行时间

### 关键实现片段
```c
timeval_t timer_get_time() {
    timeval_t tv;
    uint64 clk = r_time();
    tv.sec = boot_time + clk / CLK_FREQ;
    tv.usec = (clk % CLK_FREQ) * 1000000 / CLK_FREQ;
    return tv;
}

uint64 get_times(uint64 utms) {
    struct tms ptms;
    ptms.tms_utime = myproc()->utime;
    ptms.tms_stime = myproc()->ktime;
    // ... 累加子进程时间
    copyout(myproc()->pagetable, utms, (char *)&ptms, sizeof(ptms));
    return 0;
}
```

---

## 4. 进程计时与定时器

- 每个进程维护用户/内核态运行时间（`utime`/`ktime`），用于`times`、`getrusage`等系统调用。
- 支持`itimer`、`sleep`、`nanosleep`等定时功能。
- 进程可通过时钟中断实现定时唤醒、超时等机制。

---

## 5. 机制与特性总结

- 支持高精度定时、系统调度、进程计时、用户时间获取。
- 兼容多架构，适配不同硬件定时器。
- 提供多种时间结构体和系统调用接口，便于用户程序开发。
- tick机制为内核调度、定时任务、超时处理等提供基础。

---

SC7时钟管理模块为操作系统提供了高效、可靠的时间和定时服务，是内核调度、进程管理和用户程序时间操作的基础。 