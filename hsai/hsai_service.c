#include "types.h"
#if defined RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

/**
 * 一些不知道放在哪里的接口或服务，先放到这里
 */

/**
 * @brief 查询hartid
 */
int hsai_get_cpuid()
{
    return r_tp();
}

extern volatile int hart0_is_starting;
extern volatile int first_hart;
extern int sbi_hart_start(uint64_t hartid, uint64_t start_addr, uint64_t opaque);
#include "cpu.h"

/**
 * @brief 对sbi多核启动的支持。la无影响。如果boot hart不是hart 0则唤醒hart 0
 */
void hsai_hart_disorder_boot()
{
#if RISCV
    #if VF //VF使用单核
    #else
    /*非常高级的opensbi支持boot hart不等于0的情况*/
    if((hsai_get_cpuid() != 0)&&(hart0_is_starting == 0)) //别的hart在hart0之前启动
    { 
        hart0_is_starting = 1;
        first_hart = hsai_get_cpuid(); //记录最先启动的hart的id
        sbi_hart_start(0, 0x80200000, 0); //甚至这个时候不能打印 "唤醒hart 0"
    }
    #endif
#endif
}

/**
 * @brief 对sbi多核启动的支持。la无影响。hart 0 唤醒所有hart
 */
void hsai_hart_start_all()
{
#if RISCV
    int ret;
    // 依次启动hart 1和其他hart.最多NCPU个
    for(int i=1;i<NCPU;i++)
    {
    //不同版本的错误码不一样，比赛qemu自带OpenSBI v1.5.1，如果重复启动，错误码是3。最新的OpenSBI v1.7，错误码是7。难以通过错误码判断
        if((ret = sbi_hart_start(i, 0x80200000, 0))) // 不存在对应hart或者重复启动都会返回错误码
        {
            if(i == first_hart) //first_hart后面可能还有hart要启动
            {
                continue;
            }
            else
                break;
        }
    }
#else
    void start_secondary_harts(uintptr_t num_harts);
    start_secondary_harts(NCPU); // 尝试启动4个核
#endif
}