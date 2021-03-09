
#include <nt_core.h>


#define NT_SLAB_PAGE_MASK   3
#define NT_SLAB_PAGE        0
#define NT_SLAB_BIG         1
#define NT_SLAB_EXACT       2
#define NT_SLAB_SMALL       3

#if (NT_PTR_SIZE == 4)

#define NT_SLAB_PAGE_FREE   0
#define NT_SLAB_PAGE_BUSY   0xffffffff
#define NT_SLAB_PAGE_START  0x80000000

#define NT_SLAB_SHIFT_MASK  0x0000000f
#define NT_SLAB_MAP_MASK    0xffff0000
#define NT_SLAB_MAP_SHIFT   16

#define NT_SLAB_BUSY        0xffffffff

#else /* (NT_PTR_SIZE == 8) */

#define NT_SLAB_PAGE_FREE   0
#define NT_SLAB_PAGE_BUSY   0xffffffffffffffff
#define NT_SLAB_PAGE_START  0x8000000000000000

#define NT_SLAB_SHIFT_MASK  0x000000000000000f
#define NT_SLAB_MAP_MASK    0xffffffff00000000
#define NT_SLAB_MAP_SHIFT   32

#define NT_SLAB_BUSY        0xffffffffffffffff

#endif


#define nt_slab_slots(pool)                                                  \
    (nt_slab_page_t *) ((u_char *) (pool) + sizeof(nt_slab_pool_t))

#define nt_slab_page_type(page)   ((page)->prev & NT_SLAB_PAGE_MASK)

#define nt_slab_page_prev(page)                                              \
    (nt_slab_page_t *) ((page)->prev & ~NT_SLAB_PAGE_MASK)

#define nt_slab_page_addr(pool, page)                                        \
    ((((page) - (pool)->pages) << nt_pagesize_shift)                         \
     + (uintptr_t) (pool)->start)


#if (NT_DEBUG_MALLOC)

#define nt_slab_junk(p, size)     nt_memset(p, 0xA5, size)

#elif (NT_HAVE_DEBUG_MALLOC)

#define nt_slab_junk(p, size)                                                \
    if (nt_debug_malloc)          nt_memset(p, 0xA5, size)

#else

#define nt_slab_junk(p, size)

#endif

static nt_slab_page_t *nt_slab_alloc_pages(nt_slab_pool_t *pool,
    nt_uint_t pages);
static void nt_slab_free_pages(nt_slab_pool_t *pool, nt_slab_page_t *page,
    nt_uint_t pages);
static void nt_slab_error(nt_slab_pool_t *pool, nt_uint_t level,
    char *text);


static nt_uint_t  nt_slab_max_size;
static nt_uint_t  nt_slab_exact_size;
static nt_uint_t  nt_slab_exact_shift;


void
nt_slab_sizes_init(void)
{
    nt_uint_t  n;

    nt_slab_max_size = nt_pagesize / 2;
    nt_slab_exact_size = nt_pagesize / (8 * sizeof(uintptr_t));
    for (n = nt_slab_exact_size; n >>= 1; nt_slab_exact_shift++) {
        /* void */
    }
}


void
nt_slab_init(nt_slab_pool_t *pool)
{
    u_char           *p;
    size_t            size;
    nt_int_t         m;
    nt_uint_t        i, n, pages;
    nt_slab_page_t  *slots, *page;

    pool->min_size = (size_t) 1 << pool->min_shift;

    slots = nt_slab_slots(pool);

    p = (u_char *) slots;
    size = pool->end - p;

    nt_slab_junk(p, size);

    n = nt_pagesize_shift - pool->min_shift;

    for (i = 0; i < n; i++) {
        /* only "next" is used in list head */
        slots[i].slab = 0;
        slots[i].next = &slots[i];
        slots[i].prev = 0;
    }

    p += n * sizeof(nt_slab_page_t);

    pool->stats = (nt_slab_stat_t *) p;
    nt_memzero(pool->stats, n * sizeof(nt_slab_stat_t));

    p += n * sizeof(nt_slab_stat_t);

    size -= n * (sizeof(nt_slab_page_t) + sizeof(nt_slab_stat_t));

    pages = (nt_uint_t) (size / (nt_pagesize + sizeof(nt_slab_page_t)));

    pool->pages = (nt_slab_page_t *) p;
    nt_memzero(pool->pages, pages * sizeof(nt_slab_page_t));

    page = pool->pages;

    /* only "next" is used in list head */
    pool->free.slab = 0;
    pool->free.next = page;
    pool->free.prev = 0;

    page->slab = pages;
    page->next = &pool->free;
    page->prev = (uintptr_t) &pool->free;

    pool->start = nt_align_ptr(p + pages * sizeof(nt_slab_page_t),
                                nt_pagesize);

    m = pages - (pool->end - pool->start) / nt_pagesize;
    if (m > 0) {
        pages -= m;
        page->slab = pages;
    }

    pool->last = pool->pages + pages;
    pool->pfree = pages;

    pool->log_nomem = 1;
    pool->log_ctx = &pool->zero;
    pool->zero = '\0';
}


