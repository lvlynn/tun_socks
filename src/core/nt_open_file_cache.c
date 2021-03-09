
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <nt_core.h>
#include <nt_event.h>


/*
 * open file cache caches
 *    open file handles with stat() info;
 *    directories stat() info;
 *    files and directories errors: not found, access denied, etc.
 */


#define NT_MIN_READ_AHEAD  (128 * 1024)


static void nt_open_file_cache_cleanup(void *data);
#if (NT_HAVE_OPENAT)
static nt_fd_t nt_openat_file_owner(nt_fd_t at_fd, const u_char *name,
    nt_int_t mode, nt_int_t create, nt_int_t access, nt_log_t *log);
#if (NT_HAVE_O_PATH)
static nt_int_t nt_file_o_path_info(nt_fd_t fd, nt_file_info_t *fi,
    nt_log_t *log);
#endif
#endif
static nt_fd_t nt_open_file_wrapper(nt_str_t *name,
    nt_open_file_info_t *of, nt_int_t mode, nt_int_t create,
    nt_int_t access, nt_log_t *log);
static nt_int_t nt_file_info_wrapper(nt_str_t *name,
    nt_open_file_info_t *of, nt_file_info_t *fi, nt_log_t *log);
static nt_int_t nt_open_and_stat_file(nt_str_t *name,
    nt_open_file_info_t *of, nt_log_t *log);
static void nt_open_file_add_event(nt_open_file_cache_t *cache,
    nt_cached_open_file_t *file, nt_open_file_info_t *of, nt_log_t *log);
static void nt_open_file_cleanup(void *data);
static void nt_close_cached_file(nt_open_file_cache_t *cache,
    nt_cached_open_file_t *file, nt_uint_t min_uses, nt_log_t *log);
static void nt_open_file_del_event(nt_cached_open_file_t *file);
static void nt_expire_old_cached_files(nt_open_file_cache_t *cache,
    nt_uint_t n, nt_log_t *log);
static void nt_open_file_cache_rbtree_insert_value(nt_rbtree_node_t *temp,
    nt_rbtree_node_t *node, nt_rbtree_node_t *sentinel);
static nt_cached_open_file_t *
    nt_open_file_lookup(nt_open_file_cache_t *cache, nt_str_t *name,
    uint32_t hash);
static void nt_open_file_cache_remove(nt_event_t *ev);


nt_open_file_cache_t *
nt_open_file_cache_init(nt_pool_t *pool, nt_uint_t max, time_t inactive)
{
    nt_pool_cleanup_t     *cln;
    nt_open_file_cache_t  *cache;

    cache = nt_palloc(pool, sizeof(nt_open_file_cache_t));
    if (cache == NULL) {
        return NULL;
    }

    nt_rbtree_init(&cache->rbtree, &cache->sentinel,
                    nt_open_file_cache_rbtree_insert_value);

    nt_queue_init(&cache->expire_queue);

    cache->current = 0;
    cache->max = max;
    cache->inactive = inactive;

    cln = nt_pool_cleanup_add(pool, 0);
    if (cln == NULL) {
        return NULL;
    }

    cln->handler = nt_open_file_cache_cleanup;
    cln->data = cache;

    return cache;
}


static void
nt_open_file_cache_cleanup(void *data)
{
    nt_open_file_cache_t  *cache = data;

    nt_queue_t             *q;
    nt_cached_open_file_t  *file;

    nt_log_debug0(NT_LOG_DEBUG_CORE, nt_cycle->log, 0,
                   "open file cache cleanup");

    for ( ;; ) {

        if (nt_queue_empty(&cache->expire_queue)) {
            break;
        }

        q = nt_queue_last(&cache->expire_queue);

        file = nt_queue_data(q, nt_cached_open_file_t, queue);

        nt_queue_remove(q);

        nt_rbtree_delete(&cache->rbtree, &file->node);

        cache->current--;

        nt_log_debug1(NT_LOG_DEBUG_CORE, nt_cycle->log, 0,
                       "delete cached open file: %s", file->name);

        if (!file->err && !file->is_dir) {
            file->close = 1;
            file->count = 0;
            nt_close_cached_file(cache, file, 0, nt_cycle->log);

        } else {
            nt_free(file->name);
            nt_free(file);
        }
    }

    if (cache->current) {
        nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0,
                      "%ui items still left in open file cache",
                      cache->current);
    }

    if (cache->rbtree.root != cache->rbtree.sentinel) {
        nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0,
                      "rbtree still is not empty in open file cache");

    }
}


