
#include <nt_def.h>
#include <nt_core.h>
#include <nt_event.h>


nt_os_io_t  nt_io;


nt_int_t
nt_connection_error( nt_connection_t *c, nt_err_t err, char *text )
{
    nt_uint_t  level;

    /* Winsock may return NT_ECONNABORTED instead of NT_ECONNRESET */

    if( ( err == NT_ECONNRESET
      #if (NT_WIN32)
          || err == NT_ECONNABORTED
      #endif
        ) && c->log_error == NT_ERROR_IGNORE_ECONNRESET ) {
        return 0;

    }

    if( err == 0
        || err == NT_ECONNRESET
    #if (NT_WIN32)
        || err == NT_ECONNABORTED
    #else
        || err == NT_EPIPE
    #endif
        || err == NT_ENOTCONN
        || err == NT_ETIMEDOUT
        || err == NT_ECONNREFUSED
        || err == NT_ENETDOWN
        || err == NT_ENETUNREACH
        || err == NT_EHOSTDOWN
        || err == NT_EHOSTUNREACH ) {
        switch( c->log_error ) {

        case NT_ERROR_IGNORE_EINVAL:
        case NT_ERROR_IGNORE_ECONNRESET:
        case NT_ERROR_INFO:
            level = NT_LOG_INFO;
            break;

        default:
            level = NT_LOG_ERR;

        }


    } else {
        level = NT_LOG_ALERT;

    }

    nt_log_error( level, c->log, err, text );

    return NT_ERROR;
}
