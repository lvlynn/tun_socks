
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NT_PALLOC_H_INCLUDED_
#define _NT_PALLOC_H_INCLUDED_


#include <nt_def.h>
#include <nt_core.h>


/*
 * NT_MAX_ALLOC_FROM_POOL should be (nt_pagesize - 1), i.e. 4095 on x86.
 * On Windows NT it decreases a number of locked pages in a kernel.
 */
#define NT_MAX_ALLOC_FROM_POOL  (nt_pagesize - 1)

#define NT_DEFAULT_POOL_SIZE    (16 * 1024)

#define NT_POOL_ALIGNMENT       16
#define NT_MIN_POOL_SIZE                                                     \
    nt_align((sizeof(nt_pool_t) + 2 * sizeof(nt_pool_large_t)),            \
              NT_POOL_ALIGNMENT)


typedef void (*nt_pool_cleanup_pt)(void *data);

typedef struct nt_pool_cleanup_s  nt_pool_cleanup_t;

struct nt_pool_cleanup_s {
    nt_pool_cleanup_pt   handler;
    void                 *data;
    nt_pool_cleanup_t   *next;
};


typedef struct nt_pool_large_s  nt_pool_large_t;

struct nt_pool_large_s {
    nt_pool_large_t     *next;
    void                 *alloc;
};


typedef struct {
    u_char               *last;
    u_char               *end;
    nt_pool_t           *next;
    nt_uint_t            failed;
} nt_pool_data_t;


struct nt_pool_s {
    nt_pool_data_t       d;
    size_t                max;
    nt_pool_t           *current;
    nt_chain_t          *chain;
    nt_pool_large_t     *large;
    nt_pool_cleanup_t   *cleanup;
    nt_log_t            *log;
};


typedef struct {
    nt_fd_t              fd;
    u_char               *name;
    nt_log_t            *log;
} nt_pool_cleanup_file_t;

#if (T_DEPRECATED)
void *nt_prealloc(nt_pool_t *pool, void *p, size_t old_size, size_t new_size);
#endif

nt_pool_t *nt_create_pool(size_t size, nt_log_t *log);
void nt_destroy_pool(nt_pool_t *pool);
void nt_reset_pool(nt_pool_t *pool);

void *nt_palloc(nt_pool_t *pool, size_t size);
void *nt_pnalloc(nt_pool_t *pool, size_t size);
void *nt_pcalloc(nt_pool_t *pool, size_t size);
void *nt_pmemalign(nt_pool_t *pool, size_t size, size_t alignment);
nt_int_t nt_pfree(nt_pool_t *pool, void *p);


nt_pool_cleanup_t *nt_pool_cleanup_add(nt_pool_t *p, size_t size);
void nt_pool_run_cleanup_file(nt_pool_t *p, nt_fd_t fd);
void nt_pool_cleanup_file(void *data);
void nt_pool_delete_file(void *data);


#endif /* _NT_PALLOC_H_INCLUDED_ */
