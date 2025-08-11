#ifndef __VMA_H__
#define __VMA_H__

#include "types.h"
#include "process.h"
#include "vmem.h"

// for mmap
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define PROT_GROWSDOWN 0X01000000
#define PROT_GROWSUP 0X02000000

#define MAP_FILE 0
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0X02
#define MAP_FIXED 0x10
#define MAP_SHARED_VALIDATE 0x03
#define MAP_ANONYMOUS 0x20
#define MAP_ALLOC  0xf00    //自定义，mmap时不使用懒加载
#define MAP_FAILED ((void *)-1)

#define USER_STACK_SIZE (100 * PGSIZE)

enum segtype
{
    NONE,
    MMAP,
    STACK,
    SHARE
};
struct proc;
struct vma
{
    enum segtype type;
    int perm;    ///< vma的权限
    int orig_prot; ///< 原始权限（用于PROT_NONE的动态转换）
    uint64 addr; ///< 起始地址
    uint64 size;
    uint64 end; ///< 结束地址
    int flags;  ///< vma进程的权限
    int fd;
    uint64 f_off;
    uint64 fsize; ///< 文件大小，用于文件映射
    struct vma *prev;
    struct vma *next;
    struct shmid_kernel *shm_kernel; ///< 指向共享内存段，仅当type为SHARE时有效
};

#define SHMMNI 500

#define IPC_PRIVATE 0 //key,强制创建新的共享内存段,且该段无法通过其他进程直接复用
#define IPC_CREAT	01000		/* Create key if key does not exist. */
#define IPC_EXCL	02000		/* Fail if key exists.  */
#define IPC_NOWAIT	04000		/* Return error on wait.  */

/* Control commands for `msgctl', `semctl', and `shmctl'.  */
#define IPC_RMID	0		/* Remove identifier.  */
#define IPC_SET		1		/* Set `ipc_perm' options.  */
#define IPC_STAT	2		/* Get `ipc_perm' options.  */
#define VM_SHARE_MEMORY_REGION 0x6000000 // 共享内存从这里开始分配

/* Permission flag for shmget.  */
#define SHM_R		0400		/* or S_IRUGO from <linux/stat.h> */
#define SHM_W		0200		/* or S_IWUGO from <linux/stat.h> */

/* Flags for `shmat'.  */
#define SHM_RDONLY	010000		/* attach read-only else read-write */
#define SHM_RND		020000		/* round attach address to SHMLBA */
#define SHM_REMAP	040000		/* take-over region on attach */
#define SHM_EXEC	0100000		/* execution access */

/* Commands for `shmctl'.  */
#define SHM_LOCK	11		/* lock segment (root only) */
#define SHM_UNLOCK	12		/* unlock segment (root only) */

// SHMLBA (Shared Memory Low Boundary Address) - 通常是页大小的倍数
#define SHMLBA		PGSIZE		/* attach addr a multiple of this */
extern int sharemem_start;



// IPC权限结构体
struct ipc_perm {
    int key;        // 共享内存段键值
    uint32 uid;        // 所有者用户ID
    uint32 gid;        // 所有者组ID
    uint32 cuid;       // 创建者用户ID
    uint32 cgid;       // 创建者组ID
    uint32 mode;       // 权限模式
    unsigned short int __seq;            /* Sequence number.  */
    unsigned short int __pad2;
    uint64 __glibc_reserved1;
    uint64 __glibc_reserved2;
};

// 共享内存段信息结构体
struct shmid_ds {
    struct ipc_perm shm_perm;  // 权限信息
    uint64 shm_segsz;          // 段大小
    uint64 shm_atime;          // 最后附加时间
    uint64 shm_dtime;          // 最后分离时间
    uint64 shm_ctime;          // 最后修改时间
    int shm_cpid;              // 创建者PID
    int shm_lpid;              // 最后操作的PID
    uint64 shm_nattch;            // 当前附加数
};

// 共享内存附加记录（每个进程）
struct shm_attach {
    uint64 addr;            // 附加的虚拟地址
    int shmid;              // 共享内存段ID
    struct shm_attach *next; // 下一个附加记录
};

struct shmid_kernel
{
    // 原有字段
    int shm_key;
    int shmid; //共享内存段标识符
    uint64 size;
    int flag;
    int attach_count;       // 当前附加进程数
    int is_deleted;         // 删除标记 (1=已标记删除)
    pte_t   *shm_pages;  // 共享内存映射的虚拟内存页表项数组
    struct vma *attaches;
    
    // 从 shmid_ds 结构体加入的字段
    struct ipc_perm shm_perm;  // 权限信息
    uint32 shm_segsz;          // 段大小
    uint64 shm_atime;          // 最后附加时间
    uint64 shm_dtime;          // 最后分离时间
    uint64 shm_ctime;          // 最后修改时间
    int shm_cpid;              // 创建者PID
    int shm_lpid;              // 最后操作的PID
    int shm_nattch;            // 当前附加数
    
    // 共享内存附加记录链表（每个进程）
    struct shm_attach *shm_attach_list; // 附加记录链表头
};

extern struct shmid_kernel *shm_segs[SHMMNI]; //系统全局的共享内存数组
extern int shmid;


struct vma *vma_init(struct proc *p);
uint64 alloc_vma_stack(struct proc *p);
uint64 get_mmapperms(int prot);
uint64 experm(pgtbl_t pagetable, uint64 va, uint64 perm);
uint64 get_proc_sp(struct proc *p);
struct vma *vma_copy(struct proc *np, struct vma *head);
int vma_map(pgtbl_t old, pgtbl_t new, struct vma *vma);
int free_vma_list(struct proc *p);
void shm_init();
int free_vma(struct proc *p, uint64 start, uint64 end);
uint64 mmap(uint64 start, int64 len, int prot, int flags, int fd, int offset);
int munmap(uint64 start, int len);
struct vma *find_mmap_vma(struct vma *head);
struct vma *alloc_mmap_vma(struct proc *p, int flags, uint64 start, int64 len, int perm, int fd, int offset);
int vm_protect(pgtbl_t pagetable, uint64 va, uint64 addr, uint64 perm);
struct vma *alloc_vma(struct proc *p, enum segtype type, uint64 addr, int64 sz, int perm, int alloc, uint64 pa);
int handle_cow_write(proc_t *p, uint64 va);
int findshm(int key);
int allocshmid();

int newseg(int key, int shmflg, int size);
void sync_shared_memory(struct shmid_kernel *shp);

// msync flags
#define MS_ASYNC      1  // 异步同步
#define MS_SYNC       2  // 同步同步
#define MS_INVALIDATE 4  // 使缓存无效

int msync(uint64 addr, uint64 len, int flags);

// 权限检查相关函数
int check_shm_permissions(struct shmid_kernel *shp, int requested_perms);
int is_root_user(void);
int has_shm_permission(struct shmid_kernel *shp, int perm);

#endif