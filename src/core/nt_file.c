

#include <nt_core.h>


static nt_int_t nt_test_full_name(nt_str_t *name);


//static nt_atomic_t   temp_number = 0;
//nt_atomic_t         *nt_temp_number = &temp_number;
//nt_atomic_int_t      nt_random_number = 123456;


nt_int_t
nt_get_full_name(nt_pool_t *pool, nt_str_t *prefix, nt_str_t *name)
{
    size_t      len;
    u_char     *p, *n;
    nt_int_t   rc;

    rc = nt_test_full_name(name);

    if (rc == NT_OK) {
        return rc;
    }

    len = prefix->len;

#if (NT_WIN32)

    if (rc == 2) {
        len = rc;
    }

#endif

    n = nt_pnalloc(pool, len + name->len + 1);
    if (n == NULL) {
        return NT_ERROR;
    }

    p = nt_cpymem(n, prefix->data, len);
    nt_cpystrn(p, name->data, name->len + 1);

    name->len += len;
    name->data = n;

    return NT_OK;
}


static nt_int_t
nt_test_full_name(nt_str_t *name)
{
#if (NT_WIN32)
    u_char  c0, c1;

    c0 = name->data[0];

    if (name->len < 2) {
        if (c0 == '/') {
            return 2;
        }

        return NT_DECLINED;
    }

    c1 = name->data[1];

    if (c1 == ':') {
        c0 |= 0x20;

        if ((c0 >= 'a' && c0 <= 'z')) {
            return NT_OK;
        }

        return NT_DECLINED;
    }

    if (c1 == '/') {
        return NT_OK;
    }

    if (c0 == '/') {
        return 2;
    }

    return NT_DECLINED;

#else

    if (name->data[0] == '/') {
        return NT_OK;
    }

    return NT_DECLINED;

#endif
}


ssize_t
nt_write_chain_to_temp_file(nt_temp_file_t *tf, nt_chain_t *chain)
{
    nt_int_t  rc;

    if (tf->file.fd == NT_INVALID_FILE) {
        rc = nt_create_temp_file(&tf->file, tf->path, tf->pool,
                                  tf->persistent, tf->clean, tf->access);

        if (rc != NT_OK) {
            return rc;
        }

        if (tf->log_level) {
            nt_log_error(tf->log_level, tf->file.log, 0, "%s %V",
                          tf->warn, &tf->file.name);
        }
    }

#if (NT_THREADS && NT_HAVE_PWRITEV)

    if (tf->thread_write) {
        return nt_thread_write_chain_to_file(&tf->file, chain, tf->offset,
                                              tf->pool);
    }

#endif

    return nt_write_chain_to_file(&tf->file, chain, tf->offset, tf->pool);
}


nt_int_t
nt_create_temp_file(nt_file_t *file, nt_path_t *path, nt_pool_t *pool,
    nt_uint_t persistent, nt_uint_t clean, nt_uint_t access)
{
    size_t                    levels;
    u_char                   *p;
    uint32_t                  n;
    nt_err_t                 err;
    nt_str_t                 name;
    nt_uint_t                prefix;
    nt_pool_cleanup_t       *cln;
    nt_pool_cleanup_file_t  *clnf;

    if (file->name.len) {
        name = file->name;
        levels = 0;
        prefix = 1;

    } else {
        name = path->name;
        levels = path->len;
        prefix = 0;
    }

    file->name.len = name.len + 1 + levels + 10;

    file->name.data = nt_pnalloc(pool, file->name.len + 1);
    if (file->name.data == NULL) {
        return NT_ERROR;
    }

#if 0
    for (i = 0; i < file->name.len; i++) {
        file->name.data[i] = 'X';
    }
#endif

    p = nt_cpymem(file->name.data, name.data, name.len);

    if (prefix) {
        *p = '.';
    }

    p += 1 + levels;

    n = (uint32_t) nt_next_temp_number(0);

    cln = nt_pool_cleanup_add(pool, sizeof(nt_pool_cleanup_file_t));
    if (cln == NULL) {
        return NT_ERROR;
    }

    for ( ;; ) {
        (void) nt_sprintf(p, "%010uD%Z", n);

        if (!prefix) {
            nt_create_hashed_filename(path, file->name.data, file->name.len);
        }

        nt_log_debug1(NT_LOG_DEBUG_CORE, file->log, 0,
                       "hashed path: %s", file->name.data);

        file->fd = nt_open_tempfile(file->name.data, persistent, access);

        nt_log_debug1(NT_LOG_DEBUG_CORE, file->log, 0,
                       "temp fd:%d", file->fd);

        if (file->fd != NT_INVALID_FILE) {

            cln->handler = clean ? nt_pool_delete_file : nt_pool_cleanup_file;
            clnf = cln->data;

            clnf->fd = file->fd;
            clnf->name = file->name.data;
            clnf->log = pool->log;

            return NT_OK;
        }

        err = nt_errno;

        if (err == NT_EEXIST_FILE) {
            n = (uint32_t) nt_next_temp_number(1);
            continue;
        }

        if ((path->level[0] == 0) || (err != NT_ENOPATH)) {
            nt_log_error(NT_LOG_CRIT, file->log, err,
                          nt_open_tempfile_n " \"%s\" failed",
                          file->name.data);
            return NT_ERROR;
        }

        if (nt_create_path(file, path) == NT_ERROR) {
            return NT_ERROR;
        }
    }
}