nt_int_t
nt_open_cached_file(nt_open_file_cache_t *cache, nt_str_t *name,
    nt_open_file_info_t *of, nt_pool_t *pool)
{
    time_t                          now;
    uint32_t                        hash;
    nt_int_t                       rc;
    nt_file_info_t                 fi;
    nt_pool_cleanup_t             *cln;
    nt_cached_open_file_t         *file;
    nt_pool_cleanup_file_t        *clnf;
    nt_open_file_cache_cleanup_t  *ofcln;

    of->fd = NT_INVALID_FILE;
    of->err = 0;

    if (cache == NULL) {

        if (of->test_only) {

            if (nt_file_info_wrapper(name, of, &fi, pool->log)
                == NT_FILE_ERROR)
            {
                return NT_ERROR;
            }

            of->uniq = nt_file_uniq(&fi);
            of->mtime = nt_file_mtime(&fi);
            of->size = nt_file_size(&fi);
            of->fs_size = nt_file_fs_size(&fi);
            of->is_dir = nt_is_dir(&fi);
            of->is_file = nt_is_file(&fi);
            of->is_link = nt_is_link(&fi);
            of->is_exec = nt_is_exec(&fi);

            return NT_OK;
        }

        cln = nt_pool_cleanup_add(pool, sizeof(nt_pool_cleanup_file_t));
        if (cln == NULL) {
            return NT_ERROR;
        }

        rc = nt_open_and_stat_file(name, of, pool->log);

        if (rc == NT_OK && !of->is_dir) {
            cln->handler = nt_pool_cleanup_file;
            clnf = cln->data;

            clnf->fd = of->fd;
            clnf->name = name->data;
            clnf->log = pool->log;
        }

        return rc;
    }

    cln = nt_pool_cleanup_add(pool, sizeof(nt_open_file_cache_cleanup_t));
    if (cln == NULL) {
        return NT_ERROR;
    }

    now = nt_time();

    hash = nt_crc32_long(name->data, name->len);

    file = nt_open_file_lookup(cache, name, hash);

    if (file) {

        file->uses++;

        nt_queue_remove(&file->queue);

        if (file->fd == NT_INVALID_FILE && file->err == 0 && !file->is_dir) {

            /* file was not used often enough to keep open */

            rc = nt_open_and_stat_file(name, of, pool->log);

            if (rc != NT_OK && (of->err == 0 || !of->errors)) {
                goto failed;
            }

            goto add_event;
        }

        if (file->use_event
            || (file->event == NULL
                && (of->uniq == 0 || of->uniq == file->uniq)
                && now - file->created < of->valid
#if (NT_HAVE_OPENAT)
                && of->disable_symlinks == file->disable_symlinks
                && of->disable_symlinks_from == file->disable_symlinks_from
#endif
            ))
        {
            if (file->err == 0) {

                of->fd = file->fd;
                of->uniq = file->uniq;
                of->mtime = file->mtime;
                of->size = file->size;

                of->is_dir = file->is_dir;
                of->is_file = file->is_file;
                of->is_link = file->is_link;
                of->is_exec = file->is_exec;
                of->is_directio = file->is_directio;

                if (!file->is_dir) {
                    file->count++;
                    nt_open_file_add_event(cache, file, of, pool->log);
                }

            } else {
                of->err = file->err;
#if (NT_HAVE_OPENAT)
                of->failed = file->disable_symlinks ? nt_openat_file_n
                                                    : nt_open_file_n;
#else
                of->failed = nt_open_file_n;
#endif
            }

            goto found;
        }

        nt_log_debug4(NT_LOG_DEBUG_CORE, pool->log, 0,
                       "retest open file: %s, fd:%d, c:%d, e:%d",
                       file->name, file->fd, file->count, file->err);

        if (file->is_dir) {

            /*
             * chances that directory became file are very small
             * so test_dir flag allows to use a single syscall
             * in nt_file_info() instead of three syscalls
             */

            of->test_dir = 1;
        }

        of->fd = file->fd;
        of->uniq = file->uniq;

        rc = nt_open_and_stat_file(name, of, pool->log);

        if (rc != NT_OK && (of->err == 0 || !of->errors)) {
            goto failed;
        }

        if (of->is_dir) {

            if (file->is_dir || file->err) {
                goto update;
            }

            /* file became directory */

        } else if (of->err == 0) {  /* file */

            if (file->is_dir || file->err) {
                goto add_event;
            }

            if (of->uniq == file->uniq) {

                if (file->event) {
                    file->use_event = 1;
                }

                of->is_directio = file->is_directio;

                goto update;
            }

            /* file was changed */

        } else { /* error to cache */

            if (file->err || file->is_dir) {
                goto update;
            }

            /* file was removed, etc. */
        }

        if (file->count == 0) {

            nt_open_file_del_event(file);

            if (nt_close_file(file->fd) == NT_FILE_ERROR) {
                nt_log_error(NT_LOG_ALERT, pool->log, nt_errno,
                              nt_close_file_n " \"%V\" failed", name);
            }

            goto add_event;
        }

        nt_rbtree_delete(&cache->rbtree, &file->node);

        cache->current--;

        file->close = 1;

        goto create;
    }

    /* not found */

    rc = nt_open_and_stat_file(name, of, pool->log);

    if (rc != NT_OK && (of->err == 0 || !of->errors)) {
        goto failed;
    }

create:

    if (cache->current >= cache->max) {
        nt_expire_old_cached_files(cache, 0, pool->log);
    }

    file = nt_alloc(sizeof(nt_cached_open_file_t), pool->log);

    if (file == NULL) {
        goto failed;
    }

    file->name = nt_alloc(name->len + 1, pool->log);

    if (file->name == NULL) {
        nt_free(file);
        file = NULL;
        goto failed;
    }

    nt_cpystrn(file->name, name->data, name->len + 1);

    file->node.key = hash;

    nt_rbtree_insert(&cache->rbtree, &file->node);

    cache->current++;

    file->uses = 1;
    file->count = 0;
    file->use_event = 0;
    file->event = NULL;

add_event:

    nt_open_file_add_event(cache, file, of, pool->log);

update:

    file->fd = of->fd;
    file->err = of->err;
#if (NT_HAVE_OPENAT)
    file->disable_symlinks = of->disable_symlinks;
    file->disable_symlinks_from = of->disable_symlinks_from;
#endif

    if (of->err == 0) {
        file->uniq = of->uniq;
        file->mtime = of->mtime;
        file->size = of->size;

        file->close = 0;

        file->is_dir = of->is_dir;
        file->is_file = of->is_file;
        file->is_link = of->is_link;
        file->is_exec = of->is_exec;
        file->is_directio = of->is_directio;

        if (!of->is_dir) {
            file->count++;
        }
    }

    file->created = now;

found:

    file->accessed = now;

    nt_queue_insert_head(&cache->expire_queue, &file->queue);

    nt_log_debug5(NT_LOG_DEBUG_CORE, pool->log, 0,
                   "cached open file: %s, fd:%d, c:%d, e:%d, u:%d",
                   file->name, file->fd, file->count, file->err, file->uses);

    if (of->err == 0) {

        if (!of->is_dir) {
            cln->handler = nt_open_file_cleanup;
            ofcln = cln->data;

            ofcln->cache = cache;
            ofcln->file = file;
            ofcln->min_uses = of->min_uses;
            ofcln->log = pool->log;
        }

        return NT_OK;
    }

    return NT_ERROR;

failed:

    if (file) {
        nt_rbtree_delete(&cache->rbtree, &file->node);

        cache->current--;

        if (file->count == 0) {

            if (file->fd != NT_INVALID_FILE) {
                if (nt_close_file(file->fd) == NT_FILE_ERROR) {
                    nt_log_error(NT_LOG_ALERT, pool->log, nt_errno,
                                  nt_close_file_n " \"%s\" failed",
                                  file->name);
                }
            }

            nt_free(file->name);
            nt_free(file);

        } else {
            file->close = 1;
        }
    }

    if (of->fd != NT_INVALID_FILE) {
        if (nt_close_file(of->fd) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_ALERT, pool->log, nt_errno,
                          nt_close_file_n " \"%V\" failed", name);
        }
    }

    return NT_ERROR;
}


