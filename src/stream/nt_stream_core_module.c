
#include <nt_core.h>
#include <nt_stream.h>


static nt_int_t nt_stream_core_preconfiguration(nt_conf_t *cf);
static void *nt_stream_core_create_main_conf(nt_conf_t *cf);
static char *nt_stream_core_init_main_conf(nt_conf_t *cf, void *conf);
static void *nt_stream_core_create_srv_conf(nt_conf_t *cf);
static char *nt_stream_core_merge_srv_conf(nt_conf_t *cf, void *parent,
    void *child);
static char *nt_stream_core_error_log(nt_conf_t *cf, nt_command_t *cmd,
    void *conf);
static char *nt_stream_core_server(nt_conf_t *cf, nt_command_t *cmd,
    void *conf);
static char *nt_stream_core_listen(nt_conf_t *cf, nt_command_t *cmd,
    void *conf);
static char *nt_stream_core_resolver(nt_conf_t *cf, nt_command_t *cmd,
    void *conf);

#if (NT_STREAM_SNI)
static char *nt_stream_core_server_name(nt_conf_t *cf, nt_command_t *cmd,
    void *conf);
#endif

static nt_command_t  nt_stream_core_commands[] = {

    { nt_string("variables_hash_max_size"),
      NT_STREAM_MAIN_CONF|NT_CONF_TAKE1,
      nt_conf_set_num_slot,
      NT_STREAM_MAIN_CONF_OFFSET,
      offsetof(nt_stream_core_main_conf_t, variables_hash_max_size),
      NULL },

    { nt_string("variables_hash_bucket_size"),
      NT_STREAM_MAIN_CONF|NT_CONF_TAKE1,
      nt_conf_set_num_slot,
      NT_STREAM_MAIN_CONF_OFFSET,
      offsetof(nt_stream_core_main_conf_t, variables_hash_bucket_size),
      NULL },

    { nt_string("server"),
      NT_STREAM_MAIN_CONF|NT_CONF_BLOCK|NT_CONF_NOARGS,
      nt_stream_core_server,
      0,
      0,
      NULL },

    { nt_string("listen"),
      NT_STREAM_SRV_CONF|NT_CONF_1MORE,
      nt_stream_core_listen,
      NT_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { nt_string("error_log"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_1MORE,
      nt_stream_core_error_log,
      NT_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { nt_string("resolver"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_1MORE,
      nt_stream_core_resolver,
      NT_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { nt_string("resolver_timeout"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_msec_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_core_srv_conf_t, resolver_timeout),
      NULL },

    { nt_string("proxy_protocol_timeout"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_msec_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_core_srv_conf_t, proxy_protocol_timeout),
      NULL },

    { nt_string("tcp_nodelay"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_FLAG,
      nt_conf_set_flag_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_core_srv_conf_t, tcp_nodelay),
      NULL },

    { nt_string("preread_buffer_size"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_size_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_core_srv_conf_t, preread_buffer_size),
      NULL },

    { nt_string("preread_timeout"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_msec_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_core_srv_conf_t, preread_timeout),
      NULL },

#if (NT_STREAM_SNI)
    { nt_string("server_name"),
      NT_STREAM_SRV_CONF|NT_CONF_1MORE,
      nt_stream_core_server_name,
      NT_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { nt_string("server_names_hash_max_size"),
      NT_STREAM_MAIN_CONF|NT_CONF_TAKE1,
      nt_conf_set_num_slot,
      NT_STREAM_MAIN_CONF_OFFSET,
      offsetof(nt_stream_core_main_conf_t, server_names_hash_max_size),
      NULL },

    { nt_string("server_names_hash_bucket_size"),
      NT_STREAM_MAIN_CONF|NT_CONF_TAKE1,
      nt_conf_set_num_slot,
      NT_STREAM_MAIN_CONF_OFFSET,
      offsetof(nt_stream_core_main_conf_t, server_names_hash_bucket_size),
      NULL },

#endif
      nt_null_command
};


static nt_stream_module_t  nt_stream_core_module_ctx = {
    nt_stream_core_preconfiguration,      /* preconfiguration */
    NULL,                                  /* postconfiguration */

    nt_stream_core_create_main_conf,      /* create main configuration */
    nt_stream_core_init_main_conf,        /* init main configuration */

    nt_stream_core_create_srv_conf,       /* create server configuration */
    nt_stream_core_merge_srv_conf         /* merge server configuration */
};


nt_module_t  nt_stream_core_module = {
    NT_MODULE_V1,
    &nt_stream_core_module_ctx,           /* module context */
    nt_stream_core_commands,              /* module directives */
    NT_STREAM_MODULE,                     /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NT_MODULE_V1_PADDING
};


void
nt_stream_core_run_phases(nt_stream_session_t *s)
{
    debug( "start" );
    nt_int_t                     rc;
    nt_stream_phase_handler_t   *ph;
    nt_stream_core_main_conf_t  *cmcf;

    cmcf = nt_stream_get_module_main_conf(s, nt_stream_core_module);

    ph = cmcf->phase_engine.handlers;

    while (ph[s->phase_handler].checker) {

        rc = ph[s->phase_handler].checker(s, &ph[s->phase_handler]);

        if (rc == NT_OK) {
            return;
        }
    }
}


nt_int_t
nt_stream_core_generic_phase(nt_stream_session_t *s,
    nt_stream_phase_handler_t *ph)
{
    nt_int_t  rc;

    /*
     * generic phase checker,
     * used by all phases, except for preread and content
     */

    nt_log_debug1(NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                   "generic phase: %ui", s->phase_handler);

    rc = ph->handler(s);

    if (rc == NT_OK) {
        s->phase_handler = ph->next;
        return NT_AGAIN;
    }

    if (rc == NT_DECLINED) {
        s->phase_handler++;
        return NT_AGAIN;
    }

    if (rc == NT_AGAIN || rc == NT_DONE) {
        return NT_OK;
    }

    if (rc == NT_ERROR) {
        rc = NT_STREAM_INTERNAL_SERVER_ERROR;
    }

    nt_stream_finalize_session(s, rc);

    return NT_OK;
}


nt_int_t
nt_stream_core_preread_phase(nt_stream_session_t *s,
    nt_stream_phase_handler_t *ph)
{
    size_t                       size;
    ssize_t                      n;
    nt_int_t                    rc;
    nt_connection_t            *c;
    nt_stream_core_srv_conf_t  *cscf;

    c = s->connection;

    c->log->action = "prereading client data";

    cscf = nt_stream_get_module_srv_conf(s, nt_stream_core_module);

    if (c->read->timedout) {
        rc = NT_STREAM_OK;

    } else if (c->read->timer_set) {
        rc = NT_AGAIN;

    } else {
        rc = ph->handler(s);
    }

    while (rc == NT_AGAIN) {

        if (c->buffer == NULL) {
            c->buffer = nt_create_temp_buf(c->pool, cscf->preread_buffer_size);
            if (c->buffer == NULL) {
                rc = NT_ERROR;
                break;
            }
        }

        size = c->buffer->end - c->buffer->last;

        if (size == 0) {
            nt_log_error(NT_LOG_ERR, c->log, 0, "preread buffer full");
            rc = NT_STREAM_BAD_REQUEST;
            break;
        }

        if (c->read->eof) {
            rc = NT_STREAM_OK;
            break;
        }

        if (!c->read->ready) {
            break;
        }

        n = c->recv(c, c->buffer->last, size);

        if (n == NT_ERROR || n == 0) {
            rc = NT_STREAM_OK;
            break;
        }

        if (n == NT_AGAIN) {
            break;
        }

        c->buffer->last += n;

        rc = ph->handler(s);
    }

    if (rc == NT_AGAIN) {
        if (nt_handle_read_event(c->read, 0) != NT_OK) {
            nt_stream_finalize_session(s, NT_STREAM_INTERNAL_SERVER_ERROR);
            return NT_OK;
        }

        if (!c->read->timer_set) {
            nt_add_timer(c->read, cscf->preread_timeout);
        }

        c->read->handler = nt_stream_session_handler;

        return NT_OK;
    }

    if (c->read->timer_set) {
        nt_del_timer(c->read);
    }

    if (rc == NT_OK) {
        s->phase_handler = ph->next;
        return NT_AGAIN;
    }

    if (rc == NT_DECLINED) {
        s->phase_handler++;
        return NT_AGAIN;
    }

    if (rc == NT_DONE) {
        return NT_OK;
    }

    if (rc == NT_ERROR) {
        rc = NT_STREAM_INTERNAL_SERVER_ERROR;
    }

    nt_stream_finalize_session(s, rc);

    return NT_OK;
}


nt_int_t
nt_stream_core_content_phase(nt_stream_session_t *s,
    nt_stream_phase_handler_t *ph)
{
    nt_connection_t            *c;
    nt_stream_core_srv_conf_t  *cscf;

    c = s->connection;

    c->log->action = NULL;

    cscf = nt_stream_get_module_srv_conf(s, nt_stream_core_module);

    if (c->type == SOCK_STREAM
        && cscf->tcp_nodelay
        && nt_tcp_nodelay(c) != NT_OK)
    {
        nt_stream_finalize_session(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return NT_OK;
    }

    cscf->handler(s);

    return NT_OK;
}


static nt_int_t
nt_stream_core_preconfiguration(nt_conf_t *cf)
{
    return nt_stream_variables_add_core_vars(cf);
}


static void *
nt_stream_core_create_main_conf(nt_conf_t *cf)
{
    nt_stream_core_main_conf_t  *cmcf;

    cmcf = nt_pcalloc(cf->pool, sizeof(nt_stream_core_main_conf_t));
    if (cmcf == NULL) {
        return NULL;
    }

    if (nt_array_init(&cmcf->servers, cf->pool, 4,
                       sizeof(nt_stream_core_srv_conf_t *))
        != NT_OK)
    {
        return NULL;
    }

    if (nt_array_init(&cmcf->listen, cf->pool, 4, sizeof(nt_stream_listen_t))
        != NT_OK)
    {
        return NULL;
    }

    cmcf->variables_hash_max_size = NT_CONF_UNSET_UINT;
    cmcf->variables_hash_bucket_size = NT_CONF_UNSET_UINT;

#if (NT_STREAM_SNI)
    cmcf->server_names_hash_max_size = NT_CONF_UNSET_UINT;
    cmcf->server_names_hash_bucket_size = NT_CONF_UNSET_UINT;
#endif

    return cmcf;
}


static char *
nt_stream_core_init_main_conf(nt_conf_t *cf, void *conf)
{
    nt_stream_core_main_conf_t *cmcf = conf;

    nt_conf_init_uint_value(cmcf->variables_hash_max_size, 1024);
    nt_conf_init_uint_value(cmcf->variables_hash_bucket_size, 64);

#if (NT_STREAM_SNI)
    nt_conf_init_uint_value(cmcf->server_names_hash_max_size, 512);
    nt_conf_init_uint_value(cmcf->server_names_hash_bucket_size,
                             nt_cacheline_size);
#endif

    cmcf->variables_hash_bucket_size =
               nt_align(cmcf->variables_hash_bucket_size, nt_cacheline_size);

    if (cmcf->ncaptures) {
        cmcf->ncaptures = (cmcf->ncaptures + 1) * 3;
    }

    return NT_CONF_OK;
}


static void *
nt_stream_core_create_srv_conf(nt_conf_t *cf)
{
    nt_stream_core_srv_conf_t  *cscf;

    cscf = nt_pcalloc(cf->pool, sizeof(nt_stream_core_srv_conf_t));
    if (cscf == NULL) {
        return NULL;
    }

    /*
     * set by nt_pcalloc():
     *
     *     cscf->handler = NULL;
     *     cscf->error_log = NULL;
     */

    cscf->file_name = cf->conf_file->file.name.data;
    cscf->line = cf->conf_file->line;
    cscf->resolver_timeout = NT_CONF_UNSET_MSEC;
    cscf->proxy_protocol_timeout = NT_CONF_UNSET_MSEC;
    cscf->tcp_nodelay = NT_CONF_UNSET;
    cscf->preread_buffer_size = NT_CONF_UNSET_SIZE;
    cscf->preread_timeout = NT_CONF_UNSET_MSEC;

#if (NT_STREAM_SNI)
    if (nt_array_init(&cscf->server_names, cf->temp_pool, 4,
                       sizeof(nt_stream_server_name_t))
        != NT_OK)
    {
        return NULL;
    }
#endif

    return cscf;
}


static char *
nt_stream_core_merge_srv_conf(nt_conf_t *cf, void *parent, void *child)
{
    nt_stream_core_srv_conf_t *prev = parent;
    nt_stream_core_srv_conf_t *conf = child;
#if (NT_STREAM_SNI)
    nt_str_t                   name;
    nt_stream_server_name_t   *sn;
#endif


    nt_conf_merge_msec_value(conf->resolver_timeout,
                              prev->resolver_timeout, 30000);

    if (conf->resolver == NULL) {

        if (prev->resolver == NULL) {

            /*
             * create dummy resolver in stream {} context
             * to inherit it in all servers
             */

            prev->resolver = nt_resolver_create(cf, NULL, 0);
            if (prev->resolver == NULL) {
                return NT_CONF_ERROR;
            }
        }

        conf->resolver = prev->resolver;
    }

    if (conf->handler == NULL) {
        nt_log_error(NT_LOG_EMERG, cf->log, 0,
                      "no handler for server in %s:%ui",
                      conf->file_name, conf->line);
        return NT_CONF_ERROR;
    }

    if (conf->error_log == NULL) {
        if (prev->error_log) {
            conf->error_log = prev->error_log;
        } else {
            conf->error_log = &cf->cycle->new_log;
        }
    }

    nt_conf_merge_msec_value(conf->proxy_protocol_timeout,
                              prev->proxy_protocol_timeout, 30000);

    nt_conf_merge_value(conf->tcp_nodelay, prev->tcp_nodelay, 1);

    nt_conf_merge_size_value(conf->preread_buffer_size,
                              prev->preread_buffer_size, 16384);

    nt_conf_merge_msec_value(conf->preread_timeout,
                              prev->preread_timeout, 30000);

#if (NT_STREAM_SNI)
    if (conf->server_names.nelts == 0) {
        /* the array has 4 empty preallocated elements, so push cannot fail */
        sn = nt_array_push(&conf->server_names);
        sn->server = conf;
        nt_str_set(&sn->name, "");
    }

    sn = conf->server_names.elts;
    name = sn[0].name;

    if (name.data[0] == '.') {
        name.len--;
        name.data++;
    }

    conf->server_name.len = name.len;
    conf->server_name.data = nt_pstrdup(cf->pool, &name);
    if (conf->server_name.data == NULL) {
        return NT_CONF_ERROR;
    }
#endif

    return NT_CONF_OK;
}


static char *
nt_stream_core_error_log(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    nt_stream_core_srv_conf_t  *cscf = conf;

    return nt_log_set_log(cf, &cscf->error_log);
}


static char *
nt_stream_core_server(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char                         *rv;
    void                         *mconf;
    nt_uint_t                    m;
    nt_conf_t                    pcf;
    nt_stream_module_t          *module;
    nt_stream_conf_ctx_t        *ctx, *stream_ctx;
    nt_stream_core_srv_conf_t   *cscf, **cscfp;
    nt_stream_core_main_conf_t  *cmcf;

    ctx = nt_pcalloc(cf->pool, sizeof(nt_stream_conf_ctx_t));
    if (ctx == NULL) {
        return NT_CONF_ERROR;
    }

    stream_ctx = cf->ctx;
    ctx->main_conf = stream_ctx->main_conf;

    /* the server{}'s srv_conf */

    ctx->srv_conf = nt_pcalloc(cf->pool,
                                sizeof(void *) * nt_stream_max_module);
    if (ctx->srv_conf == NULL) {
        return NT_CONF_ERROR;
    }

    for (m = 0; cf->cycle->modules[m]; m++) {
        if (cf->cycle->modules[m]->type != NT_STREAM_MODULE) {
            continue;
        }

        module = cf->cycle->modules[m]->ctx;

        if (module->create_srv_conf) {
            mconf = module->create_srv_conf(cf);
            if (mconf == NULL) {
                return NT_CONF_ERROR;
            }

            ctx->srv_conf[cf->cycle->modules[m]->ctx_index] = mconf;
        }
    }

    /* the server configuration context */

    cscf = ctx->srv_conf[nt_stream_core_module.ctx_index];
    cscf->ctx = ctx;

    cmcf = ctx->main_conf[nt_stream_core_module.ctx_index];

    cscfp = nt_array_push(&cmcf->servers);
    if (cscfp == NULL) {
        return NT_CONF_ERROR;
    }

    *cscfp = cscf;


    /* parse inside server{} */

    pcf = *cf;
    cf->ctx = ctx;
    cf->cmd_type = NT_STREAM_SRV_CONF;

    rv = nt_conf_parse(cf, NULL);

    *cf = pcf;

    if (rv == NT_CONF_OK && !cscf->listen) {
        nt_log_error(NT_LOG_EMERG, cf->log, 0,
                      "no \"listen\" is defined for server in %s:%ui",
                      cscf->file_name, cscf->line);
        return NT_CONF_ERROR;
    }

    return rv;
}

/* 
 *   ls->type 为 当前socket 监听类型
 *   NT_SOCK_TUN = 0, 自定义类型
 *   SOCK_STREAM = 1,	
 *   SOCK_DGRAM = 2,		
 *   SOCK_RAW = 3,			
 *   SOCK_RDM = 4,			
 *   SOCK_SEQPACKET = 5,	
 *   SOCK_DCCP = 6,		
 *   SOCK_PACKET = 10,		
 *   解析stream模块中 listen 后面配置，比如so_keepalive=on
 *     */
static char *
nt_stream_core_listen(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    nt_stream_core_srv_conf_t  *cscf = conf;

    nt_str_t                    *value, size;
    nt_url_t                     u;
    nt_uint_t                    i, n, backlog;
    nt_stream_listen_t          *ls, *als;
    nt_stream_core_main_conf_t  *cmcf;

    cscf->listen = 1;

    value = cf->args->elts;

    nt_memzero(&u, sizeof(nt_url_t));

    u.url = value[1];
    u.listen = 1;

    //解析listen后面第一个参数。
    if (nt_parse_url(cf->pool, &u) != NT_OK) {
        if (u.err) {
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "%s in \"%V\" of the \"listen\" directive",
                               u.err, &u.url);
        }

        return NT_CONF_ERROR;
    }

    cmcf = nt_stream_conf_get_module_main_conf(cf, nt_stream_core_module);

    ls = nt_array_push_n(&cmcf->listen, u.naddrs);
    if (ls == NULL) {
        return NT_CONF_ERROR;
    }

    nt_memzero(ls, sizeof(nt_stream_listen_t));

    ls->backlog = NT_LISTEN_BACKLOG;
    ls->rcvbuf = -1;
    ls->sndbuf = -1;
    ls->type = SOCK_STREAM;
    ls->ctx = cf->ctx;

#if (NT_HAVE_INET6)
    ls->ipv6only = 1;
#endif

    if( u.family == NT_AF_TUN )
        ls->type = NT_AF_TUN;

    backlog = 0;

    for (i = 2; i < cf->args->nelts; i++) {

#if !(NT_WIN32)
        if (nt_strcmp(value[i].data, "udp") == 0) {
            ls->type = SOCK_DGRAM;
            continue;
        }
#endif

#if (NT_STREAM_SNI)
        if (nt_strcmp(value[i].data, "default_server") == 0
            || nt_strcmp(value[i].data, "default") == 0) {

            ls->default_server = 1;
            continue;
        }
#endif

        if (nt_strcmp(value[i].data, "bind") == 0) {
            ls->bind = 1;
            continue;
        }

        if (nt_strncmp(value[i].data, "backlog=", 8) == 0) {
            ls->backlog = nt_atoi(value[i].data + 8, value[i].len - 8);
            ls->bind = 1;

            if (ls->backlog == NT_ERROR || ls->backlog == 0) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "invalid backlog \"%V\"", &value[i]);
                return NT_CONF_ERROR;
            }

            backlog = 1;

            continue;
        }

        if (nt_strncmp(value[i].data, "rcvbuf=", 7) == 0) {
            size.len = value[i].len - 7;
            size.data = value[i].data + 7;

            ls->rcvbuf = nt_parse_size(&size);
            ls->bind = 1;

            if (ls->rcvbuf == NT_ERROR) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "invalid rcvbuf \"%V\"", &value[i]);
                return NT_CONF_ERROR;
            }

            continue;
        }

        if (nt_strncmp(value[i].data, "sndbuf=", 7) == 0) {
            size.len = value[i].len - 7;
            size.data = value[i].data + 7;

            ls->sndbuf = nt_parse_size(&size);
            ls->bind = 1;

            if (ls->sndbuf == NT_ERROR) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "invalid sndbuf \"%V\"", &value[i]);
                return NT_CONF_ERROR;
            }

            continue;
        }

        if (nt_strncmp(value[i].data, "ipv6only=o", 10) == 0) {
#if (NT_HAVE_INET6 && defined IPV6_V6ONLY)
            if (nt_strcmp(&value[i].data[10], "n") == 0) {
                ls->ipv6only = 1;

            } else if (nt_strcmp(&value[i].data[10], "ff") == 0) {
                ls->ipv6only = 0;

            } else {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "invalid ipv6only flags \"%s\"",
                                   &value[i].data[9]);
                return NT_CONF_ERROR;
            }

            ls->bind = 1;
            continue;
#else
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "bind ipv6only is not supported "
                               "on this platform");
            return NT_CONF_ERROR;
#endif
        }

        if (nt_strcmp(value[i].data, "reuseport") == 0) {
#if (NT_HAVE_REUSEPORT)
            ls->reuseport = 1;
            ls->bind = 1;
#else
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "reuseport is not supported "
                               "on this platform, ignored");
