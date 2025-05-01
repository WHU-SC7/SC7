#include "types.h"
#include "spinlock.h"
#include "timer.h"
#include "print.h"
#include "process.h"
//#include "syscall.h"
#include "vmem.h"
#include "cpu.h"
#include "print.h"
#include "hsai_trap.h"
#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif
extern proc_t pool[NPROC];

struct spinlock tickslock;
uint ticks;

#if defined SBI
extern void set_timer(uint64 stime); //< 通过sbi设置下一个时钟中断
#endif

/**
 * @brief 初始化timer, RISCV
 * RISCV的计时方式是比较time计数器和stimecmp寄存器的值，相等时产生中断
 */
void 
timer_init(void) 
{
    initlock(&tickslock, "time");

    ticks = 0;
#ifdef RISCV
    #if defined SBI //< 使用sbi
    w_sie(r_sie() | SIE_STIE); //< 虽然start已经设置了SIE_STIE,这里再设置一次
	    set_timer(r_time() + INTERVAL);
    #else           //< 不使用sbi的情况
    /* enable supervisor-mode timer interrupts. */
    w_mie(r_mie() | MIE_STIE);
  
    /* enable the sstc extension (i.e. stimecmp). */
    w_menvcfg(r_menvcfg() | (1L << 63)); 
  
    /* allow supervisor to use stimecmp and time. */
    w_mcounteren(r_mcounteren() | 2);
  
    /* ask for the very first timer interrupt. */
    w_stimecmp(r_time() + INTERVAL);
    #endif
#else
    countdown_timer_init();
#endif
}

#ifdef RISCV
/**
 * @brief 设置下一次定时器中断产生, RISCV
 * 
 */
void 
set_next_timeout(void) 
{
    uint64 next = r_time() + INTERVAL;
    /* TODO 这里未来可能会改成SBI形式 */
    #if defined SBI
    set_timer(next);
    #else
    w_stimecmp(next);
    #endif
}
#else   ///< loongarch
/**
 * @brief 设置计时器产生中断的时间, 自动装载, loongarch
 * loongarch的计时器是使用倒计时，到0时产生中断，可以设置是否自动装载
 */
void
countdown_timer_init(void)
{
    uint64 prcfg1_val;
    /* 读取特权资源配置信息1 */
    prcfg1_val = r_csr_prcfg1();

    /* 读取定时器(Timer)的有效位数 */
    uint64 timerbits = FIELD_GET(prcfg1_val,PRCFG1_TIMERBITS_LEN,
                                PRCFG1_TIMERBITS_SHIFT) + 1;
    
    /* 判断有效位是否合法 */
    assert (timerbits >= 0 && timerbits <=64, 
        "countdown_timer_init: timerbits is invalid");
    
    /* 设置定时器配置寄存器，自减至0时，自动装载initval的值 */
    uint64 tcfg_val = FIELD_WRITE(1,TCFG_EN_LEN,TCFG_EN_SHIFT) | 
          FIELD_WRITE(1,TCFG_RERIODIC_LEN,TCFG_RERIODIC_SHIFT) | 
          FIELD_WRITE(INTERVAL,TCFG_INITVAL_LEN(timerbits),TCFG_INITVAL_SHIFT);
    
    /* 启动countdown_timer */
    w_csr_tcfg(tcfg_val);
}
#endif

/**
 * @brief 自加ticks
 * 
 */
void 
timer_tick(void) 
{
#if DEBUG
    printf("timer tick\n");
#endif
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
#ifdef RISCV
    set_next_timeout();
#endif
}

/**
 * @brief 得到当前进程的运行时间
 * 
 * @return uint64 成功返回0
 */
uint64 
get_times(uint64 utms) 
{
    struct tms ptms;
    ptms.tms_utime = myproc()->utime;
    ptms.tms_stime = myproc()->ktime;
    ptms.tms_cstime = 1;
    ptms.tms_cutime = 1;

    /* 加上所有孩子进程的时间 */
    struct proc *p;
    for (p = pool; p < pool + NPROC; p++) 
    {
        acquire(&p->lock);
        if (p->parent == myproc()) {
            ptms.tms_cutime += p->utime;
            ptms.tms_cstime += p->ktime;
        }
        release(&p->lock);
    }
    copyout(myproc()->pagetable, utms, (char *)&ptms, sizeof(ptms));
    return 0;
}

timeval_t timer_get_time(){
    timeval_t tv;
    uint64 clk = r_time();
    tv.sec = clk / CLK_FREQ;
    tv.usec = (clk % CLK_FREQ) * 1000000 / CLK_FREQ;
    return tv;
}