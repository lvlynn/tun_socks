

#include <nt_def.h>
#include <nt_core.h>


#if (NT_THREADS)
#include <nt_thread_pool.h>
static void nt_thread_read_handler(void *data, nt_log_t *log);
static void nt_thread_write_chain_to_file_handler(void *data, nt_log_t *log);
#endif

static nt_chain_t *nt_chain_to_iovec(nt_iovec_t *vec, nt_chain_t *cl);
static ssize_t nt_writev_file(nt_file_t *file, nt_iovec_t *vec,
    off_t offset);


#if (NT_HAVE_FILE_AIO)

nt_uint_t  nt_file_aio = 1;

#endif


ssize_t
nt_read_file(nt_file_t *file, u_char *buf, size_t size, off_t offset)
{
    ssize_t  n;

    nt_log_debug4(NT_LOG_DEBUG_CORE, file->log, 0,
                   "read: %d, %p, %uz, %O", file->fd, buf, size, offset);

#if (NT_HAVE_PREAD)

    n = pread(file->fd, buf, size, offset);

    if (n == -1) {
        nt_log_error(NT_LOG_CRIT, file->log, nt_errno,
                      "pread() \"%s\" failed", file->name.data);
        return NT_ERROR;
    }

#else

    if (file->sys_offset != offset) {
        if (lseek(file->fd, offset, SEEK_SET) == -1) {
            nt_log_error(NT_LOG_CRIT, file->log, nt_errno,
                          "lseek() \"%s\" failed", file->name.data);
            return NT_ERROR;
        }

        file->sys_offset = offset;
    }

    n = read(file->fd, buf, size);

    if (n == -1) {
        nt_log_error(NT_LOG_CRIT, file->log, nt_errno,
                      "read() \"%s\" failed", file->name.data);
        return NT_ERROR;
    }

    file->sys_offset += n;

#endif

    file->offset += n;

    return n;
}


#if (NT_THREADS)

typedef struct {
    nt_fd_t       fd;
    nt_uint_t     write;   /* unsigned  write:1; */

    u_char        *buf;
    size_t         size;
    nt_chain_t   *chain;
    off_t          offset;

    size_t         nbytes;
    nt_err_t      err;
} nt_thread_file_ctx_t;


ssize_t
nt_thread_read(nt_file_t *file, u_char *buf, size_t size, off_t offset,
    nt_pool_t *pool)
{
    nt_thread_task_t      *task;
    nt_thread_file_ctx_t  *ctx;

    nt_log_debug4(NT_LOG_DEBUG_CORE, file->log, 0,
                   "thread read: %d, %p, %uz, %O",
                   file->fd, buf, size, offset);

    task = file->thread_task;

    if (task == NULL) {
        task = nt_thread_task_alloc(pool, sizeof(nt_thread_file_ctx_t));
        if (task == NULL) {
            return NT_ERROR;
        }

        file->thread_task = task;
    }

    ctx = task->ctx;

    if (task->event.complete) {
        task->event.complete = 0;

        if (ctx->write) {
            nt_log_error(NT_LOG_ALERT, file->log, 0,
                          "invalid thread call, read instead of write");
            return NT_ERROR;
        }

        if (ctx->err) {
            nt_log_error(NT_LOG_CRIT, file->log, ctx->err,
                          "pread() \"%s\" failed", file->name.data);
            return NT_ERROR;
        }

        return ctx->nbytes;
    }

    task->handler = nt_thread_read_handler;

    ctx->write = 0;

    ctx->fd = file->fd;
    ctx->buf = buf;
    ctx->size = size;
    ctx->offset = offset;

    if (file->thread_handler(task, file) != NT_OK) {
        return NT_ERROR;
    }

    return NT_AGAIN;
}


#if (NT_HAVE_PREAD)

static void
nt_thread_read_handler(void *data, nt_log_t *log)
{
    nt_thread_file_ctx_t *ctx = data;

    ssize_t  n;

    nt_log_debug0(NT_LOG_DEBUG_CORE, log, 0, "thread read handler");

    n = pread(ctx->fd, ctx->buf, ctx->size, ctx->offset);

    if (n == -1) {
        ctx->err = nt_errno;

    } else {
        ctx->nbytes = n;
        ctx->err = 0;
    }

#if 0
    nt_time_update();
#endif

    nt_log_debug4(NT_LOG_DEBUG_CORE, log, 0,
                   "pread: %z (err: %d) of %uz @%O",
                   n, ctx->err, ctx->size, ctx->offset);
}

