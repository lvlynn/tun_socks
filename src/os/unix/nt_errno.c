#include <nt_core.h>


static nt_str_t  *nt_sys_errlist;
static nt_str_t   nt_unknown_error = nt_string( "Unknown error" );

u_char *nt_strerror( nt_err_t err, u_char *errstr, size_t size )
{
    nt_str_t  *msg;

    msg = ( ( nt_uint_t ) err < NT_SYS_NERR ) ? &nt_sys_errlist[err] :
          &nt_unknown_error;
    size = nt_min( size, msg->len );

    return nt_cpymem( errstr, msg->data, size );

}

nt_int_t
nt_strerror_init( void )
{
    char       *msg;
    u_char     *p;
    size_t      len;
    nt_err_t   err;

    /*
     *        * nt_strerror() is not ready to work at this stage, therefore,
     *               * malloc() is used and possible errors are logged using strerror().
     *                      */

    len = NT_SYS_NERR * sizeof( nt_str_t );

    nt_sys_errlist = malloc( len );
    if( nt_sys_errlist == NULL ) {
        goto failed;

    }

    for( err = 0; err < NT_SYS_NERR; err++ ) {
        msg = strerror( err );
        len = nt_strlen( msg );

        p = malloc( len );
        if( p == NULL ) {
            goto failed;

        }

        nt_memcpy( p, msg, len );
        nt_sys_errlist[err].len = len;
        nt_sys_errlist[err].data = p;

    }

    return NT_OK;

failed:

    err = errno;
    nt_log_stderr( 0, "malloc(%uz) failed (%d: %s)", len, err, strerror( err ) );

    return NT_ERROR;

}


