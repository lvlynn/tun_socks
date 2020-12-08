
#include <nt_core.h>


#if (NT_CRYPT)

#if (NT_HAVE_GNU_CRYPT_R)

nt_int_t
nt_libc_crypt( nt_pool_t *pool, u_char *key, u_char *salt, u_char **encrypted )
{
    char               *value;
    size_t              len;
    struct crypt_data   cd;

    cd.initialized = 0;

    value = crypt_r( ( char * ) key, ( char * ) salt, &cd );

    if( value ) {
        len = nt_strlen( value ) + 1;

        *encrypted = nt_pnalloc( pool, len );
        if( *encrypted == NULL ) {
            return NT_ERROR;
        }

        nt_memcpy( *encrypted, value, len );
        return NT_OK;
    }

    nt_log_error( NT_LOG_CRIT, pool->log, nt_errno, "crypt_r() failed" );

    return NT_ERROR;
}

#else

nt_int_t
nt_libc_crypt( nt_pool_t *pool, u_char *key, u_char *salt, u_char **encrypted )
{
    char       *value;
    size_t      len;
    nt_err_t   err;

    value = crypt( ( char * ) key, ( char * ) salt );

    if( value ) {
        len = nt_strlen( value ) + 1;

        *encrypted = nt_pnalloc( pool, len );
        if( *encrypted == NULL ) {
            return NT_ERROR;
        }

        nt_memcpy( *encrypted, value, len );
        return NT_OK;
    }

    err = nt_errno;

    nt_log_error( NT_LOG_CRIT, pool->log, err, "crypt() failed" );

    return NT_ERROR;
}

#endif

#endif /* NT_CRYPT */
