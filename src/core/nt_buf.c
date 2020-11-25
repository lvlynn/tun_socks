

#include <nt_def.h>
#include <nt_core.h>


nt_buf_t *
nt_create_temp_buf(nt_pool_t *pool, size_t size)
{
    nt_buf_t *b;

    b = nt_calloc_buf(pool);
    if (b == NULL) {
        return NULL;
    }

    b->start = nt_palloc(pool, size);
    if (b->start == NULL) {
        return NULL;
    }

    /*
     * set by nt_calloc_buf():
     *
     *     b->file_pos = 0;
     *     b->file_last = 0;
     *     b->file = NULL;
     *     b->shadow = NULL;
     *     b->tag = 0;
     *     and flags
     */

    b->pos = b->start;
    b->last = b->start;
    b->end = b->last + size;
    b->temporary = 1;

    return b;
}


nt_chain_t *
nt_alloc_chain_link(nt_pool_t *pool)
{
    nt_chain_t  *cl;

    cl = pool->chain;

    if (cl) {
        pool->chain = cl->next;
        return cl;
    }

    cl = nt_palloc(pool, sizeof(nt_chain_t));
    if (cl == NULL) {
        return NULL;
    }

    return cl;
}


nt_chain_t *
nt_create_chain_of_bufs(nt_pool_t *pool, nt_bufs_t *bufs)
{
    u_char       *p;
    nt_int_t     i;
    nt_buf_t    *b;
    nt_chain_t  *chain, *cl, **ll;

    p = nt_palloc(pool, bufs->num * bufs->size);
    if (p == NULL) {
        return NULL;
    }

    ll = &chain;

    for (i = 0; i < bufs->num; i++) {

        b = nt_calloc_buf(pool);
        if (b == NULL) {
            return NULL;
        }

        /*
         * set by nt_calloc_buf():
         *
         *     b->file_pos = 0;
         *     b->file_last = 0;
         *     b->file = NULL;
         *     b->shadow = NULL;
         *     b->tag = 0;
         *     and flags
         *
         */

        b->pos = p;
        b->last = p;
        b->temporary = 1;

        b->start = p;
        p += bufs->size;
        b->end = p;

        cl = nt_alloc_chain_link(pool);
        if (cl == NULL) {
            return NULL;
        }

        cl->buf = b;
        *ll = cl;
        ll = &cl->next;
    }

    *ll = NULL;

    return chain;
}


nt_int_t
nt_chain_add_copy(nt_pool_t *pool, nt_chain_t **chain, nt_chain_t *in)
{
    nt_chain_t  *cl, **ll;

    ll = chain;

    for (cl = *chain; cl; cl = cl->next) {
        ll = &cl->next;
    }

    while (in) {
        cl = nt_alloc_chain_link(pool);
        if (cl == NULL) {
            *ll = NULL;
            return NT_ERROR;
        }

        cl->buf = in->buf;
        *ll = cl;
        ll = &cl->next;
        in = in->next;
    }

    *ll = NULL;

    return NT_OK;
}


nt_chain_t *
nt_chain_get_free_buf(nt_pool_t *p, nt_chain_t **free)
{
    nt_chain_t  *cl;

    if (*free) {
        cl = *free;
        *free = cl->next;
        cl->next = NULL;
        return cl;
    }

    cl = nt_alloc_chain_link(p);
    if (cl == NULL) {
        return NULL;
    }

    cl->buf = nt_calloc_buf(p);
    if (cl->buf == NULL) {
        return NULL;
    }

    cl->next = NULL;

    return cl;
}


void
nt_chain_update_chains(nt_pool_t *p, nt_chain_t **free, nt_chain_t **busy,
    nt_chain_t **out, nt_buf_tag_t tag)
{
    nt_chain_t  *cl;

    if (*out) {
        if (*busy == NULL) {
            *busy = *out;

        } else {
            for (cl = *busy; cl->next; cl = cl->next) { /* void */ }

            cl->next = *out;
        }

        *out = NULL;
    }

    while (*busy) {
        cl = *busy;

        if (nt_buf_size(cl->buf) != 0) {
            break;
        }

        if (cl->buf->tag != tag) {
            *busy = cl->next;
            nt_free_chain(p, cl);
            continue;
        }

        cl->buf->pos = cl->buf->start;
        cl->buf->last = cl->buf->start;

        *busy = cl->next;
        cl->next = *free;
        *free = cl;
    }
}


off_t
nt_chain_coalesce_file(nt_chain_t **in, off_t limit)
{
    off_t         total, size, aligned, fprev;
    nt_fd_t      fd;
    nt_chain_t  *cl;

    total = 0;

    cl = *in;
    fd = cl->buf->file->fd;

    do {
        size = cl->buf->file_last - cl->buf->file_pos;

        if (size > limit - total) {
            size = limit - total;

            aligned = (cl->buf->file_pos + size + nt_pagesize - 1)
                       & ~((off_t) nt_pagesize - 1);

            if (aligned <= cl->buf->file_last) {
                size = aligned - cl->buf->file_pos;
            }

            total += size;
            break;
        }

        total += size;
        fprev = cl->buf->file_pos + size;
        cl = cl->next;

    } while (cl
             && cl->buf->in_file
             && total < limit
             && fd == cl->buf->file->fd
             && fprev == cl->buf->file_pos);

    *in = cl;

    return total;
}


nt_chain_t *
nt_chain_update_sent(nt_chain_t *in, off_t sent)
{
    off_t  size;

    for ( /* void */ ; in; in = in->next) {

        if (nt_buf_special(in->buf)) {
            continue;
        }

        if (sent == 0) {
            break;
        }

        size = nt_buf_size(in->buf);

        if (sent >= size) {
            sent -= size;

            if (nt_buf_in_memory(in->buf)) {
                in->buf->pos = in->buf->last;
            }

            if (in->buf->in_file) {
                in->buf->file_pos = in->buf->file_last;
            }

            continue;
        }

        if (nt_buf_in_memory(in->buf)) {
            in->buf->pos += (size_t) sent;
        }

        if (in->buf->in_file) {
            in->buf->file_pos += sent;
        }

        break;
    }

    return in;
}
