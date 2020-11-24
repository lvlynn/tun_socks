#include <nt_def.h>
#include <nt_core.h>

//static char *nt_error_log( nt_conf_t *cf, nt_command_t *cmd, void *conf );
//static char *nt_log_set_levels( nt_conf_t *cf, nt_log_t *log );
//static void nt_log_insert( nt_log_t *log, nt_log_t *new_log );

#if (NT_DEBUG)

static void nt_log_memory_writer( nt_log_t *log, nt_uint_t level,
                                  u_char *buf, size_t len );
static void nt_log_memory_cleanup( void *data );


typedef struct {
    u_char        *start;
    u_char        *end;
    u_char        *pos;
    nt_atomic_t   written;

} nt_log_memory_buf_t;

#endif


static nt_str_t err_levels[] = {
    nt_null_string,
    nt_string( "emerg" ),
    nt_string( "alert" ),
    nt_string( "crit" ),
    nt_string( "error" ),
    nt_string( "warn" ),
    nt_string( "notice" ),
    nt_string( "info" ),
    nt_string( "debug" )
};


static nt_log_t        nt_log;
static nt_open_file_t  nt_log_file;
nt_uint_t              nt_use_stderr = 1;


#if !(NT_HAVE_VARIADIC_MACROS)

void
nt_log_error_core( nt_uint_t level, nt_log_t *log, nt_err_t err,
                   const char *fmt, va_list args )
{
#if 0
    u_char      *p, *last, *msg;
    ssize_t      n;
    nt_uint_t   wrote_stderr, debug_connection;
    u_char       errstr[NT_MAX_ERROR_STR];

    last = errstr + NT_MAX_ERROR_STR;

    p = nt_cpymem( errstr, nt_cached_err_log_time.data,
                   nt_cached_err_log_time.len );

    p = nt_slprintf( p, last, " [%V] ", &err_levels[level] );

    /* pid#tid */
    //#define NT_TID_T_FMT         "%P"
#define nt_log_tid           0
#define NT_TID_T_FMT         "%d"
    p = nt_slprintf( p, last, "%P#" NT_TID_T_FMT ": ",
                     nt_log_pid, nt_log_tid );

    if( log->connection ) {
        p = nt_slprintf( p, last, "*%uA ", log->connection );

    }

    msg = p;

    p = nt_vslprintf( p, last, fmt, args );
    if( err ) {
        p = nt_log_errno( p, last, err );
    }

    if( level != NT_LOG_DEBUG && log->handler ) {
        p = log->handler( log, p, last - p );
    }

    if( p > last - NT_LINEFEED_SIZE ) {
        p = last - NT_LINEFEED_SIZE;
    }

    nt_linefeed( p );

    wrote_stderr = 0;
    debug_connection = ( log->log_level & NT_LOG_DEBUG_CONNECTION ) != 0;


    while( log ) {

        if( log->log_level < level && !debug_connection ) {
            break;
        }

        if( log->writer ) {
            log->writer( log, level, errstr, p - errstr );
            goto next;
        }

        if( nt_time() == log->disk_full_time ) {

            /*
             * on FreeBSD writing to a full filesystem with enabled softupdates
             * may block process for much longer time than writing to non-full
             * filesystem, so we skip writing to a log for one second
             */

            goto next;
        }

        n = nt_write_fd( log->file->fd, errstr, p - errstr );

        if( n == -1 && nt_errno == NT_ENOSPC ) {
            log->disk_full_time = nt_time();
        }

        if( log->file->fd == nt_stderr ) {
            wrote_stderr = 1;
        }

next:

        log = log->next;
    }

    if( !nt_use_stderr
        || level > NT_LOG_WARN
        || wrote_stderr ) {
        return;
    }

    msg -= ( 7 + err_levels[level].len + 3 );

    ( void ) nt_sprintf( msg, "nginx: [%V] ", &err_levels[level] );

    ( void ) nt_write_console( nt_stderr, msg, p - msg );



#endif

}

#endif

#if !(NT_HAVE_VARIADIC_MACROS)

void nt_cdecl
nt_log_error( nt_uint_t level, nt_log_t *log, nt_err_t err,
              const char *fmt, ... )
{
    va_list  args;

    if( log->log_level >= level ) {
        va_start( args, fmt );
        nt_log_error_core( level, log, err, fmt, args );
        va_end( args );

    }

}


void nt_cdecl
nt_log_debug_core( nt_log_t *log, nt_err_t err, const char *fmt, ... )
{
    va_list  args;

    va_start( args, fmt );
    nt_log_error_core( NT_LOG_DEBUG, log, err, fmt, args );
    va_end( args );

}

#endif