void
nt_create_hashed_filename(nt_path_t *path, u_char *file, size_t len)
{
    size_t      i, level;
    nt_uint_t  n;

    i = path->name.len + 1;

    file[path->name.len + path->len]  = '/';

    for (n = 0; n < NT_MAX_PATH_LEVEL; n++) {
        level = path->level[n];

        if (level == 0) {
            break;
        }

        len -= level;
        file[i - 1] = '/';
        nt_memcpy(&file[i], &file[len], level);
        i += level + 1;
    }
}


nt_int_t
nt_create_path(nt_file_t *file, nt_path_t *path)
{
    size_t      pos;
    nt_err_t   err;
    nt_uint_t  i;

    pos = path->name.len;

    for (i = 0; i < NT_MAX_PATH_LEVEL; i++) {
        if (path->level[i] == 0) {
            break;
        }

        pos += path->level[i] + 1;

        file->name.data[pos] = '\0';

        nt_log_debug1(NT_LOG_DEBUG_CORE, file->log, 0,
                       "temp file: \"%s\"", file->name.data);

        if (nt_create_dir(file->name.data, 0700) == NT_FILE_ERROR) {
            err = nt_errno;
            if (err != NT_EEXIST) {
                nt_log_error(NT_LOG_CRIT, file->log, err,
                              nt_create_dir_n " \"%s\" failed",
                              file->name.data);
                return NT_ERROR;
            }
        }

        file->name.data[pos] = '/';
    }

    return NT_OK;
}


nt_err_t
nt_create_full_path(u_char *dir, nt_uint_t access)
{
    u_char     *p, ch;
    nt_err_t   err;

    err = 0;

#if (NT_WIN32)
    p = dir + 3;
#else
    p = dir + 1;
#endif

    for ( /* void */ ; *p; p++) {
        ch = *p;

        if (ch != '/') {
            continue;
        }

        *p = '\0';

        if (nt_create_dir(dir, access) == NT_FILE_ERROR) {
            err = nt_errno;

            switch (err) {
            case NT_EEXIST:
                err = 0;
            case NT_EACCES:
                break;

            default:
                return err;
            }
        }

        *p = '/';
    }

    return err;
}

#if 0
nt_atomic_uint_t
nt_next_temp_number(nt_uint_t collision)
{
    nt_atomic_uint_t  n, add;

    add = collision ? nt_random_number : 1;

    n = nt_atomic_fetch_add(nt_temp_number, add);

    return n + add;
}
#endif

/*static nt_inline uint32_t
nt_atomic_fetch_add(uint32_t *value, uint32_t add)
{
    uint32_t  old;

    old = *value;
    *value += add;

    return old;

}*/

