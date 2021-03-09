

#include <nt_core.h>

#define NT_CONF_BUFFER  4096

static nt_int_t nt_conf_add_dump(nt_conf_t *cf, nt_str_t *filename);
static nt_int_t nt_conf_handler(nt_conf_t *cf, nt_int_t last);
static nt_int_t nt_conf_read_token(nt_conf_t *cf);
static void nt_conf_flush_files(nt_cycle_t *cycle);


static nt_command_t  nt_conf_commands[] = {

    { nt_string("include"),
      NT_ANY_CONF|NT_CONF_TAKE1,
      nt_conf_include,
      0,
      0,
      NULL },

      nt_null_command
};


nt_module_t  nt_conf_module = {
    NT_MODULE_V1,
    NULL,                                  /* module context */
    nt_conf_commands,                     /* module directives */
    NT_CONF_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    nt_conf_flush_files,                  /* exit process */
    NULL,                                  /* exit master */
    NT_MODULE_V1_PADDING
};


/* The eight fixed arguments */

static nt_uint_t argument_number[] = {
    NT_CONF_NOARGS,
    NT_CONF_TAKE1,
    NT_CONF_TAKE2,
    NT_CONF_TAKE3,
    NT_CONF_TAKE4,
    NT_CONF_TAKE5,
    NT_CONF_TAKE6,
    NT_CONF_TAKE7
};


char *
nt_conf_param(nt_conf_t *cf)
{
    char             *rv;
    nt_str_t        *param;
    nt_buf_t         b;
    nt_conf_file_t   conf_file;

    param = &cf->cycle->conf_param;

    if (param->len == 0) {
        return NT_CONF_OK;
    }

    nt_memzero(&conf_file, sizeof(nt_conf_file_t));

    nt_memzero(&b, sizeof(nt_buf_t));

    b.start = param->data;
    b.pos = param->data;
    b.last = param->data + param->len;
    b.end = b.last;
    b.temporary = 1;

    conf_file.file.fd = NT_INVALID_FILE;
    conf_file.file.name.data = NULL;
    conf_file.line = 0;

    cf->conf_file = &conf_file;
    cf->conf_file->buffer = &b;

    rv = nt_conf_parse(cf, NULL);

    cf->conf_file = NULL;

    return rv;
}


static nt_int_t
nt_conf_add_dump(nt_conf_t *cf, nt_str_t *filename)
{
    off_t             size;
    u_char           *p;
    uint32_t          hash;
    nt_buf_t        *buf;
    nt_str_node_t   *sn;
    nt_conf_dump_t  *cd;

    hash = nt_crc32_long(filename->data, filename->len);

    sn = nt_str_rbtree_lookup(&cf->cycle->config_dump_rbtree, filename, hash);

    if (sn) {
        cf->conf_file->dump = NULL;
        return NT_OK;
    }

    p = nt_pstrdup(cf->cycle->pool, filename);
    if (p == NULL) {
        return NT_ERROR;
    }

    cd = nt_array_push(&cf->cycle->config_dump);
    if (cd == NULL) {
        return NT_ERROR;
    }

    size = nt_file_size(&cf->conf_file->file.info);

    buf = nt_create_temp_buf(cf->cycle->pool, (size_t) size);
    if (buf == NULL) {
        return NT_ERROR;
    }

    cd->name.data = p;
    cd->name.len = filename->len;
    cd->buffer = buf;

    cf->conf_file->dump = buf;

    sn = nt_palloc(cf->temp_pool, sizeof(nt_str_node_t));
    if (sn == NULL) {
        return NT_ERROR;
    }

    sn->node.key = hash;
    sn->str = cd->name;

    nt_rbtree_insert(&cf->cycle->config_dump_rbtree, &sn->node);

    return NT_OK;
}