#else

#error pread() is required!

#endif

#endif /* NT_THREADS */


ssize_t
nt_write_file(nt_file_t *file, u_char *buf, size_t size, off_t offset)
{
    ssize_t    n, written;
    nt_err_t  err;

    nt_log_debug4(NT_LOG_DEBUG_CORE, file->log, 0,
                   "write: %d, %p, %uz, %O", file->fd, buf, size, offset);

    written = 0;

#if (NT_HAVE_PWRITE)

    for ( ;; ) {
        n = pwrite(file->fd, buf + written, size, offset);

        if (n == -1) {
            err = nt_errno;

            if (err == NT_EINTR) {
                nt_log_debug0(NT_LOG_DEBUG_CORE, file->log, err,
                               "pwrite() was interrupted");
                continue;
            }

            nt_log_error(NT_LOG_CRIT, file->log, err,
                          "pwrite() \"%s\" failed", file->name.data);
            return NT_ERROR;
        }

        file->offset += n;
        written += n;

        if ((size_t) n == size) {
            return written;
        }

        offset += n;
        size -= n;
    }

#else

    if (file->sys_offset != offset) {
        if (lseek(file->fd, offset, SEEK_SET) == -1) {
            nt_log_error(NT_LOG_CRIT, file->log, nt_errno,
                          "lseek() \"%s\" failed", file->name.data);
            return NT_ERROR;
        }

        file->sys_offset = offset;
    }

    for ( ;; ) {
        n = write(file->fd, buf + written, size);

        if (n == -1) {
            err = nt_errno;

            if (err == NT_EINTR) {
                nt_log_debug0(NT_LOG_DEBUG_CORE, file->log, err,
                               "write() was interrupted");
                continue;
            }

            nt_log_error(NT_LOG_CRIT, file->log, err,
                          "write() \"%s\" failed", file->name.data);
            return NT_ERROR;
        }

        file->sys_offset += n;
        file->offset += n;
        written += n;

        if ((size_t) n == size) {
            return written;
        }

        size -= n;
    }
#endif
}


nt_fd_t
nt_open_tempfile(u_char *name, nt_uint_t persistent, nt_uint_t access)
{
    nt_fd_t  fd;

    fd = open((const char *) name, O_CREAT|O_EXCL|O_RDWR,
              access ? access : 0600);

    if (fd != -1 && !persistent) {
        (void) unlink((const char *) name);
    }

    return fd;
}


ssize_t
nt_write_chain_to_file(nt_file_t *file, nt_chain_t *cl, off_t offset,
    nt_pool_t *pool)
{
    ssize_t        total, n;
    nt_iovec_t    vec;
    struct iovec   iovs[NT_IOVS_PREALLOCATE];

    /* use pwrite() if there is the only buf in a chain */

    if (cl->next == NULL) {
        return nt_write_file(file, cl->buf->pos,
                              (size_t) (cl->buf->last - cl->buf->pos),
                              offset);
    }

    total = 0;

    vec.iovs = iovs;
    vec.nalloc = NT_IOVS_PREALLOCATE;

    do {
        /* create the iovec and coalesce the neighbouring bufs */
        cl = nt_chain_to_iovec(&vec, cl);

        /* use pwrite() if there is the only iovec buffer */

        if (vec.count == 1) {
            n = nt_write_file(file, (u_char *) iovs[0].iov_base,
                               iovs[0].iov_len, offset);

            if (n == NT_ERROR) {
                return n;
            }

            return total + n;
        }

        n = nt_writev_file(file, &vec, offset);

        if (n == NT_ERROR) {
            return n;
        }

        offset += n;
        total += n;

    } while (cl);

    return total;
}


