

#ifndef _NT_EVENT_TIMER_H_INCLUDED_
#define _NT_EVENT_TIMER_H_INCLUDED_


#include <nt_core.h>
//#include <nt_event.h>


#define NT_TIMER_INFINITE  (nt_msec_t) -1

#define NT_TIMER_LAZY_DELAY  300


nt_int_t nt_event_timer_init(nt_log_t *log);
nt_msec_t nt_event_find_timer(void);
void nt_event_expire_timers(void);
nt_int_t nt_event_no_timers_left(void);


extern nt_rbtree_t  nt_event_timer_rbtree;


static nt_inline void
nt_event_del_timer(nt_event_t *ev)
{
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
}


static nt_inline void
nt_event_add_timer(nt_event_t *ev, nt_msec_t timer)
{
    nt_msec_t      key;
    nt_msec_int_t  diff;

    key = nt_current_msec + timer;

    if (ev->timer_set) {

        /*
         * Use a previous timer value if difference between it and a new
         * value is less than NT_TIMER_LAZY_DELAY milliseconds: this allows
         * to minimize the rbtree operations for fast connections.
         */

        diff = (nt_msec_int_t) (key - ev->timer.key);

        if (nt_abs(diff) < NT_TIMER_LAZY_DELAY) {
            nt_log_debug3(NT_LOG_DEBUG_EVENT, ev->log, 0,
                           "event timer: %d, old: %M, new: %M",
                            nt_event_ident(ev->data), ev->timer.key, key);
            return;
        }

        nt_del_timer(ev);
    }

    ev->timer.key = key;

    nt_log_debug3(NT_LOG_DEBUG_EVENT, ev->log, 0,
                   "event timer add: %d: %M:%M",
                    nt_event_ident(ev->data), timer, ev->timer.key);

    nt_rbtree_insert(&nt_event_timer_rbtree, &ev->timer);

    ev->timer_set = 1;
}


#endif /* _NT_EVENT_TIMER_H_INCLUDED_ */
