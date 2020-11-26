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



