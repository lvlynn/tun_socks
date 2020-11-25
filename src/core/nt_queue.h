#ifndef _NT_QUEUE_H_
#define _NT_QUEUE_H_

#include <nt_def.h>
#include <nt_core.h>


//具有头节点的双向循环链表
typedef struct nt_queue_s  nt_queue_t;

struct nt_queue_s {
    nt_queue_t  *prev;
    nt_queue_t  *next;
};

//化哨兵结点: 是一个附加的链表节点，该节点作为第一个节点，但是它其实并不存储任何东西

/* 其实nt_queue_t只是一个幌子，真正存储在队列中的元素是包含nt_queue_t成员的结构。
 * 换句话说：如果一个结构想使用队列的方式存储，那么这个结构就要包含nt_queue_t成员。 */

//以h结点为哨兵的队列称之为h队列。

//初始化哨兵结点，令prev字段和next字段均指向其自身；
#define nt_queue_init(q)                                                     \
    (q)->prev = q;                                                            \
    (q)->next = q

//检查哨兵结点的prev字段是否指向其自身，以判断队列是否为空；
#define nt_queue_empty(h)                                                    \
    (h == (h)->prev)

//在哨兵结点和第一个结点之间插入新结点x；
#define nt_queue_insert_head(h, x)                                           \
    (x)->next = (h)->next;                                                    \
    (x)->next->prev = x;                                                      \
    (x)->prev = h;                                                            \
    (h)->next = x

//是nt_queue_insert_head的别名；
#define nt_queue_insert_after   nt_queue_insert_head

//在最后一个结点和哨兵结点之间插入新结点；
#define nt_queue_insert_tail(h, x)                                           \
    (x)->prev = (h)->prev;                                                    \
    (x)->prev->next = x;                                                      \
    (x)->next = h;                                                            \
    (h)->prev = x

//获取第一个结点；
#define nt_queue_head(h)                                                     \
    (h)->next

//获取最后一个结点；
#define nt_queue_last(h)                                                     \
    (h)->prev

//获取哨兵结点（即参数h）；
#define nt_queue_sentinel(h)                                                 \
    (h)

//获取下一个结点；
#define nt_queue_next(q)                                                     \
    (q)->next

//获取上一个结点；
#define nt_queue_prev(q)                                                     \
    (q)->prev


#if (NT_DEBUG)
//将结点x从队列中移除；
#define nt_queue_remove(x)                                                   \
    (x)->next->prev = (x)->prev;                                              \
    (x)->prev->next = (x)->next;                                              \
    (x)->prev = NULL;                                                         \
    (x)->next = NULL

#else

#define nt_queue_remove(x)                                                   \
    (x)->next->prev = (x)->prev;                                              \
    (x)->prev->next = (x)->next

#endif
///将h为哨兵结点的队列中q结点开始到队尾结点的整个链拆分、链接到空的n队列中，h队列中的剩余结点组成新队列；
#define nt_queue_split(h, q, n)                                              \
    (n)->prev = (h)->prev;                                                    \
    (n)->prev->next = n;                                                      \
    (n)->next = q;                                                            \
    (h)->prev = (q)->prev;                                                    \
    (h)->prev->next = h;                                                      \
    (q)->prev = n;

//将n队列中的所有结点按顺序链接到h队列末尾，n队列清空；
#define nt_queue_add(h, n)                                                   \
    (h)->prev->next = (n)->next;                                              \
    (n)->next->prev = (h)->prev;                                              \
    (h)->prev = (n)->prev;                                                    \
    (h)->prev->next = h;

////取出包括q的type类型的地址。这样我们就能够訪问type内的成员
#define nt_queue_data(q, type, link)                                         \
    (type *) ((u_char *) q - offsetof(type, link))

//使用双倍步进算法寻找queue队列的中间结点；
nt_queue_t *nt_queue_middle( nt_queue_t *queue );

//使用插入排序算法对queue队列进行排序，完成后在next方向上为升序，prev方向为降序。
void nt_queue_sort( nt_queue_t *queue,
                    nt_int_t ( *cmp )( const nt_queue_t *, const nt_queue_t * ) );


#endif