/*
	解析配置内容
*/
char *
nt_conf_parse(nt_conf_t *cf, nt_str_t *filename)
{
    char             *rv;
    nt_fd_t          fd; //int
    nt_int_t         rc;
    nt_buf_t         buf;
    nt_conf_file_t  *prev, conf_file;
    char tmp_conf_path[32] ={0};
    enum {
        parse_file = 0,
        parse_block,
        parse_param
    } type;

#if (NT_SUPPRESS_WARN)
    fd = NT_INVALID_FILE;
    prev = NULL;
#endif

    if (filename) {

        /* open configuration file */
//	debug(2, "open configuration file %s", filename->data);
	//open file
        fd = nt_open_file(filename->data, NT_FILE_RDONLY, NT_FILE_OPEN, 0);

        if (fd == NT_INVALID_FILE) {
            nt_conf_log_error(NT_LOG_EMERG, cf, nt_errno,
                               nt_open_file_n " \"%s\" failed",
                               filename->data);
            return NT_CONF_ERROR;
        }

	//转换成解密后的fd
#if 0
	get_rand_string(tmp_conf_path, 22);

	debug(2, "tmp_conf_path=%s", tmp_conf_path);
	fd = conf_decrypt_fileno( fd , tmp_conf_path);

        if (fd == NT_INVALID_FILE) {
            nt_conf_log_error(NT_LOG_EMERG, cf, nt_errno,
                               nt_open_file_n " \"%s\" failed",
                               filename->data);
            return NT_CONF_ERROR;
        }
#endif

        prev = cf->conf_file;

        cf->conf_file = &conf_file;

	// fstat
        if (nt_fd_info(fd, &cf->conf_file->file.info) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_EMERG, cf->log, nt_errno,
                          nt_fd_info_n " \"%s\" failed", filename->data);
        }

        cf->conf_file->buffer = &buf;

        buf.start = nt_alloc(NT_CONF_BUFFER, cf->log);
        if (buf.start == NULL) {
            goto failed;
        }

        buf.pos = buf.start;
        buf.last = buf.start;
        buf.end = buf.last + NT_CONF_BUFFER;
        buf.temporary = 1;

        cf->conf_file->file.fd = fd;
        cf->conf_file->file.name.len = filename->len;
        cf->conf_file->file.name.data = filename->data;
        cf->conf_file->file.offset = 0;
        cf->conf_file->file.log = cf->log;
        cf->conf_file->line = 1;

        type = parse_file;

        if (nt_dump_config
#if (NT_DEBUG)
            || 1
#endif
           )
        {
            if (nt_conf_add_dump(cf, filename) != NT_OK) {
                goto failed;
            }

        } else {
            cf->conf_file->dump = NULL;
        }

    } else if (cf->conf_file->file.fd != NT_INVALID_FILE) {

        type = parse_block;

    } else {
        type = parse_param;
    }


    for ( ;; ) {
	//解析配置的文件内容
        rc = nt_conf_read_token(cf);

        /*
         * nt_conf_read_token() may return
         *
         *    NT_ERROR             there is error
         *    NT_OK                the token terminated by ";" was found
         *    NT_CONF_BLOCK_START  the token terminated by "{" was found
         *    NT_CONF_BLOCK_DONE   the "}" was found
         *    NT_CONF_FILE_DONE    the configuration file is done
         */

        if (rc == NT_ERROR) {
            goto done;
        }

        if (rc == NT_CONF_BLOCK_DONE) {

            if (type != parse_block) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0, "unexpected \"}\"");
                goto failed;
            }

            goto done;
        }

        if (rc == NT_CONF_FILE_DONE) {

            if (type == parse_block) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "unexpected end of file, expecting \"}\"");
                goto failed;
            }

            goto done;
        }

        if (rc == NT_CONF_BLOCK_START) {

            if (type == parse_param) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "block directives are not supported "
                                   "in -g option");
                goto failed;
            }
        }

        /* rc == NT_OK || rc == NT_CONF_BLOCK_START */

        if (cf->handler) {

            /*
             * the custom handler, i.e., that is used in the http's
             * "types { ... }" directive
             */

            if (rc == NT_CONF_BLOCK_START) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0, "unexpected \"{\"");
                goto failed;
            }

            rv = (*cf->handler)(cf, NULL, cf->handler_conf);
            if (rv == NT_CONF_OK) {
                continue;
            }

            if (rv == NT_CONF_ERROR) {
                goto failed;
            }

            nt_conf_log_error(NT_LOG_EMERG, cf, 0, "%s", rv);

            goto failed;
        }


        rc = nt_conf_handler(cf, rc);

        if (rc == NT_ERROR) {
            goto failed;
        }
    }