uint32_t 
nt_next_temp_number(nt_uint_t collision)
{
    uint32_t  n, add;

    static uint32_t  temp_number = 0;
    uint32_t    *nt_temp_number = &temp_number;
    uint32_t     nt_random_number = 123456;


    add = collision ? nt_random_number : 1;

    n = nt_atomic_fetch_add(nt_temp_number, add);

    return n + add;
}



char *
nt_conf_set_path_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char  *p = conf;

    ssize_t      level;
    nt_str_t   *value;
    nt_uint_t   i, n;
    nt_path_t  *path, **slot;

    slot = (nt_path_t **) (p + cmd->offset);

    if (*slot) {
        return "is duplicate";
    }

    path = nt_pcalloc(cf->pool, sizeof(nt_path_t));
    if (path == NULL) {
        return NT_CONF_ERROR;
    }

    value = cf->args->elts;

    path->name = value[1];

    if (path->name.data[path->name.len - 1] == '/') {
        path->name.len--;
    }

    if (nt_conf_full_name(cf->cycle, &path->name, 0) != NT_OK) {
        return NT_CONF_ERROR;
    }

    path->conf_file = cf->conf_file->file.name.data;
    path->line = cf->conf_file->line;

    for (i = 0, n = 2; n < cf->args->nelts; i++, n++) {
        level = nt_atoi(value[n].data, value[n].len);
        if (level == NT_ERROR || level == 0) {
            return "invalid value";
        }

        path->level[i] = level;
        path->len += level + 1;
    }

    if (path->len > 10 + i) {
        return "invalid value";
    }

    *slot = path;

    if (nt_add_path(cf, slot) == NT_ERROR) {
        return NT_CONF_ERROR;
    }

    return NT_CONF_OK;
}


char *
nt_conf_merge_path_value(nt_conf_t *cf, nt_path_t **path, nt_path_t *prev,
    nt_path_init_t *init)
{
    nt_uint_t  i;

    if (*path) {
        return NT_CONF_OK;
    }

    if (prev) {
        *path = prev;
        return NT_CONF_OK;
    }

    *path = nt_pcalloc(cf->pool, sizeof(nt_path_t));
    if (*path == NULL) {
        return NT_CONF_ERROR;
    }

    (*path)->name = init->name;

    if (nt_conf_full_name(cf->cycle, &(*path)->name, 0) != NT_OK) {
        return NT_CONF_ERROR;
    }

    for (i = 0; i < NT_MAX_PATH_LEVEL; i++) {
        (*path)->level[i] = init->level[i];
        (*path)->len += init->level[i] + (init->level[i] ? 1 : 0);
    }

    if (nt_add_path(cf, path) != NT_OK) {
        return NT_CONF_ERROR;
    }

    return NT_CONF_OK;
}


char *
nt_conf_set_access_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char  *confp = conf;

    u_char      *p;
    nt_str_t   *value;
    nt_uint_t   i, right, shift, *access, user;

    access = (nt_uint_t *) (confp + cmd->offset);

    if (*access != NT_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    value = cf->args->elts;

    *access = 0;
    user = 0600;

    for (i = 1; i < cf->args->nelts; i++) {

        p = value[i].data;

        if (nt_strncmp(p, "user:", sizeof("user:") - 1) == 0) {
            shift = 6;
            p += sizeof("user:") - 1;
            user = 0;

        } else if (nt_strncmp(p, "group:", sizeof("group:") - 1) == 0) {
            shift = 3;
            p += sizeof("group:") - 1;

        } else if (nt_strncmp(p, "all:", sizeof("all:") - 1) == 0) {
            shift = 0;
            p += sizeof("all:") - 1;

        } else {
            goto invalid;
        }

        if (nt_strcmp(p, "rw") == 0) {
            right = 6;

        } else if (nt_strcmp(p, "r") == 0) {
            right = 4;

        } else {
            goto invalid;
        }

        *access |= right << shift;
    }

    *access |= user;

    return NT_CONF_OK;

invalid:

    nt_conf_log_error(NT_LOG_EMERG, cf, 0, "invalid value \"%V\"", &value[i]);

    return NT_CONF_ERROR;
}


