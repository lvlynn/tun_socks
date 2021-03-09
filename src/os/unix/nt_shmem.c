

#include <nt_core.h>


#if (NT_HAVE_MAP_ANON)

nt_int_t
nt_shm_alloc(nt_shm_t *shm)
{
    shm->addr = (u_char *) mmap(NULL, shm->size,
                                PROT_READ|PROT_WRITE,
                                MAP_ANON|MAP_SHARED, -1, 0);

    if (shm->addr == MAP_FAILED) {
        nt_log_error(NT_LOG_ALERT, shm->log, nt_errno,
                      "mmap(MAP_ANON|MAP_SHARED, %uz) failed", shm->size);
        return NT_ERROR;
    }

    return NT_OK;
}


void
nt_shm_free(nt_shm_t *shm)
{
    if (munmap((void *) shm->addr, shm->size) == -1) {
        nt_log_error(NT_LOG_ALERT, shm->log, nt_errno,
                      "munmap(%p, %uz) failed", shm->addr, shm->size);
    }
}

#elif (NT_HAVE_MAP_DEVZERO)

nt_int_t
nt_shm_alloc(nt_shm_t *shm)
{
    nt_fd_t  fd;

    fd = open("/dev/zero", O_RDWR);

    if (fd == -1) {
        nt_log_error(NT_LOG_ALERT, shm->log, nt_errno,
                      "open(\"/dev/zero\") failed");
        return NT_ERROR;
    }

    shm->addr = (u_char *) mmap(NULL, shm->size, PROT_READ|PROT_WRITE,
                                MAP_SHARED, fd, 0);

    if (shm->addr == MAP_FAILED) {
        nt_log_error(NT_LOG_ALERT, shm->log, nt_errno,
                      "mmap(/dev/zero, MAP_SHARED, %uz) failed", shm->size);
    }

    if (close(fd) == -1) {
        nt_log_error(NT_LOG_ALERT, shm->log, nt_errno,
                      "close(\"/dev/zero\") failed");
    }

    return (shm->addr == MAP_FAILED) ? NT_ERROR : NT_OK;
}


void
nt_shm_free(nt_shm_t *shm)
{
    if (munmap((void *) shm->addr, shm->size) == -1) {
        nt_log_error(NT_LOG_ALERT, shm->log, nt_errno,
                      "munmap(%p, %uz) failed", shm->addr, shm->size);
    }
}

#elif (NT_HAVE_SYSVSHM)

#include <sys/ipc.h>
#include <sys/shm.h>


nt_int_t
nt_shm_alloc(nt_shm_t *shm)
{
    int  id;

    id = shmget(IPC_PRIVATE, shm->size, (SHM_R|SHM_W|IPC_CREAT));

    if (id == -1) {
        nt_log_error(NT_LOG_ALERT, shm->log, nt_errno,
                      "shmget(%uz) failed", shm->size);
        return NT_ERROR;
    }

    nt_log_debug1(NT_LOG_DEBUG_CORE, shm->log, 0, "shmget id: %d", id);

    shm->addr = shmat(id, NULL, 0);

    if (shm->addr == (void *) -1) {
        nt_log_error(NT_LOG_ALERT, shm->log, nt_errno, "shmat() failed");
    }

    if (shmctl(id, IPC_RMID, NULL) == -1) {
        nt_log_error(NT_LOG_ALERT, shm->log, nt_errno,
                      "shmctl(IPC_RMID) failed");
    }

    return (shm->addr == (void *) -1) ? NT_ERROR : NT_OK;
}


void
nt_shm_free(nt_shm_t *shm)
{
    if (shmdt(shm->addr) == -1) {
        nt_log_error(NT_LOG_ALERT, shm->log, nt_errno,
                      "shmdt(%p) failed", shm->addr);
    }
}

#endif
