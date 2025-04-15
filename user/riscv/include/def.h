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


typedef struct timeval {
    uint64 sec;      // 秒
    uint64 usec;     // 微秒
} timeval_t;

#endif