void *
nt_slab_alloc(nt_slab_pool_t *pool, size_t size)
{
    void  *p;

    nt_shmtx_lock(&pool->mutex);

    p = nt_slab_alloc_locked(pool, size);

    nt_shmtx_unlock(&pool->mutex);

    return p;
}


void *
nt_slab_alloc_locked(nt_slab_pool_t *pool, size_t size)
{
    size_t            s;
    uintptr_t         p, m, mask, *bitmap;
    nt_uint_t        i, n, slot, shift, map;
    nt_slab_page_t  *page, *prev, *slots;

    if (size > nt_slab_max_size) {

        nt_log_debug1(NT_LOG_DEBUG_ALLOC, nt_cycle->log, 0,
                       "slab alloc: %uz", size);

        page = nt_slab_alloc_pages(pool, (size >> nt_pagesize_shift)
                                          + ((size % nt_pagesize) ? 1 : 0));
        if (page) {
            p = nt_slab_page_addr(pool, page);

        } else {
            p = 0;
        }

        goto done;
    }

    if (size > pool->min_size) {
        shift = 1;
        for (s = size - 1; s >>= 1; shift++) { /* void */ }
        slot = shift - pool->min_shift;

    } else {
        shift = pool->min_shift;
        slot = 0;
    }

    pool->stats[slot].reqs++;

    nt_log_debug2(NT_LOG_DEBUG_ALLOC, nt_cycle->log, 0,
                   "slab alloc: %uz slot: %ui", size, slot);

    slots = nt_slab_slots(pool);
    page = slots[slot].next;

    if (page->next != page) {

        if (shift < nt_slab_exact_shift) {

            bitmap = (uintptr_t *) nt_slab_page_addr(pool, page);

            map = (nt_pagesize >> shift) / (8 * sizeof(uintptr_t));

            for (n = 0; n < map; n++) {

                if (bitmap[n] != NT_SLAB_BUSY) {

                    for (m = 1, i = 0; m; m <<= 1, i++) {
                        if (bitmap[n] & m) {
                            continue;
                        }

                        bitmap[n] |= m;

                        i = (n * 8 * sizeof(uintptr_t) + i) << shift;

                        p = (uintptr_t) bitmap + i;

                        pool->stats[slot].used++;

                        if (bitmap[n] == NT_SLAB_BUSY) {
                            for (n = n + 1; n < map; n++) {
                                if (bitmap[n] != NT_SLAB_BUSY) {
                                    goto done;
                                }
                            }

                            prev = nt_slab_page_prev(page);
                            prev->next = page->next;
                            page->next->prev = page->prev;

                            page->next = NULL;
                            page->prev = NT_SLAB_SMALL;
                        }

                        goto done;
                    }
                }
            }

        } else if (shift == nt_slab_exact_shift) {

            for (m = 1, i = 0; m; m <<= 1, i++) {
                if (page->slab & m) {
                    continue;
                }

                page->slab |= m;

                if (page->slab == NT_SLAB_BUSY) {
                    prev = nt_slab_page_prev(page);
                    prev->next = page->next;
                    page->next->prev = page->prev;

                    page->next = NULL;
                    page->prev = NT_SLAB_EXACT;
                }

                p = nt_slab_page_addr(pool, page) + (i << shift);

                pool->stats[slot].used++;

                goto done;
            }

        } else { /* shift > nt_slab_exact_shift */

            mask = ((uintptr_t) 1 << (nt_pagesize >> shift)) - 1;
            mask <<= NT_SLAB_MAP_SHIFT;

            for (m = (uintptr_t) 1 << NT_SLAB_MAP_SHIFT, i = 0;
                 m & mask;
                 m <<= 1, i++)
            {
                if (page->slab & m) {
                    continue;
                }

                page->slab |= m;

                if ((page->slab & NT_SLAB_MAP_MASK) == mask) {
                    prev = nt_slab_page_prev(page);
                    prev->next = page->next;
                    page->next->prev = page->prev;

                    page->next = NULL;
                    page->prev = NT_SLAB_BIG;
                }

                p = nt_slab_page_addr(pool, page) + (i << shift);

                pool->stats[slot].used++;

                goto done;
            }
        }

        nt_slab_error(pool, NT_LOG_ALERT, "nt_slab_alloc(): page is busy");
        nt_debug_point();
    }

    page = nt_slab_alloc_pages(pool, 1);

    if (page) {
        if (shift < nt_slab_exact_shift) {
            bitmap = (uintptr_t *) nt_slab_page_addr(pool, page);

            n = (nt_pagesize >> shift) / ((1 << shift) * 8);

            if (n == 0) {
                n = 1;
            }

            /* "n" elements for bitmap, plus one requested */

            for (i = 0; i < (n + 1) / (8 * sizeof(uintptr_t)); i++) {
                bitmap[i] = NT_SLAB_BUSY;
            }

            m = ((uintptr_t) 1 << ((n + 1) % (8 * sizeof(uintptr_t)))) - 1;
            bitmap[i] = m;

            map = (nt_pagesize >> shift) / (8 * sizeof(uintptr_t));

            for (i = i + 1; i < map; i++) {
                bitmap[i] = 0;
            }

            page->slab = shift;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | NT_SLAB_SMALL;

            slots[slot].next = page;

            pool->stats[slot].total += (nt_pagesize >> shift) - n;

            p = nt_slab_page_addr(pool, page) + (n << shift);

            pool->stats[slot].used++;

            goto done;

        } else if (shift == nt_slab_exact_shift) {

            page->slab = 1;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | NT_SLAB_EXACT;

            slots[slot].next = page;

            pool->stats[slot].total += 8 * sizeof(uintptr_t);

            p = nt_slab_page_addr(pool, page);

            pool->stats[slot].used++;

            goto done;

        } else { /* shift > nt_slab_exact_shift */

            page->slab = ((uintptr_t) 1 << NT_SLAB_MAP_SHIFT) | shift;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | NT_SLAB_BIG;

            slots[slot].next = page;

            pool->stats[slot].total += nt_pagesize >> shift;

            p = nt_slab_page_addr(pool, page);

            pool->stats[slot].used++;

            goto done;
        }
    }

    p = 0;

    pool->stats[slot].fails++;

done:

    nt_log_debug1(NT_LOG_DEBUG_ALLOC, nt_cycle->log, 0,
                   "slab alloc: %p", (void *) p);

    return (void *) p;
}