failed:

    rc = NT_ERROR;

done:

    if (filename) {
        if (cf->conf_file->buffer->start) {
            nt_free(cf->conf_file->buffer->start);
        }

        if (nt_close_file(fd) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_ALERT, cf->log, nt_errno,
                          nt_close_file_n " %s failed",
                          filename->data);
            rc = NT_ERROR;
        }
        unlink(tmp_conf_path);
        cf->conf_file = prev;
    }

    if (rc == NT_ERROR) {
        return NT_CONF_ERROR;
    }

    return NT_CONF_OK;
}


/*
 * 解析配置文件中的各模块中的cmd 回调
 * */
static nt_int_t
nt_conf_handler(nt_conf_t *cf, nt_int_t last)
{
    char           *rv;
    void           *conf, **confp;
    nt_uint_t      i, found;
    nt_str_t      *name;
    nt_command_t  *cmd;

    name = cf->args->elts;

    found = 0;

    for (i = 0; cf->cycle->modules[i]; i++) {

        cmd = cf->cycle->modules[i]->commands;
        
        if (cmd == NULL) {
            continue;
        }
        /* debug( "i=%d, module=%s, cmd = %s",i, cf->cycle->modules[i]->name, cmd->name.data ); */

        for ( /* void */ ; cmd->name.len; cmd++) {

            if (name->len != cmd->name.len) {
                continue;
            }

            if (nt_strcmp(name->data, cmd->name.data) != 0) {
                continue;
            }

            found = 1;

            if (cf->cycle->modules[i]->type != NT_CONF_MODULE
                && cf->cycle->modules[i]->type != cf->module_type)
            {
                continue;
            }

            /* is the directive's location right ? */

            if (!(cmd->type & cf->cmd_type)) {
                continue;
            }

            if (!(cmd->type & NT_CONF_BLOCK) && last != NT_OK) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                  "directive \"%s\" is not terminated by \";\"",
                                  name->data);
                return NT_ERROR;
            }

            if ((cmd->type & NT_CONF_BLOCK) && last != NT_CONF_BLOCK_START) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "directive \"%s\" has no opening \"{\"",
                                   name->data);
                return NT_ERROR;
            }

            /* is the directive's argument count right ? */

            if (!(cmd->type & NT_CONF_ANY)) {

                if (cmd->type & NT_CONF_FLAG) {

                    if (cf->args->nelts != 2) {
                        goto invalid;
                    }

                } else if (cmd->type & NT_CONF_1MORE) {

                    if (cf->args->nelts < 2) {
                        goto invalid;
                    }

                } else if (cmd->type & NT_CONF_2MORE) {

                    if (cf->args->nelts < 3) {
                        goto invalid;
                    }

                } else if (cf->args->nelts > NT_CONF_MAX_ARGS) {

                    goto invalid;

                } else if (!(cmd->type & argument_number[cf->args->nelts - 1]))
                {
                    goto invalid;
                }
            }

            /* set up the directive's configuration context */

            conf = NULL;

            if (cmd->type & NT_DIRECT_CONF) {
                conf = ((void **) cf->ctx)[cf->cycle->modules[i]->index];

            } else if (cmd->type & NT_MAIN_CONF) {
                conf = &(((void **) cf->ctx)[cf->cycle->modules[i]->index]);

            } else if (cf->ctx) {
                confp = *(void **) ((char *) cf->ctx + cmd->conf);

                if (confp) {
                    conf = confp[cf->cycle->modules[i]->ctx_index];
                }
            }

            //这里调用了 各模块中 commands 的参数回调函数
            /* debug( "------cmd = %s", cmd->name.data ); */
            rv = cmd->set(cf, cmd, conf);
            /* debug( "返回值cmd ret = %s", rv ); */

            if (rv == NT_CONF_OK) {
                return NT_OK;
            }

            if (rv == NT_CONF_ERROR) {
                return NT_ERROR;
            }

            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "\"%s\" directive %s", name->data, rv);

            return NT_ERROR;
        }
    }

    if (found) {
        nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                           "\"%s\" directive is not allowed here", name->data);

        return NT_ERROR;
    }

    nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                       "unknown directive \"%s\"", name->data);

    return NT_ERROR;

