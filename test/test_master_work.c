#include <nt_core.h>
#include "test_common.h"
extern nt_module_t  nt_stream_socks5_module;

int main( int argc, char *const *argv )
{
    signal( SIGTERM,  tm_app_exit );
    signal( SIGHUP,  tm_app_exit );
    signal( SIGTSTP,  tm_app_exit );
    signal( SIGINT,  tm_app_exit );

    nt_log_t *log ;
    nt_pool_t *pool;
    nt_cycle_t *cycle;


    /* debug( "nt_stream_socks5_module=%p" , nt_stream_socks5_module); */

    if( nt_main_init( argc, argv ) != NT_OK )
        return 1;

#if 0
    if( tm_common_init() != NT_OK )
        return 0;

    cycle = nt_cycle;
    log = nt_cycle->log;
    pool = nt_cycle->pool;

    nt_save_argv( cycle, argc, argv );
    /* nt_os_argv = ( char **  ) argv; */
    nt_init_setproctitle( log );
    /* nt_setproctitle("test_master"); //更改主进程的进程名 */

    nt_master_process_cycle( cycle );
    while( 1 ) {}

    tm_app_exit();
#endif
    return 0;
}
