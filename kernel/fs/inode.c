#include "types.h"
#include "fs_defs.h"
#include "defs.h"
#include "spinlock.h"
#include "inode.h"
#include "vfs_ext4_ext.h"

/**
 * @brief Inode表
 * 
 */
struct {
    struct spinlock lock;
    struct inode inode[NINODE];
} itable;

/**
 * @brief inode初始化函数
 * 
 */
void 
inodeinit(void)
{
    int i = 0;

    initlock(&itable.lock, "itable");
    for(i = 0; i < NINODE; i++) {
        initlock(&itable.inode[i].lock, "inode");
        itable.inode[i].i_op = get_ext4_inode_op();
    }
}

/**
 * @brief 从inode table Get the inode object
 * 
 * @return struct inode* 
 */
struct inode *get_inode() {
    int i;
    acquire(&itable.lock);
    for(i = 0; i < NINODE; i++) {
        if (itable.inode[i].i_valid == 0) {
            itable.inode[i].i_valid = 1;
            break;
        }
    }
    release(&itable.lock);
    if (i == NINODE) {
        return NULL;
    }
    return &itable.inode[i];
}
/**
 * @brief 在inode table标记无效
 * 
 * @param inode 
 */
void free_inode(struct inode *inode) {
    acquire(&itable.lock);
    inode->i_valid = 0;
    release(&itable.lock);
}
































