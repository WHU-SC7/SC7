#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "types.h"

struct sockaddr_in{
    uint16      sin_family;   //通信类型
    uint16      sin_port;     //16位的端口号
    uint64      sin_addr;     //32位IP地址
    char        sin_zero[8];  //不使用，一般用0填充 与sockaddr大小对应
};

struct sockaddr {
    uint16 sa_family; /* 地址家族, AF_xxx */
    char sa_data[14]; /*14字节协议地址*/
};

enum sock_type {
	SOCK_DGRAM	= 1,
	SOCK_STREAM	= 2,
};

enum socket_state {
    SOCKET_UNBOUND,     // 未绑定状态
    SOCKET_BOUND,       // 已绑定地址
    SOCKET_LISTENING,   // 正在监听
    SOCKET_CONNECTED,   // 已连接
    SOCKET_CLOSED       // 已关闭
};

struct socket {
    int domain;             // 协议族
    int type;               // 套接字类型
    int protocol;           // 协议类型
    enum socket_state state; // 当前状态
    struct sockaddr_in local_addr;  // 本地地址
    struct sockaddr_in remote_addr; // 远程地址
};

// 最大数据包大小
#define MAX_PACKET_SIZE 1500

// 伪网络层函数声明
int net_send_packet(struct socket *sock, char *data, int len, struct sockaddr_in *dest);
int net_recv_packet(struct socket *sock, char *buf, int len, struct sockaddr_in *src);

/* Supported address families. */
#define PF_UNSPEC	0	/* Unspecified.  */
#define PF_LOCAL	1	/* Local to host (pipes and file-domain).  */
#define PF_UNIX		PF_LOCAL /* POSIX name for PF_LOCAL.  */
#define PF_FILE		PF_LOCAL /* Another non-standard name for PF_LOCAL.  */
#define PF_INET		2	/* IP protocol family.  */

#define SOL_SOCKET 1
#define SO_ACCEPTCONN 30
#define SO_BROADCAST 6
#define SO_DONTROUTE 5
#define SO_ERROR 4
#define SO_KEEPALIVE 9
#define SO_LINGER 13
#define SO_OOBINLINE 10
#define SO_RCVBUF 8
#define SO_RCVLOWAT 18
#define SO_REUSEADDR 2
#define SO_SNDBUF 7
#define SO_SNDLOWAT 19
#define SO_TYPE 3

#endif