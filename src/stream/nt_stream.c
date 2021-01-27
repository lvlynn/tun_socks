

#include <nt_core.h>
#include <nt_stream.h>
//#include <nt_event.h>


static char *nt_stream_block(nt_conf_t *cf, nt_command_t *cmd, void *conf);
static nt_int_t nt_stream_init_phases(nt_conf_t *cf,
    nt_stream_core_main_conf_t *cmcf);
static nt_int_t nt_stream_init_phase_handlers(nt_conf_t *cf,
    nt_stream_core_main_conf_t *cmcf);
static nt_int_t nt_stream_add_ports(nt_conf_t *cf, nt_array_t *ports,
    nt_stream_listen_t *listen);
static char *nt_stream_optimize_servers(nt_conf_t *cf, nt_array_t *ports);
static nt_int_t nt_stream_add_addrs(nt_conf_t *cf, nt_stream_port_t *stport,
    nt_stream_conf_addr_t *addr);
#if (NT_HAVE_INET6)
static nt_int_t nt_stream_add_addrs6(nt_conf_t *cf,
    nt_stream_port_t *stport, nt_stream_conf_addr_t *addr);
#endif
static nt_int_t nt_stream_cmp_conf_addrs(const void *one, const void *two);


nt_uint_t  nt_stream_max_module;


nt_stream_filter_pt  nt_stream_top_filter;


static nt_command_t  nt_stream_commands[] = {

    { nt_string("stream"),
      NT_MAIN_CONF|NT_CONF_BLOCK|NT_CONF_NOARGS,
      nt_stream_block,
      0,
      0,
      NULL },

      nt_null_command
};


static nt_core_module_t  nt_stream_module_ctx = {
    nt_string("stream"),
    NULL,
    NULL
};


nt_module_t  nt_stream_module = {
    NT_MODULE_V1,
    &nt_stream_module_ctx,                /* module context */
    nt_stream_commands,                   /* module directives */
    NT_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NT_MODULE_V1_PADDING
};


    u_char *
nt_stream_accept_log_error(nt_log_t *log, u_char *buf, size_t len)
{
    return nt_snprintf(buf, len, " while accepting new connection on %V",
                       log->data);

}

    static char *
