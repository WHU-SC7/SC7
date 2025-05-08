#include "fs.h"

#include "defs.h"

#include "types.h"
#ifdef RISCV
#include "riscv.h"
#else
#include "loongarch.h"
#endif
#include "fs_defs.h"
#include "vfs_ext4.h"
#include "vfs_vfat.h"
#include "string.h"
#include "defs.h"
#include "ops.h"
#include "cpu.h"

filesystem_t *fs_table[VFS_MAX_FS];
filesystem_op_t *fs_ops_table[VFS_MAX_FS] = 
{
    NULL,
    &vfat_fs_op,
    &ext4_fs_op,
    NULL,
};

filesystem_t ext4_fs;
filesystem_t vfat_fs;

// filesystem_t root_fs; // 仅用来加载init程序

struct spinlock fs_table_lock;

/**
 * @brief 初始化文件系统表
 * 
 */
void 
init_fs_table(void) 
{
    initlock(&fs_table_lock, "fs_table_lock");
    for (int i = 0; i < VFS_MAX_FS; i++) {
        fs_table[i] = NULL;
    }
    printf("init_fs_table finished\n");
}

/**
 * @brief 初始化文件系统，仅赋值，未实际挂载
 * 
 * @param fs 文件系统
 * @param dev 文件系统对应的设备
 * @param fs_type 文件系统类型
 * @param path 挂载路径
 */
void 
fs_init(filesystem_t *fs, int dev, fs_t fs_type, const char *path) {
    acquire(&fs_table_lock);
    fs_table[fs_type] = fs;
    fs->dev = dev;
    fs->type = fs_type;
    fs->path = path; /* path should be a string literal */
    fs->fs_op = fs_ops_table[fs_type];
    release(&fs_table_lock);
    printf("fs_init done\n");
}

/**
 * @brief 初始化文件系统，实际挂载
 * 
 * @param fs 文件系统
 * @param dev 文件系统对应的设备
 * @param fs_type 文件系统类型
 * @param path 挂载路径
 */
void 
filesystem_init(void) {
    fs_init(&ext4_fs, ROOTDEV, EXT4, "/"); // 应为"/"
    void *fs_sb = NULL;
    fs_mount(&ext4_fs, 0, fs_sb);

    // int r = ext4_cache_write_back("/", true);
    // if (r != EOK) {
    //     panic("ext4_cache write back error!\n");
    // }

    struct file_vnode *cwd = &(myproc()->cwd);
    strcpy(cwd->path, "/");
    cwd->fs = &ext4_fs;
}

// void filesystem2_init(void) {
//     acquire(&fs_table_lock);
//     fs_table[3] = &root_fs;
//     root_fs.dev = 2;
//     root_fs.type = EXT4;
//     root_fs.fs_op = fs_ops_table[root_fs.type];
//     root_fs.path = "/";
//     release(&fs_table_lock);
//     int ret = vfs_ext_mount2(&root_fs, 0, NULL);
//     printf("fs_mount done: %d\n", ret);

// }

/**
 * @brief 挂载文件系统，init中调用
 * 
 * @param fs 文件系统
 * @param rwflag 指示挂载的读写权限标志，0表示默认挂载(读写)，1为只读挂载
 * @param data 传递挂载操作的额外参数

 * @return int 状态码，0成功，-1失败
 */
int 
fs_mount(filesystem_t *fs, uint64 rwflag, void *data) {
    if (fs -> fs_op -> mount) 
    {
        int ret = fs -> fs_op -> mount(fs, rwflag, data);
        printf("fs_mount done: %d\n", ret);
        return ret;
    }
    return -1;
}

/**
 * TODO: not implemented yet
 */
int fs_umount(filesystem_t *fs) 
{  
    return fs->fs_op->umount(fs);
}

/**
 * @brief Get the fs by type object
 * 
 * @param type 文件系统类型
 * @return filesystem_t* 
 */
filesystem_t *
get_fs_by_type(fs_t type) {
    if (fs_table[type]) {
        return fs_table[type];
    }
    return NULL;
}

/**
 * @brief Get the fs by mount point object
 * 
 * @param mp mount point
 * @return filesystem_t* 
 */
filesystem_t *
get_fs_by_mount_point(const char *mp) 
{
    for (int i = 0; i < VFS_MAX_FS; i++) {
        if (fs_table[i] && fs_table[i]->path && !strcmp(fs_table[i]->path, mp)) {
            return fs_table[i];
        }
    }
    return NULL;
}

/**
 * @brief Get the fs from path object
 * 
 * @param path 理论上相对路径和绝对路径都行
 * @return struct filesystem* 
 */
struct filesystem *
get_fs_from_path(const char *path) {
    char abs_path[MAXPATH] = {0};
    get_absolute_path(path, myproc()->cwd.path, abs_path);

    size_t len = strlen(abs_path);
    char *pstart = abs_path, *pend = abs_path + len - 1;
    /*
     * FIXME: 现在逻辑是'./mnt'也会返回ext4，这显然不是我们想要的，
     * FIXME: 所以我先干掉了判断 '/' 结尾的逻辑(他的目的可能是确保是目录而非文件)
     * FIXME: 可能要修改get_absolute_path函数或者是这里的逻辑
     * 
     * 找到'/'结尾的字符串
     * 判断这个字符串是不是文件系统挂载点
     * 不是的话接着找，直到找到为止或者到达'/'
     */
    while (pend > pstart) {
        // if (*pend == '/') {
        //     *pend = '\0';
            filesystem_t *fs = get_fs_by_mount_point(pstart);
            if (fs) {
                return fs;
            }
        // }
        pend--;
    }

    if (pend == pstart) 
    {
        return get_fs_by_mount_point("/");
    }

    return NULL;
}











