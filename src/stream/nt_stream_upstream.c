

#include <nt_core.h>
#include <nt_stream.h>


static nt_int_t nt_stream_upstream_add_variables(nt_conf_t *cf);
static nt_int_t nt_stream_upstream_addr_variable(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_upstream_response_time_variable(
    nt_stream_session_t *s, nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_upstream_bytes_variable(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);

static char *nt_stream_upstream(nt_conf_t *cf, nt_command_t *cmd,
    void *dummy);
static char *nt_stream_upstream_server(nt_conf_t *cf, nt_command_t *cmd,
    void *conf);
static void *nt_stream_upstream_create_main_conf(nt_conf_t *cf);
static char *nt_stream_upstream_init_main_conf(nt_conf_t *cf, void *conf);


static nt_command_t  nt_stream_upstream_commands[] = {

    { nt_string("upstream"),
      NT_STREAM_MAIN_CONF|NT_CONF_BLOCK|NT_CONF_TAKE1,
      nt_stream_upstream,
      0,
      0,
      NULL },

    { nt_string("server"),
      NT_STREAM_UPS_CONF|NT_CONF_1MORE,
      nt_stream_upstream_server,
      NT_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

      nt_null_command
};


static nt_stream_module_t  nt_stream_upstream_module_ctx = {
    nt_stream_upstream_add_variables,     /* preconfiguration */
    NULL,                                  /* postconfiguration */

    nt_stream_upstream_create_main_conf,  /* create main configuration */
    nt_stream_upstream_init_main_conf,    /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL                                   /* merge server configuration */
};


nt_module_t  nt_stream_upstream_module = {
    NT_MODULE_V1,
    &nt_stream_upstream_module_ctx,       /* module context */
    nt_stream_upstream_commands,          /* module directives */
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


static nt_stream_variable_t  nt_stream_upstream_vars[] = {

    { nt_string("upstream_addr"), NULL,
      nt_stream_upstream_addr_variable, 0,
      NT_STREAM_VAR_NOCACHEABLE, 0 },

    { nt_string("upstream_bytes_sent"), NULL,
      nt_stream_upstream_bytes_variable, 0,
      NT_STREAM_VAR_NOCACHEABLE, 0 },

    { nt_string("upstream_connect_time"), NULL,
      nt_stream_upstream_response_time_variable, 2,
      NT_STREAM_VAR_NOCACHEABLE, 0 },

    { nt_string("upstream_first_byte_time"), NULL,
      nt_stream_upstream_response_time_variable, 1,
      NT_STREAM_VAR_NOCACHEABLE, 0 },

    { nt_string("upstream_session_time"), NULL,
      nt_stream_upstream_response_time_variable, 0,
      NT_STREAM_VAR_NOCACHEABLE, 0 },

    { nt_string("upstream_bytes_received"), NULL,
      nt_stream_upstream_bytes_variable, 1,
      NT_STREAM_VAR_NOCACHEABLE, 0 },

      nt_stream_null_variable
};


static nt_int_t
nt_stream_upstream_add_variables(nt_conf_t *cf)
{
    nt_stream_variable_t  *var, *v;

    for (v = nt_stream_upstream_vars; v->name.len; v++) {
        var = nt_stream_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NT_ERROR;
        }

        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NT_OK;
}


static nt_int_t
nt_stream_upstream_addr_variable(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    u_char                       *p;
    size_t                        len;
    nt_uint_t                    i;
    nt_stream_upstream_state_t  *state;

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    if (s->upstream_states == NULL || s->upstream_states->nelts == 0) {
        v->not_found = 1;
        return NT_OK;
    }

    len = 0;
    state = s->upstream_states->elts;

    for (i = 0; i < s->upstream_states->nelts; i++) {
        if (state[i].peer) {
            len += state[i].peer->len;
        }

        len += 2;
    }

    p = nt_pnalloc(s->connection->pool, len);
    if (p == NULL) {
        return NT_ERROR;
    }

    v->data = p;

    i = 0;

    for ( ;; ) {
        if (state[i].peer) {
            p = nt_cpymem(p, state[i].peer->data, state[i].peer->len);
        }

        if (++i == s->upstream_states->nelts) {
            break;
        }

        *p++ = ',';
        *p++ = ' ';
    }

    v->len = p - v->data;

    return NT_OK;
}


static nt_int_t
nt_stream_upstream_bytes_variable(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    u_char                       *p;
    size_t                        len;
    nt_uint_t                    i;
    nt_stream_upstream_state_t  *state;

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    if (s->upstream_states == NULL || s->upstream_states->nelts == 0) {
        v->not_found = 1;
        return NT_OK;
    }

    len = s->upstream_states->nelts * (NT_OFF_T_LEN + 2);

    p = nt_pnalloc(s->connection->pool, len);
    if (p == NULL) {
        return NT_ERROR;
    }

    v->data = p;

    i = 0;
    state = s->upstream_states->elts;

    for ( ;; ) {

        if (data == 1) {
            p = nt_sprintf(p, "%O", state[i].bytes_received);

        } else {
            p = nt_sprintf(p, "%O", state[i].bytes_sent);
        }

        if (++i == s->upstream_states->nelts) {
            break;
        }

        *p++ = ',';
        *p++ = ' ';
    }

    v->len = p - v->data;

    return NT_OK;
}


static nt_int_t
nt_stream_upstream_response_time_variable(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    u_char                       *p;
    size_t                        len;
    nt_uint_t                    i;
    nt_msec_int_t                ms;
    nt_stream_upstream_state_t  *state;

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    if (s->upstream_states == NULL || s->upstream_states->nelts == 0) {
        v->not_found = 1;
        return NT_OK;
    }

    len = s->upstream_states->nelts * (NT_TIME_T_LEN + 4 + 2);

    p = nt_pnalloc(s->connection->pool, len);
    if (p == NULL) {
        return NT_ERROR;
    }

    v->data = p;

    i = 0;
    state = s->upstream_states->elts;

    for ( ;; ) {

        if (data == 1) {
            ms = state[i].first_byte_time;

        } else if (data == 2) {
            ms = state[i].connect_time;

        } else {
            ms = state[i].response_time;
        }

        if (ms != -1) {
            ms = nt_max(ms, 0);
            p = nt_sprintf(p, "%T.%03M", (time_t) ms / 1000, ms % 1000);

        } else {
            *p++ = '-';
        }

        if (++i == s->upstream_states->nelts) {
            break;
        }

        *p++ = ',';
        *p++ = ' ';
    }

    v->len = p - v->data;

    return NT_OK;
}


static char *
nt_stream_upstream(nt_conf_t *cf, nt_command_t *cmd, void *dummy)
{
    char                            *rv;
    void                            *mconf;
    nt_str_t                       *value;
    nt_url_t                        u;
    nt_uint_t                       m;
    nt_conf_t                       pcf;
    nt_stream_module_t             *module;
    nt_stream_conf_ctx_t           *ctx, *stream_ctx;
    nt_stream_upstream_srv_conf_t  *uscf;

    nt_memzero(&u, sizeof(nt_url_t));

    value = cf->args->elts;
    u.host = value[1];
    u.no_resolve = 1;
    u.no_port = 1;

    uscf = nt_stream_upstream_add(cf, &u, NT_STREAM_UPSTREAM_CREATE
                                           |NT_STREAM_UPSTREAM_WEIGHT
                                           |NT_STREAM_UPSTREAM_MAX_CONNS
                                           |NT_STREAM_UPSTREAM_MAX_FAILS
                                           |NT_STREAM_UPSTREAM_FAIL_TIMEOUT
                                           |NT_STREAM_UPSTREAM_DOWN
                                           |NT_STREAM_UPSTREAM_BACKUP);
    if (uscf == NULL) {
        return NT_CONF_ERROR;
    }


    ctx = nt_pcalloc(cf->pool, sizeof(nt_stream_conf_ctx_t));
    if (ctx == NULL) {
        return NT_CONF_ERROR;
    }

    stream_ctx = cf->ctx;
    ctx->main_conf = stream_ctx->main_conf;

    /* the upstream{}'s srv_conf */

    ctx->srv_conf = nt_pcalloc(cf->pool,
                                sizeof(void *) * nt_stream_max_module);
    if (ctx->srv_conf == NULL) {
        return NT_CONF_ERROR;
    }

    ctx->srv_conf[nt_stream_upstream_module.ctx_index] = uscf;

    uscf->srv_conf = ctx->srv_conf;

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

    uscf->servers = nt_array_create(cf->pool, 4,
                                     sizeof(nt_stream_upstream_server_t));
    if (uscf->servers == NULL) {
        return NT_CONF_ERROR;
    }


    /* parse inside upstream{} */

    pcf = *cf;
    cf->ctx = ctx;
    cf->cmd_type = NT_STREAM_UPS_CONF;

    rv = nt_conf_parse(cf, NULL);

    *cf = pcf;

    if (rv != NT_CONF_OK) {
        return rv;
    }

    if (uscf->servers->nelts == 0) {
        nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                           "no servers are inside upstream");
        return NT_CONF_ERROR;
    }

    return rv;
}


static char *
nt_stream_upstream_server(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    nt_stream_upstream_srv_conf_t  *uscf = conf;

    time_t                         fail_timeout;
    nt_str_t                     *value, s;
    nt_url_t                      u;
    nt_int_t                      weight, max_conns, max_fails;
    nt_uint_t                     i;
    nt_stream_upstream_server_t  *us;

    us = nt_array_push(uscf->servers);
    if (us == NULL) {
        return NT_CONF_ERROR;
    }

    nt_memzero(us, sizeof(nt_stream_upstream_server_t));

    value = cf->args->elts;

    weight = 1;
    max_conns = 0;
    max_fails = 1;
    fail_timeout = 10;

    for (i = 2; i < cf->args->nelts; i++) {

        if (nt_strncmp(value[i].data, "weight=", 7) == 0) {

            if (!(uscf->flags & NT_STREAM_UPSTREAM_WEIGHT)) {
                goto not_supported;
            }

            weight = nt_atoi(&value[i].data[7], value[i].len - 7);

            if (weight == NT_ERROR || weight == 0) {
                goto invalid;
            }

            continue;
        }

        if (nt_strncmp(value[i].data, "max_conns=", 10) == 0) {

            if (!(uscf->flags & NT_STREAM_UPSTREAM_MAX_CONNS)) {
                goto not_supported;
            }

            max_conns = nt_atoi(&value[i].data[10], value[i].len - 10);

            if (max_conns == NT_ERROR) {
                goto invalid;
            }

            continue;
        }

        if (nt_strncmp(value[i].data, "max_fails=", 10) == 0) {

            if (!(uscf->flags & NT_STREAM_UPSTREAM_MAX_FAILS)) {
                goto not_supported;
            }

            max_fails = nt_atoi(&value[i].data[10], value[i].len - 10);

            if (max_fails == NT_ERROR) {
                goto invalid;
            }

            continue;
        }

        if (nt_strncmp(value[i].data, "fail_timeout=", 13) == 0) {

            if (!(uscf->flags & NT_STREAM_UPSTREAM_FAIL_TIMEOUT)) {
                goto not_supported;
            }

            s.len = value[i].len - 13;
            s.data = &value[i].data[13];

            fail_timeout = nt_parse_time(&s, 1);

            if (fail_timeout == (time_t) NT_ERROR) {
                goto invalid;
            }

            continue;
        }

        if (nt_strcmp(value[i].data, "backup") == 0) {

            if (!(uscf->flags & NT_STREAM_UPSTREAM_BACKUP)) {
                goto not_supported;
            }

            us->backup = 1;

            continue;
        }

        if (nt_strcmp(value[i].data, "down") == 0) {

            if (!(uscf->flags & NT_STREAM_UPSTREAM_DOWN)) {
                goto not_supported;
            }

            us->down = 1;

            continue;
        }

        goto invalid;
    }

    nt_memzero(&u, sizeof(nt_url_t));

    u.url = value[1];

    if (nt_parse_url(cf->pool, &u) != NT_OK) {
        if (u.err) {
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "%s in upstream \"%V\"", u.err, &u.url);
        }

        return NT_CONF_ERROR;
    }

    if (u.no_port) {
        nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                           "no port in upstream \"%V\"", &u.url);
        return NT_CONF_ERROR;
    }

    us->name = u.url;
    us->addrs = u.addrs;
    us->naddrs = u.naddrs;
    us->weight = weight;
    us->max_conns = max_conns;
    us->max_fails = max_fails;
    us->fail_timeout = fail_timeout;

    return NT_CONF_OK;

invalid:

    nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &value[i]);

    return NT_CONF_ERROR;

not_supported:

    nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                       "balancing method does not support parameter \"%V\"",
                       &value[i]);

    return NT_CONF_ERROR;
}