nt_stream_block(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    char                          *rv;
    nt_uint_t                     i, m, mi, s;
    nt_conf_t                     pcf;
    nt_array_t                    ports;
    nt_stream_listen_t           *listen;
    nt_stream_module_t           *module;
    nt_stream_conf_ctx_t         *ctx;
    nt_stream_core_srv_conf_t   **cscfp;
    nt_stream_core_main_conf_t   *cmcf;

    if (*(nt_stream_conf_ctx_t **) conf) {
        return "is duplicate";
    }

    /* the main stream context */

    ctx = nt_pcalloc(cf->pool, sizeof(nt_stream_conf_ctx_t));
    if (ctx == NULL) {
        return NT_CONF_ERROR;
    }

    *(nt_stream_conf_ctx_t **) conf = ctx;

    /* count the number of the stream modules and set up their indices */

    nt_stream_max_module = nt_count_modules(cf->cycle, NT_STREAM_MODULE);


    /* the stream main_conf context, it's the same in the all stream contexts */

    ctx->main_conf = nt_pcalloc(cf->pool,
                                 sizeof(void *) * nt_stream_max_module);
    if (ctx->main_conf == NULL) {
        return NT_CONF_ERROR;
    }


    /*
     * the stream null srv_conf context, it is used to merge
     * the server{}s' srv_conf's
     */

    ctx->srv_conf = nt_pcalloc(cf->pool,
                                sizeof(void *) * nt_stream_max_module);
    if (ctx->srv_conf == NULL) {
        return NT_CONF_ERROR;
    }


    /*
     * create the main_conf's and the null srv_conf's of the all stream modules
     */

    for (m = 0; cf->cycle->modules[m]; m++) {
        if (cf->cycle->modules[m]->type != NT_STREAM_MODULE) {
            continue;
        }

        module = cf->cycle->modules[m]->ctx;
        mi = cf->cycle->modules[m]->ctx_index;

        if (module->create_main_conf) {
            ctx->main_conf[mi] = module->create_main_conf(cf);
            if (ctx->main_conf[mi] == NULL) {
                return NT_CONF_ERROR;
            }
        }

        if (module->create_srv_conf) {
            ctx->srv_conf[mi] = module->create_srv_conf(cf);
            if (ctx->srv_conf[mi] == NULL) {
                return NT_CONF_ERROR;
            }
        }
    }


    pcf = *cf;
    cf->ctx = ctx;

    for (m = 0; cf->cycle->modules[m]; m++) {
        if (cf->cycle->modules[m]->type != NT_STREAM_MODULE) {
            continue;
        }

        module = cf->cycle->modules[m]->ctx;

        if (module->preconfiguration) {
            if (module->preconfiguration(cf) != NT_OK) {
                return NT_CONF_ERROR;
            }
        }
    }


    /* parse inside the stream{} block */

    cf->module_type = NT_STREAM_MODULE;
    cf->cmd_type = NT_STREAM_MAIN_CONF;
    rv = nt_conf_parse(cf, NULL);

    if (rv != NT_CONF_OK) {
        *cf = pcf;
        return rv;
    }


    /* init stream{} main_conf's, merge the server{}s' srv_conf's */

    cmcf = ctx->main_conf[nt_stream_core_module.ctx_index];
    cscfp = cmcf->servers.elts;

    for (m = 0; cf->cycle->modules[m]; m++) {
        if (cf->cycle->modules[m]->type != NT_STREAM_MODULE) {
            continue;
        }

        module = cf->cycle->modules[m]->ctx;
        mi = cf->cycle->modules[m]->ctx_index;

        /* init stream{} main_conf's */

        cf->ctx = ctx;

        if (module->init_main_conf) {
            rv = module->init_main_conf(cf, ctx->main_conf[mi]);
            if (rv != NT_CONF_OK) {
                *cf = pcf;
                return rv;
            }
        }

        for (s = 0; s < cmcf->servers.nelts; s++) {

            /* merge the server{}s' srv_conf's */

            cf->ctx = cscfp[s]->ctx;

            if (module->merge_srv_conf) {
                rv = module->merge_srv_conf(cf,
                                            ctx->srv_conf[mi],
                                            cscfp[s]->ctx->srv_conf[mi]);
                if (rv != NT_CONF_OK) {
                    *cf = pcf;
                    return rv;
                }
            }
        }
    }

    if (nt_stream_init_phases(cf, cmcf) != NT_OK) {
        return NT_CONF_ERROR;
    }

    for (m = 0; cf->cycle->modules[m]; m++) {
        if (cf->cycle->modules[m]->type != NT_STREAM_MODULE) {
            continue;
        }

        module = cf->cycle->modules[m]->ctx;

        if (module->postconfiguration) {
            if (module->postconfiguration(cf) != NT_OK) {
                return NT_CONF_ERROR;
            }
        }
    }

    if (nt_stream_variables_init_vars(cf) != NT_OK) {
        return NT_CONF_ERROR;
    }

    *cf = pcf;

    if (nt_stream_init_phase_handlers(cf, cmcf) != NT_OK) {
        return NT_CONF_ERROR;
    }

    if (nt_array_init(&ports, cf->temp_pool, 4, sizeof(nt_stream_conf_port_t))
        != NT_OK)
    {
        return NT_CONF_ERROR;
    }

    listen = cmcf->listen.elts;

    for (i = 0; i < cmcf->listen.nelts; i++) {
        if (nt_stream_add_ports(cf, &ports, &listen[i]) != NT_OK) {
            return NT_CONF_ERROR;
        }
    }

    return nt_stream_optimize_servers(cf, &ports);
}


