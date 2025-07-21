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
    struct vma *prev;
    struct vma *next;
    struct shmid_kernel *shm_kernel; ///< 指向共享内存段，仅当type为SHARE时有效
};

#define SHMMNI 50
#define IPC_PRIVATE 0 //key,强制创建新的共享内存段,且该段无法通过其他进程直接复用
#define IPC_CREAT	0x200 //flag，如果不存在则创建共享内存段。
#define VM_SHARE_MEMORY_REGION 0x60000000 // 共享内存从这里开始分配
extern int sharemem_start;
struct shmid_kernel
{
    int shmid; //共享内存段标识符
    uint64 size;
    int flag;
    pte_t   *shm_pages;  // 共享内存映射的虚拟内存页表项数组
    struct vma *attaches;
};

extern struct shmid_kernel *shm_segs[SHMMNI]; //系统全局的共享内存数组
extern int shmid;


struct vma *vma_init(struct proc *p);
uint64 alloc_vma_stack(struct proc *p);
int get_mmapperms(int prot);
uint64 experm(pgtbl_t pagetable, uint64 va, uint64 perm);
uint64 get_proc_sp(struct proc *p);
struct vma *vma_copy(struct proc *np, struct vma *head);
int vma_map(pgtbl_t old, pgtbl_t new, struct vma *vma);
int free_vma_list(struct proc *p);
int free_vma(struct proc *p, uint64 start, uint64 end);
uint64 mmap(uint64 start, int64 len, int prot, int flags, int fd, int offset);
int munmap(uint64 start, int len);
int get_mmapperms(int prot);
struct vma *find_mmap_vma(struct vma *head);
struct vma *alloc_mmap_vma(struct proc *p, int flags, uint64 start, int64 len, int perm, int fd, int offset);
int vm_protect(pgtbl_t pagetable, uint64 va, uint64 addr, uint64 perm);
struct vma *alloc_vma(struct proc *p, enum segtype type, uint64 addr, int64 sz, int perm, int alloc, uint64 pa);
int handle_cow_write(proc_t *p, uint64 va);


int newseg(int key, int shmflg, int size);
void sync_shared_memory(struct shmid_kernel *shp);

// msync flags
#define MS_ASYNC      1  // 异步同步
#define MS_SYNC       2  // 同步同步
#define MS_INVALIDATE 4  // 使缓存无效

int msync(uint64 addr, uint64 len, int flags);

#endif