invalid:

    nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                       "invalid number of arguments in \"%s\" directive",
                       name->data);

    return NT_ERROR;
}


static nt_int_t
nt_conf_read_token(nt_conf_t *cf)
{
    u_char      *start, ch, *src, *dst;
    off_t        file_size;
    size_t       len;
    ssize_t      n, size;
    nt_uint_t   found, need_space, last_space, sharp_comment, variable;
    nt_uint_t   quoted, s_quoted, d_quoted, start_line;
    nt_str_t   *word;
    nt_buf_t   *b, *dump;

    found = 0;
    need_space = 0;
    last_space = 1;
    sharp_comment = 0;
    variable = 0;
    quoted = 0;
    s_quoted = 0;
    d_quoted = 0;

    cf->args->nelts = 0;
    b = cf->conf_file->buffer;
    dump = cf->conf_file->dump;
    start = b->pos;
    start_line = cf->conf_file->line;

    file_size = nt_file_size(&cf->conf_file->file.info);

    for ( ;; ) {

        if (b->pos >= b->last) {

            if (cf->conf_file->file.offset >= file_size) {

                if (cf->args->nelts > 0 || !last_space) {

                    if (cf->conf_file->file.fd == NT_INVALID_FILE) {
                        nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                           "unexpected end of parameter, "
                                           "expecting \";\"");
                        return NT_ERROR;
                    }

                    nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                  "unexpected end of file, "
                                  "expecting \";\" or \"}\"");
                    return NT_ERROR;
                }

                return NT_CONF_FILE_DONE;
            }

            len = b->pos - start;

            if (len == NT_CONF_BUFFER) {
                cf->conf_file->line = start_line;

                if (d_quoted) {
                    ch = '"';

                } else if (s_quoted) {
                    ch = '\'';

                } else {
                    nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                       "too long parameter \"%*s...\" started",
                                       10, start);
                    return NT_ERROR;
                }

                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "too long parameter, probably "
                                   "missing terminating \"%c\" character", ch);
                return NT_ERROR;
            }

            if (len) {
                nt_memmove(b->start, start, len);
            }

            size = (ssize_t) (file_size - cf->conf_file->file.offset);

            if (size > b->end - (b->start + len)) {
                size = b->end - (b->start + len);
            }

	    //read file data,
            n = nt_read_file(&cf->conf_file->file, b->start + len, size,
                              cf->conf_file->file.offset);

            if (n == NT_ERROR) {
                return NT_ERROR;
            }

            if (n != size) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   nt_read_file_n " returned "
                                   "only %z bytes instead of %z",
                                   n, size);
                return NT_ERROR;
            }

            b->pos = b->start + len;
            b->last = b->pos + n;
            start = b->start;

            if (dump) {
                dump->last = nt_cpymem(dump->last, b->pos, size);
            }
        }

        ch = *b->pos++;

        if (ch == LF) {
            cf->conf_file->line++;

            if (sharp_comment) {
                sharp_comment = 0;
            }
        }

        if (sharp_comment) {
            continue;
        }

        if (quoted) {
            quoted = 0;
            continue;
        }

        if (need_space) {
            if (ch == ' ' || ch == '\t' || ch == CR || ch == LF) {
                last_space = 1;
                need_space = 0;
                continue;
            }

            if (ch == ';') {
                return NT_OK;
            }

            if (ch == '{') {
                return NT_CONF_BLOCK_START;
            }

            if (ch == ')') {
                last_space = 1;
                need_space = 0;

            } else {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "unexpected \"%c\"", ch);
                return NT_ERROR;
            }
        }

        if (last_space) {

            start = b->pos - 1;
            start_line = cf->conf_file->line;

            if (ch == ' ' || ch == '\t' || ch == CR || ch == LF) {
                continue;
            }

            switch (ch) {

            case ';':
            case '{':
                if (cf->args->nelts == 0) {
                    nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                       "unexpected \"%c\"", ch);
                    return NT_ERROR;
                }

                if (ch == '{') {
                    return NT_CONF_BLOCK_START;
                }

                return NT_OK;

            case '}':
                if (cf->args->nelts != 0) {
                    nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                       "unexpected \"}\"");
                    return NT_ERROR;
                }

                return NT_CONF_BLOCK_DONE;

            case '#':
                sharp_comment = 1;
                continue;

            case '\\':
                quoted = 1;
                last_space = 0;
                continue;

            case '"':
                start++;
                d_quoted = 1;
                last_space = 0;
                continue;

            case '\'':
                start++;
                s_quoted = 1;
                last_space = 0;
                continue;

            case '$':
                variable = 1;
                last_space = 0;
                continue;

            default:
                last_space = 0;
            }

        } else {
            if (ch == '{' && variable) {
                continue;
            }

            variable = 0;

            if (ch == '\\') {
                quoted = 1;
                continue;
            }

            if (ch == '$') {
                variable = 1;
                continue;
            }

            if (d_quoted) {
                if (ch == '"') {
                    d_quoted = 0;
                    need_space = 1;
                    found = 1;
                }

            } else if (s_quoted) {
                if (ch == '\'') {
                    s_quoted = 0;
                    need_space = 1;
                    found = 1;
                }

            } else if (ch == ' ' || ch == '\t' || ch == CR || ch == LF
                       || ch == ';' || ch == '{')
            {
                last_space = 1;
                found = 1;
            }

            if (found) {
                word = nt_array_push(cf->args);
                if (word == NULL) {
                    return NT_ERROR;
                }

                word->data = nt_pnalloc(cf->pool, b->pos - 1 - start + 1);
                if (word->data == NULL) {
                    return NT_ERROR;
                }

                for (dst = word->data, src = start, len = 0;
                     src < b->pos - 1;
                     len++)
                {
                    if (*src == '\\') {
                        switch (src[1]) {
                        case '"':
                        case '\'':
                        case '\\':
                            src++;
                            break;

                        case 't':
                            *dst++ = '\t';
                            src += 2;
                            continue;

                        case 'r':
                            *dst++ = '\r';
                            src += 2;
                            continue;

                        case 'n':
                            *dst++ = '\n';
                            src += 2;
                            continue;
                        }

                    }
                    *dst++ = *src++;
                }
                *dst = '\0';
                word->len = len;

                if (ch == ';') {
                    return NT_OK;
                }

                if (ch == '{') {
                    return NT_CONF_BLOCK_START;
                }

                found = 0;
            }
        }
    }
}


