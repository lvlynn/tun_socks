

#include <nt_core.h>


#ifndef _NT_OPEN_FILE_CACHE_H_INCLUDED_
#define _NT_OPEN_FILE_CACHE_H_INCLUDED_


#define NT_OPEN_FILE_DIRECTIO_OFF  NT_MAX_OFF_T_VALUE


typedef struct {
    nt_fd_t                 fd;
    nt_file_uniq_t          uniq;
    time_t                   mtime;
    off_t                    size;
    off_t                    fs_size;
    off_t                    directio;
    size_t                   read_ahead;

    nt_err_t                err;
    char                    *failed;

    time_t                   valid;

    nt_uint_t               min_uses;

#if (NT_HAVE_OPENAT)
    size_t                   disable_symlinks_from;
    unsigned                 disable_symlinks:2;
#endif

    unsigned                 test_dir:1;
    unsigned                 test_only:1;
    unsigned                 log:1;
    unsigned                 errors:1;
    unsigned                 events:1;

    unsigned                 is_dir:1;
    unsigned                 is_file:1;
    unsigned                 is_link:1;
    unsigned                 is_exec:1;
    unsigned                 is_directio:1;
} nt_open_file_info_t;


typedef struct nt_cached_open_file_s  nt_cached_open_file_t;

struct nt_cached_open_file_s {
    nt_rbtree_node_t        node;
    nt_queue_t              queue;

    u_char                  *name;
    time_t                   created;
    time_t                   accessed;

    nt_fd_t                 fd;
    nt_file_uniq_t          uniq;
    time_t                   mtime;
    off_t                    size;
    nt_err_t                err;

    uint32_t                 uses;

#if (NT_HAVE_OPENAT)
    size_t                   disable_symlinks_from;
    unsigned                 disable_symlinks:2;
#endif

    unsigned                 count:24;
    unsigned                 close:1;
    unsigned                 use_event:1;

    unsigned                 is_dir:1;
    unsigned                 is_file:1;
    unsigned                 is_link:1;
    unsigned                 is_exec:1;
    unsigned                 is_directio:1;

    nt_event_t             *event;
};


typedef struct {
    nt_rbtree_t             rbtree;
    nt_rbtree_node_t        sentinel;
    nt_queue_t              expire_queue;

    nt_uint_t               current;
    nt_uint_t               max;
    time_t                   inactive;
} nt_open_file_cache_t;


typedef struct {
    nt_open_file_cache_t   *cache;
    nt_cached_open_file_t  *file;
    nt_uint_t               min_uses;
    nt_log_t               *log;
} nt_open_file_cache_cleanup_t;


typedef struct {

    /* nt_connection_t stub to allow use c->fd as event ident */
    void                    *data;
    nt_event_t             *read;
    nt_event_t             *write;
    nt_fd_t                 fd;

    nt_cached_open_file_t  *file;
    nt_open_file_cache_t   *cache;
} nt_open_file_cache_event_t;


nt_open_file_cache_t *nt_open_file_cache_init(nt_pool_t *pool,
    nt_uint_t max, time_t inactive);
nt_int_t nt_open_cached_file(nt_open_file_cache_t *cache, nt_str_t *name,
    nt_open_file_info_t *of, nt_pool_t *pool);


#endif /* _NT_OPEN_FILE_CACHE_H_INCLUDED_ */
