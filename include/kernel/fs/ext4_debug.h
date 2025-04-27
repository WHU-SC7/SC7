

/**
 * @file  ext4_debug.c
 * @brief Debug printf and assert macros.
 */

#ifndef EXT4_DEBUG_H_
#define EXT4_DEBUG_H_


#include "ext4_config.h"
#include "ext4_errno.h"

#if !CONFIG_HAVE_OWN_ASSERT
#include <assert.h>
#endif

#include <inttypes.h>
#include <types.h>

#ifndef PRIu64
#define PRIu64 "llu"
#endif

#ifndef PRId64
#define PRId64 "lld"
#endif


#define DEBUG_BALLOC (1ul << 0)
#define DEBUG_BCACHE (1ul << 1)
#define DEBUG_BITMAP (1ul << 2)
#define DEBUG_BLOCK_GROUP (1ul << 3)
#define DEBUG_BLOCKDEV (1ul << 4)
#define DEBUG_DIR_IDX (1ul << 5)
#define DEBUG_DIR (1ul << 6)
#define DEBUG_EXTENT (1ul << 7)
#define DEBUG_FS (1ul << 8)
#define DEBUG_HASH (1ul << 9)
#define DEBUG_IALLOC (1ul << 10)
#define DEBUG_INODE (1ul << 11)
#define DEBUG_SUPER (1ul << 12)
#define DEBUG_XATTR (1ul << 13)
#define DEBUG_MKFS (1ul << 14)
#define DEBUG_EXT4 (1ul << 15)
#define DEBUG_JBD (1ul << 16)
#define DEBUG_MBR (1ul << 17)

#define DEBUG_NOPREFIX (1ul << 31)
#define DEBUG_ALL (0xFFFFFFFF)

static inline const char *ext4_dmask_id2str(uint32_t m) {
    switch (m) {
        case DEBUG_BALLOC:
            return "ext4_balloc: ";
        case DEBUG_BCACHE:
            return "ext4_bcache: ";
        case DEBUG_BITMAP:
            return "ext4_bitmap: ";
        case DEBUG_BLOCK_GROUP:
            return "ext4_block_group: ";
        case DEBUG_BLOCKDEV:
            return "ext4_blockdev: ";
        case DEBUG_DIR_IDX:
            return "ext4_dir_idx: ";
        case DEBUG_DIR:
            return "ext4_dir: ";
        case DEBUG_EXTENT:
            return "ext4_extent: ";
        case DEBUG_FS:
            return "ext4_fs: ";
        case DEBUG_HASH:
            return "ext4_hash: ";
        case DEBUG_IALLOC:
            return "ext4_ialloc: ";
        case DEBUG_INODE:
            return "ext4_inode: ";
        case DEBUG_SUPER:
            return "ext4_super: ";
        case DEBUG_XATTR:
            return "ext4_xattr: ";
        case DEBUG_MKFS:
            return "ext4_mkfs: ";
        case DEBUG_JBD:
            return "ext4_jbd: ";
        case DEBUG_MBR:
            return "ext4_mbr: ";
        case DEBUG_EXT4:
            return "ext4: ";
    }
    return "";
}
#define DBG_NONE ""
#define DBG_INFO "[info]  "
#define DBG_WARN "[warn]  "
#define DBG_ERROR "[error] "

/**@brief   Global mask debug set.
 * @param   m new debug mask.*/
void ext4_dmask_set(uint32_t m);

/**@brief   Global mask debug clear.
 * @param   m new debug mask.*/
void ext4_dmask_clr(uint32_t m);

/**@brief   Global debug mask get.
 * @return  debug mask*/
uint32_t ext4_dmask_get(void);

#if CONFIG_DEBUG_PRINTF
#include "defs.h"
#include "print.h"
/**@brief   Debug printf.*/
#define ext4_dbg(m, ...)                                                                                               \
    do {                                                                                                               \
        if ((ext4_dmask_get()) & (m)) {                                                                                    \
            printf("%s", ext4_dmask_id2str(m));                                                                        \
            printf(__VA_ARGS__);                                                                                       \
        }                                                                                                              \
    } while (0)
#endif

#if CONFIG_DEBUG_ASSERT
#include "print.h"
#define ext4_assert(_v) ASSERT(_v)
#endif /* EXT4_DEBUG_H_ */

#endif /* EXT4_DEBUG_H_ */
/**
 * @}
 */
