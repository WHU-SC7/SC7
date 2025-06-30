# SC7文件系统

## 概述

SC7文件系统模块实现了完整的文件系统功能，支持EXT4和VFAT两种文件系统，通过虚拟文件系统（VFS）提供统一的文件操作接口。

## 文件系统架构

### 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                        用户空间                              │
│                    (应用程序、用户库)                        │
├─────────────────────────────────────────────────────────────┤
│                        系统调用接口                          │
│                    (open, read, write, close)               │
├─────────────────────────────────────────────────────────────┤
│                        虚拟文件系统 (VFS)                    │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐           │
│  │   文件操作   │ │   目录操作   │ │   挂载管理   │           │
│  └─────────────┘ └─────────────┘ └─────────────┘           │
├─────────────────────────────────────────────────────────────┤
│                    具体文件系统实现                          │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐           │
│  │   EXT4 FS   │ │   VFAT FS   │ │   管道 FS   │           │
│  └─────────────┘ └─────────────┘ └─────────────┘           │
├─────────────────────────────────────────────────────────────┤
│                        块设备层                              │
│                    (磁盘、分区管理)                          │
└─────────────────────────────────────────────────────────────┘
```

### 核心数据结构

#### 1. 文件描述符 (file)
```c
struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
    int ref;           // 引用计数
    char readable;     // 可读标志
    char writable;     // 可写标志
    struct pipe *pipe; // 管道指针
    struct inode *ip;  // inode指针
    uint off;          // 文件偏移
    short major;       // 主设备号
};
```

#### 2. inode结构
```c
struct inode {
    uint dev;          // 设备号
    uint inum;         // inode号
    int ref;           // 引用计数
    struct sleeplock lock; // 睡眠锁
    int valid;         // 有效标志
    short type;        // 文件类型
    short major;       // 主设备号
    short minor;       // 次设备号
    short nlink;       // 硬链接数
    uint size;         // 文件大小
    uint addrs[NDIRECT+1]; // 数据块地址
};
```

#### 3. 目录项结构
```c
struct dirent {
    ushort inum;       // inode号
    char name[DIRSIZ]; // 文件名
};
```

## 虚拟文件系统 (VFS)

### VFS设计目标

1. **统一接口**: 为不同文件系统提供统一的接口
2. **可扩展性**: 易于添加新的文件系统支持
3. **性能优化**: 提供缓存和优化机制
4. **兼容性**: 支持多种文件系统格式

### VFS核心功能

#### 1. 文件操作接口
- `fileopen()`: 打开文件
- `fileread()`: 读取文件
- `filewrite()`: 写入文件
- `fileclose()`: 关闭文件
- `fileseek()`: 文件定位

#### 2. 目录操作接口
- `dirlink()`: 创建硬链接
- `dirunlink()`: 删除文件
- `dirlookup()`: 查找目录项
- `dirlink()`: 创建目录项

#### 3. 挂载管理
- `mount()`: 挂载文件系统
- `unmount()`: 卸载文件系统
- `namei()`: 路径名解析

## EXT4文件系统

### EXT4特性

1. **日志功能**: 支持日志记录，提高文件系统可靠性
2. **扩展性**: 支持大文件和大量文件
3. **性能优化**: 延迟分配、多块分配等优化
4. **兼容性**: 向后兼容EXT2/EXT3

### EXT4数据结构

#### 1. 超级块 (Superblock)
```c
struct ext4_super_block {
    uint32_t s_inodes_count;      // inode总数
    uint32_t s_blocks_count_lo;   // 块总数
    uint32_t s_r_blocks_count_lo; // 保留块数
    uint32_t s_free_blocks_count_lo; // 空闲块数
    uint32_t s_free_inodes_count; // 空闲inode数
    uint32_t s_first_data_block;  // 第一个数据块
    uint32_t s_log_block_size;    // 块大小对数
    uint32_t s_log_cluster_size;  // 簇大小对数
    uint32_t s_blocks_per_group;  // 每组块数
    uint32_t s_clusters_per_group; // 每组簇数
    uint32_t s_inodes_per_group;  // 每组inode数
    uint32_t s_mtime;             // 最后挂载时间
    uint32_t s_wtime;             // 最后写入时间
    uint16_t s_mnt_count;         // 挂载次数
    uint16_t s_max_mnt_count;     // 最大挂载次数
    uint16_t s_magic;             // 魔数
    uint16_t s_state;             // 文件系统状态
    uint16_t s_errors;            // 错误处理方式
    uint16_t s_minor_rev_level;   // 次版本号
    uint32_t s_lastcheck;         // 最后检查时间
    uint32_t s_checkinterval;     // 检查间隔
    uint32_t s_creator_os;        // 创建操作系统
    uint32_t s_rev_level;         // 版本号
    uint16_t s_def_resuid;        // 默认保留用户ID
    uint16_t s_def_resgid;        // 默认保留组ID
    uint32_t s_first_ino;         // 第一个非保留inode
    uint16_t s_inode_size;        // inode大小
    uint16_t s_block_group_nr;    // 块组号
    uint32_t s_feature_compat;    // 兼容特性
    uint32_t s_feature_incompat;  // 不兼容特性
    uint32_t s_feature_ro_compat; // 只读兼容特性
    uint8_t s_uuid[16];           // 文件系统UUID
    char s_volume_name[16];       // 卷名
    char s_last_mounted[64];      // 最后挂载点
    uint32_t s_journal_uuid[4];   // 日志UUID
};
```

#### 2. 块组描述符
```c
struct ext4_group_desc {
    uint32_t bg_block_bitmap_lo;  // 块位图块号
    uint32_t bg_inode_bitmap_lo;  // inode位图块号
    uint32_t bg_inode_table_lo;   // inode表起始块号
    uint16_t bg_free_blocks_count_lo; // 空闲块数
    uint16_t bg_free_inodes_count_lo; // 空闲inode数
    uint16_t bg_used_dirs_count_lo;   // 使用目录数
    uint16_t bg_pad;              // 填充
    uint32_t bg_reserved[3];      // 保留字段
};
```

#### 3. inode结构
```c
struct ext4_inode {
    uint16_t i_mode;              // 文件模式
    uint16_t i_uid;               // 用户ID
    uint32_t i_size_lo;           // 文件大小低32位
    uint32_t i_atime;             // 访问时间
    uint32_t i_ctime;             // 创建时间
    uint32_t i_mtime;             // 修改时间
    uint32_t i_dtime;             // 删除时间
    uint16_t i_gid;               // 组ID
    uint16_t i_links_count;       // 硬链接数
    uint32_t i_blocks_lo;         // 块数
    uint32_t i_flags;             // 文件标志
    uint32_t i_block[EXT4_N_BLOCKS]; // 块指针数组
    uint32_t i_generation;        // 文件版本
    uint32_t i_file_acl_lo;       // 文件ACL
    uint32_t i_size_high;         // 文件大小高32位
    uint32_t i_obso_faddr;        // 废弃字段
    uint16_t i_obso_frag;         // 废弃字段
    uint16_t i_obso_fsize;        // 废弃字段
    uint16_t i_pad1;              // 填充
    uint16_t i_reserved2[2];      // 保留字段
    uint32_t i_reserved2[2];      // 保留字段
};
```

### EXT4核心功能

#### 1. 文件创建
```c
// 创建新文件
int ext4_create(struct inode *dir, char *name, int mode, struct inode **ip)
{
    // 1. 分配新的inode
    // 2. 初始化inode结构
    // 3. 在目录中创建目录项
    // 4. 更新目录inode
    // 5. 返回新inode
}
```

#### 2. 文件读写
```c
// 读取文件数据
int ext4_read(struct inode *ip, char *dst, uint off, uint n)
{
    // 1. 检查文件偏移和大小
    // 2. 计算需要读取的块
    // 3. 从磁盘读取数据块
    // 4. 复制数据到用户缓冲区
    // 5. 更新文件偏移
}

