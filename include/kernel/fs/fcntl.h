#ifndef __FNCTL_H__
#define __FNCTL_H__

#define O_RDONLY  0x000     ///< 只读
#define O_WRONLY  0x001     ///< 只写    
#define O_RDWR    0x002     ///< 读写
#define O_CREATE  0100     ///< 如果指定的文件不存在，则创建该文件。
#define O_TRUNC   0x400     ///< 如果文件已存在且以写方式打开，则将文件长度截断为0，即清空文件内容
#define O_DIRECTORY 0x004   ///< 要求打开的目标必须是一个目录，否则打开失败
#define O_CLOEXEC 0x008     ///< 在执行 exec 系列函数时，自动关闭该文件描述符（close on exec）

#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define F_DUPFD_CLOEXEC 1030

/* 
 * 用于表示当前工作目录的文件描述符 
 * 在诸如 openat()、fstatat() 等系统调用中，
 * AT_FDCWD 表示路径是相对于当前进程的工作目录，
 * 而不是相对于某个打开的目录文件描述符。
 */
#define AT_FDCWD -100

/* 文件权限位 */
#define S_ISUID 0004000  /* set user id on execution */
#define S_ISGID 0002000  /* set group id on execution */

/* 执行权限位 */
#define S_IXUSR 0000100  /* execute permission for owner */
#define S_IXGRP 0000010  /* execute permission for group */
#define S_IXOTH 0000001  /* execute permission for others */

#endif  ///<__FNCTL_H__