static nt_chain_t *
nt_chain_to_iovec(nt_iovec_t *vec, nt_chain_t *cl)
{
    size_t         total, size;
    u_char        *prev;
    nt_uint_t     n;
    struct iovec  *iov;

    iov = NULL;
    prev = NULL;
    total = 0;
    n = 0;

    for ( /* void */ ; cl; cl = cl->next) {

        if (nt_buf_special(cl->buf)) {
            continue;
        }

        size = cl->buf->last - cl->buf->pos;

        if (prev == cl->buf->pos) {
            iov->iov_len += size;

        } else {
            if (n == vec->nalloc) {
                break;
            }

            iov = &vec->iovs[n++];

            iov->iov_base = (void *) cl->buf->pos;
            iov->iov_len = size;
        }

        prev = cl->buf->pos + size;
        total += size;
    }

    vec->count = n;
    vec->size = total;

    return cl;
}


static ssize_t
nt_writev_file(nt_file_t *file, nt_iovec_t *vec, off_t offset)
{
    ssize_t    n;
    nt_err_t  err;

    nt_log_debug3(NT_LOG_DEBUG_CORE, file->log, 0,
                   "writev: %d, %uz, %O", file->fd, vec->size, offset);

#if (NT_HAVE_PWRITEV)

eintr:

    n = pwritev(file->fd, vec->iovs, vec->count, offset);

    if (n == -1) {
        err = nt_errno;

        if (err == NT_EINTR) {
            nt_log_debug0(NT_LOG_DEBUG_CORE, file->log, err,
                           "pwritev() was interrupted");
            goto eintr;
        }

        nt_log_error(NT_LOG_CRIT, file->log, err,
                      "pwritev() \"%s\" failed", file->name.data);
        return NT_ERROR;
    }

    if ((size_t) n != vec->size) {
        nt_log_error(NT_LOG_CRIT, file->log, 0,
                      "pwritev() \"%s\" has written only %z of %uz",
                      file->name.data, n, vec->size);
        return NT_ERROR;
    }

#else

    if (file->sys_offset != offset) {
        if (lseek(file->fd, offset, SEEK_SET) == -1) {
            nt_log_error(NT_LOG_CRIT, file->log, nt_errno,
                          "lseek() \"%s\" failed", file->name.data);
            return NT_ERROR;
        }

        file->sys_offset = offset;
    }

eintr:

    n = writev(file->fd, vec->iovs, vec->count);

    if (n == -1) {
        err = nt_errno;

        if (err == NT_EINTR) {
            nt_log_debug0(NT_LOG_DEBUG_CORE, file->log, err,
                           "writev() was interrupted");
            goto eintr;
        }

        nt_log_error(NT_LOG_CRIT, file->log, err,
                      "writev() \"%s\" failed", file->name.data);
        return NT_ERROR;
    }

    if ((size_t) n != vec->size) {
        nt_log_error(NT_LOG_CRIT, file->log, 0,
                      "writev() \"%s\" has written only %z of %uz",
                      file->name.data, n, vec->size);
        return NT_ERROR;
    }

    file->sys_offset += n;

#endif

    file->offset += n;

    return n;
}


#if (NT_THREADS)

ssize_t
nt_thread_write_chain_to_file(nt_file_t *file, nt_chain_t *cl, off_t offset,
    nt_pool_t *pool)
{
    nt_thread_task_t      *task;
    nt_thread_file_ctx_t  *ctx;

    nt_log_debug3(NT_LOG_DEBUG_CORE, file->log, 0,
                   "thread write chain: %d, %p, %O",
                   file->fd, cl, offset);

    task = file->thread_task;

    if (task == NULL) {
        task = nt_thread_task_alloc(pool,
                                     sizeof(nt_thread_file_ctx_t));
        if (task == NULL) {
            return NT_ERROR;
        }

        file->thread_task = task;
    }

    ctx = task->ctx;

    if (task->event.complete) {
        task->event.complete = 0;

        if (!ctx->write) {
            nt_log_error(NT_LOG_ALERT, file->log, 0,
                          "invalid thread call, write instead of read");
            return NT_ERROR;
        }

        if (ctx->err || ctx->nbytes == 0) {
            nt_log_error(NT_LOG_CRIT, file->log, ctx->err,
                          "pwritev() \"%s\" failed", file->name.data);
            return NT_ERROR;
        }

        file->offset += ctx->nbytes;
        return ctx->nbytes;
    }

    task->handler = nt_thread_write_chain_to_file_handler;

    ctx->write = 1;

    ctx->fd = file->fd;
    ctx->chain = cl;
    ctx->offset = offset;

    if (file->thread_handler(task, file) != NT_OK) {
        return NT_ERROR;
    }

    return NT_AGAIN;
}


