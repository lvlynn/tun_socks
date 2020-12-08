
//#include <nt_config.h>
#include <nt_core.h>
//#include <nt_event.h>


nt_rbtree_t              nt_event_timer_rbtree;
static nt_rbtree_node_t  nt_event_timer_sentinel;

/*
 * the event timer rbtree may contain the duplicate keys, however,
 * it should not be a problem, because we use the rbtree to find
 * a minimum timer value only
 */

nt_int_t
nt_event_timer_init(nt_log_t *log)
{
    nt_rbtree_init(&nt_event_timer_rbtree, &nt_event_timer_sentinel,
                    nt_rbtree_insert_timer_value);

    return NT_OK;
}


nt_msec_t
nt_event_find_timer(void)
{
    nt_msec_int_t      timer;
    nt_rbtree_node_t  *node, *root, *sentinel;

    if (nt_event_timer_rbtree.root == &nt_event_timer_sentinel) {
        return NT_TIMER_INFINITE;
    }

    root = nt_event_timer_rbtree.root;
    sentinel = nt_event_timer_rbtree.sentinel;

    node = nt_rbtree_min(root, sentinel);

    printf( "k=%ld, t=%ld\n", node->key , nt_current_msec );
    timer = (nt_msec_int_t) (node->key - nt_current_msec);

    return (nt_msec_t) (timer > 0 ? timer : 0);
}

//循环查找事件最小的事件
void
nt_event_expire_timers(void)
{
    nt_event_t        *ev;
    nt_rbtree_node_t  *node, *root, *sentinel;

    sentinel = nt_event_timer_rbtree.sentinel;

    /*
     *  只处理已经比 nt_current_msec 小的事件，即已经过期的事件
     *
     * */
    for ( ;; ) {
        root = nt_event_timer_rbtree.root;

        if (root == sentinel) {
            return;
        }

        node = nt_rbtree_min(root, sentinel);

        /* node->key > nt_current_msec */

        if ((nt_msec_int_t) (node->key - nt_current_msec) > 0) {
            return;
        }

        ev = (nt_event_t *) ((char *) node - offsetof(nt_event_t, timer));

        nt_log_debug2(NT_LOG_DEBUG_EVENT, ev->log, 0,
                       "event timer del: %d: %M",
                       nt_event_ident(ev->data), ev->timer.key);

        nt_rbtree_delete(&nt_event_timer_rbtree, &ev->timer);

#if (NT_DEBUG)
        ev->timer.left = NULL;
        ev->timer.right = NULL;
        ev->timer.parent = NULL;
#endif

        ev->timer_set = 0;

        ev->timedout = 1;

        ev->handler(ev);
    }
}


nt_int_t
nt_event_no_timers_left(void)
{
    nt_event_t        *ev;
    nt_rbtree_node_t  *node, *root, *sentinel;

    sentinel = nt_event_timer_rbtree.sentinel;
    root = nt_event_timer_rbtree.root;

    if (root == sentinel) {
        return NT_OK;
    }

    for (node = nt_rbtree_min(root, sentinel);
         node;
         node = nt_rbtree_next(&nt_event_timer_rbtree, node))
    {
        ev = (nt_event_t *) ((char *) node - offsetof(nt_event_t, timer));

        if (!ev->cancelable) {
            return NT_AGAIN;
        }
    }

    /* only cancelable timers left */

    return NT_OK;
}