nt_int_t
nt_add_path(nt_conf_t *cf, nt_path_t **slot)
{
    nt_uint_t   i, n;
    nt_path_t  *path, **p;

    path = *slot;

    p = cf->cycle->paths.elts;
    for (i = 0; i < cf->cycle->paths.nelts; i++) {
        if (p[i]->name.len == path->name.len
            && nt_strcmp(p[i]->name.data, path->name.data) == 0)
        {
            if (p[i]->data != path->data) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "the same path name \"%V\" "
                                   "used in %s:%ui and",
                                   &p[i]->name, p[i]->conf_file, p[i]->line);
                return NT_ERROR;
            }

            for (n = 0; n < NT_MAX_PATH_LEVEL; n++) {
                if (p[i]->level[n] != path->level[n]) {
                    if (path->conf_file == NULL) {
                        if (p[i]->conf_file == NULL) {
                            nt_log_error(NT_LOG_EMERG, cf->log, 0,
                                      "the default path name \"%V\" has "
                                      "the same name as another default path, "
                                      "but the different levels, you need to "
                                      "redefine one of them in http section",
                                      &p[i]->name);
                            return NT_ERROR;
                        }

                        nt_log_error(NT_LOG_EMERG, cf->log, 0,
                                      "the path name \"%V\" in %s:%ui has "
                                      "the same name as default path, but "
                                      "the different levels, you need to "
                                      "define default path in http section",
                                      &p[i]->name, p[i]->conf_file, p[i]->line);
                        return NT_ERROR;
                    }

                    nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                      "the same path name \"%V\" in %s:%ui "
                                      "has the different levels than",
                                      &p[i]->name, p[i]->conf_file, p[i]->line);
                    return NT_ERROR;
                }

                if (p[i]->level[n] == 0) {
                    break;
                }
            }

            *slot = p[i];

            return NT_OK;
        }
    }

    p = nt_array_push(&cf->cycle->paths);
    if (p == NULL) {
        return NT_ERROR;
    }

    *p = path;

    return NT_OK;
}


nt_int_t
nt_create_paths(nt_cycle_t *cycle, nt_uid_t user)
{
    nt_err_t         err;
    nt_uint_t        i;
    nt_path_t      **path;

    path = cycle->paths.elts;
    for (i = 0; i < cycle->paths.nelts; i++) {

        if (nt_create_dir(path[i]->name.data, 0700) == NT_FILE_ERROR) {
            err = nt_errno;
            if (err != NT_EEXIST) {
                nt_log_error(NT_LOG_EMERG, cycle->log, err,
                              nt_create_dir_n " \"%s\" failed",
                              path[i]->name.data);
                return NT_ERROR;
            }
        }

        if (user == (nt_uid_t) NT_CONF_UNSET_UINT) {
            continue;
        }

#if !(NT_WIN32)
        {
        nt_file_info_t   fi;

        if (nt_file_info(path[i]->name.data, &fi) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                          nt_file_info_n " \"%s\" failed", path[i]->name.data);
            return NT_ERROR;
        }

        if (fi.st_uid != user) {
            if (chown((const char *) path[i]->name.data, user, -1) == -1) {
                nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                              "chown(\"%s\", %d) failed",
                              path[i]->name.data, user);
                return NT_ERROR;
            }
        }

        if ((fi.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR))
                                                  != (S_IRUSR|S_IWUSR|S_IXUSR))
        {
            fi.st_mode |= (S_IRUSR|S_IWUSR|S_IXUSR);

            if (chmod((const char *) path[i]->name.data, fi.st_mode) == -1) {
                nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                              "chmod() \"%s\" failed", path[i]->name.data);
                return NT_ERROR;
            }
        }
        }
#endif
    }

    return NT_OK;
}


nt_int_t
nt_ext_rename_file(nt_str_t *src, nt_str_t *to, nt_ext_rename_file_t *ext)
{
    u_char           *name;
    nt_err_t         err;
    nt_copy_file_t   cf;

#if !(NT_WIN32)

    if (ext->access) {
        if (nt_change_file_access(src->data, ext->access) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_CRIT, ext->log, nt_errno,
                          nt_change_file_access_n " \"%s\" failed", src->data);
            err = 0;
            goto failed;
        }
    }

