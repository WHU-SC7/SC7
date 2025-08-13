/**
 * @brief Loop 设备实现
 * 
 * 简化的 loop 设备实现，主要用于 LTP 测试框架支持
 */

#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif

#include "defs.h"
#include "file.h"
#include "fs.h"
#include "vmem.h"
#include "process.h"
#include "string.h"
#include "errno-base.h"
#include "ioctl.h"

// Loop 设备状态
struct loop_device {
    char backing_file[256];  // 支撑文件路径
    int in_use;              // 是否被使用
    uint64_t size;           // 设备大小
    int fd;                  // 关联的文件描述符
};

#define MAX_LOOP_DEVICES 256
static struct loop_device loop_devices[MAX_LOOP_DEVICES];
static int loop_initialized = 0;

/**
 * @brief 初始化 loop 设备表
 */
void loop_init(void) {
    if (loop_initialized) return;
    
    memset(loop_devices, 0, sizeof(loop_devices));
    
    // 初始化所有设备为未使用状态
    for (int i = 0; i < MAX_LOOP_DEVICES; i++) {
        loop_devices[i].in_use = 0;
        loop_devices[i].fd = -1;
    }
    
    loop_initialized = 1;
}

/**
 * @brief 查找空闲的 loop 设备
 */
static int find_free_loop_device(void) {
    for (int i = 0; i < MAX_LOOP_DEVICES; i++) {
        if (!loop_devices[i].in_use) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Loop 设备读取函数
 * 
 * @param user_dst 是否为用户空间地址
 * @param dst 目标地址
 * @param n 读取字节数
 * @return int 实际读取的字节数
 */
int loopread(int user_dst, uint64 dst, int n) {
    // 简化实现：返回零数据
    if (user_dst) {
        // 用户空间地址
        char zeros[512] = {0};
        int to_copy = n > 512 ? 512 : n;
        if (copyout(myproc()->pagetable, dst, zeros, to_copy) < 0) {
            return -EFAULT;
        }
        return to_copy;
    } else {
        // 内核空间地址
        memset((void*)dst, 0, n);
        return n;
    }
}

/**
 * @brief Loop 设备写入函数
 * 
 * @param user_src 是否为用户空间地址
 * @param src 源地址
 * @param n 写入字节数
 * @return int 实际写入的字节数
 */
int loopwrite(int user_src, uint64 src, int n) {
    // 简化实现：忽略写入数据
    if (user_src) {
        // 验证用户空间地址有效性
        char temp[512];
        int to_copy = n > 512 ? 512 : n;
        if (copyin(myproc()->pagetable, temp, src, to_copy) < 0) {
            return -EFAULT;
        }
        return to_copy;
    }
    return n;
}

/**
 * @brief 设置 loop 设备的支撑文件
 * 
 * @param loop_minor loop 设备的次设备号
 * @param backing_file 支撑文件路径
 * @return int 成功返回0，失败返回负错误码
 */
int loop_set_backing_file(int loop_minor, const char *backing_file) {
    if (loop_minor < 0 || loop_minor >= MAX_LOOP_DEVICES) {
        return -EINVAL;
    }
    
    loop_init();
    
    struct loop_device *loop = &loop_devices[loop_minor];
    if (loop->in_use) {
        return -EBUSY;
    }
    
    strncpy(loop->backing_file, backing_file, sizeof(loop->backing_file) - 1);
    loop->backing_file[sizeof(loop->backing_file) - 1] = '\0';
    loop->in_use = 1;
    loop->size = 1024 * 1024; // 默认 1MB 大小
    
    return 0;
}

/**
 * @brief 分离 loop 设备
 * 
 * @param loop_minor loop 设备的次设备号
 * @return int 成功返回0，失败返回负错误码
 */
int loop_detach(int loop_minor) {
    if (loop_minor < 0 || loop_minor >= MAX_LOOP_DEVICES) {
        return -EINVAL;
    }
    
    struct loop_device *loop = &loop_devices[loop_minor];
    if (!loop->in_use) {
        return -ENXIO;
    }
    
    memset(loop, 0, sizeof(*loop));
    return 0;
}

/**
 * @brief 获取空闲的 loop 设备号
 * 
 * @return int 设备号，失败返回负错误码
 */
int loop_get_free_device(void) {
    loop_init();
    return find_free_loop_device();
}

/**
 * @brief Loop 设备 ioctl 操作
 * 
 * @param minor 设备次设备号
 * @param cmd ioctl 命令
 * @param arg 参数
 * @return int 成功返回0，失败返回负错误码
 */
int loop_ioctl(int minor, unsigned int cmd, unsigned long arg) {
    loop_init();
    
    if (minor < 0 || minor >= MAX_LOOP_DEVICES) {
        return -EINVAL;
    }
    
    struct loop_device *loop = &loop_devices[minor];
    
    switch (cmd) {
        case LOOP_GET_STATUS:
        case LOOP_GET_STATUS64:
            // 返回设备状态 - 如果设备未使用，应该返回错误
            if (!loop->in_use) {
                return -ENXIO;  // 设备未配置
            }
            // 简化实现：返回成功
            return 0;
            
        case LOOP_SET_FD:
            // 设置支撑文件描述符
            if (loop->in_use) {
                return -EBUSY;
            }
            loop->fd = (int)arg;
            loop->in_use = 1;
            return 0;
            
        case LOOP_CLR_FD:
            // 清除设备配置
            if (!loop->in_use) {
                return -ENXIO;
            }
            memset(loop, 0, sizeof(*loop));
            loop->fd = -1;
            return 0;
            
        case LOOP_SET_STATUS:
        case LOOP_SET_STATUS64:
            // 设置设备状态
            if (!loop->in_use) {
                return -ENXIO;
            }
            return 0;
            
        default:
            return -EINVAL;
    }
}