#if (NT_HAVE_OPENAT)

static nt_fd_t
nt_openat_file_owner(nt_fd_t at_fd, const u_char *name,
    nt_int_t mode, nt_int_t create, nt_int_t access, nt_log_t *log)
{
    nt_fd_t         fd;
    nt_err_t        err;
    nt_file_info_t  fi, atfi;

    /*
     * To allow symlinks with the same owner, use openat() (followed
     * by fstat()) and fstatat(AT_SYMLINK_NOFOLLOW), and then compare
     * uids between fstat() and fstatat().
     *
     * As there is a race between openat() and fstatat() we don't
     * know if openat() in fact opened symlink or not.  Therefore,
     * we have to compare uids even if fstatat() reports the opened
     * component isn't a symlink (as we don't know whether it was
     * symlink during openat() or not).
     */

    fd = nt_openat_file(at_fd, name, mode, create, access);

    if (fd == NT_INVALID_FILE) {
        return NT_INVALID_FILE;
    }

    if (nt_file_at_info(at_fd, name, &atfi, AT_SYMLINK_NOFOLLOW)
        == NT_FILE_ERROR)
    {
        err = nt_errno;
        goto failed;
    }

#if (NT_HAVE_O_PATH)
    if (nt_file_o_path_info(fd, &fi, log) == NT_ERROR) {
        err = nt_errno;
        goto failed;
    }
#else
    if (nt_fd_info(fd, &fi) == NT_FILE_ERROR) {
        err = nt_errno;
        goto failed;
    }
#endif

    if (fi.st_uid != atfi.st_uid) {
        err = NT_ELOOP;
        goto failed;
    }

    return fd;

failed:

    if (nt_close_file(fd) == NT_FILE_ERROR) {
        nt_log_error(NT_LOG_ALERT, log, nt_errno,
                      nt_close_file_n " \"%s\" failed", name);
    }

    nt_set_errno(err);

    return NT_INVALID_FILE;
}