static void
nt_thread_write_chain_to_file_handler(void *data, nt_log_t *log)
{
    nt_thread_file_ctx_t *ctx = data;

#if (NT_HAVE_PWRITEV)

    off_t          offset;
    ssize_t        n;
    nt_err_t      err;
    nt_chain_t   *cl;
    nt_iovec_t    vec;
    struct iovec   iovs[NT_IOVS_PREALLOCATE];

    vec.iovs = iovs;
    vec.nalloc = NT_IOVS_PREALLOCATE;

    cl = ctx->chain;
    offset = ctx->offset;

    ctx->nbytes = 0;
    ctx->err = 0;

    do {
        /* create the iovec and coalesce the neighbouring bufs */
        cl = nt_chain_to_iovec(&vec, cl);

eintr:

        n = pwritev(ctx->fd, iovs, vec.count, offset);

        if (n == -1) {
            err = nt_errno;

            if (err == NT_EINTR) {
                nt_log_debug0(NT_LOG_DEBUG_CORE, log, err,
                               "pwritev() was interrupted");
                goto eintr;
            }

            ctx->err = err;
            return;
        }

        if ((size_t) n != vec.size) {
            ctx->nbytes = 0;
            return;
        }

        ctx->nbytes += n;
        offset += n;
    } while (cl);

#else

    ctx->err = NT_ENOSYS;
    return;

#endif
}

#endif /* NT_THREADS */


nt_int_t
nt_set_file_time(u_char *name, nt_fd_t fd, time_t s)
{
    struct timeval  tv[2];

    tv[0].tv_sec = nt_time();
    tv[0].tv_usec = 0;
    tv[1].tv_sec = s;
    tv[1].tv_usec = 0;

    if (utimes((char *) name, tv) != -1) {
        return NT_OK;
    }

    return NT_ERROR;
}


nt_int_t
nt_create_file_mapping(nt_file_mapping_t *fm)
{
    fm->fd = nt_open_file(fm->name, NT_FILE_RDWR, NT_FILE_TRUNCATE,
                           NT_FILE_DEFAULT_ACCESS);

    if (fm->fd == NT_INVALID_FILE) {
        nt_log_error(NT_LOG_CRIT, fm->log, nt_errno,
                      nt_open_file_n " \"%s\" failed", fm->name);
        return NT_ERROR;
    }

    if (ftruncate(fm->fd, fm->size) == -1) {
        nt_log_error(NT_LOG_CRIT, fm->log, nt_errno,
                      "ftruncate() \"%s\" failed", fm->name);
        goto failed;
    }

    fm->addr = mmap(NULL, fm->size, PROT_READ|PROT_WRITE, MAP_SHARED,
                    fm->fd, 0);
    if (fm->addr != MAP_FAILED) {
        return NT_OK;
    }

    nt_log_error(NT_LOG_CRIT, fm->log, nt_errno,
                  "mmap(%uz) \"%s\" failed", fm->size, fm->name);

failed:

    if (nt_close_file(fm->fd) == NT_FILE_ERROR) {
        nt_log_error(NT_LOG_ALERT, fm->log, nt_errno,
                      nt_close_file_n " \"%s\" failed", fm->name);
    }

    return NT_ERROR;
}


void
nt_close_file_mapping(nt_file_mapping_t *fm)
{
    if (munmap(fm->addr, fm->size) == -1) {
        nt_log_error(NT_LOG_CRIT, fm->log, nt_errno,
                      "munmap(%uz) \"%s\" failed", fm->size, fm->name);
    }

    if (nt_close_file(fm->fd) == NT_FILE_ERROR) {
        nt_log_error(NT_LOG_ALERT, fm->log, nt_errno,
                      nt_close_file_n " \"%s\" failed", fm->name);
    }
}


