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

struct utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char domainname[65];
};

#define AT_FDCWD (-100) //相对路径
#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200
#define O_TRUNC   0x400
#define O_DIRECTORY 0x004
#define O_CLOEXEC 0x008



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