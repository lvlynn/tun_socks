#include <nt_core.h>
#include <nt_time.h>

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
//#if 0
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



//#endif

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

void nt_cdecl
nt_log_abort( nt_err_t err, const char *fmt, ... )
{
    u_char   *p;
    va_list   args;
    u_char    errstr[NT_MAX_CONF_ERRSTR];

    va_start( args, fmt );
    p = nt_vsnprintf( errstr, sizeof( errstr ) - 1, fmt, args );
    va_end( args );

    nt_log_error( NT_LOG_ALERT, nt_cycle->log, err,
                   "%*s", p - errstr, errstr );

}

void nt_cdecl
nt_log_stderr( nt_err_t err, const char *fmt, ... )
{
    u_char   *p, *last;
    va_list   args;
    u_char    errstr[NT_MAX_ERROR_STR];

    last = errstr + NT_MAX_ERROR_STR;

    p = nt_cpymem( errstr, "nginx: ", 7 );

    va_start( args, fmt );
    p = nt_vslprintf( p, last, fmt, args );
    va_end( args );

    if( err ) {
        p = nt_log_errno( p, last, err );

    }

    if( p > last - NT_LINEFEED_SIZE ) {
        p = last - NT_LINEFEED_SIZE;

    }

    nt_linefeed( p );

    ( void ) nt_write_console( nt_stderr, errstr, p - errstr );

}

u_char *
nt_log_errno( u_char *buf, u_char *last, nt_err_t err )
{
    if( buf > last - 50 ) {

        /* leave a space for an error code */

        buf = last - 50;
        *buf++ = '.';
        *buf++ = '.';
        *buf++ = '.';
    }

    #if (NT_WIN32)
    buf = nt_slprintf( buf, last, ( ( unsigned ) err < 0x80000000 )
                        ? " (%d: " : " (%Xd: ", err );
    #else
    buf = nt_slprintf( buf, last, " (%d: ", err );
    #endif

    buf = nt_strerror( err, buf, last - buf );

    if( buf < last ) {
        *buf++ = ')';
    }

    return buf;
}

