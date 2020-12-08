

#ifndef _NT_FILE_H_INCLUDED_
#define _NT_FILE_H_INCLUDED_


#include <nt_core.h>


struct nt_file_s {
    nt_fd_t                   fd;
    nt_str_t                  name;
    nt_file_info_t            info;

    off_t                      offset;
    off_t                      sys_offset;

    nt_log_t                 *log;

#if (NT_THREADS || NT_COMPAT)
    nt_int_t                (*thread_handler)(nt_thread_task_t *task,
                                               nt_file_t *file);
    void                      *thread_ctx;
    nt_thread_task_t         *thread_task;
#endif

#if (NT_HAVE_FILE_AIO || NT_COMPAT)
    nt_event_aio_t           *aio;
#endif

    unsigned                   valid_info:1;
    unsigned                   directio:1;
};


#define NT_MAX_PATH_LEVEL  3


typedef nt_msec_t (*nt_path_manager_pt) (void *data);
typedef nt_msec_t (*nt_path_purger_pt) (void *data);
typedef void (*nt_path_loader_pt) (void *data);


typedef struct {
    nt_str_t                  name;
    size_t                     len;
    size_t                     level[NT_MAX_PATH_LEVEL];

    nt_path_manager_pt        manager;
    nt_path_purger_pt         purger;
    nt_path_loader_pt         loader;
    void                      *data;

    u_char                    *conf_file;
    nt_uint_t                 line;
} nt_path_t;


typedef struct {
    nt_str_t                  name;
    size_t                     level[NT_MAX_PATH_LEVEL];
} nt_path_init_t;


typedef struct {
    nt_file_t                 file;
    off_t                      offset;
    nt_path_t                *path;
    nt_pool_t                *pool;
    char                      *warn;

    nt_uint_t                 access;

    unsigned                   log_level:8;
    unsigned                   persistent:1;
    unsigned                   clean:1;
    unsigned                   thread_write:1;
} nt_temp_file_t;


typedef struct {
    nt_uint_t                 access;
    nt_uint_t                 path_access;
    time_t                     time;
    nt_fd_t                   fd;

    unsigned                   create_path:1;
    unsigned                   delete_file:1;

    nt_log_t                 *log;
} nt_ext_rename_file_t;


typedef struct {
    off_t                      size;
    size_t                     buf_size;

    nt_uint_t                 access;
    time_t                     time;

    nt_log_t                 *log;
} nt_copy_file_t;


typedef struct nt_tree_ctx_s  nt_tree_ctx_t;

typedef nt_int_t (*nt_tree_init_handler_pt) (void *ctx, void *prev);
typedef nt_int_t (*nt_tree_handler_pt) (nt_tree_ctx_t *ctx, nt_str_t *name);

struct nt_tree_ctx_s {
    off_t                      size;
    off_t                      fs_size;
    nt_uint_t                 access;
    time_t                     mtime;

    nt_tree_init_handler_pt   init_handler;
    nt_tree_handler_pt        file_handler;
    nt_tree_handler_pt        pre_tree_handler;
    nt_tree_handler_pt        post_tree_handler;
    nt_tree_handler_pt        spec_handler;

    void                      *data;
    size_t                     alloc;

    nt_log_t                 *log;
};


nt_int_t nt_get_full_name(nt_pool_t *pool, nt_str_t *prefix,
    nt_str_t *name);

ssize_t nt_write_chain_to_temp_file(nt_temp_file_t *tf, nt_chain_t *chain);
nt_int_t nt_create_temp_file(nt_file_t *file, nt_path_t *path,
    nt_pool_t *pool, nt_uint_t persistent, nt_uint_t clean,
    nt_uint_t access);
void nt_create_hashed_filename(nt_path_t *path, u_char *file, size_t len);
nt_int_t nt_create_path(nt_file_t *file, nt_path_t *path);
nt_err_t nt_create_full_path(u_char *dir, nt_uint_t access);
nt_int_t nt_add_path(nt_conf_t *cf, nt_path_t **slot);
nt_int_t nt_create_paths(nt_cycle_t *cycle, nt_uid_t user);
nt_int_t nt_ext_rename_file(nt_str_t *src, nt_str_t *to,
    nt_ext_rename_file_t *ext);
nt_int_t nt_copy_file(u_char *from, u_char *to, nt_copy_file_t *cf);
nt_int_t nt_walk_tree(nt_tree_ctx_t *ctx, nt_str_t *tree);

//nt_atomic_uint_t nt_next_temp_number(nt_uint_t collision);
uint32_t nt_next_temp_number(nt_uint_t collision);


char *nt_conf_set_path_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf);
char *nt_conf_merge_path_value(nt_conf_t *cf, nt_path_t **path,
    nt_path_t *prev, nt_path_init_t *init);
char *nt_conf_set_access_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf);


//extern nt_atomic_t      *nt_temp_number;
//extern nt_atomic_int_t   nt_random_number;


#endif /* _NT_FILE_H_INCLUDED_ */
