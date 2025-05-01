#include "ops.h"

#include "fcntl.h"

#include "file.h"
#include "fs.h"
#include "defs.h"
#include "string.h"
#include "vfs_ext4_ext.h"
#include "cpu.h"

/**
 * @brief 根据传入的路径 path 和目录文件描述符 dirfd，查找对应的 inode
 * 
 * //TODO 没实现
 * 根据 dirfd 逻辑决定从哪里作为解析的起点
 * 
 * @param path 目录文件路径，绝对路径/相对路径
 * @param dirfd 目录文件描述符
 * @param name 如果非NULL，表示希望获取的是path的父目录的inode
 * @return struct inode* 
 */
struct inode *find_inode(char *path, int dirfd, char *name) {
    // struct inode *ip;
    // struct proc *p = myproc();
    //
    // //绝对路径 || 相对路径
    // if (*path == '/' || dirfd == AT_FDCWD) {
    //     ip = (name == NULL) ? namei(path) : nameiparent(path, name);
    //     if (ip == 0) {
    //         return 0;
    //     }
    //     return ip;
    // }
    // struct file *f;
    // if (dirfd < 0 || dirfd >= NOFILE || (f=p->ofile[dirfd]) == 0) {
    //     return 0;
    // }
    // struct inode *oldcwd = p->cwd;
    // p->cwd = f->f_ip;
    // ip = (name == NULL) ? namei(path) : nameiparent(path, name);
    // p->cwd = oldcwd;
    // if (ip == 0) {
    //     return 0;
    // }
    // return ip;
    return NULL;
}
/**
 * @brief Get the absolute path object
 * 
 * @param path 路径，可以是绝对路径或相对路径
 * @param cwd 当前工作目录
 * @param absolute_path 最终的绝对路径
 */
void get_absolute_path(const char *path, const char *cwd, char *absolute_path) {
    /* 为空，工作目录 */
    if (path == NULL) 
    {
        strncpy(absolute_path, cwd, strlen(cwd));
    }
    /* '/'开头为绝对路径 */ 
    else if (path[0] == '/') 
    {
        strcpy(absolute_path, path);
    }
    /* 相对路径，拼接成绝对路径 */ 
    else 
    {
        strcpy(absolute_path, cwd);
        strcat(absolute_path, "/");
        strcat(absolute_path, path);
    }
    /* 处理 ./ and ../ */
    char *p = absolute_path;
    while (*p != '\0') 
    {
        /* ./ */
        if (*p == '.' && *(p + 1) == '/') 
        {
            strcpy(p, p + 2);
        }
        /* ../ */ 
        else if (*p == '.' && *(p + 1) == '.' && *(p + 2) == '/') 
        {
            char *q = p - 2;
            while (q >= absolute_path && *q != '/') 
            {
                q--;
            }
            if (q >= absolute_path) 
            {
                strcpy(q + 1, p + 3);
                p = q;
            } 
            else 
            {
                strcpy(absolute_path, p + 3);
                p = absolute_path;
            }
        } 
        else 
        {
            p++;
        }
    }

    /* 处理尾巴的 . 和 .. */
    p = absolute_path + strlen(absolute_path) - 2;
    if ((*p == '/' || *p == '.') && *(p + 1) == '.' && *(p + 2) == '\0') 
    {
        *p = *(p + 1) = *(p + 2) = '\0';
    }
    p--;
    if (*p == '/' && *(p + 1) == '.' && *(p + 2) == '.' && *(p + 3) == '\0') 
    {
        if (p == absolute_path) 
        {
            *(p + 1) = *(p + 2) = *(p + 3) = '\0';
            return;
        }
        char *q = p - 1;
        while (q >= absolute_path && *q != '/') 
        {
            q--;
        }
        if (q >= absolute_path) 
        {
            *q = '\0';
            p = q;
        } 
        else 
        {
            strcpy(absolute_path, p + 4);
            p = absolute_path;
        }
    }

    while (absolute_path[0] == '/' && absolute_path[1] == '/') 
    {
        strcpy(absolute_path, absolute_path + 1);
    }
    size_t len = strlen(absolute_path);
    if (absolute_path[len - 1] == '/') 
    {
        absolute_path[len - 1] = '\0';
        --len;
    }
    if (strlen(absolute_path) == 0) 
    {
        strcpy(absolute_path, "/");
    } 
    else if (absolute_path[0] != '/') 
    {
        size_t len2 = strlen(absolute_path);
        char *x = absolute_path + len2;
        *(x + 1) = 0;
        while (x > absolute_path) 
        {
            *x = *(x - 1);
            x--;
        }
        *x = '/';
    }
}

// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
// static char*
// skipelem(char *path, char *name)
// {
//     char *s;
//     int len;

//     while(*path == '/')
//         path++;
//     if(*path == 0)
//         return 0;
//     s = path;
//     while(*path != '/' && *path != 0)
//         path++;
//     len = path - s;
//     if(len >= DIRSIZ)
//         memmove(name, s, DIRSIZ);
//     else {
//         memmove(name, s, len);
//         name[len] = 0;
//     }
//     while(*path == '/')
//         path++;
//     return path;
// }

/**
 * @brief  根据路径名查找并返回对应的 inode（索引节点）
 * //TODO 没有实现
 * 该函数从根目录或当前工作目录开始，解析路径字符串 path，
 * 逐级查找目录项，直到找到目标文件或目录对应的 inode。
 * 
 * 如果 nameiparent 不为 0，则函数返回的是路径中最后一个元素的父目录 inode，
 * 并将最后一个路径元素的名字拷贝到参数 name 中（name 缓冲区大小至少为 DIRSIZ）。
 * 这对于创建新文件时非常有用（要先找到父目录 inode）。
 * 
 * 注意：该函数在调用时必须处于一个事务内（atomic），因为它使用了 iput() 等可能修改引用计数的函数。
 * 
 * @param path 路径字符串，绝对路径或相对路径
 * @param nameiparent 是否返回父目录 inode，非零表示是，零表示返回目标文件或目录 inode
 * @param name 用于存储最后一个路径元素的名称，必须至少能容纳 DIRSIZ 字节
 * @return struct inode* 找到的 inode 指针，失败返回 NULL
 */
static struct inode*
namex(char *path, int nameiparent, char *name)
{
    // struct inode *ip, *next;
    //
    // // if(*path == '/') {
    // //
    // // }
    // // else
    // //     ip = idup(myproc()->cwd);
    //
    // while((path = skipelem(path, name)) != 0){
    //     ilock(ip);
    //     if(ip->i_type != T_DIR){
    //         iunlockput(ip);
    //         return 0;
    //     }
    //     if(nameiparent && *path == '\0'){
    //         // Stop one level early.
    //         iunlock(ip);
    //         return ip;
    //     }
    //     if((next = dirlookup(ip, name, 0)) == 0){
    //         iunlockput(ip);
    //         return 0;
    //     }
    //     iunlockput(ip);
    //     ip = next;
    // }
    // if(nameiparent){
    //     iput(ip);
    //     return 0;
    // }
    // return ip;
    return NULL;
}

/**
 * @brief 将路径字符串转换为对应的 inode（索引节点）指针
 * 
 * @param path 路径名
 * @return struct inode* inode指针
 */
struct inode*
namei(char *path)
{
    char name[EXT4_PATH_LONG_MAX];
    get_absolute_path(path, myproc()->cwd.path, name);
    // printf("%s %s\n", name, myproc()->cwd.path);
    return vfs_ext_namei(name);
}

/**
 * @brief 根据路径名查找路径中最后一个元素的父目录 inode，并将最后一个路径元素名拷贝到 name
 * //TODO 没有实现
 * 该函数调用了 namex 函数，传入参数 nameiparent=1，表示只返回路径中最后一个路径元素的父目录 inode，
 * 并将该元素名称存储在 name 参数中。
 * 
 * 这对于文件创建、删除等操作常用，因为往往需要先定位父目录 inode，再针对最后一个文件名进行操作。
 * 
 * @param path 路径字符串，绝对路径或相对路径
 * @param name 用于存储路径最后一个路径元素的名称，大小至少为 DIRSIZ
 * 
 * @return 返回路径中最后一个元素的父目录 inode，失败返回 NULL
 */
struct inode*
nameiparent(char *path, char *name)
{
    return namex(path, 1, name);
}