#endif

    if (ext->time != -1) {
        if (nt_set_file_time(src->data, ext->fd, ext->time) != NT_OK) {
            nt_log_error(NT_LOG_CRIT, ext->log, nt_errno,
                          nt_set_file_time_n " \"%s\" failed", src->data);
            err = 0;
            goto failed;
        }
    }

    if (nt_rename_file(src->data, to->data) != NT_FILE_ERROR) {
        return NT_OK;
    }

    err = nt_errno;

    if (err == NT_ENOPATH) {

        if (!ext->create_path) {
            goto failed;
        }

        err = nt_create_full_path(to->data, nt_dir_access(ext->path_access));

        if (err) {
            nt_log_error(NT_LOG_CRIT, ext->log, err,
                          nt_create_dir_n " \"%s\" failed", to->data);
            err = 0;
            goto failed;
        }

        if (nt_rename_file(src->data, to->data) != NT_FILE_ERROR) {
            return NT_OK;
        }

        err = nt_errno;
    }

#if (NT_WIN32)

    if (err == NT_EEXIST || err == NT_EEXIST_FILE) {
        err = nt_win32_rename_file(src, to, ext->log);

        if (err == 0) {
            return NT_OK;
        }
    }

#endif

    if (err == NT_EXDEV) {

        cf.size = -1;
        cf.buf_size = 0;
        cf.access = ext->access;
        cf.time = ext->time;
        cf.log = ext->log;

        name = nt_alloc(to->len + 1 + 10 + 1, ext->log);
        if (name == NULL) {
            return NT_ERROR;
        }

        (void) nt_sprintf(name, "%*s.%010uD%Z", to->len, to->data,
                           (uint32_t) nt_next_temp_number(0));

        if (nt_copy_file(src->data, name, &cf) == NT_OK) {

            if (nt_rename_file(name, to->data) != NT_FILE_ERROR) {
                nt_free(name);

                if (nt_delete_file(src->data) == NT_FILE_ERROR) {
                    nt_log_error(NT_LOG_CRIT, ext->log, nt_errno,
                                  nt_delete_file_n " \"%s\" failed",
                                  src->data);
                    return NT_ERROR;
                }

                return NT_OK;
            }

            nt_log_error(NT_LOG_CRIT, ext->log, nt_errno,
                          nt_rename_file_n " \"%s\" to \"%s\" failed",
                          name, to->data);

            if (nt_delete_file(name) == NT_FILE_ERROR) {
                nt_log_error(NT_LOG_CRIT, ext->log, nt_errno,
                              nt_delete_file_n " \"%s\" failed", name);

            }
        }

        nt_free(name);

        err = 0;
    }

failed:

    if (ext->delete_file) {
        if (nt_delete_file(src->data) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_CRIT, ext->log, nt_errno,
                          nt_delete_file_n " \"%s\" failed", src->data);
        }
    }

    if (err) {
        nt_log_error(NT_LOG_CRIT, ext->log, err,
                      nt_rename_file_n " \"%s\" to \"%s\" failed",
                      src->data, to->data);
    }

    return NT_ERROR;
}


