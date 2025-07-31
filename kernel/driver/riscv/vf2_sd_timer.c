#include "vf2_driver.h"

// 读取时间戳
uint64 read_tick(void) {
    uint64 time;
    asm volatile("rdtime %0" : "=r" (time));
    return time;
}

// 微秒级延时
void delay_us(uint64 microseconds) {
    uint64 start = read_tick();
    uint64 target = start + (microseconds * TIME_BASE / 1000000);
    
    while (read_tick() < target) {
        // 忙等待
    }
}

// 启动定时器
timer_t timer_start(uint64 microseconds) {
    timer_t timer;
    timer.deadline = read_tick() + (microseconds * TIME_BASE / 1000000);
    return timer;
}

// 检查定时器是否超时
int timer_timeout(timer_t *timer) {
    return read_tick() >= timer->deadline;
} 