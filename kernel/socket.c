// #include "types.h"

// /**
//  * @brief 模拟网络层发送数据包
//  *
//  * @param sock 套接字结构
//  * @param data 数据缓冲区
//  * @param len 数据长度
//  * @param dest 目标地址
//  * @return int 发送的字节数
//  */
// int net_send_packet(struct socket *sock, char *data, int len, struct sockaddr_in *dest)
// {
//     // 在实际实现中，这里会调用具体的网络协议栈
//     DEBUG_LOG_LEVEL(LOG_DEBUG, "Sending %d bytes to %d.%d.%d.%d:%d\n",
//                     len,
//                     (dest->sin_addr >> 24) & 0xFF,
//                     (dest->sin_addr >> 16) & 0xFF,
//                     (dest->sin_addr >> 8) & 0xFF,
//                     dest->sin_addr & 0xFF,
//                     ntohs(dest->sin_port));

//     // 模拟发送成功
//     return len;
// }

// /**
//  * @brief 模拟网络层接收数据包
//  *
//  * @param sock 套接字结构
//  * @param buf 接收缓冲区
//  * @param len 缓冲区长度
//  * @param src 发送方地址
//  * @return int 接收的字节数
//  */
// int net_recv_packet(struct socket *sock, char *buf, int len, struct sockaddr_in *src)
// {
//     // 在实际实现中，这里会从网络队列中获取数据包
//     // 模拟接收一个数据包
//     static char test_data[] = "Test network packet";
//     int recv_len = sizeof(test_data);

//     if (recv_len > len)
//     {
//         recv_len = len;
//     }

//     memcpy(buf, test_data, recv_len);

//     // 填充模拟的发送方地址
//     src->sin_family = PF_INET;
//     src->sin_port = htons(12345);
//     src->sin_addr = 0x0100007F; // 127.0.0.1

//     DEBUG_LOG_LEVEL(LOG_DEBUG, "Received %d bytes from %d.%d.%d.%d:%d\n",
//                     recv_len,
//                     (src->sin_addr >> 24) & 0xFF,
//                     (src->sin_addr >> 16) & 0xFF,
//                     (src->sin_addr >> 8) & 0xFF,
//                     src->sin_addr & 0xFF,
//                     ntohs(src->sin_port));

//     return recv_len;
// }
#include "socket.h"
#include "print.h"
#include "errno-base.h"
#include "string.h"

int sock_bind(struct socket *sock, struct sockaddr_in *addr, int addrlen)
{
    // 检查套接字状态
    if (sock->state != SOCKET_UNBOUND)
    {
        DEBUG_LOG_LEVEL(LOG_ERROR, "Socket already bound or in invalid state\n");
        return -EINVAL;
    }

    if (addr->sin_port == 0)
    {
        addr->sin_port = 2000;
    }
    memmove(&sock->local_addr, &addr, sizeof(struct sockaddr_in));
    sock->state = SOCKET_BOUND; ///< 设置状态为已绑定
    return 0;
}