nt_int_t
nt_copy_file(u_char *from, u_char *to, nt_copy_file_t *cf)
{
    char             *buf;
    off_t             size;
    time_t            time;
    size_t            len;
    ssize_t           n;
    nt_fd_t          fd, nfd;
    nt_int_t         rc;
    nt_uint_t        access;
    nt_file_info_t   fi;

    rc = NT_ERROR;
    buf = NULL;
    nfd = NT_INVALID_FILE;

    fd = nt_open_file(from, NT_FILE_RDONLY, NT_FILE_OPEN, 0);

    if (fd == NT_INVALID_FILE) {
        nt_log_error(NT_LOG_CRIT, cf->log, nt_errno,
                      nt_open_file_n " \"%s\" failed", from);
        goto failed;
    }

    if (cf->size != -1 && cf->access != 0 && cf->time != -1) {
        size = cf->size;
        access = cf->access;
        time = cf->time;

    } else {
        if (nt_fd_info(fd, &fi) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_ALERT, cf->log, nt_errno,
                          nt_fd_info_n " \"%s\" failed", from);

            goto failed;
        }

        size = (cf->size != -1) ? cf->size : nt_file_size(&fi);
        access = cf->access ? cf->access : nt_file_access(&fi);
        time = (cf->time != -1) ? cf->time : nt_file_mtime(&fi);
    }

    len = cf->buf_size ? cf->buf_size : 65536;

    if ((off_t) len > size) {
        len = (size_t) size;
    }

    buf = nt_alloc(len, cf->log);
    if (buf == NULL) {
        goto failed;
    }

    nfd = nt_open_file(to, NT_FILE_WRONLY, NT_FILE_TRUNCATE, access);

    if (nfd == NT_INVALID_FILE) {
        nt_log_error(NT_LOG_CRIT, cf->log, nt_errno,
                      nt_open_file_n " \"%s\" failed", to);
        goto failed;
    }

    while (size > 0) {

        if ((off_t) len > size) {
            len = (size_t) size;
        }

        n = nt_read_fd(fd, buf, len);

        if (n == -1) {
            nt_log_error(NT_LOG_ALERT, cf->log, nt_errno,
                          nt_read_fd_n " \"%s\" failed", from);
            goto failed;
        }

        if ((size_t) n != len) {
            nt_log_error(NT_LOG_ALERT, cf->log, 0,
                          nt_read_fd_n " has read only %z of %O from %s",
                          n, size, from);
            goto failed;
        }

        n = nt_write_fd(nfd, buf, len);

        if (n == -1) {
            nt_log_error(NT_LOG_ALERT, cf->log, nt_errno,
                          nt_write_fd_n " \"%s\" failed", to);
            goto failed;
        }

        if ((size_t) n != len) {
            nt_log_error(NT_LOG_ALERT, cf->log, 0,
                          nt_write_fd_n " has written only %z of %O to %s",
                          n, size, to);
            goto failed;
        }

        size -= n;
    }

    if (nt_set_file_time(to, nfd, time) != NT_OK) {
        nt_log_error(NT_LOG_ALERT, cf->log, nt_errno,
                      nt_set_file_time_n " \"%s\" failed", to);
        goto failed;
    }

    rc = NT_OK;

failed:

    if (nfd != NT_INVALID_FILE) {
        if (nt_close_file(nfd) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_ALERT, cf->log, nt_errno,
                          nt_close_file_n " \"%s\" failed", to);
        }
    }

    if (fd != NT_INVALID_FILE) {
        if (nt_close_file(fd) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_ALERT, cf->log, nt_errno,
                          nt_close_file_n " \"%s\" failed", from);
        }
    }

    if (buf) {
        nt_free(buf);
    }

    return rc;
}


/*
 * ctx->init_handler() - see ctx->alloc
 * ctx->file_handler() - file handler
 * ctx->pre_tree_handler() - handler is called before entering directory
 * ctx->post_tree_handler() - handler is called after leaving directory
 * ctx->spec_handler() - special (socket, FIFO, etc.) file handler
 *
 * ctx->data - some data structure, it may be the same on all levels, or
 *     reallocated if ctx->alloc is nonzero
 *
 * ctx->alloc - a size of data structure that is allocated at every level
 *     and is initialized by ctx->init_handler()
 *
 * ctx->log - a log
 *
 * on fatal (memory) error handler must return NT_ABORT to stop walking tree
 */