nt_stream_upstream_srv_conf_t *
nt_stream_upstream_add(nt_conf_t *cf, nt_url_t *u, nt_uint_t flags)
{
    nt_uint_t                        i;
    nt_stream_upstream_server_t     *us;
    nt_stream_upstream_srv_conf_t   *uscf, **uscfp;
    nt_stream_upstream_main_conf_t  *umcf;

    if (!(flags & NT_STREAM_UPSTREAM_CREATE)) {

        if (nt_parse_url(cf->pool, u) != NT_OK) {
            if (u->err) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "%s in upstream \"%V\"", u->err, &u->url);
            }

            return NULL;
        }
    }

    umcf = nt_stream_conf_get_module_main_conf(cf, nt_stream_upstream_module);

    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {

        if (uscfp[i]->host.len != u->host.len
            || nt_strncasecmp(uscfp[i]->host.data, u->host.data, u->host.len)
               != 0)
        {
            continue;
        }

        if ((flags & NT_STREAM_UPSTREAM_CREATE)
             && (uscfp[i]->flags & NT_STREAM_UPSTREAM_CREATE))
        {
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "duplicate upstream \"%V\"", &u->host);
            return NULL;
        }

        if ((uscfp[i]->flags & NT_STREAM_UPSTREAM_CREATE) && !u->no_port) {
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "upstream \"%V\" may not have port %d",
                               &u->host, u->port);
            return NULL;
        }

        if ((flags & NT_STREAM_UPSTREAM_CREATE) && !uscfp[i]->no_port) {
            nt_log_error(NT_LOG_EMERG, cf->log, 0,
                          "upstream \"%V\" may not have port %d in %s:%ui",
                          &u->host, uscfp[i]->port,
                          uscfp[i]->file_name, uscfp[i]->line);
            return NULL;
        }

        if (uscfp[i]->port != u->port) {
            continue;
        }

        if (flags & NT_STREAM_UPSTREAM_CREATE) {
            uscfp[i]->flags = flags;
        }

        return uscfp[i];
    }

    uscf = nt_pcalloc(cf->pool, sizeof(nt_stream_upstream_srv_conf_t));
    if (uscf == NULL) {
        return NULL;
    }

    uscf->flags = flags;
    uscf->host = u->host;
    uscf->file_name = cf->conf_file->file.name.data;
    uscf->line = cf->conf_file->line;
    uscf->port = u->port;
    uscf->no_port = u->no_port;

    if (u->naddrs == 1 && (u->port || u->family == AF_UNIX)) {
        uscf->servers = nt_array_create(cf->pool, 1,
                                         sizeof(nt_stream_upstream_server_t));
        if (uscf->servers == NULL) {
            return NULL;
        }

        us = nt_array_push(uscf->servers);
        if (us == NULL) {
            return NULL;
        }

        nt_memzero(us, sizeof(nt_stream_upstream_server_t));

        us->addrs = u->addrs;
        us->naddrs = 1;
    }

    uscfp = nt_array_push(&umcf->upstreams);
    if (uscfp == NULL) {
        return NULL;
    }

    *uscfp = uscf;

    return uscf;
}


static void *
nt_stream_upstream_create_main_conf(nt_conf_t *cf)
{
    nt_stream_upstream_main_conf_t  *umcf;

    umcf = nt_pcalloc(cf->pool, sizeof(nt_stream_upstream_main_conf_t));
    if (umcf == NULL) {
        return NULL;
    }

    if (nt_array_init(&umcf->upstreams, cf->pool, 4,
                       sizeof(nt_stream_upstream_srv_conf_t *))
        != NT_OK)
    {
        return NULL;
    }

    return umcf;
}


static char *
nt_stream_upstream_init_main_conf(nt_conf_t *cf, void *conf)
{
    nt_stream_upstream_main_conf_t *umcf = conf;

    nt_uint_t                        i;
    nt_stream_upstream_init_pt       init;
    nt_stream_upstream_srv_conf_t  **uscfp;

    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {

        init = uscfp[i]->peer.init_upstream
                                         ? uscfp[i]->peer.init_upstream
                                         : nt_stream_upstream_init_round_robin;

        if (init(cf, uscfp[i]) != NT_OK) {
            return NT_CONF_ERROR;
        }
    }

    return NT_CONF_OK;
}