void *
nt_slab_calloc(nt_slab_pool_t *pool, size_t size)
{
    void  *p;

    nt_shmtx_lock(&pool->mutex);

    p = nt_slab_calloc_locked(pool, size);

    nt_shmtx_unlock(&pool->mutex);

    return p;
}


void *
nt_slab_calloc_locked(nt_slab_pool_t *pool, size_t size)
{
    void  *p;

    p = nt_slab_alloc_locked(pool, size);
    if (p) {
        nt_memzero(p, size);
    }

    return p;
}


void
nt_slab_free(nt_slab_pool_t *pool, void *p)
{
    nt_shmtx_lock(&pool->mutex);

    nt_slab_free_locked(pool, p);

    nt_shmtx_unlock(&pool->mutex);
}


void
nt_slab_free_locked(nt_slab_pool_t *pool, void *p)
{
    size_t            size;
    uintptr_t         slab, m, *bitmap;
    nt_uint_t        i, n, type, slot, shift, map;
    nt_slab_page_t  *slots, *page;

    nt_log_debug1(NT_LOG_DEBUG_ALLOC, nt_cycle->log, 0, "slab free: %p", p);

    if ((u_char *) p < pool->start || (u_char *) p > pool->end) {
        nt_slab_error(pool, NT_LOG_ALERT, "nt_slab_free(): outside of pool");
        goto fail;
    }

    n = ((u_char *) p - pool->start) >> nt_pagesize_shift;
    page = &pool->pages[n];
    slab = page->slab;
    type = nt_slab_page_type(page);

    switch (type) {

    case NT_SLAB_SMALL:

        shift = slab & NT_SLAB_SHIFT_MASK;
        size = (size_t) 1 << shift;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        n = ((uintptr_t) p & (nt_pagesize - 1)) >> shift;
        m = (uintptr_t) 1 << (n % (8 * sizeof(uintptr_t)));
        n /= 8 * sizeof(uintptr_t);
        bitmap = (uintptr_t *)
                             ((uintptr_t) p & ~((uintptr_t) nt_pagesize - 1));

        if (bitmap[n] & m) {
            slot = shift - pool->min_shift;

            if (page->next == NULL) {
                slots = nt_slab_slots(pool);

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | NT_SLAB_SMALL;
                page->next->prev = (uintptr_t) page | NT_SLAB_SMALL;
            }

            bitmap[n] &= ~m;

            n = (nt_pagesize >> shift) / ((1 << shift) * 8);

            if (n == 0) {
                n = 1;
            }

            i = n / (8 * sizeof(uintptr_t));
            m = ((uintptr_t) 1 << (n % (8 * sizeof(uintptr_t)))) - 1;

            if (bitmap[i] & ~m) {
                goto done;
            }

            map = (nt_pagesize >> shift) / (8 * sizeof(uintptr_t));

            for (i = i + 1; i < map; i++) {
                if (bitmap[i]) {
                    goto done;
                }
            }

            nt_slab_free_pages(pool, page, 1);

            pool->stats[slot].total -= (nt_pagesize >> shift) - n;

            goto done;
        }

        goto chunk_already_free;

    case NT_SLAB_EXACT:

        m = (uintptr_t) 1 <<
                (((uintptr_t) p & (nt_pagesize - 1)) >> nt_slab_exact_shift);
        size = nt_slab_exact_size;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        if (slab & m) {
            slot = nt_slab_exact_shift - pool->min_shift;

            if (slab == NT_SLAB_BUSY) {
                slots = nt_slab_slots(pool);

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | NT_SLAB_EXACT;
                page->next->prev = (uintptr_t) page | NT_SLAB_EXACT;
            }

            page->slab &= ~m;

            if (page->slab) {
                goto done;
            }

            nt_slab_free_pages(pool, page, 1);

            pool->stats[slot].total -= 8 * sizeof(uintptr_t);

            goto done;
        }

        goto chunk_already_free;

    case NT_SLAB_BIG:

        shift = slab & NT_SLAB_SHIFT_MASK;
        size = (size_t) 1 << shift;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        m = (uintptr_t) 1 << ((((uintptr_t) p & (nt_pagesize - 1)) >> shift)
                              + NT_SLAB_MAP_SHIFT);

        if (slab & m) {
            slot = shift - pool->min_shift;

            if (page->next == NULL) {
                slots = nt_slab_slots(pool);

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | NT_SLAB_BIG;
                page->next->prev = (uintptr_t) page | NT_SLAB_BIG;
            }

            page->slab &= ~m;

            if (page->slab & NT_SLAB_MAP_MASK) {
                goto done;
            }

            nt_slab_free_pages(pool, page, 1);

            pool->stats[slot].total -= nt_pagesize >> shift;

            goto done;
        }

        goto chunk_already_free;

    case NT_SLAB_PAGE:

        if ((uintptr_t) p & (nt_pagesize - 1)) {
            goto wrong_chunk;
        }

        if (!(slab & NT_SLAB_PAGE_START)) {
            nt_slab_error(pool, NT_LOG_ALERT,
                           "nt_slab_free(): page is already free");
            goto fail;
        }

        if (slab == NT_SLAB_PAGE_BUSY) {
            nt_slab_error(pool, NT_LOG_ALERT,
                           "nt_slab_free(): pointer to wrong page");
            goto fail;
        }

        size = slab & ~NT_SLAB_PAGE_START;

        nt_slab_free_pages(pool, page, size);

        nt_slab_junk(p, size << nt_pagesize_shift);

        return;
    }

    /* not reached */

    return;

done:

    pool->stats[slot].used--;

    nt_slab_junk(p, size);

    return;

wrong_chunk:

    nt_slab_error(pool, NT_LOG_ALERT,
                   "nt_slab_free(): pointer to wrong chunk");

    goto fail;

chunk_already_free:

    nt_slab_error(pool, NT_LOG_ALERT,
                   "nt_slab_free(): chunk is already free");

fail:

    return;
}