#if 0
nt_log_t *
nt_log_init( u_char *prefix )
{
    u_char  *p, *name;
    size_t   nlen, plen;

    nt_log.file = &nt_log_file;
    nt_log.log_level = NT_LOG_NOTICE;

    name = ( u_char * ) NT_ERROR_LOG_PATH;

    /*
     * we use nt_strlen() here since BCC warns about
     * condition is always false and unreachable code
     */

    nlen = nt_strlen( name );

    if( nlen == 0 ) {
        nt_log_file.fd = nt_stderr;
        return &nt_log;
    }

    p = NULL;

    #if (NT_WIN32)
    if( name[1] != ':' ) {
    #else
    if( name[0] != '/' ) {
    #endif

        if( prefix ) {
            plen = nt_strlen( prefix );

        } else {
            #ifdef NT_PREFIX
            prefix = ( u_char * ) NT_PREFIX;
            plen = nt_strlen( prefix );
            #else
            plen = 0;
            #endif
        }

        if( plen ) {
            name = malloc( plen + nlen + 2 );
            if( name == NULL ) {
                return NULL;
            }

            p = nt_cpymem( name, prefix, plen );

            if( !nt_path_separator( *( p - 1 ) ) ) {
                *p++ = '/';
            }

            nt_cpystrn( p, ( u_char * ) NT_ERROR_LOG_PATH, nlen + 1 );

            p = name;
        }
    }

    nt_log_file.fd = nt_open_file( name, NT_FILE_APPEND,
                                     NT_FILE_CREATE_OR_OPEN,
                                     NT_FILE_DEFAULT_ACCESS );

    if( nt_log_file.fd == NT_INVALID_FILE ) {
        nt_log_stderr( nt_errno,
                        "[alert] could not open error log file: "
                        nt_open_file_n " \"%s\" failed", name );
        #if (NT_WIN32)
        nt_event_log( nt_errno,
                       "could not open error log file: "
                       nt_open_file_n " \"%s\" failed", name );
        #endif

        nt_log_file.fd = nt_stderr;
    }

    if( p ) {
        nt_free( p );
    }

    return &nt_log;
}

nt_int_t
nt_log_open_default( nt_cycle_t *cycle )
{
    nt_log_t         *log;
    static nt_str_t   error_log = nt_string( NT_ERROR_LOG_PATH );

    if( nt_log_get_file_log( &cycle->new_log ) != NULL ) {
        return NT_OK;
    }

    if( cycle->new_log.log_level != 0 ) {
        /* there are some error logs, but no files */

        log = nt_pcalloc( cycle->pool, sizeof( nt_log_t ) );
        if( log == NULL ) {
            return NT_ERROR;
        }

    } else {
        /* no error logs at all */
        log = &cycle->new_log;
    }

    log->log_level = NT_LOG_ERR;

    log->file = nt_conf_open_file( cycle, &error_log );
    if( log->file == NULL ) {
        return NT_ERROR;
    }

    if( log != &cycle->new_log ) {
        nt_log_insert( &cycle->new_log, log );
    }

    return NT_OK;
}

nt_int_t
nt_log_redirect_stderr( nt_cycle_t *cycle )
{
    nt_fd_t  fd;

    if( cycle->log_use_stderr ) {
        return NT_OK;
    }

    /* file log always exists when we are called */
    fd = nt_log_get_file_log( cycle->log )->file->fd;

    if( fd != nt_stderr ) {
        if( nt_set_stderr( fd ) == NT_FILE_ERROR ) {
            nt_log_error( NT_LOG_ALERT, cycle->log, nt_errno,
                           nt_set_stderr_n " failed" );

            return NT_ERROR;
        }
    }

    return NT_OK;
}


nt_log_t *
nt_log_get_file_log( nt_log_t *head )
{
    nt_log_t  *log;

    for( log = head; log; log = log->next ) {
        if( log->file != NULL ) {
            return log;
        }
    }

    return NULL;
}

static char *
nt_log_set_levels( nt_conf_t *cf, nt_log_t *log )
{
    nt_uint_t   i, n, d, found;
    nt_str_t   *value;

    if( cf->args->nelts == 2 ) {
        log->log_level = NT_LOG_ERR;
        return NT_CONF_OK;
    }

    value = cf->args->elts;

    for( i = 2; i < cf->args->nelts; i++ ) {
        found = 0;

        for( n = 1; n <= NT_LOG_DEBUG; n++ ) {
            if( nt_strcmp( value[i].data, err_levels[n].data ) == 0 ) {

                if( log->log_level != 0 ) {
                    nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                                        "duplicate log level \"%V\"",
                                        &value[i] );
                    return NT_CONF_ERROR;
                }

                log->log_level = n;
                found = 1;
                break;
            }
        }

        for( n = 0, d = NT_LOG_DEBUG_FIRST; d <= NT_LOG_DEBUG_LAST; d <<= 1 ) {
            if( nt_strcmp( value[i].data, debug_levels[n++] ) == 0 ) {
                if( log->log_level & ~NT_LOG_DEBUG_ALL ) {
                    nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                                        "invalid log level \"%V\"",
                                        &value[i] );
                    return NT_CONF_ERROR;
                }

                log->log_level |= d;
                found = 1;
                break;
            }
        }


        if( !found ) {
            nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                                "invalid log level \"%V\"", &value[i] );
            return NT_CONF_ERROR;
        }
    }

    if( log->log_level == NT_LOG_DEBUG ) {
        log->log_level = NT_LOG_DEBUG_ALL;
    }

    return NT_CONF_OK;
}

static char *
nt_error_log( nt_conf_t *cf, nt_command_t *cmd, void *conf )
{
    nt_log_t  *dummy;

    dummy = &cf->cycle->new_log;

    return nt_log_set_log( cf, &dummy );
}