char *
nt_conf_include(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char        *rv;
    nt_int_t    n;
    nt_str_t   *value, file, name;
    nt_glob_t   gl;

    value = cf->args->elts;
    file = value[1];

    nt_log_debug1(NT_LOG_DEBUG_CORE, cf->log, 0, "include %s", file.data);

    if (nt_conf_full_name(cf->cycle, &file, 1) != NT_OK) {
        return NT_CONF_ERROR;
    }

    if (strpbrk((char *) file.data, "*?[") == NULL) {

        nt_log_debug1(NT_LOG_DEBUG_CORE, cf->log, 0, "include %s", file.data);

        return nt_conf_parse(cf, &file);
    }

    nt_memzero(&gl, sizeof(nt_glob_t));

    gl.pattern = file.data;
    gl.log = cf->log;
    gl.test = 1;

    if (nt_open_glob(&gl) != NT_OK) {
        nt_conf_log_error(NT_LOG_EMERG, cf, nt_errno,
                           nt_open_glob_n " \"%s\" failed", file.data);
        return NT_CONF_ERROR;
    }

    rv = NT_CONF_OK;

    for ( ;; ) {
        n = nt_read_glob(&gl, &name);

        if (n != NT_OK) {
            break;
        }

        file.len = name.len++;
        file.data = nt_pstrdup(cf->pool, &name);
        if (file.data == NULL) {
            return NT_CONF_ERROR;
        }

        nt_log_debug1(NT_LOG_DEBUG_CORE, cf->log, 0, "include %s", file.data);

        rv = nt_conf_parse(cf, &file);

        if (rv != NT_CONF_OK) {
            break;
        }
    }

    nt_close_glob(&gl);

    return rv;
}


