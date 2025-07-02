#include "types.h"
#include "timer.h"

/*hsai提供给kernel的函数*/

/*hsai_trap*/
void hsai_trap_init();

/*hsai_timer*/
//hsai应该提供下面三个函数还有一些变量
void timer_init(); //初始化时钟

void timer_tick(); //时钟中断的处理函数

void r_time(); //读时钟值,hal层提供的，理论上hsai应该包装一下

extern uint64 boot_time;

#define CLK_FREQ 10000000ul//以宏或者函数的形式提供



//然后是一些给内核使用的函数，还没有解离出去
uint64 get_times(uint64 utms);

timeval_t timer_get_time();


