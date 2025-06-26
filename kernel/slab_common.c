
#include "types.h"
#include "slab.h"
#include "pmem.h"
#include "print.h"
#include "string.h"

struct slab_allocator *slab_allocator; ///< 管理slab

uint64 slab_size_table[FIXED_CACHE_LEVEL_NUM] = {
    8,
    16,
    32,
    64,
    128,
    256,
    512,
    1024
};

#define PAGE_SIZE 4096
int simple_alloc_count= PAGE_SIZE; ///< 标识boot_page使用了多少字节
void *slab_boot_page;

/**
 * @brief slab初始化使用,
 * @bug 未清零导致异常
 */
static void *simple_alloc(uint64 size)
{
    if(simple_alloc_count+size>PAGE_SIZE)
    {
        slab_boot_page = pmem_alloc_pages(1); //不可能这时候就没有内存了吧!
        simple_alloc_count=0; //新的页没有使用
    }

    char *alloc_ptr = (char *)slab_boot_page;
    alloc_ptr = alloc_ptr +simple_alloc_count; //之前这里还加了size，导致异常
    memset((void *)alloc_ptr,0,size);
    //LOG("[simple_alloc]alloc_ptr: %x\n",alloc_ptr);
    simple_alloc_count += size; //分配了size出去
    //LOG("分配了%d字节\n",size);
    return alloc_ptr;
}

/**
 * @brief 按给定size创建一个slab
 */
static struct slab* __create_slab_with_size(uint32 size)
{
    struct slab *slab = pmem_alloc_pages(1);
    struct object *object, *object_next;
        //slab链表尾插法
    slab->next = NULL;
    object = (struct object *)((char *)slab + sizeof(struct slab)); //第一个object的位置
    //printf("第一个object的位置: %x\n",object);
    slab->object = object;  
    
    int object_num = (PAGE_SIZE - sizeof(struct slab)) / size;
    /*把剩下的object_num-1个object插入slab链表*/
    for(int j=0;j<object_num-1;j++)
    {
        object_next = (struct object *)((char *)object + size);
        //printf("object链表元素: %x\n",object_next);
        object->next = object_next;
        object = object_next;
    }
    slab->magic = SLAB_MAGIC;
    slab->size = size;
    slab->free = object_num; ///< slab初始完毕
    return slab;
}

/**
 * @brief 初始化slab
 * 
 * slab初始化时，要分配空间给struct kmem_cache和slab_allocator，但是这个时候slab不能给自己分配小块内存(未初始化!)，
 * 要先使用simple_alloc为slab分配空间，之后就可以slab_alloc了
 */
void slab_init()
{
    slab_allocator = (struct slab_allocator *)simple_alloc(sizeof(struct slab_allocator));
    slab_allocator->state = DOWN;
    struct kmem_cache *c;

    for(int i=0;i<FIXED_CACHE_LEVEL_NUM;i++)
    {
        c = (struct kmem_cache *)simple_alloc(sizeof(struct kmem_cache));
        c->size = slab_size_table[i]; // 当前kmem_cache的object大小

        /*初始化时，给每个kmem_cache分配一个slab*/
        struct slab *slab = __create_slab_with_size(c->size);

        c->full_slab = NULL;
        c->free_slab = slab; ///< kmem_cache初始完毕
        slab_allocator->fixed_cache_list[i] = c; ///< 创建好的kmem_cache加入slab_allocator 
    }

    slab_allocator->state = FULL; //大概是完成了
    LOG_LEVEL(LOG_INFO,"slab初始化完成\n");
    
}

/**
 * @brief 创建给定对象大小的cache，也允许外界使用
 * @param object_size 对象的大小
 * @return 返回创建的kmem_cache地址
 * 
 * [todo]
 */
struct kmem_cache *kmem_cache_create(uint64 object_size)
{
    return 0;
}

/**
 * @brief 把size对齐到2的整数次幂
 */
static uint64 __slab_size(uint64 size)
{
    // 4096是2的12次方，预设size<4096
    for(int i=0;i<FIXED_CACHE_LEVEL_NUM;i++)
    {
        if(size <= slab_size_table[i])
        {
            //printf("[__slab_size]size: %d, 匹配到: %d\n",size,slab_size_table[i]);
            return slab_size_table[i];
        }
    }
    return 0;
}

/**
 * @brief 根据size找到fixed_cache_list的cache
 */
static struct kmem_cache *__fine_kmem_cache(uint64 size)
{
    struct kmem_cache *c;
    // 4096是2的12次方，预设size<4096
    for(int i=0;i<FIXED_CACHE_LEVEL_NUM;i++)
    {
        c = slab_allocator->fixed_cache_list[i];
        //printf("slab_allocator->fixed_cache_list[%d]的size: %d\n",i,c->size);
        if((size == c->size))
        {
            //printf("[__fine_kmem_cache]size: %d, 匹配到: fixed_cache_list[%d]\n",size,i);
            return slab_allocator->fixed_cache_list[i];
        }
    }
    return 0; //没找到，size不对
}