#if (NT_HAVE_O_PATH)

static nt_int_t
nt_file_o_path_info(nt_fd_t fd, nt_file_info_t *fi, nt_log_t *log)
{
    static nt_uint_t  use_fstat = 1;

    /*
     * In Linux 2.6.39 the O_PATH flag was introduced that allows to obtain
     * a descriptor without actually opening file or directory.  It requires
     * less permissions for path components, but till Linux 3.6 fstat() returns
     * EBADF on such descriptors, and fstatat() with the AT_EMPTY_PATH flag
     * should be used instead.
     *
     * Three scenarios are handled in this function:
     *
     * 1) The kernel is newer than 3.6 or fstat() with O_PATH support was
     *    backported by vendor.  Then fstat() is used.
     *
     * 2) The kernel is newer than 2.6.39 but older than 3.6.  In this case
     *    the first call of fstat() returns EBADF and we fallback to fstatat()
     *    with AT_EMPTY_PATH which was introduced at the same time as O_PATH.
     *
     * 3) The kernel is older than 2.6.39 but nginx was build with O_PATH
     *    support.  Since descriptors are opened with O_PATH|O_RDONLY flags
     *    and O_PATH is ignored by the kernel then the O_RDONLY flag is
     *    actually used.  In this case fstat() just works.
     */

    if (use_fstat) {
        if (nt_fd_info(fd, fi) != NT_FILE_ERROR) {
            return NT_OK;
        }

        if (nt_errno != NT_EBADF) {
            return NT_ERROR;
        }

        nt_log_error(NT_LOG_NOTICE, log, 0,
                      "fstat(O_PATH) failed with EBADF, "
                      "switching to fstatat(AT_EMPTY_PATH)");

        use_fstat = 0;
    }

    if (nt_file_at_info(fd, "", fi, AT_EMPTY_PATH) != NT_FILE_ERROR) {
        return NT_OK;
    }

    return NT_ERROR;
}

