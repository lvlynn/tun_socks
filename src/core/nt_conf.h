#ifndef _NT_CONF_H_
#define _NT_CONF_H_

#include <nt_def.h>
#include <nt_core.h>


/*
 *        AAAA  number of arguments
 *      FF      command flags
 *    TT        command type, i.e. HTTP "location" or "server" command
 */

#define NT_CONF_NOARGS      0x00000001
#define NT_CONF_TAKE1       0x00000002
#define NT_CONF_TAKE2       0x00000004
#define NT_CONF_TAKE3       0x00000008
#define NT_CONF_TAKE4       0x00000010
#define NT_CONF_TAKE5       0x00000020
#define NT_CONF_TAKE6       0x00000040
#define NT_CONF_TAKE7       0x00000080

#define NT_CONF_MAX_ARGS    8

#define NT_CONF_TAKE12      (NT_CONF_TAKE1|NT_CONF_TAKE2)
#define NT_CONF_TAKE13      (NT_CONF_TAKE1|NT_CONF_TAKE3)

#define NT_CONF_TAKE23      (NT_CONF_TAKE2|NT_CONF_TAKE3)

#define NT_CONF_TAKE123     (NT_CONF_TAKE1|NT_CONF_TAKE2|NT_CONF_TAKE3)
#define NT_CONF_TAKE1234    (NT_CONF_TAKE1|NT_CONF_TAKE2|NT_CONF_TAKE3   \
                              |NT_CONF_TAKE4)

#define NT_CONF_ARGS_NUMBER 0x000000ff
#define NT_CONF_BLOCK       0x00000100
#define NT_CONF_FLAG        0x00000200
#define NT_CONF_ANY         0x00000400
#define NT_CONF_1MORE       0x00000800
#define NT_CONF_2MORE       0x00001000

#define NT_DIRECT_CONF      0x00010000

#define NT_MAIN_CONF        0x01000000
#define NT_ANY_CONF         0x1F000000



#define NT_CONF_UNSET       -1
#define NT_CONF_UNSET_UINT  (nt_uint_t) -1
#define NT_CONF_UNSET_PTR   (void *) -1
#define NT_CONF_UNSET_SIZE  (size_t) -1
#define NT_CONF_UNSET_MSEC  (nt_msec_t) -1


#define NT_CONF_OK          NULL
#define NT_CONF_ERROR       (void *) -1

#define NT_CONF_BLOCK_START 1
#define NT_CONF_BLOCK_DONE  2
#define NT_CONF_FILE_DONE   3

#define NT_CORE_MODULE      0x45524F43  /* "CORE" */
#define NT_CONF_MODULE      0x464E4F43  /* "CONF" */


#define NT_MAX_CONF_ERRSTR  1024


struct nt_command_s {
    nt_str_t             name;
    nt_uint_t            type;
    char               *(*set)(nt_conf_t *cf, nt_command_t *cmd, void *conf);
    nt_uint_t            conf;
    nt_uint_t            offset;
    void                 *post;
};

#define nt_null_command  { nt_null_string, 0, NULL, 0, 0, NULL }


struct nt_open_file_s {
    nt_fd_t              fd;
    nt_str_t             name;

    void                (*flush)(nt_open_file_t *file, nt_log_t *log);
    void                 *data;
};


typedef struct {
    nt_file_t            file;
    nt_buf_t            *buffer;
    nt_buf_t            *dump;
    nt_uint_t            line;
} nt_conf_file_t;


typedef struct {
    nt_str_t             name;
    nt_buf_t            *buffer;
} nt_conf_dump_t;


typedef char *(*nt_conf_handler_pt)(nt_conf_t *cf,
    nt_command_t *dummy, void *conf);


struct nt_conf_s {
    char                 *name;
    nt_array_t          *args;

    nt_cycle_t          *cycle;
    nt_pool_t           *pool;
    nt_pool_t           *temp_pool;
    nt_conf_file_t      *conf_file;
    nt_log_t            *log;

    void                 *ctx;
    nt_uint_t            module_type;
    nt_uint_t            cmd_type;

    nt_conf_handler_pt   handler;
    void                 *handler_conf;
#if (NT_SSL && NT_SSL_ASYNC)
    nt_flag_t            no_ssl_init;
#endif
};


typedef char *(*nt_conf_post_handler_pt) (nt_conf_t *cf,
    void *data, void *conf);

typedef struct {
    nt_conf_post_handler_pt  post_handler;
} nt_conf_post_t;


typedef struct {
    nt_conf_post_handler_pt  post_handler;
    char                     *old_name;
    char                     *new_name;
} nt_conf_deprecated_t;


typedef struct {
    nt_conf_post_handler_pt  post_handler;
    nt_int_t                 low;
    nt_int_t                 high;
} nt_conf_num_bounds_t;


typedef struct {
    nt_str_t                 name;
    nt_uint_t                value;
} nt_conf_enum_t;


#define NT_CONF_BITMASK_SET  1

