/**
 * @file  ext4_journal.h
 * @brief Journal handle functions
 */

#ifndef EXT4_JOURNAL_H_
#define EXT4_JOURNAL_H_


#include "ext4_config.h"
#include "ext4_types.h"
#include "queue.h"
#include "tree.h"
#include "ext4_fs.h"

/**
 * @brief 用于表示文件系统的日志块设备和相关信息
 */
struct jbd_fs {
    struct ext4_blockdev *bdev; ///< 指向块设备的指针，用于访问底层存储
    struct ext4_inode_ref inode_ref; ///< 目录或文件的i-node引用，包含元数据
    struct jbd_sb sb; ///< 日志超级块，包含日志相关的元数据

    bool dirty; ///< 标记文件系统是否脏（即有未刷新的数据）
};

/**
 * @brief 用于表示日志中的缓冲区
 */
struct jbd_buf {
    uint32_t jbd_lba; ///< 日志块的逻辑块地址
    struct ext4_block block; ///< 实际的块数据
    struct jbd_trans *trans; ///< 指向当前事务的指针
    struct jbd_block_rec *block_rec; ///< 指向块记录的指针
    TAILQ_ENTRY(jbd_buf) buf_node; ///< 用于在缓冲区链表中链接的节点
    TAILQ_ENTRY(jbd_buf) dirty_buf_node; ///< 用于在脏缓冲区链表中链接的节点
};


/**
 * @brief 用于表示撤销记录，管理事务中的撤销信息
 */
struct jbd_revoke_rec {
    ext4_fsblk_t lba; ///< 被撤销的逻辑块地址
    RB_ENTRY(jbd_revoke_rec) revoke_node; ///< 用于在撤销树中链接的节点
};


/**
 * @brief 用于记录日志中的块信息
 */
struct jbd_block_rec {
    ext4_fsblk_t lba; ///< 日志中记录的逻辑块地址
    struct jbd_trans *trans; ///< 指向关联事务的指针
    RB_ENTRY(jbd_block_rec) block_rec_node; ///< 用于在块记录树中链接的节点
    LIST_ENTRY(jbd_block_rec) tbrec_node; ///< 用于在事务块记录列表中链接的节点
    TAILQ_HEAD(jbd_buf_dirty, jbd_buf) dirty_buf_queue; ///< 脏缓冲区队列
};

/**
 * @brief 表示一个日志事务，用于管理文件系统的元数据更新
 */
struct jbd_trans {
    uint32_t trans_id; ///< 事务的唯一标识符
    uint32_t start_iblock; ///< 事务开始时的块索引
    int alloc_blocks; ///< 分配的块数
    int data_cnt; ///< 数据块计数
    uint32_t data_csum; ///< 数据的校验和
    int written_cnt; ///< 已写入的块计数
    int error; ///< 事务中的错误标志

    struct jbd_journal *journal; ///< 指向相关日志的指针

    TAILQ_HEAD(jbd_trans_buf, jbd_buf) buf_queue; ///< 缓冲区队列
    RB_HEAD(jbd_revoke_tree, jbd_revoke_rec) revoke_root; ///< 撤销记录树
    LIST_HEAD(jbd_trans_block_rec, jbd_block_rec) tbrec_list; ///< 事务块记录列表
    TAILQ_ENTRY(jbd_trans) trans_node; ///< 用于在事务链表中链接的节点
};

/**
 * @brief 用于表示文件系统的日志，管理事务和块记录
 */
struct jbd_journal {
    uint32_t first; ///< 日志的第一个块索引
    uint32_t start; ///< 日志开始块索引
    uint32_t last; ///< 日志最后一个块索引
    uint32_t trans_id; ///< 当前日志的事务ID
    uint32_t alloc_trans_id; ///< 分配的事务ID

    uint32_t block_size; ///< 日志块的大小

    TAILQ_HEAD(jbd_cp_queue, jbd_trans) cp_queue; ///< 检查点队列
    RB_HEAD(jbd_block, jbd_block_rec) block_rec_root; ///< 块记录树的根节点

    struct jbd_fs *jbd_fs; ///< 指向关联的文件系统信息
};

int jbd_get_fs(struct ext4_fs *fs, struct jbd_fs *jbd_fs);
int jbd_put_fs(struct jbd_fs *jbd_fs);
int jbd_inode_bmap(struct jbd_fs *jbd_fs, ext4_lblk_t iblock, ext4_fsblk_t *fblock);
int jbd_recover(struct jbd_fs *jbd_fs);
int jbd_journal_start(struct jbd_fs *jbd_fs, struct jbd_journal *journal);
int jbd_journal_stop(struct jbd_journal *journal);
struct jbd_trans *jbd_journal_new_trans(struct jbd_journal *journal);
int jbd_trans_set_block_dirty(struct jbd_trans *trans, struct ext4_block *block);
int jbd_trans_revoke_block(struct jbd_trans *trans, ext4_fsblk_t lba);
int jbd_trans_try_revoke_block(struct jbd_trans *trans, ext4_fsblk_t lba);
void jbd_journal_free_trans(struct jbd_journal *journal, struct jbd_trans *trans, bool abort);
int jbd_journal_commit_trans(struct jbd_journal *journal, struct jbd_trans *trans);
void jbd_journal_purge_cp_trans(struct jbd_journal *journal, bool flush, bool once);


#endif /* EXT4_JOURNAL_H_ */

/**
 * @}
 */