#endif

#endif /* NT_HAVE_OPENAT */


static nt_fd_t
nt_open_file_wrapper(nt_str_t *name, nt_open_file_info_t *of,
    nt_int_t mode, nt_int_t create, nt_int_t access, nt_log_t *log)
{
    nt_fd_t  fd;

#if !(NT_HAVE_OPENAT)

    fd = nt_open_file(name->data, mode, create, access);

    if (fd == NT_INVALID_FILE) {
        of->err = nt_errno;
        of->failed = nt_open_file_n;
        return NT_INVALID_FILE;
    }

    return fd;

#else

    u_char           *p, *cp, *end;
    nt_fd_t          at_fd;
    nt_str_t         at_name;

    if (of->disable_symlinks == NT_DISABLE_SYMLINKS_OFF) {
        fd = nt_open_file(name->data, mode, create, access);

        if (fd == NT_INVALID_FILE) {
            of->err = nt_errno;
            of->failed = nt_open_file_n;
            return NT_INVALID_FILE;
        }

        return fd;
    }

    p = name->data;
    end = p + name->len;

    at_name = *name;

    if (of->disable_symlinks_from) {

        cp = p + of->disable_symlinks_from;

        *cp = '\0';

        at_fd = nt_open_file(p, NT_FILE_SEARCH|NT_FILE_NONBLOCK,
                              NT_FILE_OPEN, 0);

        *cp = '/';

        if (at_fd == NT_INVALID_FILE) {
            of->err = nt_errno;
            of->failed = nt_open_file_n;
            return NT_INVALID_FILE;
        }

        at_name.len = of->disable_symlinks_from;
        p = cp + 1;

    } else if (*p == '/') {

        at_fd = nt_open_file("/",
                              NT_FILE_SEARCH|NT_FILE_NONBLOCK,
                              NT_FILE_OPEN, 0);

        if (at_fd == NT_INVALID_FILE) {
            of->err = nt_errno;
            of->failed = nt_openat_file_n;
            return NT_INVALID_FILE;
        }

        at_name.len = 1;
        p++;

    } else {
        at_fd = NT_AT_FDCWD;
    }

    for ( ;; ) {
        cp = nt_strlchr(p, end, '/');
        if (cp == NULL) {
            break;
        }

        if (cp == p) {
            p++;
            continue;
        }

        *cp = '\0';

        if (of->disable_symlinks == NT_DISABLE_SYMLINKS_NOTOWNER) {
            fd = nt_openat_file_owner(at_fd, p,
                                       NT_FILE_SEARCH|NT_FILE_NONBLOCK,
                                       NT_FILE_OPEN, 0, log);

        } else {
            fd = nt_openat_file(at_fd, p,
                           NT_FILE_SEARCH|NT_FILE_NONBLOCK|NT_FILE_NOFOLLOW,
                           NT_FILE_OPEN, 0);
        }

        *cp = '/';

        if (fd == NT_INVALID_FILE) {
            of->err = nt_errno;
            of->failed = nt_openat_file_n;
            goto failed;
        }

        if (at_fd != NT_AT_FDCWD && nt_close_file(at_fd) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_ALERT, log, nt_errno,
                          nt_close_file_n " \"%V\" failed", &at_name);
        }

        p = cp + 1;
        at_fd = fd;
        at_name.len = cp - at_name.data;
    }

    if (p == end) {

        /*
         * If pathname ends with a trailing slash, assume the last path
         * component is a directory and reopen it with requested flags;
         * if not, fail with ENOTDIR as per POSIX.
         *
         * We cannot rely on O_DIRECTORY in the loop above to check
         * that the last path component is a directory because
         * O_DIRECTORY doesn't work on FreeBSD 8.  Fortunately, by
         * reopening a directory, we don't depend on it at all.
         */

        fd = nt_openat_file(at_fd, ".", mode, create, access);
        goto done;
    }

    if (of->disable_symlinks == NT_DISABLE_SYMLINKS_NOTOWNER
        && !(create & (NT_FILE_CREATE_OR_OPEN|NT_FILE_TRUNCATE)))
    {
        fd = nt_openat_file_owner(at_fd, p, mode, create, access, log);

    } else {
        fd = nt_openat_file(at_fd, p, mode|NT_FILE_NOFOLLOW, create, access);
    }

