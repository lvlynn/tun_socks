#ifndef _NT_LINUX_H_
#define _NT_LINUX_H_


#include <nt_core.h>

typedef uid_t  nt_uid_t;
typedef gid_t  nt_gid_t;


nt_int_t nt_libc_crypt( nt_pool_t *pool, u_char *key, u_char *salt,
                        u_char **encrypted );


#endif