static nt_int_t
nt_stream_init_phases(nt_conf_t *cf, nt_stream_core_main_conf_t *cmcf)
{
    if (nt_array_init(&cmcf->phases[NT_STREAM_POST_ACCEPT_PHASE].handlers,
                       cf->pool, 1, sizeof(nt_stream_handler_pt))
        != NT_OK)
    {
        return NT_ERROR;
    }

    if (nt_array_init(&cmcf->phases[NT_STREAM_PREACCESS_PHASE].handlers,
                       cf->pool, 1, sizeof(nt_stream_handler_pt))
        != NT_OK)
    {
        return NT_ERROR;
    }

    if (nt_array_init(&cmcf->phases[NT_STREAM_ACCESS_PHASE].handlers,
                       cf->pool, 1, sizeof(nt_stream_handler_pt))
        != NT_OK)
    {
        return NT_ERROR;
    }

    if (nt_array_init(&cmcf->phases[NT_STREAM_SSL_PHASE].handlers,
                       cf->pool, 1, sizeof(nt_stream_handler_pt))
        != NT_OK)
    {
        return NT_ERROR;
    }

    if (nt_array_init(&cmcf->phases[NT_STREAM_PREREAD_PHASE].handlers,
                       cf->pool, 1, sizeof(nt_stream_handler_pt))
        != NT_OK)
    {
        return NT_ERROR;
    }

    if (nt_array_init(&cmcf->phases[NT_STREAM_LOG_PHASE].handlers,
                       cf->pool, 1, sizeof(nt_stream_handler_pt))
        != NT_OK)
    {
        return NT_ERROR;
    }

    return NT_OK;
}

#if (NT_STREAM_SNI)

static int nt_libc_cdecl
nt_stream_cmp_dns_wildcards(const void *one, const void *two)
{
    nt_hash_key_t  *first, *second;

    first = (nt_hash_key_t *) one;
    second = (nt_hash_key_t *) two;

    return nt_dns_strcmp(first->key.data, second->key.data);
}


static nt_int_t
nt_stream_server_names(nt_conf_t *cf, nt_stream_core_main_conf_t *cmcf,
    nt_stream_conf_addr_t *addr)
{
    nt_int_t                   rc;
    nt_uint_t                  n, s;
    nt_hash_init_t             hash;
    nt_hash_keys_arrays_t      ha;
    nt_stream_server_name_t   *name;
    nt_stream_core_srv_conf_t **cscfp;

    nt_memzero(&ha, sizeof(nt_hash_keys_arrays_t));

    ha.temp_pool = nt_create_pool(NT_DEFAULT_POOL_SIZE, cf->log);
    if (ha.temp_pool == NULL) {
        return NT_ERROR;
    }

    ha.pool = cf->pool;

    if (nt_hash_keys_array_init(&ha, NT_HASH_LARGE) != NT_OK) {
        goto failed;
    }

    cscfp = addr->servers.elts;

    for (s = 0; s < addr->servers.nelts; s++) {

        name = cscfp[s]->server_names.elts;

        for (n = 0; n < cscfp[s]->server_names.nelts; n++) {

            rc = nt_hash_add_key(&ha, &name[n].name, name[n].server,
                                  NT_HASH_WILDCARD_KEY);

            if (rc == NT_ERROR) {
                return NT_ERROR;
            }

            if (rc == NT_DECLINED) {
                nt_log_error(NT_LOG_EMERG, cf->log, 0,
                              "invalid server name or wildcard \"%V\"",
                              &name[n].name);
                return NT_ERROR;
            }

            if (rc == NT_BUSY) {
                nt_log_error(NT_LOG_WARN, cf->log, 0,
                              "conflicting server name \"%V\" ignored",
                              &name[n].name);
            }
        }
    }

    hash.key = nt_hash_key_lc;
    hash.max_size = cmcf->server_names_hash_max_size;
    hash.bucket_size = cmcf->server_names_hash_bucket_size;
    hash.name = "server_names_hash";
    hash.pool = cf->pool;

    if (ha.keys.nelts) {
        hash.hash = &addr->hash;
        hash.temp_pool = NULL;

        if (nt_hash_init(&hash, ha.keys.elts, ha.keys.nelts) != NT_OK) {
            goto failed;
        }
    }

    if (ha.dns_wc_head.nelts) {

        nt_qsort(ha.dns_wc_head.elts, (size_t) ha.dns_wc_head.nelts,
                  sizeof(nt_hash_key_t), nt_stream_cmp_dns_wildcards);

        hash.hash = NULL;
        hash.temp_pool = ha.temp_pool;

        if (nt_hash_wildcard_init(&hash, ha.dns_wc_head.elts,
                                   ha.dns_wc_head.nelts)
            != NT_OK)
        {
            goto failed;
        }

        addr->wc_head = (nt_hash_wildcard_t *) hash.hash;
    }

    if (ha.dns_wc_tail.nelts) {

        nt_qsort(ha.dns_wc_tail.elts, (size_t) ha.dns_wc_tail.nelts,
                  sizeof(nt_hash_key_t), nt_stream_cmp_dns_wildcards);

        hash.hash = NULL;
        hash.temp_pool = ha.temp_pool;

        if (nt_hash_wildcard_init(&hash, ha.dns_wc_tail.elts,
                                   ha.dns_wc_tail.nelts)
            != NT_OK)
        {
            goto failed;
        }

        addr->wc_tail = (nt_hash_wildcard_t *) hash.hash;
    }

    nt_destroy_pool(ha.temp_pool);

    return NT_OK;

failed:

    nt_destroy_pool(ha.temp_pool);

    return NT_ERROR;
}