nt_int_t
nt_walk_tree(nt_tree_ctx_t *ctx, nt_str_t *tree)
{
    void       *data, *prev;
    u_char     *p, *name;
    size_t      len;
    nt_int_t   rc;
    nt_err_t   err;
    nt_str_t   file, buf;
    nt_dir_t   dir;

    nt_str_null(&buf);

    nt_log_debug1(NT_LOG_DEBUG_CORE, ctx->log, 0,
                   "walk tree \"%V\"", tree);

    if (nt_open_dir(tree, &dir) == NT_ERROR) {
        nt_log_error(NT_LOG_CRIT, ctx->log, nt_errno,
                      nt_open_dir_n " \"%s\" failed", tree->data);
        return NT_ERROR;
    }

    prev = ctx->data;

    if (ctx->alloc) {
        data = nt_alloc(ctx->alloc, ctx->log);
        if (data == NULL) {
            goto failed;
        }

        if (ctx->init_handler(data, prev) == NT_ABORT) {
            goto failed;
        }

        ctx->data = data;

    } else {
        data = NULL;
    }

    for ( ;; ) {

        nt_set_errno(0);

        if (nt_read_dir(&dir) == NT_ERROR) {
            err = nt_errno;

            if (err == NT_ENOMOREFILES) {
                rc = NT_OK;

            } else {
                nt_log_error(NT_LOG_CRIT, ctx->log, err,
                              nt_read_dir_n " \"%s\" failed", tree->data);
                rc = NT_ERROR;
            }

            goto done;
        }

        len = nt_de_namelen(&dir);
        name = nt_de_name(&dir);

        nt_log_debug2(NT_LOG_DEBUG_CORE, ctx->log, 0,
                      "tree name %uz:\"%s\"", len, name);

        if (len == 1 && name[0] == '.') {
            continue;
        }

        if (len == 2 && name[0] == '.' && name[1] == '.') {
            continue;
        }

        file.len = tree->len + 1 + len;

        if (file.len > buf.len) {

            if (buf.len) {
                nt_free(buf.data);
            }

            buf.len = tree->len + 1 + len;

            buf.data = nt_alloc(buf.len + 1, ctx->log);
            if (buf.data == NULL) {
                goto failed;
            }
        }

        p = nt_cpymem(buf.data, tree->data, tree->len);
        *p++ = '/';
        nt_memcpy(p, name, len + 1);

        file.data = buf.data;

        nt_log_debug1(NT_LOG_DEBUG_CORE, ctx->log, 0,
                       "tree path \"%s\"", file.data);

        if (!dir.valid_info) {
            if (nt_de_info(file.data, &dir) == NT_FILE_ERROR) {
                nt_log_error(NT_LOG_CRIT, ctx->log, nt_errno,
                              nt_de_info_n " \"%s\" failed", file.data);
                continue;
            }
        }

        if (nt_de_is_file(&dir)) {

            nt_log_debug1(NT_LOG_DEBUG_CORE, ctx->log, 0,
                           "tree file \"%s\"", file.data);

            ctx->size = nt_de_size(&dir);
            ctx->fs_size = nt_de_fs_size(&dir);
            ctx->access = nt_de_access(&dir);
            ctx->mtime = nt_de_mtime(&dir);

            if (ctx->file_handler(ctx, &file) == NT_ABORT) {
                goto failed;
            }

        } else if (nt_de_is_dir(&dir)) {

            nt_log_debug1(NT_LOG_DEBUG_CORE, ctx->log, 0,
                           "tree enter dir \"%s\"", file.data);

            ctx->access = nt_de_access(&dir);
            ctx->mtime = nt_de_mtime(&dir);

            rc = ctx->pre_tree_handler(ctx, &file);

            if (rc == NT_ABORT) {
                goto failed;
            }

            if (rc == NT_DECLINED) {
                nt_log_debug1(NT_LOG_DEBUG_CORE, ctx->log, 0,
                               "tree skip dir \"%s\"", file.data);
                continue;
            }

            if (nt_walk_tree(ctx, &file) == NT_ABORT) {
                goto failed;
            }

            ctx->access = nt_de_access(&dir);
            ctx->mtime = nt_de_mtime(&dir);

            if (ctx->post_tree_handler(ctx, &file) == NT_ABORT) {
                goto failed;
            }

        } else {

            nt_log_debug1(NT_LOG_DEBUG_CORE, ctx->log, 0,
                           "tree special \"%s\"", file.data);

            if (ctx->spec_handler(ctx, &file) == NT_ABORT) {
                goto failed;
            }
        }
    }

failed:

    rc = NT_ABORT;

done:

    if (buf.len) {
        nt_free(buf.data);
    }

    if (data) {
        nt_free(data);
        ctx->data = prev;
    }

    if (nt_close_dir(&dir) == NT_ERROR) {
        nt_log_error(NT_LOG_CRIT, ctx->log, nt_errno,
                      nt_close_dir_n " \"%s\" failed", tree->data);
    }

    return rc;
}