static void *__alloc_from_kmem_cache(struct kmem_cache *kmem_cache)
{
    struct slab *slab;
    struct object *object;
    /*尝试在free_slab查找*/
    if((slab = kmem_cache->free_slab))
    {
        /*从free_slab分配一个*/
        if(slab->free>1) ///< 至少有两个object
        {
            object = slab->object;
            slab->object = object->next;
            printf("分配前还有%d个object\n",slab->free);
            slab->free--;
            return (void *)object;
        }
        else if(slab->free == 1)///< 只有一个了
        {
            object = slab->object;
            /*slab链表置空*/
            slab->object = NULL;
            slab->free = 0;
            /*把这个slab放到kmem_cache的full_slab链表中*/
            kmem_cache->free_slab = slab->next;
            struct slab *slab_iterator=kmem_cache->full_slab;

            if(!slab_iterator) //如果kmem_cache->full_slab为空，直接插入链表头
            {
                kmem_cache->full_slab = slab;
            }
            else //kmem_cache->full_slab有元素
            {
                while(slab_iterator->next) //遍历到最后一个元素.尾插法，因为先来slab更有可能先被释放
                    slab_iterator = slab_iterator->next;
                slab_iterator->next = slab;
                slab->next = NULL;
            }
            printf("分配前只有一个object了,分配后移动到full_slab\n");
            return (void *)object;
        }
        else
            panic("slab->free异常\n");
    }
    else ///< free_slab没有，创建一个slab给kmem_cache再分配
    {
        slab = __create_slab_with_size(kmem_cache->size); //至少有3个
        kmem_cache->free_slab = slab;
        object = slab->object;
        slab->object = object->next;
        printf("没有空闲slab了,重新分配一个slab,分配前还有%d个object\n",slab->free);
        slab->free--;
        return (void *)object;
    }
    return 0;
}

/**
 * @brief slab分配，只处理小于一个页的。认为0<size<=4096
 */
void *slab_alloc(uint64 size)
{
    struct kmem_cache *s;
    /*处理size*/
    uint32 aligned_size = __slab_size(size); 
    if((s=__fine_kmem_cache(aligned_size))) ///< 找到了对应大小且有余量的kmem_cache
    {
        void *ptr = __alloc_from_kmem_cache(s);
        LOG("[slab_alloc]分配%d字节在地址0x%x\n",size,ptr);
        return ptr;
    }
    else
    {
        panic("[slab_alloc]没有找到kmem_cache!\n");
        return 0;
    }
}

/**
 * @brief
 */
void slab_free(uint64 addr)
{
    /*检查页开头的8字节是不是等于slab的magic*/
    uint64 page_start = addr & ~(PAGE_SIZE - 1); //页起始地址
    uint64 *magic = (uint64 *)page_start;
    if(*magic == SLAB_MAGIC)
    {
        printf("[slab_free]addr地址在slab页中\n");
        struct slab *slab = (struct slab *)page_start;
        printf("通过地址%x得知所属slab->size: %d\n",addr,slab->size);

        /*检查是否对齐*/
        if((addr-sizeof(struct slab))%slab->size) ///< 有余数说明没对齐
        {
            panic("[slab_free]addr地址没有对齐,不能释放");
        }
        else
        {
            printf("地址是对齐的，对应第%d个object\n",(addr-sizeof(struct slab)-page_start)/slab->size);
        }

        /*把addr所在的object插回slab的链表*/
        struct object *object = (struct object *)addr;
        if(slab->free == 0) ///< 链表为空，说明slab在full_list，要把slab放回kmem_cache的free_list
        {
            slab->object = object;
            object->next = NULL;
            slab->free++;
            struct kmem_cache *kmem_cache = __fine_kmem_cache(slab->size);
            if(kmem_cache)
            {
                printf("找到对应slab的kmem_cache\n");
                kmem_cache->full_slab = slab->next;
                slab->next = kmem_cache->free_slab; //先这样
                kmem_cache->free_slab = slab;
                printf("插入到链表头并加入kmem_cache的free_slab\n");
            }
        }
        else ///< 链表不为空，找到正确的地方插入链表
        {
            struct object *object_iterator = slab->object;
            if(object < slab->object)
            {
                object->next = slab->object;
                slab->object = object;
            }
            while(object)
            {
                if((object_iterator < object) && (object < object_iterator->next) &&(object_iterator->next != NULL))
                {
                    break;
                }
                if(object_iterator->next != NULL)
                    object_iterator = object_iterator->next;
                else //遍历到最后一个元素，还不满足
                    break;
                
            }
            /*插入到object_iterator后面*/
            object->next = object_iterator->next;
            object_iterator->next = object;
            slab->free++;
            printf("插入到链表中\n");
        }
    }
    else
    {
        panic("[slab_free]addr地址不在slab页中,不能释放");
    }
}

extern void shutdown();
void test_slab()
{
    slab_init();
    /*初始化正常*/

    // int size = 4;
    // for(int i=0;i<FIXED_CACHE_LEVEL_NUM;i++)
    // {
    //     size *=2;
    //     slab_alloc(size);
    // }
    //slab_alloc(842);
    // for(int i=0;i<26;i++)
    // {
    //     char *ptr = (char *)slab_alloc(8);
    //     *ptr = i+65;
    // }
    // for(int i=0;i<300;i++)
    // {
    //     slab_alloc(16);
    // }
    /*以上说明分配功能基本正常*/

    void *ptr_1 = slab_alloc(64);
    void *ptr_2 = slab_alloc(64);
    for(int i=0;i<64;i++)
    {
        slab_alloc(64);
    }
    slab_free((uint64)ptr_2);
    slab_free((uint64)ptr_1);
    slab_alloc(64);
    slab_alloc(64);

    LOG("test_slab success!\n");
    shutdown();
}