nt_int_t
nt_conf_full_name(nt_cycle_t *cycle, nt_str_t *name, nt_uint_t conf_prefix)
{
    nt_str_t  *prefix;

    prefix = conf_prefix ? &cycle->conf_prefix : &cycle->prefix;

    return nt_get_full_name(cycle->pool, prefix, name);
}


nt_open_file_t *
nt_conf_open_file(nt_cycle_t *cycle, nt_str_t *name)
{
    nt_str_t         full;
    nt_uint_t        i;
    nt_list_part_t  *part;
    nt_open_file_t  *file;

#if (NT_SUPPRESS_WARN)
    nt_str_null(&full);
#endif

    if (name->len) {
        full = *name;

        if (nt_conf_full_name(cycle, &full, 0) != NT_OK) {
            return NULL;
        }

        part = &cycle->open_files.part;
        file = part->elts;

        for (i = 0; /* void */ ; i++) {

            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }
                part = part->next;
                file = part->elts;
                i = 0;
            }

            if (full.len != file[i].name.len) {
                continue;
            }

            if (nt_strcmp(full.data, file[i].name.data) == 0) {
                return &file[i];
            }
        }
    }

    file = nt_list_push(&cycle->open_files);
    if (file == NULL) {
        return NULL;
    }

    if (name->len) {
        file->fd = NT_INVALID_FILE;
        file->name = full;

    } else {
        file->fd = nt_stderr;
        file->name = *name;
    }

    file->flush = NULL;
    file->data = NULL;

    return file;
}


static void
nt_conf_flush_files(nt_cycle_t *cycle)
{
    nt_uint_t        i;
    nt_list_part_t  *part;
    nt_open_file_t  *file;

    nt_log_debug0(NT_LOG_DEBUG_CORE, cycle->log, 0, "flush files");

    part = &cycle->open_files.part;
    file = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            file = part->elts;
            i = 0;
        }

        if (file[i].flush) {
            file[i].flush(&file[i], cycle->log);
        }
    }
}


void nt_cdecl
nt_conf_log_error(nt_uint_t level, nt_conf_t *cf, nt_err_t err,
    const char *fmt, ...)
{
    u_char   errstr[NT_MAX_CONF_ERRSTR], *p, *last;
    va_list  args;

    last = errstr + NT_MAX_CONF_ERRSTR;

    va_start(args, fmt);
    p = nt_vslprintf(errstr, last, fmt, args);
    va_end(args);

    if (err) {
        p = nt_log_errno(p, last, err);
    }

    if (cf->conf_file == NULL) {
        nt_log_error(level, cf->log, 0, "%*s", p - errstr, errstr);
        return;
    }

    if (cf->conf_file->file.fd == NT_INVALID_FILE) {
        nt_log_error(level, cf->log, 0, "%*s in command line",
                      p - errstr, errstr);
        return;
    }

    nt_log_error(level, cf->log, 0, "%*s in %s:%ui",
                  p - errstr, errstr,
                  cf->conf_file->file.name.data, cf->conf_file->line);
}


