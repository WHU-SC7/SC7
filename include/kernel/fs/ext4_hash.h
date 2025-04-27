

/**
 * @file  ext4_hash.h
 * @brief Directory indexing hash functions.
 */

#ifndef EXT4_HASH_H_
#define EXT4_HASH_H_


#include "ext4_config.h"

#include "types.h"

/**
 * @brief 用于存储哈希的相关信息
 * 
 */
struct ext4_hash_info {
    uint32_t hash;          ///< 哈希值
    uint32_t minor_hash;    ///< 次要哈希值
    uint32_t hash_version;  ///< 哈希版本
    
    /* 哈希种子,通常来自超级块(superblock), 用于初始化哈希计算 */
    const uint32_t *seed;
};              


/**@brief   Directory entry name hash function.
 * @param   name entry name
 * @param   len entry name length
 * @param   hash_seed (from superblock)
 * @param   hash_version version (from superblock)
 * @param   hash_minor output value
 * @param   hash_major output value
 * @return  standard error code*/
int ext2_htree_hash(const char *name, int len, const uint32_t *hash_seed, int hash_version, uint32_t *hash_major,
                    uint32_t *hash_minor);


#endif /* EXT4_HASH_H_ */

/**
 * @}
 */
