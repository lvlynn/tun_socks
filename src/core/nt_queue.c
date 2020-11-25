#include <nt_def.h>
#include <nt_core.h>


/*
 * find the middle queue element if the queue has odd number of elements
 * or the first element of the queue's second part otherwise
 */

nt_queue_t *
nt_queue_middle( nt_queue_t *queue )
{
    nt_queue_t  *middle, *next;

    middle = nt_queue_head( queue );

    if( middle == nt_queue_last( queue ) ) {
        return middle;
    }

    next = nt_queue_head( queue );

    for( ;; ) {
        middle = nt_queue_next( middle );

        next = nt_queue_next( next );

        if( next == nt_queue_last( queue ) ) {
            return middle;
        }

        next = nt_queue_next( next );

        if( next == nt_queue_last( queue ) ) {
            return middle;
        }
    }
}

/* the stable insertion sort */

void
nt_queue_sort( nt_queue_t *queue,
                nt_int_t ( *cmp )( const nt_queue_t *, const nt_queue_t * ) )
{
    nt_queue_t  *q, *prev, *next;

    q = nt_queue_head( queue );

    if( q == nt_queue_last( queue ) ) {
        return;
    }

    for( q = nt_queue_next( q ); q != nt_queue_sentinel( queue ); q = next ) {

        prev = nt_queue_prev( q );
        next = nt_queue_next( q );

        nt_queue_remove( q );

        do {
            if( cmp( prev, q ) <= 0 ) {
                break;
            }

            prev = nt_queue_prev( prev );

        } while( prev != nt_queue_sentinel( queue ) );

        nt_queue_insert_after( prev, q );
    }
}