char *
nt_conf_set_flag_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char  *p = conf;

    nt_str_t        *value;
    nt_flag_t       *fp;
    nt_conf_post_t  *post;

    fp = (nt_flag_t *) (p + cmd->offset);

    if (*fp != NT_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (nt_strcasecmp(value[1].data, (u_char *) "on") == 0) {
        *fp = 1;

    } else if (nt_strcasecmp(value[1].data, (u_char *) "off") == 0) {
        *fp = 0;

    } else {
        nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                     "invalid value \"%s\" in \"%s\" directive, "
                     "it must be \"on\" or \"off\"",
                     value[1].data, cmd->name.data);
        return NT_CONF_ERROR;
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, fp);
    }

    return NT_CONF_OK;
}


char *
nt_conf_set_str_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char  *p = conf;

    nt_str_t        *field, *value;
    nt_conf_post_t  *post;

    field = (nt_str_t *) (p + cmd->offset);

    if (field->data) {
        return "is duplicate";
    }

    value = cf->args->elts;

    *field = value[1];

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, field);
    }

    return NT_CONF_OK;
}


char *
nt_conf_set_str_array_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char  *p = conf;

    nt_str_t         *value, *s;
    nt_array_t      **a;
    nt_conf_post_t   *post;

    a = (nt_array_t **) (p + cmd->offset);

    if (*a == NT_CONF_UNSET_PTR) {
        *a = nt_array_create(cf->pool, 4, sizeof(nt_str_t));
        if (*a == NULL) {
            return NT_CONF_ERROR;
        }
    }

    s = nt_array_push(*a);
    if (s == NULL) {
        return NT_CONF_ERROR;
    }

    value = cf->args->elts;

    *s = value[1];

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, s);
    }

    return NT_CONF_OK;
}


char *
nt_conf_set_keyval_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char  *p = conf;

    nt_str_t         *value;
    nt_array_t      **a;
    nt_keyval_t      *kv;
    nt_conf_post_t   *post;

    a = (nt_array_t **) (p + cmd->offset);

    if (*a == NULL) {
        *a = nt_array_create(cf->pool, 4, sizeof(nt_keyval_t));
        if (*a == NULL) {
            return NT_CONF_ERROR;
        }
    }

    kv = nt_array_push(*a);
    if (kv == NULL) {
        return NT_CONF_ERROR;
    }

    value = cf->args->elts;

    kv->key = value[1];
    kv->value = value[2];

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, kv);
    }

    return NT_CONF_OK;
}


char *
nt_conf_set_num_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char  *p = conf;

    nt_int_t        *np;
    nt_str_t        *value;
    nt_conf_post_t  *post;


    np = (nt_int_t *) (p + cmd->offset);

    if (*np != NT_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;
    *np = nt_atoi(value[1].data, value[1].len);
    if (*np == NT_ERROR) {
        return "invalid number";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, np);
    }

    return NT_CONF_OK;
}


char *
nt_conf_set_size_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char  *p = conf;

    size_t           *sp;
    nt_str_t        *value;
    nt_conf_post_t  *post;


    sp = (size_t *) (p + cmd->offset);
    if (*sp != NT_CONF_UNSET_SIZE) {
        return "is duplicate";
    }

    value = cf->args->elts;

    *sp = nt_parse_size(&value[1]);
    if (*sp == (size_t) NT_ERROR) {
        return "invalid value";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, sp);
    }

    return NT_CONF_OK;
}


char *
nt_conf_set_off_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char  *p = conf;

    off_t            *op;
    nt_str_t        *value;
    nt_conf_post_t  *post;


    op = (off_t *) (p + cmd->offset);
    if (*op != NT_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    *op = nt_parse_offset(&value[1]);
    if (*op == (off_t) NT_ERROR) {
        return "invalid value";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, op);
    }

    return NT_CONF_OK;
}


char *
nt_conf_set_msec_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char  *p = conf;

    nt_msec_t       *msp;
    nt_str_t        *value;
    nt_conf_post_t  *post;


    msp = (nt_msec_t *) (p + cmd->offset);
    if (*msp != NT_CONF_UNSET_MSEC) {
        return "is duplicate";
    }

    value = cf->args->elts;

    *msp = nt_parse_time(&value[1], 0);
    if (*msp == (nt_msec_t) NT_ERROR) {
        return "invalid value";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, msp);
    }

    return NT_CONF_OK;
}