typedef struct {
    nt_str_t                 name;
    nt_uint_t                mask;
} nt_conf_bitmask_t;



char * nt_conf_deprecated(nt_conf_t *cf, void *post, void *data);
char *nt_conf_check_num_bounds(nt_conf_t *cf, void *post, void *data);


//得到module对应的配置
#define nt_get_conf(conf_ctx, module)  conf_ctx[module.index]



#define nt_conf_init_value(conf, default)                                   \
    if (conf == NT_CONF_UNSET) {                                            \
        conf = default;                                                      \
    }

#define nt_conf_init_ptr_value(conf, default)                               \
    if (conf == NT_CONF_UNSET_PTR) {                                        \
        conf = default;                                                      \
    }

#define nt_conf_init_uint_value(conf, default)                              \
    if (conf == NT_CONF_UNSET_UINT) {                                       \
        conf = default;                                                      \
    }

#define nt_conf_init_size_value(conf, default)                              \
    if (conf == NT_CONF_UNSET_SIZE) {                                       \
        conf = default;                                                      \
    }

#define nt_conf_init_msec_value(conf, default)                              \
    if (conf == NT_CONF_UNSET_MSEC) {                                       \
        conf = default;                                                      \
    }

#define nt_conf_merge_value(conf, prev, default)                            \
    if (conf == NT_CONF_UNSET) {                                            \
        conf = (prev == NT_CONF_UNSET) ? default : prev;                    \
    }

#define nt_conf_merge_ptr_value(conf, prev, default)                        \
    if (conf == NT_CONF_UNSET_PTR) {                                        \
        conf = (prev == NT_CONF_UNSET_PTR) ? default : prev;                \
    }

#define nt_conf_merge_uint_value(conf, prev, default)                       \
    if (conf == NT_CONF_UNSET_UINT) {                                       \
        conf = (prev == NT_CONF_UNSET_UINT) ? default : prev;               \
    }

#define nt_conf_merge_msec_value(conf, prev, default)                       \
    if (conf == NT_CONF_UNSET_MSEC) {                                       \
        conf = (prev == NT_CONF_UNSET_MSEC) ? default : prev;               \
    }

#define nt_conf_merge_sec_value(conf, prev, default)                        \
    if (conf == NT_CONF_UNSET) {                                            \
        conf = (prev == NT_CONF_UNSET) ? default : prev;                    \
    }

#define nt_conf_merge_size_value(conf, prev, default)                       \
    if (conf == NT_CONF_UNSET_SIZE) {                                       \
        conf = (prev == NT_CONF_UNSET_SIZE) ? default : prev;               \
    }

#define nt_conf_merge_off_value(conf, prev, default)                        \
    if (conf == NT_CONF_UNSET) {                                            \
        conf = (prev == NT_CONF_UNSET) ? default : prev;                    \
    }

#define nt_conf_merge_str_value(conf, prev, default)                        \
    if (conf.data == NULL) {                                                 \
        if (prev.data) {                                                     \
            conf.len = prev.len;                                             \
            conf.data = prev.data;                                           \
        } else {                                                             \
            conf.len = sizeof(default) - 1;                                  \
            conf.data = (u_char *) default;                                  \
        }                                                                    \
    }

#define nt_conf_merge_bufs_value(conf, prev, default_num, default_size)     \
    if (conf.num == 0) {                                                     \
        if (prev.num) {                                                      \
            conf.num = prev.num;                                             \
            conf.size = prev.size;                                           \
        } else {                                                             \
            conf.num = default_num;                                          \
            conf.size = default_size;                                        \
        }                                                                    \
    }

#define nt_conf_merge_bitmask_value(conf, prev, default)                    \
    if (conf == 0) {                                                         \
        conf = (prev == 0) ? default : prev;                                 \
    }


char *nt_conf_param(nt_conf_t *cf);
char *nt_conf_parse(nt_conf_t *cf, nt_str_t *filename);
char *nt_conf_include(nt_conf_t *cf, nt_command_t *cmd, void *conf);


nt_int_t nt_conf_full_name(nt_cycle_t *cycle, nt_str_t *name,
    nt_uint_t conf_prefix);
nt_open_file_t *nt_conf_open_file(nt_cycle_t *cycle, nt_str_t *name);
void nt_cdecl nt_conf_log_error(nt_uint_t level, nt_conf_t *cf,
    nt_err_t err, const char *fmt, ...);


char *nt_conf_set_flag_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf);
char *nt_conf_set_str_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf);
char *nt_conf_set_str_array_slot(nt_conf_t *cf, nt_command_t *cmd,
    void *conf);
char *nt_conf_set_keyval_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf);
char *nt_conf_set_num_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf);
char *nt_conf_set_size_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf);
char *nt_conf_set_off_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf);
char *nt_conf_set_msec_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf);
char *nt_conf_set_sec_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf);
char *nt_conf_set_bufs_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf);
char *nt_conf_set_enum_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf);
char *nt_conf_set_bitmask_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf);
#endif

