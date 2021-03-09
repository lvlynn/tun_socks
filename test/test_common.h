#ifndef _TEST_COMMON_H_
#define _TEST_COMMON_H_

#include "nt_core.h"


nt_log_t *tm_init_log(){
    nt_log_t *log;

    log = nt_log_init( NULL );
    log->log_level = NT_LOG_DEBUG_ALL;

    nt_log_debug1( NT_LOG_DEBUG_EVENT, log, 0,
                   "timer delta: %M", nt_current_msec );

    return log;
}

static nt_cycle_t      nt_exit_cycle;
static nt_log_t        nt_exit_log;
static nt_open_file_t  nt_exit_log_file;
void tm_app_exit(){
    debug( "tm_app_exit" ); 
    if( nt_cycle != NULL ) {
        nt_cycle_t *cycle;
        cycle = nt_cycle;
        nt_exit_log = *nt_log_get_file_log(nt_cycle->log);

        nt_exit_log_file.fd = nt_exit_log.file->fd;
        nt_exit_log.file = &nt_exit_log_file;
        nt_exit_log.next = NULL;
        nt_exit_log.writer = NULL;

        nt_exit_cycle.log = &nt_exit_log;
        nt_exit_cycle.files = nt_cycle->files;
        nt_exit_cycle.files_n = nt_cycle->files_n;
        nt_cycle = &nt_exit_cycle;


        nt_destroy_pool(cycle->pool);
        // nt_destroy_pool( nt_cycle->pool );
        // nt_master_process_exit( nt_cycle );
        exit( 0 );
        return ;
    }
}


nt_int_t tm_common_init(){
    nt_log_t *log;
    nt_cycle_t *cycle;
    nt_pool_t *pool;

    nt_time_init();

    log = tm_init_log();

    //初始化一个内存池
    pool = nt_create_pool( NT_CYCLE_POOL_SIZE * 10, log );
    if( pool == NULL ) {
        return NT_ERROR;
    }
    pool->log = log;

    //从内存池中申请cycle
    cycle = nt_pcalloc( pool, sizeof( nt_cycle_t ) );
    if( cycle == NULL ) {
        nt_destroy_pool( pool );
        return NT_ERROR;
    }

    cycle->log = log;
    cycle->pool = pool;
    nt_cycle = cycle;
    return NT_OK;
}

#endif

