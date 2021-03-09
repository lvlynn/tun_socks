#ifndef _NT_CYCLE_H_
#define _NT_CYCLE_H_

#include <nt_core.h>

#ifndef NT_CYCLE_POOL_SIZE
    #define NT_CYCLE_POOL_SIZE     NT_DEFAULT_POOL_SIZE
#endif


#define NT_DEBUG_POINTS_STOP   1
#define NT_DEBUG_POINTS_ABORT  2


typedef struct nt_shm_zone_s  nt_shm_zone_t;

typedef nt_int_t ( *nt_shm_zone_init_pt )( nt_shm_zone_t *zone, void *data );


struct nt_shm_zone_s {
    void                     *data;
    nt_shm_t                 shm;
    nt_shm_zone_init_pt      init;
    void                     *tag;
    void                     *sync;
    nt_uint_t                noreuse;  /* unsigned  noreuse:1; */
};

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


typedef struct {
    nt_flag_t                daemon;
    nt_flag_t                master;

    nt_msec_t                timer_resolution;
    nt_msec_t                shutdown_timeout;

    nt_int_t                 worker_processes;
    nt_int_t                 debug_points;

    nt_int_t                 rlimit_nofile;
    off_t                     rlimit_core;

    int                       priority;

    nt_uint_t                cpu_affinity_auto;
    nt_uint_t                cpu_affinity_n;
    nt_cpuset_t             *cpu_affinity;

    char                     *username;
    nt_uid_t                 user;
    nt_gid_t                 group;

    nt_str_t                 working_directory;
    nt_str_t                 lock_file;

    nt_str_t                 pid;
    nt_str_t                 oldpid;

    nt_array_t               env;
    char                    **environment;

    nt_uint_t                transparent;  /* unsigned  transparent:1; */
} nt_core_conf_t;


#define nt_is_init_cycle(cycle)  (cycle->conf_ctx == NULL)


nt_cycle_t *nt_init_cycle( nt_cycle_t *old_cycle );
nt_int_t nt_create_pidfile( nt_str_t *name, nt_log_t *log );
void nt_delete_pidfile( nt_cycle_t *cycle );
nt_int_t nt_signal_process( nt_cycle_t *cycle, char *sig );
void nt_reopen_files( nt_cycle_t *cycle, nt_uid_t user );
char **nt_set_environment( nt_cycle_t *cycle, nt_uint_t *last );
nt_pid_t nt_exec_new_binary( nt_cycle_t *cycle, char *const *argv );
nt_cpuset_t *nt_get_cpu_affinity( nt_uint_t n );
nt_shm_zone_t *nt_shared_memory_add( nt_conf_t *cf, nt_str_t *name,
                                     size_t size, void *tag );
void nt_set_shutdown_timer( nt_cycle_t *cycle );


extern volatile nt_cycle_t  *nt_cycle;
extern nt_array_t            nt_old_cycles;
extern nt_module_t           nt_core_module;
extern nt_uint_t             nt_test_config;
extern nt_uint_t             nt_dump_config;
extern nt_uint_t             nt_quiet_mode;
#if (T_NT_SHOW_INFO)
    extern nt_uint_t             nt_show_modules;
    extern nt_uint_t             nt_show_directives;
#endif

#endif
