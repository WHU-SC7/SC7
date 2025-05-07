#ifndef __FS_DEFS_H__
#define __FS_DEFS_H__

#define NOFILE       128                ///< open files per process
#define NFILE        100                ///< open files per system
#define NINODE       50                 ///< maximum number of active i-nodes
#define NDEV         10                 ///< maximum major device number
#define ROOTDEV       1                 ///< device number of file system root disk
#define MAXARG       32                 ///< max exec arguments
#define MAXOPBLOCKS  1000               ///< max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS * 3)  ///< max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS * 3)  ///< size of disk block cache
#define FSSIZE       1000               ///< size of file system in blocks
#define MAXPATH      260                ///< maximum file path name
#define VFS_MAX_FS   4                  ///< VFS 中最多的fs个数
#define TMPDEV       2                  ///< NOTE 用于挂载的临时设备号
#endif ///< __FS_DEFS_H__