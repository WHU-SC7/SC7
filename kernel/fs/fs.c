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
filesystem_t *fs_table[VFS_MAX_FS];

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
void init_fs(void)
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
void fs_register(int dev, fs_t fs_type, const char *path)
{
    acquire(&fs_table_lock);
    filesystem_t *fs = get_fs_by_type(fs_type);
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
int fs_mount(int dev, fs_t fs_type,
             const char *path, uint64 rwflag, const void *data)
{
    fs_register(dev, fs_type, path);
    filesystem_t *fs = get_fs_by_type(fs_type);
    if (fs->fs_op->mount)
    {
        int ret = fs->fs_op->mount(fs, rwflag, data);
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
    for (int i = 0; i < VFS_MAX_FS; i++)
    {
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
get_fs_from_path(const char *path)
{
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
    while (pend > pstart)
    {
        if (*pend == '/')
        {
            *pend = '\0';
            filesystem_t *fs = get_fs_by_mount_point(pstart);
            if (fs)
            {
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

void dir_init(void)
{
    struct inode *ip;
    if ((ip = namei("/dev/null")) == NULL)
        vfs_ext4_mknod("/dev/null", T_CHR, (DEVNULL << 8) | 0);
    else
        free_inode(ip);

    if ((ip = namei("/proc")) == NULL)
        vfs_ext4_mkdir("/proc", 0777);
    else
        free_inode(ip);

    if ((ip = namei("/proc/mounts")) == NULL)
        vfs_ext4_mkdir("/proc/mounts", 0777);
    else
        free_inode(ip);

    if ((ip = namei("/proc/mounts")) == NULL)
        vfs_ext4_mkdir("/proc/mounts", 0777);
    else
        free_inode(ip);

    if ((ip = namei("/bin/ls")) == NULL)
        create_file("/bin/ls", "/bin/ls", O_WRONLY | O_CREATE);
    else
        free_inode(ip);

    if ((ip = namei("/bin/ls")) == NULL)
        create_file("/bin/ls", "/bin/ls", O_WRONLY | O_CREATE);
    else
        free_inode(ip);

    if ((ip = namei("/proc/meminfo")) == NULL)
    {
        // 创建包含完整内存信息的文件
        create_file("/proc/meminfo",
                    "MemTotal:       1835008 kB\n"
                    "MemFree:         935008 kB\n"
                    "MemAvailable:    935008 kB\n"
                    "Buffers:              0 kB\n"
                    "Cached:               0 kB\n"
                    "SwapCached:           0 kB\n"
                    "Active:               0 kB\n"
                    "Inactive:             0 kB\n"
                    "Active(anon):         0 kB\n"
                    "Inactive(anon):       0 kB\n"
                    "Active(file):         0 kB\n"
                    "Inactive(file):       0 kB\n"
                    "Unevictable:          0 kB\n"
                    "Mlocked:              0 kB\n"
                    "SwapTotal:            0 kB\n"
                    "SwapFree:             0 kB\n"
                    "Dirty:                0 kB\n"
                    "Writeback:            0 kB\n"
                    "AnonPages:            0 kB\n"
                    "Mapped:               0 kB\n"
                    "Shmem:                0 kB\n"
                    "KReclaimable:         0 kB\n"
                    "Slab:                 0 kB\n"
                    "SReclaimable:         0 kB\n"
                    "SUnreclaim:           0 kB\n"
                    "KernelStack:          0 kB\n"
                    "PageTables:           0 kB\n"
                    "NFS_Unstable:         0 kB\n"
                    "Bounce:               0 kB\n"
                    "WritebackTmp:         0 kB\n"
                    "CommitLimit:          0 kB\n"
                    "Committed_AS:         0 kB\n"
                    "VmallocTotal:         0 kB\n"
                    "VmallocUsed:          0 kB\n"
                    "VmallocChunk:         0 kB\n"
                    "Percpu:               0 kB\n"
                    "HardwareCorrupted:    0 kB\n"
                    "AnonHugePages:        0 kB\n"
                    "ShmemHugePages:       0 kB\n"
                    "ShmemPmdMapped:       0 kB\n"
                    "FileHugePages:        0 kB\n"
                    "FilePmdMapped:        0 kB\n"
                    "CmaTotal:             0 kB\n"
                    "CmaFree:              0 kB\n"
                    "HugePages_Total:      0\n"
                    "HugePages_Free:       0\n"
                    "HugePages_Rsvd:       0\n"
                    "HugePages_Surp:       0\n"
                    "Hugepagesize:      2048 kB\n"
                    "Hugetlb:               0 kB\n"
                    "DirectMap4k:           0 kB\n"
                    "DirectMap2M:           0 kB\n"
                    "DirectMap1G:           0 kB\n",
                    O_WRONLY | O_CREATE);
    }
    else
        free_inode(ip);

    if ((ip = namei("/etc/passwd")) == NULL)
    {
        // 创建 /etc/passwd 文件，包含基本的用户信息
        create_file("/etc/passwd",
                    "root:x:0:0:root:/root:/bin/sh\n"
                    "nobody:x:65534:65534:nobody:/:/bin/false\n"
                    "daemon:x:1:1:daemon:/usr/sbin:/bin/false\n"
                    "bin:x:2:2:bin:/bin:/bin/false\n"
                    "sys:x:3:3:sys:/dev:/bin/false\n"
                    "sync:x:4:65534:sync:/bin:/bin/sync\n"
                    "games:x:5:60:games:/usr/games:/bin/false\n"
                    "man:x:6:12:man:/var/cache/man:/bin/false\n"
                    "lp:x:7:7:lp:/var/spool/lpd:/bin/false\n"
                    "mail:x:8:8:mail:/var/mail:/bin/false\n"
                    "news:x:9:9:news:/var/spool/news:/bin/false\n"
                    "uucp:x:10:10:uucp:/var/spool/uucp:/bin/false\n"
                    "proxy:x:13:13:proxy:/bin:/bin/false\n"
                    "www-data:x:33:33:www-data:/var/www:/bin/false\n"
                    "backup:x:34:34:backup:/var/backups:/bin/false\n"
                    "list:x:38:38:Mailing List Manager:/var/list:/bin/false\n"
                    "irc:x:39:39:ircd:/var/run/ircd:/bin/false\n"
                    "gnats:x:41:41:Gnats Bug-Reporting System (admin):/var/lib/gnats:/bin/false\n"
                    "_apt:x:100:65534::/nonexistent:/bin/false\n"
                    "systemd-timesync:x:101:102:systemd Time Synchronization,,,:/run/systemd:/bin/false\n"
                    "systemd-network:x:102:103:systemd Network Management,,,:/run/systemd/netif:/bin/false\n"
                    "systemd-resolve:x:103:104:systemd Resolver,,,:/run/systemd/resolve:/bin/false\n"
                    "messagebus:x:104:110::/nonexistent:/bin/false\n"
                    "sshd:x:105:65534::/run/sshd:/usr/sbin/nologin\n"
                    "systemd-coredump:x:999:999:systemd Core Dumper:/:/sbin/nologin\n",
                    O_WRONLY | O_CREATE);
    }
    else
        free_inode(ip);

    if ((ip = namei("/etc/group")) == NULL)
    {
        // 创建 /etc/group 文件，包含基本的组信息
        create_file("/etc/group",
                    "root:x:0:root\n"
                    "daemon:x:1:daemon\n"
                    "bin:x:2:bin\n"
                    "sys:x:3:sys\n"
                    "adm:x:4:adm\n"
                    "tty:x:5:\n"
                    "disk:x:6:root\n"
                    "lp:x:7:lp\n"
                    "mail:x:8:mail\n"
                    "news:x:9:news\n"
                    "uucp:x:10:uucp\n"
                    "man:x:12:man\n"
                    "proxy:x:13:proxy\n"
                    "kmem:x:15:\n"
                    "dialout:x:20:\n"
                    "fax:x:21:\n"
                    "voice:x:22:\n"
                    "cdrom:x:24:\n"
                    "floppy:x:25:\n"
                    "tape:x:26:\n"
                    "sudo:x:27:\n"
                    "audio:x:29:\n"
                    "dip:x:30:\n"
                    "www-data:x:33:www-data\n"
                    "backup:x:34:backup\n"
                    "operator:x:37:\n"
                    "list:x:38:\n"
                    "irc:x:39:irc\n"
                    "src:x:40:\n"
                    "gnats:x:41:gnats\n"
                    "shadow:x:42:\n"
                    "utmp:x:43:\n"
                    "video:x:44:\n"
                    "sasl:x:45:\n"
                    "plugdev:x:46:\n"
                    "staff:x:50:\n"
                    "games:x:60:games\n"
                    "users:x:100:\n"
                    "nogroup:x:65534:\n"
                    "systemd-timesync:x:101:\n"
                    "systemd-network:x:102:\n"
                    "systemd-resolve:x:103:\n"
                    "messagebus:x:104:\n"
                    "sshd:x:105:\n"
                    "systemd-coredump:x:999:\n",
                    O_WRONLY | O_CREATE);
    }
    else
        free_inode(ip);

    if ((ip = namei("/dev/misc/rtc")) == NULL)
        vfs_ext4_mkdir("/dev/misc/rtc", 0777);
    else
        free_inode(ip);

    if ((ip = namei("proc/self/exe")) == NULL)
        vfs_ext4_mkdir("proc/self/exe", 0777);
    else
        free_inode(ip);

    if ((ip = namei("/dev/zero")) == NULL)
        vfs_ext4_mknod("/dev/zero", T_CHR, (DEVZERO << 8) | 0);
    else
        free_inode(ip);

    if ((ip = namei("/tmp")) != NULL)
    {
        vfs_ext4_rm("tmp");
        free_inode(ip);
    }

    if ((ip = namei("/dev/shm")) != NULL)
    {
        vfs_ext4_rm("/dev/shm");
        free_inode(ip);
    }

    if ((ip = namei("/output.txt")) != NULL)
    {
        vfs_ext4_rm("output.txt");
        free_inode(ip);
    } // /glibc/.gitconfig.lock

    if ((ip = namei("/glibc/.gitconfig.lock")) != NULL)
    {
        vfs_ext4_rm("/glibc/.gitconfig.lock");
        free_inode(ip);
    }  

    if ((ip = namei("/usr")) == NULL)
        vfs_ext4_mkdir("/usr", 0777);
    else
        free_inode(ip);

    if ((ip = namei("/usr/lib")) == NULL)
        vfs_ext4_mkdir("/usr/lib", 0777);
    else
        free_inode(ip);

    if ((ip = namei("/bin")) == NULL)
        vfs_ext4_mkdir("/bin", 0777);
    else
        free_inode(ip);

    /* 创建一些 loop 设备文件 */
    if ((ip = namei("/dev/loop0")) == NULL)
    {
        for (int i = 0; i < 10; i++) ///< 只创建 10 个设备
        {
            char loop_path[64];
            uint32 dev_id = (DEVLOOP << 8) | i; ///< 主设备号在高8位，次设备号在低8位

            snprintf(loop_path, sizeof(loop_path), "/dev/loop%d", i);
            vfs_ext4_mknod(loop_path, T_BLK, dev_id);
        }
    }
    else
        free_inode(ip);

    /* 创建 /dev/block 目录，链接设备 */
    if ((ip = namei("/dev/block")) == NULL)
    {
        vfs_ext4_mkdir("/dev/block", 0755);
        for (int i = 0; i < 10; i++) ///< 只创建 10 个设备
        {
            char old_block_path[64], new_block_path[64];
            snprintf(new_block_path, sizeof(new_block_path), "/dev/block/%d:%d", DEVLOOP, i);
            snprintf(old_block_path, sizeof(old_block_path), "/dev/loop%d", i);
            ext4_fsymlink(old_block_path, new_block_path);
        }
    }
    else
        free_inode(ip);

    if ((ip = namei("/etc/gitconfig")) == NULL)
    {
        create_file("/etc/gitconfig",
                    "[user]\n"
                    "email = 115697417+Crzax@users.noreply.github.com\n"
                    "name = Crzax\n",
                    O_WRONLY | O_CREATE);
    }
    else
        free_inode(ip);
    
    // if ((ip = namei("/glibc/.gitconfig")) == NULL)
    // {
    //     create_file("/glibc/.gitconfig",
    //                 "[user]\n"
    //                 "email = 115697417+Crzax@users.noreply.github.com\n"
    //                 "name = Crzax\n",
    //                 O_WRONLY | O_CREATE);
    // }
    // else
    //     free_inode(ip); 
    
    // if ((ip = namei("/glibc/.config/git/config")) == NULL)
    // {
    //     create_file("/glibc/.config/git/config",
    //                 "[user]\n"
    //                 "email = 115697417+Crzax@users.noreply.github.com\n"
    //                 "name = Crzax\n",
    //                 O_WRONLY | O_CREATE);
    // }
    // else
    //     free_inode(ip); ///glibc/.git/config

    // if ((ip = namei("/glibc/.git/config")) == NULL)
    // {
    //     create_file("/glibc/.git/config",
    //                 "[user]\n"
    //                 "email = 115697417+Crzax@users.noreply.github.com\n"
    //                 "name = Crzax\n",
    //                 O_WRONLY | O_CREATE);
    // }
    // else
    //     free_inode(ip); ///glibc/.git/config
    
}