// 写入文件数据
int ext4_write(struct inode *ip, char *src, uint off, uint n)
{
    // 1. 检查文件偏移和权限
    // 2. 分配新的数据块（如果需要）
    // 3. 将数据写入磁盘
    // 4. 更新inode信息
    // 5. 更新文件大小
}
```

#### 3. 目录操作
```c
// 查找目录项
struct inode* ext4_lookup(struct inode *dir, char *name, uint *poff)
{
    // 1. 读取目录内容
    // 2. 遍历目录项
    // 3. 匹配文件名
    // 4. 返回对应的inode
}

// 创建目录项
int ext4_link(struct inode *dir, char *name, struct inode *ip)
{
    // 1. 检查目录权限
    // 2. 分配目录项空间
    // 3. 写入目录项信息
    // 4. 更新目录inode
}
```

## VFAT文件系统

### VFAT特性

1. **兼容性**: 与FAT文件系统兼容
2. **简单性**: 结构简单，易于实现
3. **广泛支持**: 被多种操作系统支持
4. **限制**: 文件大小和数量有限制

### VFAT数据结构

#### 1. 引导扇区 (Boot Sector)
```c
struct fat_boot_sector {
    uint8_t bs_jmp_boot[3];       // 跳转指令
    uint8_t bs_oem_name[8];       // OEM名称
    uint16_t bpb_bytes_per_sec;   // 每扇区字节数
    uint8_t bpb_sec_per_clus;     // 每簇扇区数
    uint16_t bpb_rsvd_sec_cnt;    // 保留扇区数
    uint8_t bpb_num_fats;         // FAT表数量
    uint16_t bpb_root_ent_cnt;    // 根目录项数
    uint16_t bpb_tot_sec16;       // 总扇区数（16位）
    uint8_t bpb_media;            // 媒体描述符
    uint16_t bpb_fat_sz16;        // FAT表大小（16位）
    uint16_t bpb_sec_per_trk;     // 每磁道扇区数
    uint16_t bpb_num_heads;       // 磁头数
    uint32_t bpb_hidd_sec;        // 隐藏扇区数
    uint32_t bpb_tot_sec32;       // 总扇区数（32位）
    uint32_t bpb_fat_sz32;        // FAT表大小（32位）
    uint16_t bpb_ext_flags;       // 扩展标志
    uint16_t bpb_fs_ver;          // 文件系统版本
    uint32_t bpb_root_clus;       // 根目录起始簇
    uint16_t bpb_fs_info;         // 文件系统信息扇区
    uint16_t bpb_bk_boot_sec;     // 备份引导扇区
    uint8_t bpb_reserved[12];     // 保留字段
    uint8_t bs_drv_num;           // 驱动器号
    uint8_t bs_reserved1;         // 保留字段
    uint8_t bs_boot_sig;          // 引导签名
    uint32_t bs_vol_id;           // 卷序列号
    uint8_t bs_vol_lab[11];       // 卷标
    uint8_t bs_fil_sys_type[8];   // 文件系统类型
};
```

#### 2. 目录项 (Directory Entry)
```c
struct fat_dir_entry {
    uint8_t name[11];             // 文件名（8.3格式）
    uint8_t attr;                 // 文件属性
    uint8_t nt_res;               // NT保留
    uint8_t crt_time_tenth;       // 创建时间（10毫秒）
    uint16_t crt_time;            // 创建时间
    uint16_t crt_date;            // 创建日期
    uint16_t lst_acc_date;        // 最后访问日期
    uint16_t fst_clus_hi;         // 起始簇号高16位
    uint16_t wrt_time;            // 最后写入时间
    uint16_t wrt_date;            // 最后写入日期
    uint16_t fst_clus_lo;         // 起始簇号低16位
    uint32_t file_size;           // 文件大小
};
```

### VFAT核心功能

#### 1. 文件查找
```c
// 在目录中查找文件
struct fat_dir_entry* fat_find_file(struct fat_dir_entry *dir, char *name)
{
    // 1. 遍历目录项
    // 2. 比较文件名
    // 3. 返回匹配的目录项
}
```

#### 2. 簇链管理
```c
// 读取簇链
int fat_read_cluster_chain(uint32_t start_cluster, char *buffer, uint size)
{
    // 1. 从起始簇开始
    // 2. 读取FAT表获取下一簇
    // 3. 读取簇数据
    // 4. 继续直到文件结束
}

