
#include <nt_core.h>


#if (NT_HAVE_ATOMIC_OPS)


static void nt_shmtx_wakeup(nt_shmtx_t *mtx);


nt_int_t
nt_shmtx_create(nt_shmtx_t *mtx, nt_shmtx_sh_t *addr, u_char *name)
{
    mtx->lock = &addr->lock;

    if (mtx->spin == (nt_uint_t) -1) {
        return NT_OK;
    }

    mtx->spin = 2048;

#if (NT_HAVE_POSIX_SEM)

    mtx->wait = &addr->wait;

    if (sem_init(&mtx->sem, 1, 0) == -1) {
        nt_log_error(NT_LOG_ALERT, nt_cycle->log, nt_errno,
                      "sem_init() failed");
    } else {
        mtx->semaphore = 1;
    }

#endif

    return NT_OK;
}


void
nt_shmtx_destroy(nt_shmtx_t *mtx)
{
#if (NT_HAVE_POSIX_SEM)

    if (mtx->semaphore) {
        if (sem_destroy(&mtx->sem) == -1) {
            nt_log_error(NT_LOG_ALERT, nt_cycle->log, nt_errno,
                          "sem_destroy() failed");
        }
    }

#endif
}


nt_uint_t
nt_shmtx_trylock(nt_shmtx_t *mtx)
{
    return (*mtx->lock == 0 && nt_atomic_cmp_set(mtx->lock, 0, nt_pid));
}


void
nt_shmtx_lock(nt_shmtx_t *mtx)
{
    nt_uint_t         i, n;

    nt_log_debug0(NT_LOG_DEBUG_CORE, nt_cycle->log, 0, "shmtx lock");

    for ( ;; ) {

        if (*mtx->lock == 0 && nt_atomic_cmp_set(mtx->lock, 0, nt_pid)) {
            return;
        }

        if (nt_ncpu > 1) {

            for (n = 1; n < mtx->spin; n <<= 1) {

                for (i = 0; i < n; i++) {
                    nt_cpu_pause();
                }

                if (*mtx->lock == 0
                    && nt_atomic_cmp_set(mtx->lock, 0, nt_pid))
                {
                    return;
                }
            }
        }

#if (NT_HAVE_POSIX_SEM)

        if (mtx->semaphore) {
            (void) nt_atomic_fetch_add(mtx->wait, 1);

            if (*mtx->lock == 0 && nt_atomic_cmp_set(mtx->lock, 0, nt_pid)) {
                (void) nt_atomic_fetch_add(mtx->wait, -1);
                return;
            }

            nt_log_debug1(NT_LOG_DEBUG_CORE, nt_cycle->log, 0,
                           "shmtx wait %uA", *mtx->wait);

            while (sem_wait(&mtx->sem) == -1) {
                nt_err_t  err;

                err = nt_errno;

                if (err != NT_EINTR) {
                    nt_log_error(NT_LOG_ALERT, nt_cycle->log, err,
                                  "sem_wait() failed while waiting on shmtx");
                    break;
                }
            }

            nt_log_debug0(NT_LOG_DEBUG_CORE, nt_cycle->log, 0,
                           "shmtx awoke");

            continue;
        }

#endif

        nt_sched_yield();
    }
}


void
nt_shmtx_unlock(nt_shmtx_t *mtx)
{
    if (mtx->spin != (nt_uint_t) -1) {
        nt_log_debug0(NT_LOG_DEBUG_CORE, nt_cycle->log, 0, "shmtx unlock");
    }

    if (nt_atomic_cmp_set(mtx->lock, nt_pid, 0)) {
        nt_shmtx_wakeup(mtx);
    }
}


nt_uint_t
nt_shmtx_force_unlock(nt_shmtx_t *mtx, nt_pid_t pid)
{
    nt_log_debug0(NT_LOG_DEBUG_CORE, nt_cycle->log, 0,
                   "shmtx forced unlock");

    if (nt_atomic_cmp_set(mtx->lock, pid, 0)) {
        nt_shmtx_wakeup(mtx);
        return 1;
    }

    return 0;
}


static void
nt_shmtx_wakeup(nt_shmtx_t *mtx)
{
#if (NT_HAVE_POSIX_SEM)
    nt_atomic_uint_t  wait;

    if (!mtx->semaphore) {
        return;
    }

    for ( ;; ) {

        wait = *mtx->wait;

        if ((nt_atomic_int_t) wait <= 0) {
            return;
        }

        if (nt_atomic_cmp_set(mtx->wait, wait, wait - 1)) {
            break;
        }
    }

    nt_log_debug1(NT_LOG_DEBUG_CORE, nt_cycle->log, 0,
                   "shmtx wake %uA", wait);

    if (sem_post(&mtx->sem) == -1) {
        nt_log_error(NT_LOG_ALERT, nt_cycle->log, nt_errno,
                      "sem_post() failed while wake shmtx");
    }

#endif
}


#else


nt_int_t
nt_shmtx_create(nt_shmtx_t *mtx, nt_shmtx_sh_t *addr, u_char *name)
{
    if (mtx->name) {

        if (nt_strcmp(name, mtx->name) == 0) {
            mtx->name = name;
            return NT_OK;
        }

        nt_shmtx_destroy(mtx);
    }

    mtx->fd = nt_open_file(name, NT_FILE_RDWR, NT_FILE_CREATE_OR_OPEN,
                            NT_FILE_DEFAULT_ACCESS);

    if (mtx->fd == NT_INVALID_FILE) {
        nt_log_error(NT_LOG_EMERG, nt_cycle->log, nt_errno,
                      nt_open_file_n " \"%s\" failed", name);
        return NT_ERROR;
    }

    if (nt_delete_file(name) == NT_FILE_ERROR) {
        nt_log_error(NT_LOG_ALERT, nt_cycle->log, nt_errno,
                      nt_delete_file_n " \"%s\" failed", name);
    }

    mtx->name = name;

    return NT_OK;
}


void
nt_shmtx_destroy(nt_shmtx_t *mtx)
{
    if (nt_close_file(mtx->fd) == NT_FILE_ERROR) {
        nt_log_error(NT_LOG_ALERT, nt_cycle->log, nt_errno,
                      nt_close_file_n " \"%s\" failed", mtx->name);
    }
}


nt_uint_t
nt_shmtx_trylock(nt_shmtx_t *mtx)
{
    nt_err_t  err;

    err = nt_trylock_fd(mtx->fd);

    if (err == 0) {
        return 1;
    }

    if (err == NT_EAGAIN) {
        return 0;
    }

#if __osf__ /* Tru64 UNIX */

    if (err == NT_EACCES) {
        return 0;
    }

#endif

    nt_log_abort(err, nt_trylock_fd_n " %s failed", mtx->name);

    return 0;
}


void
nt_shmtx_lock(nt_shmtx_t *mtx)
{
    nt_err_t  err;

    err = nt_lock_fd(mtx->fd);

    if (err == 0) {
        return;
    }

    nt_log_abort(err, nt_lock_fd_n " %s failed", mtx->name);
}


void
nt_shmtx_unlock(nt_shmtx_t *mtx)
{
    nt_err_t  err;

    err = nt_unlock_fd(mtx->fd);

    if (err == 0) {
        return;
    }

    nt_log_abort(err, nt_unlock_fd_n " %s failed", mtx->name);
}


nt_uint_t
nt_shmtx_force_unlock(nt_shmtx_t *mtx, nt_pid_t pid)
{
    return 0;
}

#endif