done:

    if (fd == NT_INVALID_FILE) {
        of->err = nt_errno;
        of->failed = nt_openat_file_n;
    }

failed:

    if (at_fd != NT_AT_FDCWD && nt_close_file(at_fd) == NT_FILE_ERROR) {
        nt_log_error(NT_LOG_ALERT, log, nt_errno,
                      nt_close_file_n " \"%V\" failed", &at_name);
    }

    return fd;
#endif
}


static nt_int_t
nt_file_info_wrapper(nt_str_t *name, nt_open_file_info_t *of,
    nt_file_info_t *fi, nt_log_t *log)
{
    nt_int_t  rc;

#if !(NT_HAVE_OPENAT)

    rc = nt_file_info(name->data, fi);

    if (rc == NT_FILE_ERROR) {
        of->err = nt_errno;
        of->failed = nt_file_info_n;
        return NT_FILE_ERROR;
    }

    return rc;

#else

    nt_fd_t  fd;

    if (of->disable_symlinks == NT_DISABLE_SYMLINKS_OFF) {

        rc = nt_file_info(name->data, fi);

        if (rc == NT_FILE_ERROR) {
            of->err = nt_errno;
            of->failed = nt_file_info_n;
            return NT_FILE_ERROR;
        }

        return rc;
    }

    fd = nt_open_file_wrapper(name, of, NT_FILE_RDONLY|NT_FILE_NONBLOCK,
                               NT_FILE_OPEN, 0, log);

    if (fd == NT_INVALID_FILE) {
        return NT_FILE_ERROR;
    }

    rc = nt_fd_info(fd, fi);

    if (rc == NT_FILE_ERROR) {
        of->err = nt_errno;
        of->failed = nt_fd_info_n;
    }

    if (nt_close_file(fd) == NT_FILE_ERROR) {
        nt_log_error(NT_LOG_ALERT, log, nt_errno,
                      nt_close_file_n " \"%V\" failed", name);
    }

    return rc;
#endif
}


static nt_int_t
nt_open_and_stat_file(nt_str_t *name, nt_open_file_info_t *of,
    nt_log_t *log)
{
    nt_fd_t         fd;
    nt_file_info_t  fi;

    if (of->fd != NT_INVALID_FILE) {

        if (nt_file_info_wrapper(name, of, &fi, log) == NT_FILE_ERROR) {
            of->fd = NT_INVALID_FILE;
            return NT_ERROR;
        }

        if (of->uniq == nt_file_uniq(&fi)) {
            goto done;
        }

    } else if (of->test_dir) {

        if (nt_file_info_wrapper(name, of, &fi, log) == NT_FILE_ERROR) {
            of->fd = NT_INVALID_FILE;
            return NT_ERROR;
        }

        if (nt_is_dir(&fi)) {
            goto done;
        }
    }

    if (!of->log) {

        /*
         * Use non-blocking open() not to hang on FIFO files, etc.
         * This flag has no effect on a regular files.
         */

        fd = nt_open_file_wrapper(name, of, NT_FILE_RDONLY|NT_FILE_NONBLOCK,
                                   NT_FILE_OPEN, 0, log);

    } else {
        fd = nt_open_file_wrapper(name, of, NT_FILE_APPEND,
                                   NT_FILE_CREATE_OR_OPEN,
                                   NT_FILE_DEFAULT_ACCESS, log);
    }

    if (fd == NT_INVALID_FILE) {
        of->fd = NT_INVALID_FILE;
        return NT_ERROR;
    }

    if (nt_fd_info(fd, &fi) == NT_FILE_ERROR) {
        nt_log_error(NT_LOG_CRIT, log, nt_errno,
                      nt_fd_info_n " \"%V\" failed", name);

        if (nt_close_file(fd) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_ALERT, log, nt_errno,
                          nt_close_file_n " \"%V\" failed", name);
        }

        of->fd = NT_INVALID_FILE;

        return NT_ERROR;
    }

    if (nt_is_dir(&fi)) {
        if (nt_close_file(fd) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_ALERT, log, nt_errno,
                          nt_close_file_n " \"%V\" failed", name);
        }

        of->fd = NT_INVALID_FILE;

    } else {
        of->fd = fd;

        if (of->read_ahead && nt_file_size(&fi) > NT_MIN_READ_AHEAD) {
            if (nt_read_ahead(fd, of->read_ahead) == NT_ERROR) {
                nt_log_error(NT_LOG_ALERT, log, nt_errno,
                              nt_read_ahead_n " \"%V\" failed", name);
            }
        }

        if (of->directio <= nt_file_size(&fi)) {
            if (nt_directio_on(fd) == NT_FILE_ERROR) {
                nt_log_error(NT_LOG_ALERT, log, nt_errno,
                              nt_directio_on_n " \"%V\" failed", name);

            } else {
                of->is_directio = 1;
            }
        }
    }