nt_int_t
nt_open_dir(nt_str_t *name, nt_dir_t *dir)
{
    dir->dir = opendir((const char *) name->data);

    if (dir->dir == NULL) {
        return NT_ERROR;
    }

    dir->valid_info = 0;

    return NT_OK;
}


nt_int_t
nt_read_dir(nt_dir_t *dir)
{
    dir->de = readdir(dir->dir);

    if (dir->de) {
#if (NT_HAVE_D_TYPE)
        dir->type = dir->de->d_type;
#else
        dir->type = 0;
#endif
        return NT_OK;
    }

    return NT_ERROR;
}


nt_int_t
nt_open_glob(nt_glob_t *gl)
{
    int  n;

    n = glob((char *) gl->pattern, 0, NULL, &gl->pglob);

    if (n == 0) {
        return NT_OK;
    }

#ifdef GLOB_NOMATCH

    if (n == GLOB_NOMATCH && gl->test) {
        return NT_OK;
    }

#endif

    return NT_ERROR;
}


nt_int_t
nt_read_glob(nt_glob_t *gl, nt_str_t *name)
{
    size_t  count;

#ifdef GLOB_NOMATCH
    count = (size_t) gl->pglob.gl_pathc;
#else
    count = (size_t) gl->pglob.gl_matchc;
#endif

    if (gl->n < count) {

        name->len = (size_t) nt_strlen(gl->pglob.gl_pathv[gl->n]);
        name->data = (u_char *) gl->pglob.gl_pathv[gl->n];
        gl->n++;

        return NT_OK;
    }

    return NT_DONE;
}


void
nt_close_glob(nt_glob_t *gl)
{
    globfree(&gl->pglob);
}


nt_err_t
nt_trylock_fd(nt_fd_t fd)
{
    struct flock  fl;

    nt_memzero(&fl, sizeof(struct flock));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;

    if (fcntl(fd, F_SETLK, &fl) == -1) {
        return nt_errno;
    }

    return 0;
}


nt_err_t
nt_lock_fd(nt_fd_t fd)
{
    struct flock  fl;

    nt_memzero(&fl, sizeof(struct flock));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;

    if (fcntl(fd, F_SETLKW, &fl) == -1) {
        return nt_errno;
    }

    return 0;
}


nt_err_t
nt_unlock_fd(nt_fd_t fd)
{
    struct flock  fl;

    nt_memzero(&fl, sizeof(struct flock));
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;

    if (fcntl(fd, F_SETLK, &fl) == -1) {
        return  nt_errno;
    }

    return 0;
}


#if (NT_HAVE_POSIX_FADVISE) && !(NT_HAVE_F_READAHEAD)

nt_int_t
nt_read_ahead(nt_fd_t fd, size_t n)
{
    int  err;

    err = posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

    if (err == 0) {
        return 0;
    }

    nt_set_errno(err);
    return NT_FILE_ERROR;
}

#endif


#if (NT_HAVE_O_DIRECT)

nt_int_t
nt_directio_on(nt_fd_t fd)
{
    int  flags;

    flags = fcntl(fd, F_GETFL);

    if (flags == -1) {
        return NT_FILE_ERROR;
    }

    return fcntl(fd, F_SETFL, flags | O_DIRECT);
}


nt_int_t
nt_directio_off(nt_fd_t fd)
{
    int  flags;

    flags = fcntl(fd, F_GETFL);

    if (flags == -1) {
        return NT_FILE_ERROR;
    }

    return fcntl(fd, F_SETFL, flags & ~O_DIRECT);
}

#endif


#if (NT_HAVE_STATFS)

size_t
nt_fs_bsize(u_char *name)
{
    struct statfs  fs;

    if (statfs((char *) name, &fs) == -1) {
        return 512;
    }

    if ((fs.f_bsize % 512) != 0) {
        return 512;
    }

    return (size_t) fs.f_bsize;
}

#elif (NT_HAVE_STATVFS)

size_t
nt_fs_bsize(u_char *name)
{
    struct statvfs  fs;

    if (statvfs((char *) name, &fs) == -1) {
        return 512;
    }

    if ((fs.f_frsize % 512) != 0) {
        return 512;
    }

    return (size_t) fs.f_frsize;
}

#else

size_t
nt_fs_bsize(u_char *name)
{
    return 512;
}

#endif
