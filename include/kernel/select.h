#ifndef __SELECT_H__
#define __SELECT_H__

#include "types.h"
#include "time.h"

/**
 * @brief 文件描述符集合的最大大小
 * 通常设置为1024，对应32位系统中的一个长整型数组
 */
#define FD_SETSIZE 1024

/**
 * @brief 每个长整型可以存储的文件描述符数量
 * 32位系统为32，64位系统为64
 */
#define __NFDBITS (8 * sizeof(long))

/**
 * @brief 计算文件描述符在数组中的索引
 */
#define __FD_ELT(d) ((d) / __NFDBITS)

/**
 * @brief 计算文件描述符在长整型中的位偏移
 */
#define __FD_MASK(d) ((long)1 << ((d) % __NFDBITS))

/**
 * @brief 文件描述符集合结构体
 * 使用位图表示文件描述符集合
 */
typedef struct {
    long __fds_bits[FD_SETSIZE / __NFDBITS];
} fd_set;

/**
 * @brief 检查文件描述符是否在集合中
 * @param fd 文件描述符
 * @param set 文件描述符集合
 * @return 非零值表示在集合中，0表示不在集合中
 */
#define FD_ISSET(fd, set) \
    ((set)->__fds_bits[__FD_ELT(fd)] & __FD_MASK(fd))

/**
 * @brief 将文件描述符添加到集合中
 * @param fd 文件描述符
 * @param set 文件描述符集合
 */
#define FD_SET(fd, set) \
    ((set)->__fds_bits[__FD_ELT(fd)] |= __FD_MASK(fd))

/**
 * @brief 从集合中移除文件描述符
 * @param fd 文件描述符
 * @param set 文件描述符集合
 */
#define FD_CLR(fd, set) \
    ((set)->__fds_bits[__FD_ELT(fd)] &= ~__FD_MASK(fd))

/**
 * @brief 清空文件描述符集合
 * @param set 文件描述符集合
 */
#define FD_ZERO(set) \
    memset((set), 0, sizeof(fd_set))

/**
 * @brief 32位时间结构体，用于select系统调用
 * 兼容32位应用程序的select调用
 */
struct old_timespec32 {
    int32_t tv_sec;   ///< 秒数
    int32_t tv_nsec;  ///< 纳秒数
};

/**
 * @brief pollfd结构体，用于poll系统调用
 */
struct pollfd {
    int fd;        ///< 文件描述符
    short events;  ///< 请求的事件
    short revents; ///< 返回的事件
};

/**
 * @brief poll事件定义
 */
#define POLLIN     0x0001  ///< 可读
#define POLLPRI    0x0002  ///< 高优先级可读
#define POLLOUT    0x0004  ///< 可写
#define POLLERR    0x0008  ///< 错误
#define POLLHUP    0x0010  ///< 挂起
#define POLLNVAL   0x0020  ///< 无效请求
#define POLLRDNORM 0x0040  ///< 普通数据可读
#define POLLRDBAND 0x0080  ///< 带外数据可读
#define POLLWRNORM 0x0100  ///< 普通数据可写
#define POLLWRBAND 0x0200  ///< 带外数据可写

/**
 * @brief select函数的返回值
 */
#define SELECT_OK      0   ///< 正常返回
#define SELECT_TIMEOUT 1   ///< 超时返回
#define SELECT_ERROR   -1  ///< 错误返回

#endif /* __SELECT_H__ */ 