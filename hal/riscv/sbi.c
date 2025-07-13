#include "types.h"

#if defined SBI
const uint64 SBI_SET_TIMER = 0;
const uint64 SBI_CONSOLE_PUTCHAR = 1;
const uint64 SBI_CONSOLE_GETCHAR = 2;
const uint64 SBI_CLEAR_IPI = 3;
const uint64 SBI_SEND_IPI = 4;
const uint64 SBI_REMOTE_FENCE_I = 5;
const uint64 SBI_REMOTE_SFENCE_VMA = 6;
const uint64 SBI_REMOTE_SFENCE_VMA_ASID = 7;
const uint64 SBI_SHUTDOWN = 8;

// HSM 扩展 ID 和功能号.
#define SBI_EXT_HSM                 0x48534D  // "HSM" ASCII
#define SBI_HSM_HART_START           0
#define SBI_HSM_HART_STOP            1
#define SBI_HSM_HART_GET_STATUS      2
#define SBI_HSM_HART_SUSPEND         3

static int inline sbi_call(uint64 which, uint64 arg0, uint64 arg1, uint64 arg2)
{
	register uint64 a0 asm("a0") = arg0;
	register uint64 a1 asm("a1") = arg1;
	register uint64 a2 asm("a2") = arg2;
	register uint64 a7 asm("a7") = which;
	asm volatile("ecall"    
		     : "=r"(a0)
		     : "r"(a0), "r"(a1), "r"(a2), "r"(a7)
		     : "memory");
	return a0;
}

int sbi_hart_start(uint64_t hartid, uint64_t start_addr, uint64_t opaque) {
    register uint64_t a0 asm("a0") = hartid;
    register uint64_t a1 asm("a1") = start_addr;
    register uint64_t a2 asm("a2") = opaque;
    register uint64_t a6 asm("a6") = SBI_HSM_HART_START;
    register uint64_t a7 asm("a7") = SBI_EXT_HSM;
    register uint64_t error_code asm("a0");

    asm volatile (
        "ecall"
        : "=r"(error_code)
        : "r"(a0), "r"(a1), "r"(a2), "r"(a6), "r"(a7)
        : "memory"
    );
    return error_code;  // 返回 0 表示成功，负数表示错误
}

/*暂时没用上*/
void sbi_hart_stop(void) {
    register uint64_t a6 asm("a6") = SBI_HSM_HART_STOP;
    register uint64_t a7 asm("a7") = SBI_EXT_HSM;

    asm volatile (
        "ecall"
        : /* 无输出 */
        : "r"(a6), "r"(a7)
        : "memory"
    );
    // 调用成功时不会返回
    while (1) {}  // 死循环防止意外继续执行
}

void console_putchar(int c)
{
	sbi_call(SBI_CONSOLE_PUTCHAR, c, 0, 0);
}

int console_getchar()
{
	return sbi_call(SBI_CONSOLE_GETCHAR, 0, 0, 0);
}

void shutdown()
{
	sbi_call(SBI_SHUTDOWN, 0, 0, 0);
}

void set_timer(uint64 stime)
{
	sbi_call(SBI_SET_TIMER, stime, 0, 0);
}
#else //< 不使用sbi，不需要编译上面的函数

#endif