static nt_slab_page_t *
nt_slab_alloc_pages(nt_slab_pool_t *pool, nt_uint_t pages)
{
    nt_slab_page_t  *page, *p;

    for (page = pool->free.next; page != &pool->free; page = page->next) {

        if (page->slab >= pages) {

            if (page->slab > pages) {
                page[page->slab - 1].prev = (uintptr_t) &page[pages];

                page[pages].slab = page->slab - pages;
                page[pages].next = page->next;
                page[pages].prev = page->prev;

                p = (nt_slab_page_t *) page->prev;
                p->next = &page[pages];
                page->next->prev = (uintptr_t) &page[pages];

            } else {
                p = (nt_slab_page_t *) page->prev;
                p->next = page->next;
                page->next->prev = page->prev;
            }

            page->slab = pages | NT_SLAB_PAGE_START;
            page->next = NULL;
            page->prev = NT_SLAB_PAGE;

            pool->pfree -= pages;

            if (--pages == 0) {
                return page;
            }

            for (p = page + 1; pages; pages--) {
                p->slab = NT_SLAB_PAGE_BUSY;
                p->next = NULL;
                p->prev = NT_SLAB_PAGE;
                p++;
            }

            return page;
        }
    }

    if (pool->log_nomem) {
        nt_slab_error(pool, NT_LOG_CRIT,
                       "nt_slab_alloc() failed: no memory");
    }

    return NULL;
}


