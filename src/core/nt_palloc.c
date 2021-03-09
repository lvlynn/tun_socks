
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <nt_def.h>
#include <nt_core.h>


static nt_inline void *nt_palloc_small( nt_pool_t *pool, size_t size,
                                        nt_uint_t align );
static void *nt_palloc_block( nt_pool_t *pool, size_t size );
static void *nt_palloc_large( nt_pool_t *pool, size_t size );


nt_pool_t *
nt_create_pool( size_t size, nt_log_t *log )
{
    nt_pool_t  *p;

    p = nt_memalign( NT_POOL_ALIGNMENT, size, log );
    if( p == NULL ) {
        return NULL;
    }

    p->d.last = ( u_char * ) p + sizeof( nt_pool_t );
    p->d.end = ( u_char * ) p + size;
    p->d.next = NULL;
    p->d.failed = 0;

    size = size - sizeof( nt_pool_t );
    p->max = ( size < NT_MAX_ALLOC_FROM_POOL ) ? size : NT_MAX_ALLOC_FROM_POOL;

    p->current = p;
    p->chain = NULL;
    p->large = NULL;
    p->cleanup = NULL;
    p->log = log;

    return p;
}


void
nt_destroy_pool( nt_pool_t *pool )
{
    nt_pool_t          *p, *n;
    nt_pool_large_t    *l;
    nt_pool_cleanup_t  *c;

    for( c = pool->cleanup; c; c = c->next ) {
        if( c->handler ) {
            nt_log_debug1( NT_LOG_DEBUG_ALLOC, pool->log, 0,
                           "run cleanup: %p", c );
            c->handler( c->data );
        }
    }

    #if (NT_DEBUG)

    /*
     * we could allocate the pool->log from this pool
     * so we cannot use this log while free()ing the pool
     */

    for( l = pool->large; l; l = l->next ) {
        nt_log_debug1( NT_LOG_DEBUG_ALLOC, pool->log, 0, "free: %p", l->alloc );
    }

    for( p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next ) {
        nt_log_debug2( NT_LOG_DEBUG_ALLOC, pool->log, 0,
                       "free: %p, unused: %uz", p, p->d.end - p->d.last );

        if( n == NULL ) {
            break;
        }
    }

    #endif

    for( l = pool->large; l; l = l->next ) {
        /* debug("free, l = %p", l->next); */
        /* debug("free, l = %p", l->alloc); */
        if( l->alloc  ) {
            nt_free( l->alloc );
            /* debug("free end, l = %p", l->alloc); */
        }
    }

    for( p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next ) {
        /* debug("free, p = %p", p); */
        nt_free( p );

        if( n == NULL ) {
            break;
        }
    }
}


void
nt_reset_pool( nt_pool_t *pool )
{
    nt_pool_t        *p;
    nt_pool_large_t  *l;

    for( l = pool->large; l; l = l->next ) {
        if( l->alloc ) {
            nt_free( l->alloc );
        }
    }

    for( p = pool; p; p = p->d.next ) {
        p->d.last = ( u_char * ) p + sizeof( nt_pool_t );
        p->d.failed = 0;
    }

    pool->current = pool;
    pool->chain = NULL;
    pool->large = NULL;
}

/*
 * size 不能等于0
 *
 * */
void *
nt_palloc( nt_pool_t *pool, size_t size )
{
    #if !(NT_DEBUG_PALLOC)
 //   debug( "size=%d, pool->max=%d", size, pool->max );
    if( size <= pool->max ) {
        return nt_palloc_small( pool, size, 1 );
    }
    #endif

    return nt_palloc_large( pool, size );
}


void *
nt_pnalloc( nt_pool_t *pool, size_t size )
{
    #if !(NT_DEBUG_PALLOC)
    if( size <= pool->max ) {
        return nt_palloc_small( pool, size, 0 );
    }
    #endif

    return nt_palloc_large( pool, size );
}


static nt_inline void *
nt_palloc_small( nt_pool_t *pool, size_t size, nt_uint_t align )
{
    u_char      *m;
    nt_pool_t  *p;

    p = pool->current;

    do {
        m = p->d.last;

        if( align ) {
            m = nt_align_ptr( m, NT_ALIGNMENT );
        }

        if( ( size_t )( p->d.end - m ) >= size ) {
            p->d.last = m + size;
            return m;
        }

        p = p->d.next;

    } while( p );

    return nt_palloc_block( pool, size );
}


static void *
nt_palloc_block( nt_pool_t *pool, size_t size )
{
    u_char      *m;
    size_t       psize;
    nt_pool_t  *p, *new;

    psize = ( size_t )( pool->d.end - ( u_char * ) pool );

    m = nt_memalign( NT_POOL_ALIGNMENT, psize, pool->log );
    if( m == NULL ) {
        return NULL;
    }

    new = ( nt_pool_t * ) m;

    new->d.end = m + psize;
    new->d.next = NULL;
    new->d.failed = 0;

    m += sizeof( nt_pool_data_t );
    m = nt_align_ptr( m, NT_ALIGNMENT );
    new->d.last = m + size;

    for( p = pool->current; p->d.next; p = p->d.next ) {
        if( p->d.failed++ > 4 ) {
            pool->current = p->d.next;
        }
    }

    p->d.next = new;

    return m;
}