char *
nt_conf_set_sec_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char  *p = conf;

    time_t           *sp;
    nt_str_t        *value;
    nt_conf_post_t  *post;


    sp = (time_t *) (p + cmd->offset);
    if (*sp != NT_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    *sp = nt_parse_time(&value[1], 1);
    if (*sp == (time_t) NT_ERROR) {
        return "invalid value";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, sp);
    }

    return NT_CONF_OK;
}


char *
nt_conf_set_bufs_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char *p = conf;

    nt_str_t   *value;
    nt_bufs_t  *bufs;


    bufs = (nt_bufs_t *) (p + cmd->offset);
    if (bufs->num) {
        return "is duplicate";
    }

    value = cf->args->elts;

    bufs->num = nt_atoi(value[1].data, value[1].len);
    if (bufs->num == NT_ERROR || bufs->num == 0) {
        return "invalid value";
    }

    bufs->size = nt_parse_size(&value[2]);
    if (bufs->size == (size_t) NT_ERROR || bufs->size == 0) {
        return "invalid value";
    }

    return NT_CONF_OK;
}


char *
nt_conf_set_enum_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char  *p = conf;

    nt_uint_t       *np, i;
    nt_str_t        *value;
    nt_conf_enum_t  *e;

    np = (nt_uint_t *) (p + cmd->offset);

    if (*np != NT_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    value = cf->args->elts;
    e = cmd->post;

    for (i = 0; e[i].name.len != 0; i++) {
        if (e[i].name.len != value[1].len
            || nt_strcasecmp(e[i].name.data, value[1].data) != 0)
        {
            continue;
        }

        *np = e[i].value;

        return NT_CONF_OK;
    }

    nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                       "invalid value \"%s\"", value[1].data);

    return NT_CONF_ERROR;
}


char *
nt_conf_set_bitmask_slot(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char  *p = conf;

    nt_uint_t          *np, i, m;
    nt_str_t           *value;
    nt_conf_bitmask_t  *mask;


    np = (nt_uint_t *) (p + cmd->offset);
    value = cf->args->elts;
    mask = cmd->post;

    for (i = 1; i < cf->args->nelts; i++) {
        for (m = 0; mask[m].name.len != 0; m++) {

            if (mask[m].name.len != value[i].len
                || nt_strcasecmp(mask[m].name.data, value[i].data) != 0)
            {
                continue;
            }

            if (*np & mask[m].mask) {
                nt_conf_log_error(NT_LOG_WARN, cf, 0,
                                   "duplicate value \"%s\"", value[i].data);

            } else {
                *np |= mask[m].mask;
            }

            break;
        }

        if (mask[m].name.len == 0) {
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "invalid value \"%s\"", value[i].data);

            return NT_CONF_ERROR;
        }
    }

    return NT_CONF_OK;
}


#if 0

char *
nt_conf_unsupported(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    return "unsupported on this platform";
}

#endif


char *
nt_conf_deprecated(nt_conf_t *cf, void *post, void *data)
{
    nt_conf_deprecated_t  *d = post;

    nt_conf_log_error(NT_LOG_WARN, cf, 0,
                       "the \"%s\" directive is deprecated, "
                       "use the \"%s\" directive instead",
                       d->old_name, d->new_name);

    return NT_CONF_OK;
}


char *
nt_conf_check_num_bounds(nt_conf_t *cf, void *post, void *data)
{
    nt_conf_num_bounds_t  *bounds = post;
    nt_int_t  *np = data;

    if (bounds->high == -1) {
        if (*np >= bounds->low) {
            return NT_CONF_OK;
        }

        nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                           "value must be equal to or greater than %i",
                           bounds->low);

        return NT_CONF_ERROR;
    }

    if (*np >= bounds->low && *np <= bounds->high) {
        return NT_CONF_OK;
    }

    nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                       "value must be between %i and %i",
                       bounds->low, bounds->high);

    return NT_CONF_ERROR;
}
