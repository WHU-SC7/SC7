#include "types.h"
uint64 hsai_get_mem_start() {
    // 返回物理内存起始地址，例如从硬件寄存器或全局变量获取
    return 0x80000000;  // 示例地址，需根据实际硬件调整
}
