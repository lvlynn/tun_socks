

#ifndef _NT_SLAB_H_INCLUDED_
#define _NT_SLAB_H_INCLUDED_


#include <nt_core.h>


typedef struct nt_slab_page_s  nt_slab_page_t;

struct nt_slab_page_s {
    uintptr_t         slab;
    nt_slab_page_t  *next;
    uintptr_t         prev;
};


typedef struct {
    nt_uint_t        total;
    nt_uint_t        used;

    nt_uint_t        reqs;
    nt_uint_t        fails;
} nt_slab_stat_t;


typedef struct {
    nt_shmtx_sh_t    lock;

    size_t            min_size;
    size_t            min_shift;

    nt_slab_page_t  *pages;
    nt_slab_page_t  *last;
    nt_slab_page_t   free;

    nt_slab_stat_t  *stats;
    nt_uint_t        pfree;

    u_char           *start;
    u_char           *end;

    nt_shmtx_t       mutex;

    u_char           *log_ctx;
    u_char            zero;

    unsigned          log_nomem:1;

    void             *data;
    void             *addr;
} nt_slab_pool_t;


void nt_slab_sizes_init(void);
void nt_slab_init(nt_slab_pool_t *pool);
void *nt_slab_alloc(nt_slab_pool_t *pool, size_t size);
void *nt_slab_alloc_locked(nt_slab_pool_t *pool, size_t size);
void *nt_slab_calloc(nt_slab_pool_t *pool, size_t size);
void *nt_slab_calloc_locked(nt_slab_pool_t *pool, size_t size);
void nt_slab_free(nt_slab_pool_t *pool, void *p);
void nt_slab_free_locked(nt_slab_pool_t *pool, void *p);


#endif /* _NT_SLAB_H_INCLUDED_ */