/* add the server core module configuration to the address:port */

static nt_int_t
nt_stream_add_server(nt_conf_t *cf, nt_stream_core_srv_conf_t *cscf,
    nt_stream_conf_addr_t *addr)
{
    nt_uint_t                  i;
    nt_stream_core_srv_conf_t  **server;

    if (addr->servers.elts == NULL) {
        if (nt_array_init(&addr->servers, cf->temp_pool, 4,
                           sizeof(nt_stream_core_srv_conf_t *))
            != NT_OK)
        {
            return NT_ERROR;
        }

    } else {
        server = addr->servers.elts;
        for (i = 0; i < addr->servers.nelts; i++) {
            if (server[i] == cscf) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "a duplicate listen");
                return NT_ERROR;
            }
        }
    }

    server = nt_array_push(&addr->servers);
    if (server == NULL) {
        return NT_ERROR;
    }

    *server = cscf;

    return NT_OK;
}
#endif


static nt_int_t
nt_stream_init_phase_handlers(nt_conf_t *cf,
    nt_stream_core_main_conf_t *cmcf)
{
    nt_int_t                     j;
    nt_uint_t                    i, n;
    nt_stream_handler_pt        *h;
    nt_stream_phase_handler_t   *ph;
    nt_stream_phase_handler_pt   checker;

    n = 1 /* content phase */;

    for (i = 0; i < NT_STREAM_LOG_PHASE; i++) {
        n += cmcf->phases[i].handlers.nelts;
    }

    ph = nt_pcalloc(cf->pool,
                     n * sizeof(nt_stream_phase_handler_t) + sizeof(void *));
    if (ph == NULL) {
        return NT_ERROR;
    }

    cmcf->phase_engine.handlers = ph;
    n = 0;

    for (i = 0; i < NT_STREAM_LOG_PHASE; i++) {
        h = cmcf->phases[i].handlers.elts;

        switch (i) {

        case NT_STREAM_PREREAD_PHASE:
            checker = nt_stream_core_preread_phase;
            break;

        case NT_STREAM_CONTENT_PHASE:
            ph->checker = nt_stream_core_content_phase;
            n++;
            ph++;

            continue;

        default:
            checker = nt_stream_core_generic_phase;
        }

        n += cmcf->phases[i].handlers.nelts;

        for (j = cmcf->phases[i].handlers.nelts - 1; j >= 0; j--) {
            ph->checker = checker;
            ph->handler = h[j];
            ph->next = n;
            ph++;
        }
    }

    return NT_OK;
}