done:

    of->uniq = nt_file_uniq(&fi);
    of->mtime = nt_file_mtime(&fi);
    of->size = nt_file_size(&fi);
    of->fs_size = nt_file_fs_size(&fi);
    of->is_dir = nt_is_dir(&fi);
    of->is_file = nt_is_file(&fi);
    of->is_link = nt_is_link(&fi);
    of->is_exec = nt_is_exec(&fi);

    return NT_OK;
}


/*
 * we ignore any possible event setting error and
 * fallback to usual periodic file retests
 */

static void
nt_open_file_add_event(nt_open_file_cache_t *cache,
    nt_cached_open_file_t *file, nt_open_file_info_t *of, nt_log_t *log)
{
    nt_open_file_cache_event_t  *fev;

    if (!(nt_event_flags & NT_USE_VNODE_EVENT)
        || !of->events
        || file->event
        || of->fd == NT_INVALID_FILE
        || file->uses < of->min_uses)
    {
        return;
    }

    file->use_event = 0;

    file->event = nt_calloc(sizeof(nt_event_t), log);
    if (file->event== NULL) {
        return;
    }

    fev = nt_alloc(sizeof(nt_open_file_cache_event_t), log);
    if (fev == NULL) {
        nt_free(file->event);
        file->event = NULL;
        return;
    }

    fev->fd = of->fd;
    fev->file = file;
    fev->cache = cache;

    file->event->handler = nt_open_file_cache_remove;
    file->event->data = fev;

    /*
     * although vnode event may be called while nt_cycle->poll
     * destruction, however, cleanup procedures are run before any
     * memory freeing and events will be canceled.
     */

    file->event->log = nt_cycle->log;

    if (nt_add_event(file->event, NT_VNODE_EVENT, NT_ONESHOT_EVENT)
        != NT_OK)
    {
        nt_free(file->event->data);
        nt_free(file->event);
        file->event = NULL;
        return;
    }

    /*
     * we do not set file->use_event here because there may be a race
     * condition: a file may be deleted between opening the file and
     * adding event, so we rely upon event notification only after
     * one file revalidation on next file access
     */

    return;
}


static void
nt_open_file_cleanup(void *data)
{
    nt_open_file_cache_cleanup_t  *c = data;

    c->file->count--;

    nt_close_cached_file(c->cache, c->file, c->min_uses, c->log);

    /* drop one or two expired open files */
    nt_expire_old_cached_files(c->cache, 1, c->log);
}


static void
nt_close_cached_file(nt_open_file_cache_t *cache,
    nt_cached_open_file_t *file, nt_uint_t min_uses, nt_log_t *log)
{
    nt_log_debug5(NT_LOG_DEBUG_CORE, log, 0,
                   "close cached open file: %s, fd:%d, c:%d, u:%d, %d",
                   file->name, file->fd, file->count, file->uses, file->close);

    if (!file->close) {

        file->accessed = nt_time();

        nt_queue_remove(&file->queue);

        nt_queue_insert_head(&cache->expire_queue, &file->queue);

        if (file->uses >= min_uses || file->count) {
            return;
        }
    }

    nt_open_file_del_event(file);

    if (file->count) {
        return;
    }

    if (file->fd != NT_INVALID_FILE) {

        if (nt_close_file(file->fd) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_ALERT, log, nt_errno,
                          nt_close_file_n " \"%s\" failed", file->name);
        }

        file->fd = NT_INVALID_FILE;
    }

    if (!file->close) {
        return;
    }

    nt_free(file->name);
    nt_free(file);
}


