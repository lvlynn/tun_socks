#ifndef _NT_CYCLE_H_
#define _NT_CYCLE_H_

#include <nt_core.h>

#ifndef NT_CYCLE_POOL_SIZE
#define NT_CYCLE_POOL_SIZE     NT_DEFAULT_POOL_SIZE                                                                         
#endif

//用于存储 大循环配置
struct nt_cycle_s {

    void                  ****conf_ctx;  //配置
    nt_pool_t               *pool;

    nt_log_t                *log;
    nt_log_t                 new_log;

    nt_uint_t                log_use_stderr;  /* unsigned  log_use_stderr:1; */

    nt_connection_t        **files;
    nt_connection_t         *free_connections;
    nt_uint_t                free_connection_n;

    nt_module_t            **modules;
    nt_uint_t                modules_n;
    nt_uint_t                modules_used;    /* unsigned  modules_used:1; */

    //   nt_process_event;
    nt_queue_t               reusable_connections_queue;
    nt_uint_t                reusable_connections_n;

    nt_array_t               listening;
    nt_array_t               paths;

    nt_array_t               config_dump;
    nt_rbtree_t              config_dump_rbtree;
    nt_rbtree_node_t         config_dump_sentinel;

    nt_list_t                open_files;
    nt_list_t                shared_memory;

    nt_uint_t                connection_n;
    nt_uint_t                files_n;

    nt_connection_t         *connections;
    nt_event_t              *read_events;
    nt_event_t              *write_events;
    #if (NT_SSL && NT_SSL_ASYNC)
    nt_event_t              *async_events;
    #endif

    nt_cycle_t              *old_cycle;

    nt_str_t                 conf_file;
    nt_str_t                 conf_param;
    nt_str_t                 conf_prefix;
    nt_str_t                 prefix;
    nt_str_t                 lock_file;
    nt_str_t                 hostname;
    #if (NT_SSL && NT_SSL_ASYNC)
    nt_flag_t                no_ssl_init;
    #endif

};

extern volatile nt_cycle_t  *nt_cycle;

extern nt_uint_t             nt_test_config;
extern nt_uint_t             nt_dump_config;
#endif