static void *
nt_palloc_large( nt_pool_t *pool, size_t size )
{
    void              *p;
    nt_uint_t         n;
    nt_pool_large_t  *large;

    p = nt_alloc( size, pool->log );
    if( p == NULL ) {
        return NULL;
    }

    n = 0;

    for( large = pool->large; large; large = large->next ) {
        if( large->alloc == NULL ) {
            large->alloc = p;
            return p;
        }

        if( n++ > 3 ) {
            break;
        }
    }

    large = nt_palloc_small( pool, sizeof( nt_pool_large_t ), 1 );
    if( large == NULL ) {
        nt_free( p );
        return NULL;
    }

    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}


void *
nt_pmemalign( nt_pool_t *pool, size_t size, size_t alignment )
{
    void              *p;
    nt_pool_large_t  *large;

    p = nt_memalign( alignment, size, pool->log );
    if( p == NULL ) {
        return NULL;
    }

    large = nt_palloc_small( pool, sizeof( nt_pool_large_t ), 1 );
    if( large == NULL ) {
        nt_free( p );
        return NULL;
    }

    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}


nt_int_t
nt_pfree( nt_pool_t *pool, void *p )
{
    nt_pool_large_t  *l;

    for( l = pool->large; l; l = l->next ) {
        if( p == l->alloc ) {
            nt_log_debug1( NT_LOG_DEBUG_ALLOC, pool->log, 0,
                           "free: %p", l->alloc );
            nt_free( l->alloc );
            l->alloc = NULL;

            return NT_OK;
        }
    }

    return NT_DECLINED;
}


void *
nt_pcalloc( nt_pool_t *pool, size_t size )
{
    void *p;

    p = nt_palloc( pool, size );
    if( p ) {
        nt_memzero( p, size );
    }

    return p;
}


#if (T_DEPRECATED)
void *
nt_prealloc( nt_pool_t *pool, void *p, size_t old_size, size_t new_size )
{
    void *new;
    nt_pool_t *node;

    if( p == NULL ) {
        return nt_palloc( pool, new_size );
    }

    if( new_size == 0 ) {
        if( ( u_char * ) p + old_size == pool->d.last ) {
            pool->d.last = p;
        } else {
            nt_pfree( pool, p );
        }

        return NULL;
    }

    if( old_size <= pool->max ) {
        for( node = pool; node; node = node->d.next ) {
            if( ( u_char * )p + old_size == node->d.last
                && ( u_char * )p + new_size <= node->d.end ) {
                node->d.last = ( u_char * )p + new_size;
                return p;
            }
        }
    }

    if( new_size <= old_size ) {
        return p;
    }

    new = nt_palloc( pool, new_size );
    if( new == NULL ) {
        return NULL;
    }

    nt_memcpy( new, p, old_size );

    nt_pfree( pool, p );

    return new;
}
#endif


nt_pool_cleanup_t *
nt_pool_cleanup_add( nt_pool_t *p, size_t size )
{
    nt_pool_cleanup_t  *c;

    c = nt_palloc( p, sizeof( nt_pool_cleanup_t ) );
    if( c == NULL ) {
        return NULL;
    }

    if( size ) {
        c->data = nt_palloc( p, size );
        if( c->data == NULL ) {
            return NULL;
        }

    } else {
        c->data = NULL;
    }

    c->handler = NULL;
    c->next = p->cleanup;

    p->cleanup = c;

    nt_log_debug1( NT_LOG_DEBUG_ALLOC, p->log, 0, "add cleanup: %p", c );

    return c;
}


void
nt_pool_run_cleanup_file( nt_pool_t *p, nt_fd_t fd )
{
    nt_pool_cleanup_t       *c;
    nt_pool_cleanup_file_t  *cf;

    for( c = p->cleanup; c; c = c->next ) {
        if( c->handler == nt_pool_cleanup_file ) {

            cf = c->data;

            if( cf->fd == fd ) {
                c->handler( cf );
                c->handler = NULL;
                return;
            }
        }
    }
}


void
nt_pool_cleanup_file( void *data )
{
    nt_pool_cleanup_file_t  *c = data;

    nt_log_debug1( NT_LOG_DEBUG_ALLOC, c->log, 0, "file cleanup: fd:%d",
                   c->fd );

    if( nt_close_file( c->fd ) == NT_FILE_ERROR ) {
        nt_log_error( NT_LOG_ALERT, c->log, nt_errno,
                      nt_close_file_n " \"%s\" failed", c->name );
    }
}


void
nt_pool_delete_file( void *data )
{
    nt_pool_cleanup_file_t  *c = data;

    nt_err_t  err;

    nt_log_debug2( NT_LOG_DEBUG_ALLOC, c->log, 0, "file cleanup: fd:%d %s",
                   c->fd, c->name );

    if( nt_delete_file( c->name ) == NT_FILE_ERROR ) {
        err = nt_errno;

        if( err != NT_ENOENT ) {
            nt_log_error( NT_LOG_CRIT, c->log, err,
                          nt_delete_file_n " \"%s\" failed", c->name );
        }
    }

    if( nt_close_file( c->fd ) == NT_FILE_ERROR ) {
        nt_log_error( NT_LOG_ALERT, c->log, nt_errno,
                      nt_close_file_n " \"%s\" failed", c->name );
    }
}


#if 0

static void *
nt_get_cached_block( size_t size )
{
    void                     *p;
    nt_cached_block_slot_t  *slot;

    if( nt_cycle->cache == NULL ) {
        return NULL;
    }

    slot = &nt_cycle->cache[( size + nt_pagesize - 1 ) / nt_pagesize];

    slot->tries++;

    if( slot->number ) {
        p = slot->block;
        slot->block = slot->block->next;
        slot->number--;
        return p;
    }

    return NULL;
}

#endif