static void
nt_open_file_del_event(nt_cached_open_file_t *file)
{
    if (file->event == NULL) {
        return;
    }

    (void) nt_del_event(file->event, NT_VNODE_EVENT,
                         file->count ? NT_FLUSH_EVENT : NT_CLOSE_EVENT);

    nt_free(file->event->data);
    nt_free(file->event);
    file->event = NULL;
    file->use_event = 0;
}


static void
nt_expire_old_cached_files(nt_open_file_cache_t *cache, nt_uint_t n,
    nt_log_t *log)
{
    time_t                   now;
    nt_queue_t             *q;
    nt_cached_open_file_t  *file;

    now = nt_time();

    /*
     * n == 1 deletes one or two inactive files
     * n == 0 deletes least recently used file by force
     *        and one or two inactive files
     */

    while (n < 3) {

        if (nt_queue_empty(&cache->expire_queue)) {
            return;
        }

        q = nt_queue_last(&cache->expire_queue);

        file = nt_queue_data(q, nt_cached_open_file_t, queue);

        if (n++ != 0 && now - file->accessed <= cache->inactive) {
            return;
        }

        nt_queue_remove(q);

        nt_rbtree_delete(&cache->rbtree, &file->node);

        cache->current--;

        nt_log_debug1(NT_LOG_DEBUG_CORE, log, 0,
                       "expire cached open file: %s", file->name);

        if (!file->err && !file->is_dir) {
            file->close = 1;
            nt_close_cached_file(cache, file, 0, log);

        } else {
            nt_free(file->name);
            nt_free(file);
        }
    }
}


static void
nt_open_file_cache_rbtree_insert_value(nt_rbtree_node_t *temp,
    nt_rbtree_node_t *node, nt_rbtree_node_t *sentinel)
{
    nt_rbtree_node_t       **p;
    nt_cached_open_file_t    *file, *file_temp;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            file = (nt_cached_open_file_t *) node;
            file_temp = (nt_cached_open_file_t *) temp;

            p = (nt_strcmp(file->name, file_temp->name) < 0)
                    ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    nt_rbt_red(node);
}


static nt_cached_open_file_t *
nt_open_file_lookup(nt_open_file_cache_t *cache, nt_str_t *name,
    uint32_t hash)
{
    nt_int_t                rc;
    nt_rbtree_node_t       *node, *sentinel;
    nt_cached_open_file_t  *file;

    node = cache->rbtree.root;
    sentinel = cache->rbtree.sentinel;

    while (node != sentinel) {

        if (hash < node->key) {
            node = node->left;
            continue;
        }

        if (hash > node->key) {
            node = node->right;
            continue;
        }

        /* hash == node->key */

        file = (nt_cached_open_file_t *) node;

        rc = nt_strcmp(name->data, file->name);

        if (rc == 0) {
            return file;
        }

        node = (rc < 0) ? node->left : node->right;
    }

    return NULL;
}


static void
nt_open_file_cache_remove(nt_event_t *ev)
{
    nt_cached_open_file_t       *file;
    nt_open_file_cache_event_t  *fev;

    fev = ev->data;
    file = fev->file;

    nt_queue_remove(&file->queue);

    nt_rbtree_delete(&fev->cache->rbtree, &file->node);

    fev->cache->current--;

    /* NT_ONESHOT_EVENT was already deleted */
    file->event = NULL;
    file->use_event = 0;

    file->close = 1;

    nt_close_cached_file(fev->cache, file, 0, ev->log);

    /* free memory only when fev->cache and fev->file are already not needed */

    nt_free(ev->data);
    nt_free(ev);
}