static nt_int_t
nt_stream_add_ports(nt_conf_t *cf, nt_array_t *ports,
    nt_stream_listen_t *listen)
{
    in_port_t                p;
    nt_uint_t               i;
    struct sockaddr         *sa;
    nt_stream_conf_port_t  *port;
    nt_stream_conf_addr_t  *addr;
#if (NT_STREAM_SNI)
    nt_stream_core_srv_conf_t *cscf;
#endif

    sa = listen->sockaddr;
    p = nt_inet_get_port(sa);

    port = ports->elts;
    for (i = 0; i < ports->nelts; i++) {

        if (p == port[i].port
            && listen->type == port[i].type
            && sa->sa_family == port[i].family)
        {
            /* a port is already in the port list */

            port = &port[i];
            goto found;
        }
    }

    /* add a port to the port list */

    port = nt_array_push(ports);
    if (port == NULL) {
        return NT_ERROR;
    }

    port->family = sa->sa_family;
    port->type = listen->type;
    port->port = p;

    if (nt_array_init(&port->addrs, cf->temp_pool, 2,
                       sizeof(nt_stream_conf_addr_t))
        != NT_OK)
    {
        return NT_ERROR;
    }

found:
#if (NT_STREAM_SNI)

    cscf = listen->ctx->srv_conf[nt_stream_core_module.ctx_index];
    addr = port->addrs.elts;

    for (i = 0; i < port->addrs.nelts; i++) {
        if (nt_cmp_sockaddr(listen->sockaddr, listen->socklen,
            addr[i].opt.sockaddr,
            addr[i].opt.socklen, 0)
            != NT_OK)
        {
            continue;
        }

        /* the address is already in the address list */

        if (nt_stream_add_server(cf, cscf, &addr[i]) != NT_OK) {
            return NT_ERROR;
        }

        if (listen->default_server) {

            if (addr[i].opt.default_server) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                        "a duplicate default server for");
                return NT_ERROR;
            }
            addr[i].default_server = cscf;
            addr[i].opt.default_server = 1;
        }
        return NT_OK;
    }

    addr = nt_array_push(&port->addrs);
    if (addr == NULL) {
        return NT_ERROR;
    }
    nt_memset(addr, 0, sizeof(nt_stream_conf_addr_t));
    addr->opt = *listen;
    addr->default_server = cscf;

    return nt_stream_add_server(cf, cscf, &addr[i]);

#else

    addr = nt_array_push(&port->addrs);
    if (addr == NULL) {
        return NT_ERROR;
    }

    addr->opt = *listen;

    return NT_OK;
#endif
}


