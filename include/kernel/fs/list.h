#ifndef __LIST_H__
#define __LIST_H__
#include "types.h"
#include "spinlock.h"
#include <stddef.h>

// 给定一个结构体变量指针，算出结构体的首地址
// 一个给定变量偏移
#ifndef container_of
#define container_of(ptr, type, member)                                                                                \
    ({                                                                                                                 \
        const typeof(((type *) 0)->member) *__mptr = (ptr);                                                            \
        (type *) ((char *) __mptr - offsetof(type, member));                                                           \
    })
#endif


struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

typedef struct list_head list_head_t;

//将结点初始化为链表头
static inline void list_init(list_head_t *list) {
    list->next = list;
    list->prev = list;
}
//链表加 头插法
static inline void list_add(list_head_t *lnew, list_head_t *head) {
    head->next->prev = lnew;
    lnew->prev = head;
    lnew->next = head->next;
    head->next = lnew;
}
//移除出去当前的结点
static inline void list_del(list_head_t *entry) {
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
    entry->prev = entry->next = NULL;
}
//把节点从A Move to B
static inline void list_move(list_head_t *list, list_head_t *head) {
    list_del(list);
    list_add(list, head);
}

//判断链表是否空
static inline int list_empty(list_head_t *head) {
    return head->next == head;
}

//合并链表
static inline void list_splice(list_head_t *list, list_head_t *head) {
    list_head_t *first = list->next;
    list_head_t *last = list->prev;
    list_head_t *at = head->next;

    first->prev = head;
    head->next = first;

    last->next = at;
    at->prev = last;
}
#endif /* __LIST_H__ */