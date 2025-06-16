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
#include "cpu.h"

/**
 * @brief 支持的文件系统
 * 
 */
filesystem_t EXT4_FS;
filesystem_t VFAT_FS;

/**
 * @brief 文件系统表
 * 
 */
filesystem_t* fs_table[VFS_MAX_FS];

/** 
 * @brief 文件系统操作函数表
 * 
 */
filesystem_op_t *fs_op_table[VFS_MAX_FS];

/** 
 * @brief 文件系统表锁
 * 
 * 该结构体用于保护对文件系统表的访问。
 */
struct spinlock fs_table_lock;

/**
 * @brief 初始化文件系统与函数
 * 
 */
void 
init_fs(void) 
{
    initlock(&fs_table_lock, "fs_table_lock");
    for (int i = 0; i < VFS_MAX_FS; i++) 
    {
        fs_table[i] = NULL;
        fs_op_table[i] = NULL;
    }
    
    fs_table[EXT4] = &EXT4_FS;
    fs_table[VFAT] = &VFAT_FS;
    
    fs_op_table[EXT4] = &EXT4_FS_OP;
    fs_op_table[VFAT] = &VFAT_FS_OP;
#if DEBUG
    LOG_LEVEL(LOG_INFO, "init_fs finished\n");
#endif
}

/**
 * @brief 注册文件系统
 * 
 * @param fs 
 * @param dev 
 * @param fs_type 
 * @param path 
 */
void 
fs_register(int dev, fs_t fs_type, const char *path) 
{
    acquire(&fs_table_lock);
    filesystem_t* fs = get_fs_by_type(fs_type);
    fs->dev = dev;
    fs->type = fs_type;
    fs->path = path; /* path should be a string literal */
    fs->fs_op = fs_op_table[fs_type];
    release(&fs_table_lock);
#if DEBUG
    LOG_LEVEL(LOG_INFO, "fs_register OK!\n");
#endif
}

/**
 * @brief 挂载文件系统
 * 
 * @param fs 文件系统
 * @param dev 设备号
 * @param fs_type 文件系统类型 
 * @param path 挂载路径
 * @param rwflag 读写权限
 * @param data 额外数据
 * @return int 成功0，失败-1
 */
int 
fs_mount(int dev, fs_t fs_type, 
        const char *path, uint64 rwflag, const void *data) 
{
    fs_register(dev, fs_type, path);
    filesystem_t* fs = get_fs_by_type(fs_type);
    if (fs -> fs_op -> mount) 
    {
        int ret = fs -> fs_op -> mount(fs, rwflag, data);
#if DEBUG
        LOG_LEVEL(LOG_INFO, "fs_mount done: %d\n", ret);
#endif
        return ret;
    }
    return -1;
}

/**
 * @brief 卸载fs文件系统
 * 
 * @param fs 要卸载的文件系统
 * @return int 成功0，失败-1
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
get_fs_by_type(fs_t type) 
{
    return fs_table[type];
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
        if (fs_table[i] && fs_table[i]->path && !strcmp(fs_table[i]->path, mp)) 
        {
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
    strcat(abs_path, "/");

    size_t len = strlen(abs_path);
    char *pstart = abs_path, *pend = abs_path + len - 1;
    
    /* 
     * 找到'/'结尾的字符串
     * 判断这个字符串是不是文件系统挂载点
     * 不是的话接着找，直到找到为止或者到达'/'
     */
    while (pend > pstart) {
        if (*pend == '/') {
            *pend = '\0';
            filesystem_t *fs = get_fs_by_mount_point(pstart);
            if (fs) {
                return fs;
            }
        }
        pend--;
    }

    if (pend == pstart) 
    {
        return get_fs_by_mount_point("/");
    }

    return NULL;
}

void 
dir_init(void)
{
    if (namei("/dev/null") == NULL)
        vfs_ext4_mknod("/dev/null", T_CHR, DEVNULL);
    
    if (namei("/proc") == NULL)
        vfs_ext4_mkdir("/proc", 0777);
    
    if (namei("/proc/mounts") == NULL)
        vfs_ext4_mkdir("/proc/mounts", 0777);

    if (namei("/proc/mounts") == NULL)
        vfs_ext4_mkdir("/proc/mounts", 0777);

    if (namei("/proc/meminfo") == NULL)
        vfs_ext4_mkdir("/proc/meminfo", 0777);
    
    if (namei("/dev/misc/rtc") == NULL)
        vfs_ext4_mkdir("/dev/misc/rtc", 0777);

    if (namei("/dev/zero") == NULL)
        vfs_ext4_mknod("/dev/zero", T_CHR, DEVZERO);
}