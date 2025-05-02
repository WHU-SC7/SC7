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

struct kstat {
    uint64 st_dev;
    uint64 st_ino;
    uint32 st_mode;
    uint32 st_nlink;
    uint32 st_uid;
    uint32 st_gid;
    uint64 st_rdev;
    uint64 __pad;
    uint64 st_size;
    uint32 st_blksize;
    uint32 __pad2;
    uint64 st_blocks;
    uint64 st_atime_sec;
    uint64 st_atime_nsec;
    uint64 st_mtime_sec;
    uint64 st_mtime_nsec;
    uint64 st_ctime_sec;
    uint64 st_ctime_nsec;
    //unsigned __unused[2];
};

#define O_RDONLY  0x000     ///< 只读
#define O_WRONLY  0x001     ///< 只写    
#define O_RDWR    0x002     ///< 读写
#define O_CREATE  0100     ///< 如果指定的文件不存在，则创建该文件。
#define O_TRUNC   0x400     ///< 如果文件已存在且以写方式打开，则将文件长度截断为0，即清空文件内容
#define O_DIRECTORY 0x004   ///< 要求打开的目标必须是一个目录，否则打开失败
#define O_CLOEXEC 0x008     ///< 在执行 exec 系列函数时，自动关闭该文件描述符（close on exec）

/* 
 * 用于表示当前工作目录的文件描述符 
 * 在诸如 openat()、fstatat() 等系统调用中，
 * AT_FDCWD 表示路径是相对于当前进程的工作目录，
 * 而不是相对于某个打开的目录文件描述符。
 */
#define AT_FDCWD -100

#define CONSOLE 1

// for mmap
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define PROT_GROWSDOWN 0X01000000
#define PROT_GROWSUP 0X02000000

#define MAP_FILE 0
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0X02
#define MAP_FAILED ((void *)-1)

#endif