static void
nt_slab_free_pages(nt_slab_pool_t *pool, nt_slab_page_t *page,
    nt_uint_t pages)
{
    nt_slab_page_t  *prev, *join;

    pool->pfree += pages;

    page->slab = pages--;

    if (pages) {
        nt_memzero(&page[1], pages * sizeof(nt_slab_page_t));
    }

    if (page->next) {
        prev = nt_slab_page_prev(page);
        prev->next = page->next;
        page->next->prev = page->prev;
    }

    join = page + page->slab;

    if (join < pool->last) {

        if (nt_slab_page_type(join) == NT_SLAB_PAGE) {

            if (join->next != NULL) {
                pages += join->slab;
                page->slab += join->slab;

                prev = nt_slab_page_prev(join);
                prev->next = join->next;
                join->next->prev = join->prev;

                join->slab = NT_SLAB_PAGE_FREE;
                join->next = NULL;
                join->prev = NT_SLAB_PAGE;
            }
        }
    }

    if (page > pool->pages) {
        join = page - 1;

        if (nt_slab_page_type(join) == NT_SLAB_PAGE) {

            if (join->slab == NT_SLAB_PAGE_FREE) {
                join = nt_slab_page_prev(join);
            }

            if (join->next != NULL) {
                pages += join->slab;
                join->slab += page->slab;

                prev = nt_slab_page_prev(join);
                prev->next = join->next;
                join->next->prev = join->prev;

                page->slab = NT_SLAB_PAGE_FREE;
                page->next = NULL;
                page->prev = NT_SLAB_PAGE;

                page = join;
            }
        }
    }

    if (pages) {
        page[pages].prev = (uintptr_t) page;
    }

    page->prev = (uintptr_t) &pool->free;
    page->next = pool->free.next;

    page->next->prev = (uintptr_t) page;

    pool->free.next = page;
}


static void
nt_slab_error(nt_slab_pool_t *pool, nt_uint_t level, char *text)
{
    nt_log_error(level, nt_cycle->log, 0, "%s%s", text, pool->log_ctx);
}
