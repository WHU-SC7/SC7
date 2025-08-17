#include "spinlock.h"
#include "namespace.h"
#include "string.h"

/* UTS命名空间全局数组 */
struct uts_namespace uts_namespaces[MAX_UTS_NAMESPACES];
spinlock_t uts_ns_lock; ///< 保护UTS命名空间数组的锁

/**
 * @brief 初始化UTS命名空间系统
 */
void init_uts_namespaces(void)
{
    initlock(&uts_ns_lock, "uts_ns_lock");

    /* 初始化默认命名空间（ID为0）*/
    uts_namespaces[0].used = 1;
    uts_namespaces[0].ref_count = 1;
    strncpy(uts_namespaces[0].hostname, DEFAULTHOSTNAME, UTS_HOSTNAME_LEN - 1);
    uts_namespaces[0].hostname[UTS_HOSTNAME_LEN - 1] = '\0';
    initlock(&uts_namespaces[0].lock, "uts_ns_0");

    /* 初始化其他命名空间槽位 */
    for (int i = 1; i < MAX_UTS_NAMESPACES; i++)
    {
        uts_namespaces[i].used = 0;
        uts_namespaces[i].ref_count = 0;
        initlock(&uts_namespaces[i].lock, "uts_ns");
    }
}

/**
 * @brief 创建新的UTS命名空间
 * @param parent_ns_id 父命名空间ID
 * @return 新命名空间ID，失败返回-1
 */
int create_uts_namespace(int parent_ns_id)
{
    acquire(&uts_ns_lock);

    int new_id = -1;
    for (int i = 1; i < MAX_UTS_NAMESPACES; i++)
    {
        if (!uts_namespaces[i].used)
        {
            new_id = i;
            break;
        }
    }

    if (new_id == -1)
    {
        release(&uts_ns_lock);
        return -1; ///< 没有可用的命名空间槽位
    }

    /* 初始化新命名空间 */
    uts_namespaces[new_id].used = 1;
    uts_namespaces[new_id].ref_count = 1;

    /* 从父命名空间复制主机名 */
    if (parent_ns_id >= 0 && parent_ns_id < MAX_UTS_NAMESPACES && uts_namespaces[parent_ns_id].used)
    {
        strncpy(uts_namespaces[new_id].hostname, uts_namespaces[parent_ns_id].hostname, UTS_HOSTNAME_LEN);
    }
    else
    {
        strncpy(uts_namespaces[new_id].hostname, DEFAULTHOSTNAME, UTS_HOSTNAME_LEN - 1);
        uts_namespaces[new_id].hostname[UTS_HOSTNAME_LEN - 1] = '\0';
    }

    release(&uts_ns_lock);
    return new_id;
}

/**
 * @brief 增加UTS命名空间的引用计数
 * @param ns_id 命名空间ID
 */
void uts_namespace_get(int ns_id)
{
    if (ns_id >= 0 && ns_id < MAX_UTS_NAMESPACES && uts_namespaces[ns_id].used)
    {
        acquire(&uts_ns_lock);
        uts_namespaces[ns_id].ref_count++;
        release(&uts_ns_lock);
    }
}

/**
 * @brief 减少UTS命名空间的引用计数，如果计数为0则释放
 * @param ns_id 命名空间ID
 */
void uts_namespace_put(int ns_id)
{
    if (ns_id <= 0 || ns_id >= MAX_UTS_NAMESPACES || !uts_namespaces[ns_id].used)
    {
        return; ///< 默认命名空间(ID=0)不释放
    }

    acquire(&uts_ns_lock);
    uts_namespaces[ns_id].ref_count--;
    if (uts_namespaces[ns_id].ref_count <= 0)
    {
        uts_namespaces[ns_id].used = 0;
        uts_namespaces[ns_id].ref_count = 0;
        memset(uts_namespaces[ns_id].hostname, 0, UTS_HOSTNAME_LEN);
    }
    release(&uts_ns_lock);
}