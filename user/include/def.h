#ifndef __DEF_H__
#define __DEF_H__

/* Explicitly-sized versions of integer types */
typedef char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;
typedef unsigned int uint;
typedef int pid_t;

#define NULL ((void *)0)


typedef struct timeval {
    uint64 sec;      // 秒
    uint64 usec;     // 微秒
} timeval_t;

typedef struct tms
{
    long tms_utime;  ///< 用户态时间
    long tms_stime;  ///< 内核态时间
    long tms_cutime; ///< 子进程用户态时间
    long tms_cstime; ///< 子进程内核态时间
} tms_t;

struct utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char domainname[65];
};

#define AT_FDCWD (-100) //相对路径
#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_RDWR 0x002 // 可读可写
#define O_CREATE 0x40
#define O_DIRECTORY 0x10000


#endif