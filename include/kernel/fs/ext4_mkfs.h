


/**
 * @file  ext4_mkfs.h
 * @brief
 */

#ifndef EXT4_MKFS_H_
#define EXT4_MKFS_H_


#include "ext4_config.h"
#include "ext4_types.h"

#include "ext4_blockdev.h"
#include "ext4_fs.h"

#include "types.h"

struct ext4_mkfs_info {
    uint64_t len;
    uint32_t block_size;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t inode_size;
    uint32_t inodes;
    uint32_t journal_blocks;
    uint32_t feat_ro_compat;
    uint32_t feat_compat;
    uint32_t feat_incompat;
    uint32_t bg_desc_reserve_blocks;
    uint16_t dsc_size;
    uint8_t uuid[UUID_SIZE];
    bool journal;
    const char *label;
};


int ext4_mkfs_read_info(struct ext4_blockdev *bd, struct ext4_mkfs_info *info);

int ext4_mkfs(struct ext4_fs *fs, struct ext4_blockdev *bd, struct ext4_mkfs_info *info, int fs_type);


#endif /* EXT4_MKFS_H_ */

/**
 * @}
 */
