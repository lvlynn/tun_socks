

#ifndef _NT_SHMEM_H_INCLUDED_
#define _NT_SHMEM_H_INCLUDED_


#include <nt_core.h>


typedef struct {
    u_char      *addr;
    size_t       size;
    nt_str_t    name;
    nt_log_t   *log;
    nt_uint_t   exists;   /* unsigned  exists:1;  */
} nt_shm_t;


nt_int_t nt_shm_alloc(nt_shm_t *shm);
void nt_shm_free(nt_shm_t *shm);


#endif /* _NT_SHMEM_H_INCLUDED_ */
