
#ifndef _NT_SHMTX_H_INCLUDED_
#define _NT_SHMTX_H_INCLUDED_


#include <nt_core.h>
#include <nt_process.h>


typedef struct {
    nt_atomic_t   lock;
#if (NT_HAVE_POSIX_SEM)
    nt_atomic_t   wait;
#endif
} nt_shmtx_sh_t;


typedef struct {
#if (NT_HAVE_ATOMIC_OPS)
    nt_atomic_t  *lock;
#if (NT_HAVE_POSIX_SEM)
    nt_atomic_t  *wait;
    nt_uint_t     semaphore;
    sem_t          sem;
#endif
#else
    nt_fd_t       fd;
    u_char        *name;
#endif
    nt_uint_t     spin;
} nt_shmtx_t;


nt_int_t nt_shmtx_create(nt_shmtx_t *mtx, nt_shmtx_sh_t *addr,
    u_char *name);
void nt_shmtx_destroy(nt_shmtx_t *mtx);
nt_uint_t nt_shmtx_trylock(nt_shmtx_t *mtx);
void nt_shmtx_lock(nt_shmtx_t *mtx);
void nt_shmtx_unlock(nt_shmtx_t *mtx);
nt_uint_t nt_shmtx_force_unlock(nt_shmtx_t *mtx, nt_pid_t pid);


#endif /* _NT_SHMTX_H_INCLUDED_ */
