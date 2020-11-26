#ifndef _NT_EVENT_POSTED_H_
#define _NT_EVENT_POSTED_H_

#include <nt_def.h>
#include <nt_core.h>
#include <nt_event.h>


#define nt_post_event(ev, q)                                                 \
    \
    if (!(ev)->posted) {                                                      \
        (ev)->posted = 1;                                                     \
        nt_queue_insert_tail(q, &(ev)->queue);                               \
        \
        nt_log_debug1(NT_LOG_DEBUG_CORE, (ev)->log, 0, "post event %p", ev);\
        \
    } else  {                                                                 \
        nt_log_debug1(NT_LOG_DEBUG_CORE, (ev)->log, 0,                      \
                      "update posted event %p", ev);                         \
    }


#define nt_delete_posted_event(ev)                                           \
    \
    (ev)->posted = 0;                                                         \
    nt_queue_remove(&(ev)->queue);                                           \
    \
    nt_log_debug1(NT_LOG_DEBUG_CORE, (ev)->log, 0,                          \
                  "delete posted event %p", ev);



void nt_event_process_posted( nt_cycle_t *cycle, nt_queue_t *posted );


extern nt_queue_t  nt_posted_accept_events;
extern nt_queue_t  nt_posted_events;


#endif
