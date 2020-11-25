

#ifndef _NT_BUF_H_INCLUDED_
#define _NT_BUF_H_INCLUDED_


#include <nt_def.h>
#include <nt_core.h>


typedef void *            nt_buf_tag_t;

typedef struct nt_buf_s  nt_buf_t;

struct nt_buf_s {
    u_char          *pos;
    u_char          *last;
    off_t            file_pos;
    off_t            file_last;

    u_char          *start;         /* start of buffer */
    u_char          *end;           /* end of buffer */
    nt_buf_tag_t    tag;
    nt_file_t      *file;
    nt_buf_t       *shadow;


    /* the buf's content could be changed */
    unsigned         temporary:1;

    /*
     * the buf's content is in a memory cache or in a read only memory
     * and must not be changed
     */
    unsigned         memory:1;

    /* the buf's content is mmap()ed and must not be changed */
    unsigned         mmap:1;

    unsigned         recycled:1;
    unsigned         in_file:1;
    unsigned         flush:1;
    unsigned         sync:1;
    unsigned         last_buf:1;
    unsigned         last_in_chain:1;

    unsigned         last_shadow:1;
    unsigned         temp_file:1;

    /* STUB */ int   num;
};


struct nt_chain_s {
    nt_buf_t    *buf;
    nt_chain_t  *next;
};


typedef struct {
    nt_int_t    num;
    size_t       size;
} nt_bufs_t;


typedef struct nt_output_chain_ctx_s  nt_output_chain_ctx_t;

typedef nt_int_t (*nt_output_chain_filter_pt)(void *ctx, nt_chain_t *in);

typedef void (*nt_output_chain_aio_pt)(nt_output_chain_ctx_t *ctx,
    nt_file_t *file);

struct nt_output_chain_ctx_s {
    nt_buf_t                   *buf;
    nt_chain_t                 *in;
    nt_chain_t                 *free;
    nt_chain_t                 *busy;

    unsigned                     sendfile:1;
    unsigned                     directio:1;
    unsigned                     unaligned:1;
    unsigned                     need_in_memory:1;
    unsigned                     need_in_temp:1;
    unsigned                     aio:1;

#if (NT_HAVE_FILE_AIO || NT_COMPAT)
    nt_output_chain_aio_pt      aio_handler;
#if (NT_HAVE_AIO_SENDFILE || NT_COMPAT)
    ssize_t                    (*aio_preload)(nt_buf_t *file);
#endif
#endif

#if (NT_THREADS || NT_COMPAT)
    nt_int_t                  (*thread_handler)(nt_thread_task_t *task,
                                                 nt_file_t *file);
    nt_thread_task_t           *thread_task;
#endif

    off_t                        alignment;

    nt_pool_t                  *pool;
    nt_int_t                    allocated;
    nt_bufs_t                   bufs;
    nt_buf_tag_t                tag;

    nt_output_chain_filter_pt   output_filter;
    void                        *filter_ctx;
};


typedef struct {
    nt_chain_t                 *out;
    nt_chain_t                **last;
    nt_connection_t            *connection;
    nt_pool_t                  *pool;
    off_t                        limit;
} nt_chain_writer_ctx_t;


#define NT_CHAIN_ERROR     (nt_chain_t *) NT_ERROR


#define nt_buf_in_memory(b)        (b->temporary || b->memory || b->mmap)
#define nt_buf_in_memory_only(b)   (nt_buf_in_memory(b) && !b->in_file)

#define nt_buf_special(b)                                                   \
    ((b->flush || b->last_buf || b->sync)                                    \
     && !nt_buf_in_memory(b) && !b->in_file)

#define nt_buf_sync_only(b)                                                 \
    (b->sync                                                                 \
     && !nt_buf_in_memory(b) && !b->in_file && !b->flush && !b->last_buf)

#define nt_buf_size(b)                                                      \
    (nt_buf_in_memory(b) ? (off_t) (b->last - b->pos):                      \
                            (b->file_last - b->file_pos))

nt_buf_t *nt_create_temp_buf(nt_pool_t *pool, size_t size);
nt_chain_t *nt_create_chain_of_bufs(nt_pool_t *pool, nt_bufs_t *bufs);


#define nt_alloc_buf(pool)  nt_palloc(pool, sizeof(nt_buf_t))
#define nt_calloc_buf(pool) nt_pcalloc(pool, sizeof(nt_buf_t))

nt_chain_t *nt_alloc_chain_link(nt_pool_t *pool);
#define nt_free_chain(pool, cl)                                             \
    cl->next = pool->chain;                                                  \
    pool->chain = cl


#if 0
nt_int_t nt_output_chain(nt_output_chain_ctx_t *ctx, nt_chain_t *in);
nt_int_t nt_chain_writer(void *ctx, nt_chain_t *in);

nt_int_t nt_chain_add_copy(nt_pool_t *pool, nt_chain_t **chain,
    nt_chain_t *in);
nt_chain_t *nt_chain_get_free_buf(nt_pool_t *p, nt_chain_t **free);
void nt_chain_update_chains(nt_pool_t *p, nt_chain_t **free,
    nt_chain_t **busy, nt_chain_t **out, nt_buf_tag_t tag);

off_t nt_chain_coalesce_file(nt_chain_t **in, off_t limit);

nt_chain_t *nt_chain_update_sent(nt_chain_t *in, off_t sent);
#endif

#endif /* _NT_BUF_H_INCLUDED_ */
