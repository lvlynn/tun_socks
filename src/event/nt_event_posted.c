#include <nt_def.h>
#include <nt_core.h>


nt_queue_t  nt_posted_accept_events;
nt_queue_t  nt_posted_events;

 void
nt_event_process_posted( nt_cycle_t *cycle, nt_queue_t *posted )
{
    nt_queue_t  *q;
    nt_event_t  *ev;

    while( !nt_queue_empty( posted ) ) {

        q = nt_queue_head( posted );
        ev = nt_queue_data( q, nt_event_t, queue );

        nt_log_debug1( NT_LOG_DEBUG_EVENT, cycle->log, 0,
                       "posted event %p", ev );

        nt_delete_posted_event( ev );

        ev->handler( ev );

    }

}

