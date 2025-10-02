#ifndef TLIBC_H
#define TLIBC_H
// 临时存放core.c和test.c都需要的宏定义

//type.h

//放到什么头文件？
#define AT_FDCWD -100 // 当前工作目录

// fcntl.h
#define O_RDONLY 0x000                      ///< 只读
#define O_WRONLY 0x001                      ///< 只写
#define O_RDWR 0x002                        ///< 读写
#define O_CREAT 0100                        ///< 如果指定的文件不存在，则创建该文件。
#define O_CREATE 0100                       ///< 如果指定的文件不存在，则创建该文件。(别名)
#define O_TRUNC 0x400                       ///< 如果文件已存在且以写方式打开，则将文件长度截断为0，即清空文件内容

#endif