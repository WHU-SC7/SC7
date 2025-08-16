#ifndef __NAMESPACE_H__
#define __NAMESPACE_H__

#include "spinlock.h"

/* UTS命名空间相关定义 */
#define MAX_UTS_NAMESPACES 64  ///< 最大UTS命名空间数量
#define UTS_HOSTNAME_LEN 65    ///< 主机名最大长度
#define DEFAULTHOSTNAME "DOGE" ///< 默认主机名

/* UTS命名空间结构 */
struct uts_namespace
{
    int used;                        ///< 是否被使用
    int ref_count;                   ///< 引用计数
    char hostname[UTS_HOSTNAME_LEN]; ///< 主机名
    spinlock_t lock;                 ///< 保护锁
};

void init_uts_namespaces(void);
int create_uts_namespace(int parent_ns_id);
void uts_namespace_get(int ns_id);
void uts_namespace_put(int ns_id);
extern struct uts_namespace uts_namespaces[MAX_UTS_NAMESPACES];
#endif ///< __NAMESPACE_H__