#endif
            continue;
        }

        if (nt_strcmp(value[i].data, "ssl") == 0) {
#if (NT_STREAM_SSL)
            nt_stream_ssl_conf_t  *sslcf;

            sslcf = nt_stream_conf_get_module_srv_conf(cf,
                                                        nt_stream_ssl_module);

            sslcf->listen = 1;
            sslcf->file = cf->conf_file->file.name.data;
            sslcf->line = cf->conf_file->line;

            ls->ssl = 1;

            continue;
#else
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "the \"ssl\" parameter requires "
                               "nt_stream_ssl_module");
            return NT_CONF_ERROR;
#endif
        }

        if (nt_strncmp(value[i].data, "so_keepalive=", 13) == 0) {

            if (nt_strcmp(&value[i].data[13], "on") == 0) {
                ls->so_keepalive = 1;

            } else if (nt_strcmp(&value[i].data[13], "off") == 0) {
                ls->so_keepalive = 2;

            } else {

#if (NT_HAVE_KEEPALIVE_TUNABLE)
                u_char     *p, *end;
                nt_str_t   s;

                end = value[i].data + value[i].len;
                s.data = value[i].data + 13;

                p = nt_strlchr(s.data, end, ':');
                if (p == NULL) {
                    p = end;
                }

                if (p > s.data) {
                    s.len = p - s.data;

                    ls->tcp_keepidle = nt_parse_time(&s, 1);
                    if (ls->tcp_keepidle == (time_t) NT_ERROR) {
                        goto invalid_so_keepalive;
                    }
                }

                s.data = (p < end) ? (p + 1) : end;

                p = nt_strlchr(s.data, end, ':');
                if (p == NULL) {
                    p = end;
                }

                if (p > s.data) {
                    s.len = p - s.data;

                    ls->tcp_keepintvl = nt_parse_time(&s, 1);
                    if (ls->tcp_keepintvl == (time_t) NT_ERROR) {
                        goto invalid_so_keepalive;
                    }
                }

                s.data = (p < end) ? (p + 1) : end;

                if (s.data < end) {
                    s.len = end - s.data;

                    ls->tcp_keepcnt = nt_atoi(s.data, s.len);
                    if (ls->tcp_keepcnt == NT_ERROR) {
                        goto invalid_so_keepalive;
                    }
                }

                if (ls->tcp_keepidle == 0 && ls->tcp_keepintvl == 0
                    && ls->tcp_keepcnt == 0)
                {
                    goto invalid_so_keepalive;
                }

                ls->so_keepalive = 1;

#else

                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "the \"so_keepalive\" parameter accepts "
                                   "only \"on\" or \"off\" on this platform");
                return NT_CONF_ERROR;

#endif
            }

            ls->bind = 1;

            continue;