char *
nt_log_set_log( nt_conf_t *cf, nt_log_t **head )
{
    nt_log_t          *new_log;
    nt_str_t          *value, name;
    nt_syslog_peer_t  *peer;

    if( *head != NULL && ( *head )->log_level == 0 ) {
        new_log = *head;

    } else {

        new_log = nt_pcalloc( cf->pool, sizeof( nt_log_t ) );
        if( new_log == NULL ) {
            return NT_CONF_ERROR;
        }

        if( *head == NULL ) {
            *head = new_log;
        }
    }

    value = cf->args->elts;

    if( nt_strcmp( value[1].data, "stderr" ) == 0 ) {
        nt_str_null( &name );
        cf->cycle->log_use_stderr = 1;

        new_log->file = nt_conf_open_file( cf->cycle, &name );
        if( new_log->file == NULL ) {
            return NT_CONF_ERROR;
        }

    } else if( nt_strncmp( value[1].data, "memory:", 7 ) == 0 ) {

        #if (NT_DEBUG)
        size_t                 size, needed;
        nt_pool_cleanup_t    *cln;
        nt_log_memory_buf_t  *buf;

        value[1].len -= 7;
        value[1].data += 7;

        needed = sizeof( "MEMLOG  :" NT_LINEFEED )
                 + cf->conf_file->file.name.len
                 + NT_SIZE_T_LEN
                 + NT_INT_T_LEN
                 + NT_MAX_ERROR_STR;

        size = nt_parse_size( &value[1] );

        if( size == ( size_t ) NT_ERROR || size < needed ) {
            nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                                "invalid buffer size \"%V\"", &value[1] );
            return NT_CONF_ERROR;
        }

        buf = nt_pcalloc( cf->pool, sizeof( nt_log_memory_buf_t ) );
        if( buf == NULL ) {
            return NT_CONF_ERROR;
        }

        buf->start = nt_pnalloc( cf->pool, size );
        if( buf->start == NULL ) {
            return NT_CONF_ERROR;
        }

        buf->end = buf->start + size;

        buf->pos = nt_slprintf( buf->start, buf->end, "MEMLOG %uz %V:%ui%N",
                                 size, &cf->conf_file->file.name,
                                 cf->conf_file->line );

        nt_memset( buf->pos, ' ', buf->end - buf->pos );

        cln = nt_pool_cleanup_add( cf->pool, 0 );
        if( cln == NULL ) {
            return NT_CONF_ERROR;
        }

        cln->data = new_log;
        cln->handler = nt_log_memory_cleanup;

        new_log->writer = nt_log_memory_writer;
        new_log->wdata = buf;

        #else
        nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                            "nginx was built without debug support" );
        return NT_CONF_ERROR;
        #endif
    } else if( nt_strncmp( value[1].data, "syslog:", 7 ) == 0 ) {
        peer = nt_pcalloc( cf->pool, sizeof( nt_syslog_peer_t ) );
        if( peer == NULL ) {
            return NT_CONF_ERROR;
        }

        if( nt_syslog_process_conf( cf, peer ) != NT_CONF_OK ) {
            return NT_CONF_ERROR;
        }

        new_log->writer = nt_syslog_writer;
        new_log->wdata = peer;

        #if (T_PIPES) && !(NT_WIN32)
    } else if( nt_strncmp( value[1].data, "pipe:", 5 ) == 0 ) {

        if( value[1].len == 5 ) {
            return NT_CONF_ERROR;
        }

        value[1].len -= 5;
        value[1].data += 5;

        nt_open_pipe_t *pipe_conf = nt_conf_open_pipe( cf->cycle, &value[1], "w" );
        if( pipe_conf == NULL ) {
            return NT_CONF_ERROR;
        }


        new_log->file = pipe_conf->open_fd;

        #ifdef LOG_PIPE_NEED_BACKUP
        if( new_log->file != NULL ) {
            name = nt_log_error_backup;
            if( nt_conf_full_name( cf->cycle, &name, 0 ) != NT_OK ) {
                return "fail to set backup";
            }

            new_log->file->name = name;
        }
        #endif
        #endif

    } else {
        new_log->file = nt_conf_open_file( cf->cycle, &value[1] );
        if( new_log->file == NULL ) {
            return NT_CONF_ERROR;
        }
    }

    if( nt_log_set_levels( cf, new_log ) != NT_CONF_OK ) {
        return NT_CONF_ERROR;
    }

    if( *head != new_log ) {
        nt_log_insert( *head, new_log );
    }

    return NT_CONF_OK;
}

nt_log_insert( nt_log_t *log, nt_log_t *new_log )
{
    nt_log_t  tmp;

    if( new_log->log_level > log->log_level ) {

        /*
         * list head address is permanent, insert new log after
         * head and swap its contents with head
         */

        tmp = *log;
        *log = *new_log;
        *new_log = tmp;

        log->next = new_log;
        return;
    }

    while( log->next ) {
        if( new_log->log_level > log->next->log_level ) {
            new_log->next = log->next;
            log->next = new_log;
            return;
        }

        log = log->next;
    }

    log->next = new_log;
}
#endif

#if (NT_DEBUG)

static void
nt_log_memory_writer( nt_log_t *log, nt_uint_t level, u_char *buf,
                       size_t len )
{
    u_char                *p;
    size_t                 avail, written;
    nt_log_memory_buf_t  *mem;

    mem = log->wdata;

    if( mem == NULL ) {
        return;
    }

    written = nt_atomic_fetch_add( &mem->written, len );

    p = mem->pos + written % ( mem->end - mem->pos );

    avail = mem->end - p;

    if( avail >= len ) {
        nt_memcpy( p, buf, len );

    } else {
        nt_memcpy( p, buf, avail );
        nt_memcpy( mem->pos, buf + avail, len - avail );
    }
}


static void
nt_log_memory_cleanup( void *data )
{
    nt_log_t *log = data;

    nt_log_debug0( NT_LOG_DEBUG_CORE, log, 0, "destroy memory log buffer" );

    log->wdata = NULL;
}

#endif
