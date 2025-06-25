#include "types.h"

/**
 * 固定内存块的级别,总共10个级别
 *  8,16,32,64,128,256,512,1024
 * 为了简单，最大只支持1024
 */
#define FIXED_CACHE_LEVEL_NUM 8

/*允许同时存在的custom_cache数量*/
#define CUSTOM_CACHE_LEVEL_NUM 10

struct object{
    struct object* next;
};

#define SLAB_MAGIC 0x534C414242414C53

struct slab{
    uint64 magic;
    struct slab *next;
    struct object *object; // 顺序链表，指向第一个空闲object
    uint32 size;
    uint32 free;
};

struct kmem_cache{
    // per_CPU_cache
    struct slab *free_slab; // 有空闲object的slab块 
    struct slab *full_slab; // object分配完了的slab块
    uint64 size; // 要分配对象的大小
};

enum slab_state {
	DOWN,			/* No slab functionality yet */
	PARTIAL,		/* SLUB: kmem_cache_node available */
	UP,			/* Slab caches usable but not all extras yet */
	FULL			/* Everything is working */
};

struct slab_allocator{
    struct kmem_cache *fixed_cache_list[FIXED_CACHE_LEVEL_NUM]; // 固定大小的kmem_cache
    struct kmem_cache *custom_cache_list[CUSTOM_CACHE_LEVEL_NUM]; // 自定义大小的kmem_cache
    enum slab_state state;
};

void *slab_alloc(uint64 size);
void slab_free(uint64 addr);
void slab_init();
void test_slab();