#if (NT_HAVE_KEEPALIVE_TUNABLE)
        invalid_so_keepalive:

            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "invalid so_keepalive value: \"%s\"",
                               &value[i].data[13]);
            return NT_CONF_ERROR;
#endif
        }

        if (nt_strcmp(value[i].data, "proxy_protocol") == 0) {
            ls->proxy_protocol = 1;
            continue;
        }

        nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                           "the invalid \"%V\" parameter", &value[i]);
        return NT_CONF_ERROR;
    }

    if (ls->type == SOCK_DGRAM) {
        if (backlog) {
            return "\"backlog\" parameter is incompatible with \"udp\"";
        }

#if (NT_STREAM_SSL)
        if (ls->ssl) {
            return "\"ssl\" parameter is incompatible with \"udp\"";
        }
#endif

        if (ls->so_keepalive) {
            return "\"so_keepalive\" parameter is incompatible with \"udp\"";
        }

        if (ls->proxy_protocol) {
            return "\"proxy_protocol\" parameter is incompatible with \"udp\"";
        }
    }

    als = cmcf->listen.elts;

    debug( "ls->type=%d, SOCK=%d", ls->type, SOCK_DGRAM );
    for (n = 0; n < u.naddrs; n++) {
        ls[n] = ls[0];

        ls[n].sockaddr = u.addrs[n].sockaddr;
        ls[n].socklen = u.addrs[n].socklen;
        ls[n].addr_text = u.addrs[n].name;
        ls[n].wildcard = nt_inet_wildcard(ls[n].sockaddr);

#if (NT_STREAM_SNI)
        continue;
#endif

        for (i = 0; i < cmcf->listen.nelts - u.naddrs + n; i++) {
            if (ls[n].type != als[i].type) {
                continue;
            }

            if (nt_cmp_sockaddr(als[i].sockaddr, als[i].socklen,
                                 ls[n].sockaddr, ls[n].socklen, 1)
                != NT_OK)
            {
                continue;
            }

            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "duplicate \"%V\" address and port pair",
                               &ls[n].addr_text);
            return NT_CONF_ERROR;
        }
    }

    return NT_CONF_OK;
}