// 分配新簇
uint32_t fat_alloc_cluster()
{
    // 1. 扫描FAT表
    // 2. 找到空闲簇
    // 3. 标记为已使用
    // 4. 返回簇号
}
```

## 文件系统缓存

### 缓存机制

#### 1. 块缓存
```c
struct buf {
    int valid;          // 有效标志
    int disk;           // 磁盘标志
    uint dev;           // 设备号
    uint blockno;       // 块号
    struct sleeplock lock; // 睡眠锁
    uint refcnt;        // 引用计数
    struct buf *prev;   // LRU链表前驱
    struct buf *next;   // LRU链表后继
    uchar data[BSIZE];  // 数据缓冲区
};
```

#### 2. 缓存操作
```c
// 获取缓存块
struct buf* bread(uint dev, uint blockno)
{
    // 1. 查找缓存
    // 2. 如果未命中，从磁盘读取
    // 3. 返回缓存块
}

// 写入缓存块
void bwrite(struct buf *b)
{
    // 1. 标记为脏
    // 2. 写入磁盘
    // 3. 清除脏标志
}
```

### 缓存策略

1. **LRU替换**: 最近最少使用替换策略
2. **写回策略**: 延迟写入磁盘
3. **预读机制**: 预读相关数据块
4. **缓存一致性**: 保证缓存数据一致性

## 文件系统挂载

### 挂载机制

#### 1. 挂载点管理
```c
struct mount {
    struct inode *root;    // 根inode
    struct inode *mnt;     // 挂载点inode
    char path[256];        // 挂载路径
};
```

#### 2. 挂载操作
```c
// 挂载文件系统
int mount(char *path, char *device, char *fstype)
{
    // 1. 解析挂载路径
    // 2. 打开设备文件
    // 3. 读取文件系统信息
    // 4. 创建挂载点
    // 5. 建立挂载关系
}
```

## 性能优化

### 1. 缓存优化
- **块缓存**: 减少磁盘I/O操作
- **inode缓存**: 缓存inode信息
- **目录缓存**: 缓存目录项信息

### 2. 预读机制
- **顺序预读**: 预读连续数据块
- **目录预读**: 预读目录内容
- **元数据预读**: 预读文件系统元数据

### 3. 写优化
- **延迟写入**: 批量写入操作
- **异步I/O**: 非阻塞I/O操作
- **写合并**: 合并相邻写操作

## 错误处理

### 1. 文件系统错误
- **损坏检测**: 检测文件系统损坏
- **恢复机制**: 自动恢复损坏数据
- **日志记录**: 记录错误信息

### 2. 权限检查
- **访问权限**: 检查文件访问权限
- **所有权**: 检查文件所有权
- **安全策略**: 实施安全策略

### 3. 资源管理
- **空间检查**: 检查磁盘空间
- **配额管理**: 用户配额管理
- **资源限制**: 限制资源使用

## 总结

SC7文件系统模块实现了完整的文件系统功能，通过VFS提供统一的接口，支持EXT4和VFAT两种文件系统。系统具备良好的性能、可靠性和可扩展性，为应用程序提供了完整的文件操作支持。

主要特点：
- **模块化设计**: 清晰的层次结构
- **多文件系统支持**: EXT4和VFAT
- **性能优化**: 缓存和预读机制
- **错误处理**: 完善的错误处理机制
- **可扩展性**: 易于添加新文件系统 