static char *
nt_stream_optimize_servers(nt_conf_t *cf, nt_array_t *ports)
{
    nt_uint_t                   i, p, last, bind_wildcard;
    nt_listening_t             *ls;
    nt_stream_port_t           *stport;
    nt_stream_conf_port_t      *port;
    nt_stream_conf_addr_t      *addr;
    nt_stream_core_srv_conf_t  *cscf;

#if (NT_STREAM_SNI)
    nt_stream_core_main_conf_t *cmcf;
#endif
    port = ports->elts;
    for (p = 0; p < ports->nelts; p++) {

        nt_sort(port[p].addrs.elts, (size_t) port[p].addrs.nelts,
                 sizeof(nt_stream_conf_addr_t), nt_stream_cmp_conf_addrs);

        addr = port[p].addrs.elts;
        last = port[p].addrs.nelts;

        /*
         * if there is the binding to the "*:port" then we need to bind()
         * to the "*:port" only and ignore the other bindings
         */

        if (addr[last - 1].opt.wildcard) {
            addr[last - 1].opt.bind = 1;
            bind_wildcard = 1;

        } else {
            bind_wildcard = 0;
        }

        i = 0;

        while (i < last) {

            if (bind_wildcard && !addr[i].opt.bind) {
                i++;
                continue;
            }

            ls = nt_create_listening(cf, addr[i].opt.sockaddr,
                                      addr[i].opt.socklen);
            if (ls == NULL) {
                return NT_CONF_ERROR;
            }

            ls->addr_ntop = 1;
            ls->handler = nt_stream_init_connection;
            ls->pool_size = 256;
            ls->type = addr[i].opt.type;

            cscf = addr->opt.ctx->srv_conf[nt_stream_core_module.ctx_index];

            ls->logp = cscf->error_log;
            ls->log.data = &ls->addr_text;
            ls->log.handler = nt_stream_accept_log_error;

            ls->backlog = addr[i].opt.backlog;
            ls->rcvbuf = addr[i].opt.rcvbuf;
            ls->sndbuf = addr[i].opt.sndbuf;

            ls->wildcard = addr[i].opt.wildcard;

            ls->keepalive = addr[i].opt.so_keepalive;
#if (NT_HAVE_KEEPALIVE_TUNABLE)
            ls->keepidle = addr[i].opt.tcp_keepidle;
            ls->keepintvl = addr[i].opt.tcp_keepintvl;
            ls->keepcnt = addr[i].opt.tcp_keepcnt;
#endif

#if (NT_HAVE_INET6)
            ls->ipv6only = addr[i].opt.ipv6only;
#endif

#if (NT_HAVE_REUSEPORT)
            ls->reuseport = addr[i].opt.reuseport;
#endif

            stport = nt_palloc(cf->pool, sizeof(nt_stream_port_t));
            if (stport == NULL) {
                return NT_CONF_ERROR;
            }

            ls->servers = stport;

            stport->naddrs = i + 1;

#if (NT_STREAM_SNI)
            cmcf = addr->opt.ctx->main_conf[nt_stream_core_module.ctx_index];
            /*Because of ssl_sni_force we have to do this even one server*/
            if (addr[i].servers.nelts >= 1) {
                if (nt_stream_server_names(cf, cmcf, &addr[i]) != NT_OK) {
                    return NT_CONF_ERROR;
                }
            }
#endif

            switch (ls->sockaddr->sa_family) {
#if (NT_HAVE_INET6)
            case AF_INET6:
                if (nt_stream_add_addrs6(cf, stport, addr) != NT_OK) {
                    return NT_CONF_ERROR;
                }
                break;
#endif
            default: /* AF_INET */
                if (nt_stream_add_addrs(cf, stport, addr) != NT_OK) {
                    return NT_CONF_ERROR;
                }
                break;
            }

            addr++;
            last--;
        }
    }

    return NT_CONF_OK;
}