static char *
nt_stream_core_resolver(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    nt_stream_core_srv_conf_t  *cscf = conf;

    nt_str_t  *value;

    if (cscf->resolver) {
        return "is duplicate";
    }

    value = cf->args->elts;

    cscf->resolver = nt_resolver_create(cf, &value[1], cf->args->nelts - 1);
    if (cscf->resolver == NULL) {
        return NT_CONF_ERROR;
    }

    return NT_CONF_OK;
}

#if (NT_STREAM_SNI)
static char *
nt_stream_core_server_name(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    nt_stream_core_srv_conf_t *cscf = conf;

    u_char                   ch;
    nt_str_t               *value;
    nt_uint_t               i;
    nt_stream_server_name_t  *sn;

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {

        ch = value[i].data[0];

        if ((ch == '*' && (value[i].len < 3 || value[i].data[1] != '.'))
            || (ch == '.' && value[i].len < 2))
        {
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "server name \"%V\" is invalid", &value[i]);
            return NT_CONF_ERROR;
        }

        if (nt_strchr(value[i].data, '/')) {
            nt_conf_log_error(NT_LOG_WARN, cf, 0,
                               "server name \"%V\" has suspicious symbols",
                               &value[i]);
        }

        sn = nt_array_push(&cscf->server_names);
        if (sn == NULL) {
            return NT_CONF_ERROR;
        }

        sn->server = cscf;

        if (nt_strcasecmp(value[i].data, (u_char *) "$hostname") == 0) {
            sn->name = cf->cycle->hostname;

        } else {
            sn->name = value[i];
        }

        if (value[i].data[0] != '~') {
            nt_strlow(sn->name.data, sn->name.data, sn->name.len);
            continue;
        }

    }

    return NT_CONF_OK;
}
#endif