static nt_int_t
nt_stream_add_addrs(nt_conf_t *cf, nt_stream_port_t *stport,
    nt_stream_conf_addr_t *addr)
{
    nt_uint_t             i;
    struct sockaddr_in    *sin;
    nt_stream_in_addr_t  *addrs;
#if (NT_STREAM_SNI)
    nt_stream_virtual_names_t  *vn;
#endif

    stport->addrs = nt_pcalloc(cf->pool,
                                stport->naddrs * sizeof(nt_stream_in_addr_t));
    if (stport->addrs == NULL) {
        return NT_ERROR;
    }

    addrs = stport->addrs;

    for (i = 0; i < stport->naddrs; i++) {

        sin = (struct sockaddr_in *) addr[i].opt.sockaddr;
        addrs[i].addr = sin->sin_addr.s_addr;

        addrs[i].conf.ctx = addr[i].opt.ctx;
#if (NT_STREAM_SSL)
        addrs[i].conf.ssl = addr[i].opt.ssl;
#endif
        addrs[i].conf.proxy_protocol = addr[i].opt.proxy_protocol;
        addrs[i].conf.addr_text = addr[i].opt.addr_text;

#if (NT_STREAM_SNI)
        addrs[i].conf.default_server = addr[i].default_server;

        if (addr[i].hash.buckets == NULL
            && (addr[i].wc_head == NULL
                || addr[i].wc_head->hash.buckets == NULL)
            && (addr[i].wc_tail == NULL
                || addr[i].wc_tail->hash.buckets == NULL)
            )
        {
            continue;
        }

        vn = nt_palloc(cf->pool, sizeof(nt_stream_virtual_names_t));
        if (vn == NULL) {
            return NT_ERROR;
        }

        addrs[i].conf.virtual_names = vn;

        vn->names.hash = addr[i].hash;
        vn->names.wc_head = addr[i].wc_head;
        vn->names.wc_tail = addr[i].wc_tail;
#endif
    }

    return NT_OK;
}


#if (NT_HAVE_INET6)

static nt_int_t
nt_stream_add_addrs6(nt_conf_t *cf, nt_stream_port_t *stport,
    nt_stream_conf_addr_t *addr)
{
    nt_uint_t              i;
    struct sockaddr_in6    *sin6;
    nt_stream_in6_addr_t  *addrs6;
#if (NT_STREAM_SNI)
    nt_stream_virtual_names_t  *vn;
#endif

    stport->addrs = nt_pcalloc(cf->pool,
                                stport->naddrs * sizeof(nt_stream_in6_addr_t));
    if (stport->addrs == NULL) {
        return NT_ERROR;
    }

    addrs6 = stport->addrs;

    for (i = 0; i < stport->naddrs; i++) {

        sin6 = (struct sockaddr_in6 *) addr[i].opt.sockaddr;
        addrs6[i].addr6 = sin6->sin6_addr;

        addrs6[i].conf.ctx = addr[i].opt.ctx;
#if (NT_STREAM_SSL)
        addrs6[i].conf.ssl = addr[i].opt.ssl;
#endif
        addrs6[i].conf.proxy_protocol = addr[i].opt.proxy_protocol;
        addrs6[i].conf.addr_text = addr[i].opt.addr_text;

#if (NT_STREAM_SNI)
        if (addr[i].hash.buckets == NULL
            && (addr[i].wc_head == NULL
                || addr[i].wc_head->hash.buckets == NULL)
            && (addr[i].wc_tail == NULL
                || addr[i].wc_tail->hash.buckets == NULL)
            )
        {
            continue;
        }

        vn = nt_palloc(cf->pool, sizeof(nt_stream_virtual_names_t));
        if (vn == NULL) {
            return NT_ERROR;
        }

        addrs6[i].conf.virtual_names = vn;

        vn->names.hash = addr[i].hash;
        vn->names.wc_head = addr[i].wc_head;
        vn->names.wc_tail = addr[i].wc_tail;
#endif
    }

    return NT_OK;
}

#endif


static nt_int_t
nt_stream_cmp_conf_addrs(const void *one, const void *two)
{
    nt_stream_conf_addr_t  *first, *second;

    first = (nt_stream_conf_addr_t *) one;
    second = (nt_stream_conf_addr_t *) two;

    if (first->opt.wildcard) {
        /* a wildcard must be the last resort, shift it to the end */
        return 1;
    }

    if (second->opt.wildcard) {
        /* a wildcard must be the last resort, shift it to the end */
        return -1;
    }

    if (first->opt.bind && !second->opt.bind) {
        /* shift explicit bind()ed addresses to the start */
        return -1;
    }

    if (!first->opt.bind && second->opt.bind) {
        /* shift explicit bind()ed addresses to the start */
        return 1;
    }

    /* do not sort by default */

    return 0;
}
