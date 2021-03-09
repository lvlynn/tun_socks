
/*
 * Copyright (C) Roman Arutyunyan
 * Copyright (C) Nginx, Inc.
 */


#include <nt_core.h>
#include <nt_stream.h>


typedef struct {
    nt_addr_t                      *addr;
    nt_stream_complex_value_t      *value;
#if (NT_HAVE_TRANSPARENT_PROXY)
    nt_uint_t                       transparent; /* unsigned  transparent:1; */
#endif
} nt_stream_upstream_local_t;


typedef struct {
    nt_msec_t                       connect_timeout;
    nt_msec_t                       timeout;
    nt_msec_t                       next_upstream_timeout;
    size_t                           buffer_size;
    nt_stream_complex_value_t      *upload_rate;
    nt_stream_complex_value_t      *download_rate;
    nt_uint_t                       requests;
    nt_uint_t                       responses;
    nt_uint_t                       next_upstream_tries;
    nt_flag_t                       next_upstream;
    nt_flag_t                       proxy_protocol;
    nt_stream_upstream_local_t     *local;
    nt_flag_t                       socket_keepalive;

#if (NT_STREAM_SSL)
    nt_flag_t                       ssl_enable;
    nt_flag_t                       ssl_session_reuse;
    nt_uint_t                       ssl_protocols;
    nt_str_t                        ssl_ciphers;
    nt_stream_complex_value_t      *ssl_name;
    nt_flag_t                       ssl_server_name;

    nt_flag_t                       ssl_verify;
    nt_uint_t                       ssl_verify_depth;
    nt_str_t                        ssl_trusted_certificate;
    nt_str_t                        ssl_crl;
    nt_str_t                        ssl_certificate;
    nt_str_t                        ssl_certificate_key;
    nt_array_t                     *ssl_passwords;

    nt_ssl_t                       *ssl;
#endif

    nt_stream_upstream_srv_conf_t  *upstream;
    nt_stream_complex_value_t      *upstream_value;
} nt_stream_proxy_srv_conf_t;


static void nt_stream_proxy_handler(nt_stream_session_t *s);
static nt_int_t nt_stream_proxy_eval(nt_stream_session_t *s,
    nt_stream_proxy_srv_conf_t *pscf);
static nt_int_t nt_stream_proxy_set_local(nt_stream_session_t *s,
    nt_stream_upstream_t *u, nt_stream_upstream_local_t *local);
static void nt_stream_proxy_connect(nt_stream_session_t *s);
static void nt_stream_proxy_init_upstream(nt_stream_session_t *s);
static void nt_stream_proxy_resolve_handler(nt_resolver_ctx_t *ctx);
static void nt_stream_proxy_upstream_handler(nt_event_t *ev);
static void nt_stream_proxy_downstream_handler(nt_event_t *ev);
static void nt_stream_proxy_process_connection(nt_event_t *ev,
    nt_uint_t from_upstream);
static void nt_stream_proxy_connect_handler(nt_event_t *ev);
static nt_int_t nt_stream_proxy_test_connect(nt_connection_t *c);
static void nt_stream_proxy_process(nt_stream_session_t *s,
    nt_uint_t from_upstream, nt_uint_t do_write);
static nt_int_t nt_stream_proxy_test_finalize(nt_stream_session_t *s,
    nt_uint_t from_upstream);
static void nt_stream_proxy_next_upstream(nt_stream_session_t *s);
static void nt_stream_proxy_finalize(nt_stream_session_t *s, nt_uint_t rc);
static u_char *nt_stream_proxy_log_error(nt_log_t *log, u_char *buf,
    size_t len);

static void *nt_stream_proxy_create_srv_conf(nt_conf_t *cf);
static char *nt_stream_proxy_merge_srv_conf(nt_conf_t *cf, void *parent,
    void *child);
static char *nt_stream_proxy_pass(nt_conf_t *cf, nt_command_t *cmd,
    void *conf);
static char *nt_stream_proxy_bind(nt_conf_t *cf, nt_command_t *cmd,
    void *conf);

#if (NT_STREAM_SSL)

static nt_int_t nt_stream_proxy_send_proxy_protocol(nt_stream_session_t *s);
static char *nt_stream_proxy_ssl_password_file(nt_conf_t *cf,
    nt_command_t *cmd, void *conf);
static void nt_stream_proxy_ssl_init_connection(nt_stream_session_t *s);
static void nt_stream_proxy_ssl_handshake(nt_connection_t *pc);
static void nt_stream_proxy_ssl_save_session(nt_connection_t *c);
static nt_int_t nt_stream_proxy_ssl_name(nt_stream_session_t *s);
static nt_int_t nt_stream_proxy_set_ssl(nt_conf_t *cf,
    nt_stream_proxy_srv_conf_t *pscf);


static nt_conf_bitmask_t  nt_stream_proxy_ssl_protocols[] = {
    { nt_string("SSLv2"), NT_SSL_SSLv2 },
    { nt_string("SSLv3"), NT_SSL_SSLv3 },
    { nt_string("TLSv1"), NT_SSL_TLSv1 },
    { nt_string("TLSv1.1"), NT_SSL_TLSv1_1 },
    { nt_string("TLSv1.2"), NT_SSL_TLSv1_2 },
    { nt_string("TLSv1.3"), NT_SSL_TLSv1_3 },
    { nt_null_string, 0 }
};

#endif


static nt_conf_deprecated_t  nt_conf_deprecated_proxy_downstream_buffer = {
    nt_conf_deprecated, "proxy_downstream_buffer", "proxy_buffer_size"
};

static nt_conf_deprecated_t  nt_conf_deprecated_proxy_upstream_buffer = {
    nt_conf_deprecated, "proxy_upstream_buffer", "proxy_buffer_size"
};


static nt_command_t  nt_stream_proxy_commands[] = {

    { nt_string("proxy_pass"),
      NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_stream_proxy_pass,
      NT_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { nt_string("proxy_bind"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE12,
      nt_stream_proxy_bind,
      NT_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { nt_string("proxy_socket_keepalive"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_FLAG,
      nt_conf_set_flag_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, socket_keepalive),
      NULL },

    { nt_string("proxy_connect_timeout"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_msec_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, connect_timeout),
      NULL },

    { nt_string("proxy_timeout"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_msec_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, timeout),
      NULL },

    { nt_string("proxy_buffer_size"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_size_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, buffer_size),
      NULL },

    { nt_string("proxy_downstream_buffer"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_size_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, buffer_size),
      &nt_conf_deprecated_proxy_downstream_buffer },

    { nt_string("proxy_upstream_buffer"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_size_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, buffer_size),
      &nt_conf_deprecated_proxy_upstream_buffer },

    { nt_string("proxy_upload_rate"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_stream_set_complex_value_size_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, upload_rate),
      NULL },

    { nt_string("proxy_download_rate"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_stream_set_complex_value_size_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, download_rate),
      NULL },

    { nt_string("proxy_requests"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_num_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, requests),
      NULL },

    { nt_string("proxy_responses"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_num_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, responses),
      NULL },

    { nt_string("proxy_next_upstream"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_FLAG,
      nt_conf_set_flag_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, next_upstream),
      NULL },

    { nt_string("proxy_next_upstream_tries"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_num_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, next_upstream_tries),
      NULL },

    { nt_string("proxy_next_upstream_timeout"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_msec_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, next_upstream_timeout),
      NULL },

    { nt_string("proxy_protocol"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_FLAG,
      nt_conf_set_flag_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, proxy_protocol),
      NULL },

#if (NT_STREAM_SSL)

    { nt_string("proxy_ssl"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_FLAG,
      nt_conf_set_flag_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, ssl_enable),
      NULL },

    { nt_string("proxy_ssl_session_reuse"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_FLAG,
      nt_conf_set_flag_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, ssl_session_reuse),
      NULL },

    { nt_string("proxy_ssl_protocols"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_1MORE,
      nt_conf_set_bitmask_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, ssl_protocols),
      &nt_stream_proxy_ssl_protocols },

    { nt_string("proxy_ssl_ciphers"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_str_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, ssl_ciphers),
      NULL },

    { nt_string("proxy_ssl_name"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_stream_set_complex_value_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, ssl_name),
      NULL },

    { nt_string("proxy_ssl_server_name"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_FLAG,
      nt_conf_set_flag_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, ssl_server_name),
      NULL },

    { nt_string("proxy_ssl_verify"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_FLAG,
      nt_conf_set_flag_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, ssl_verify),
      NULL },

    { nt_string("proxy_ssl_verify_depth"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_num_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, ssl_verify_depth),
      NULL },

    { nt_string("proxy_ssl_trusted_certificate"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_str_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, ssl_trusted_certificate),
      NULL },

    { nt_string("proxy_ssl_crl"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_str_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, ssl_crl),
      NULL },

    { nt_string("proxy_ssl_certificate"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_str_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, ssl_certificate),
      NULL },

    { nt_string("proxy_ssl_certificate_key"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_conf_set_str_slot,
      NT_STREAM_SRV_CONF_OFFSET,
      offsetof(nt_stream_proxy_srv_conf_t, ssl_certificate_key),
      NULL },

    { nt_string("proxy_ssl_password_file"),
      NT_STREAM_MAIN_CONF|NT_STREAM_SRV_CONF|NT_CONF_TAKE1,
      nt_stream_proxy_ssl_password_file,
      NT_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

#endif

      nt_null_command
};


static nt_stream_module_t  nt_stream_proxy_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    nt_stream_proxy_create_srv_conf,      /* create server configuration */
    nt_stream_proxy_merge_srv_conf        /* merge server configuration */
};


nt_module_t  nt_stream_proxy_module = {
    NT_MODULE_V1,
    &nt_stream_proxy_module_ctx,          /* module context */
    nt_stream_proxy_commands,             /* module directives */
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


static void
nt_stream_proxy_handler(nt_stream_session_t *s)
{
    u_char                           *p;
    nt_str_t                        *host;
    nt_uint_t                        i;
    nt_connection_t                 *c;
    nt_resolver_ctx_t               *ctx, temp;
    nt_stream_upstream_t            *u;
    nt_stream_core_srv_conf_t       *cscf;
    nt_stream_proxy_srv_conf_t      *pscf;
    nt_stream_upstream_srv_conf_t   *uscf, **uscfp;
    nt_stream_upstream_main_conf_t  *umcf;

    c = s->connection;

    pscf = nt_stream_get_module_srv_conf(s, nt_stream_proxy_module);

    nt_log_debug0(NT_LOG_DEBUG_STREAM, c->log, 0,
                   "proxy connection handler");

    u = nt_pcalloc(c->pool, sizeof(nt_stream_upstream_t));
    if (u == NULL) {
        nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    s->upstream = u;

    s->log_handler = nt_stream_proxy_log_error;

    u->requests = 1;

    u->peer.log = c->log;
    u->peer.log_error = NT_ERROR_ERR;

    if (nt_stream_proxy_set_local(s, u, pscf->local) != NT_OK) {
        nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    if (pscf->socket_keepalive) {
        u->peer.so_keepalive = 1;
    }

    u->peer.type = c->type;
    u->start_sec = nt_time();

    c->write->handler = nt_stream_proxy_downstream_handler;
    c->read->handler = nt_stream_proxy_downstream_handler;

    s->upstream_states = nt_array_create(c->pool, 1,
                                          sizeof(nt_stream_upstream_state_t));
    if (s->upstream_states == NULL) {
        nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    p = nt_pnalloc(c->pool, pscf->buffer_size);
    if (p == NULL) {
        nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    u->downstream_buf.start = p;
    u->downstream_buf.end = p + pscf->buffer_size;
    u->downstream_buf.pos = p;
    u->downstream_buf.last = p;

    if (c->read->ready) {
        nt_post_event(c->read, &nt_posted_events);
    }

    if (pscf->upstream_value) {
        if (nt_stream_proxy_eval(s, pscf) != NT_OK) {
            nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }
    }

    if (u->resolved == NULL) {

        uscf = pscf->upstream;

    } else {

#if (NT_STREAM_SSL)
        u->ssl_name = u->resolved->host;
#endif

        host = &u->resolved->host;

        umcf = nt_stream_get_module_main_conf(s, nt_stream_upstream_module);

        uscfp = umcf->upstreams.elts;

        for (i = 0; i < umcf->upstreams.nelts; i++) {

            uscf = uscfp[i];

            if (uscf->host.len == host->len
                && ((uscf->port == 0 && u->resolved->no_port)
                     || uscf->port == u->resolved->port)
                && nt_strncasecmp(uscf->host.data, host->data, host->len) == 0)
            {
                goto found;
            }
        }

        if (u->resolved->sockaddr) {

            if (u->resolved->port == 0
                && u->resolved->sockaddr->sa_family != AF_UNIX)
            {
                nt_log_error(NT_LOG_ERR, c->log, 0,
                              "no port in upstream \"%V\"", host);
                nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
                return;
            }

            if (nt_stream_upstream_create_round_robin_peer(s, u->resolved)
                != NT_OK)
            {
                nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
                return;
            }

            nt_stream_proxy_connect(s);

            return;
        }

        if (u->resolved->port == 0) {
            nt_log_error(NT_LOG_ERR, c->log, 0,
                          "no port in upstream \"%V\"", host);
            nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        temp.name = *host;

        cscf = nt_stream_get_module_srv_conf(s, nt_stream_core_module);

        ctx = nt_resolve_start(cscf->resolver, &temp);
        if (ctx == NULL) {
            nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        if (ctx == NT_NO_RESOLVER) {
            nt_log_error(NT_LOG_ERR, c->log, 0,
                          "no resolver defined to resolve %V", host);
            nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        ctx->name = *host;
        ctx->handler = nt_stream_proxy_resolve_handler;
        ctx->data = s;
        ctx->timeout = cscf->resolver_timeout;

        u->resolved->ctx = ctx;

        if (nt_resolve_name(ctx) != NT_OK) {
            u->resolved->ctx = NULL;
            nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        return;
    }

found:

    if (uscf == NULL) {
        nt_log_error(NT_LOG_ALERT, c->log, 0, "no upstream configuration");
        nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    u->upstream = uscf;

#if (NT_STREAM_SSL)
    u->ssl_name = uscf->host;
#endif

    if (uscf->peer.init(s, uscf) != NT_OK) {
        nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    u->peer.start_time = nt_current_msec;

    if (pscf->next_upstream_tries
        && u->peer.tries > pscf->next_upstream_tries)
    {
        u->peer.tries = pscf->next_upstream_tries;
    }

    nt_stream_proxy_connect(s);
}


static nt_int_t
nt_stream_proxy_eval(nt_stream_session_t *s,
    nt_stream_proxy_srv_conf_t *pscf)
{
    nt_str_t               host;
    nt_url_t               url;
    nt_stream_upstream_t  *u;

    if (nt_stream_complex_value(s, pscf->upstream_value, &host) != NT_OK) {
        return NT_ERROR;
    }

    nt_memzero(&url, sizeof(nt_url_t));

    url.url = host;
    url.no_resolve = 1;

    if (nt_parse_url(s->connection->pool, &url) != NT_OK) {
        if (url.err) {
            nt_log_error(NT_LOG_ERR, s->connection->log, 0,
                          "%s in upstream \"%V\"", url.err, &url.url);
        }

        return NT_ERROR;
    }

    u = s->upstream;

    u->resolved = nt_pcalloc(s->connection->pool,
                              sizeof(nt_stream_upstream_resolved_t));
    if (u->resolved == NULL) {
        return NT_ERROR;
    }

    if (url.addrs) {
        u->resolved->sockaddr = url.addrs[0].sockaddr;
        u->resolved->socklen = url.addrs[0].socklen;
        u->resolved->name = url.addrs[0].name;
        u->resolved->naddrs = 1;
    }

    u->resolved->host = url.host;
    u->resolved->port = url.port;
    u->resolved->no_port = url.no_port;

    return NT_OK;
}


static nt_int_t
nt_stream_proxy_set_local(nt_stream_session_t *s, nt_stream_upstream_t *u,
    nt_stream_upstream_local_t *local)
{
    nt_int_t    rc;
    nt_str_t    val;
    nt_addr_t  *addr;

    if (local == NULL) {
        u->peer.local = NULL;
        return NT_OK;
    }

#if (NT_HAVE_TRANSPARENT_PROXY)
    u->peer.transparent = local->transparent;
#endif

    if (local->value == NULL) {
        u->peer.local = local->addr;
        return NT_OK;
    }

    if (nt_stream_complex_value(s, local->value, &val) != NT_OK) {
        return NT_ERROR;
    }

    if (val.len == 0) {
        return NT_OK;
    }

    addr = nt_palloc(s->connection->pool, sizeof(nt_addr_t));
    if (addr == NULL) {
        return NT_ERROR;
    }

    rc = nt_parse_addr_port(s->connection->pool, addr, val.data, val.len);
    if (rc == NT_ERROR) {
        return NT_ERROR;
    }

    if (rc != NT_OK) {
        nt_log_error(NT_LOG_ERR, s->connection->log, 0,
                      "invalid local address \"%V\"", &val);
        return NT_OK;
    }

    addr->name = val;
    u->peer.local = addr;

    return NT_OK;
}


static void
nt_stream_proxy_connect(nt_stream_session_t *s)
{
    nt_int_t                     rc;
    nt_connection_t             *c, *pc;
    nt_stream_upstream_t        *u;
    nt_stream_proxy_srv_conf_t  *pscf;

    c = s->connection;

    c->log->action = "connecting to upstream";

    pscf = nt_stream_get_module_srv_conf(s, nt_stream_proxy_module);

    u = s->upstream;

    u->connected = 0;
    u->proxy_protocol = pscf->proxy_protocol;

    if (u->state) {
        u->state->response_time = nt_current_msec - u->start_time;
    }

    u->state = nt_array_push(s->upstream_states);
    if (u->state == NULL) {
        nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    nt_memzero(u->state, sizeof(nt_stream_upstream_state_t));

    u->start_time = nt_current_msec;

    u->state->connect_time = (nt_msec_t) -1;
    u->state->first_byte_time = (nt_msec_t) -1;
    u->state->response_time = (nt_msec_t) -1;

    rc = nt_event_connect_peer(&u->peer);

    nt_log_debug1(NT_LOG_DEBUG_STREAM, c->log, 0, "proxy connect: %i", rc);

    if (rc == NT_ERROR) {
        nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    u->state->peer = u->peer.name;

    if (rc == NT_BUSY) {
        nt_log_error(NT_LOG_ERR, c->log, 0, "no live upstreams");
        nt_stream_proxy_finalize(s, NT_STREAM_BAD_GATEWAY);
        return;
    }

    if (rc == NT_DECLINED) {
        nt_stream_proxy_next_upstream(s);
        return;
    }

    /* rc == NT_OK || rc == NT_AGAIN || rc == NT_DONE */

    pc = u->peer.connection;

    pc->data = s;
    pc->log = c->log;
    pc->pool = c->pool;
    pc->read->log = c->log;
    pc->write->log = c->log;

    if (rc != NT_AGAIN) {
        nt_stream_proxy_init_upstream(s);
        return;
    }

    pc->read->handler = nt_stream_proxy_connect_handler;
    pc->write->handler = nt_stream_proxy_connect_handler;

    nt_add_timer(pc->write, pscf->connect_timeout);
}


static void
nt_stream_proxy_init_upstream(nt_stream_session_t *s)
{
    u_char                       *p;
    nt_chain_t                  *cl;
    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_upstream_t        *u;
    nt_stream_core_srv_conf_t   *cscf;
    nt_stream_proxy_srv_conf_t  *pscf;

    u = s->upstream;
    pc = u->peer.connection;

    cscf = nt_stream_get_module_srv_conf(s, nt_stream_core_module);

    if (pc->type == SOCK_STREAM
        && cscf->tcp_nodelay
        && nt_tcp_nodelay(pc) != NT_OK)
    {
        nt_stream_proxy_next_upstream(s);
        return;
    }

    pscf = nt_stream_get_module_srv_conf(s, nt_stream_proxy_module);

#if (NT_STREAM_SSL)

    if (pc->type == SOCK_STREAM && pscf->ssl) {

        if (u->proxy_protocol) {
            if (nt_stream_proxy_send_proxy_protocol(s) != NT_OK) {
                return;
            }

            u->proxy_protocol = 0;
        }

        if (pc->ssl == NULL) {
            nt_stream_proxy_ssl_init_connection(s);
            return;
        }
    }

#endif

    c = s->connection;

    if (c->log->log_level >= NT_LOG_INFO) {
        nt_str_t  str;
        u_char     addr[NT_SOCKADDR_STRLEN];

        str.len = NT_SOCKADDR_STRLEN;
        str.data = addr;

        if (nt_connection_local_sockaddr(pc, &str, 1) == NT_OK) {
            handler = c->log->handler;
            c->log->handler = NULL;

            nt_log_error(NT_LOG_INFO, c->log, 0,
                          "%sproxy %V connected to %V",
                          pc->type == SOCK_DGRAM ? "udp " : "",
                          &str, u->peer.name);

            c->log->handler = handler;
        }
    }

    u->state->connect_time = nt_current_msec - u->start_time;

    if (u->peer.notify) {
        u->peer.notify(&u->peer, u->peer.data,
                       NT_STREAM_UPSTREAM_NOTIFY_CONNECT);
    }

    if (u->upstream_buf.start == NULL) {
        p = nt_pnalloc(c->pool, pscf->buffer_size);
        if (p == NULL) {
            nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        u->upstream_buf.start = p;
        u->upstream_buf.end = p + pscf->buffer_size;
        u->upstream_buf.pos = p;
        u->upstream_buf.last = p;
    }

    if (c->buffer && c->buffer->pos < c->buffer->last) {
        nt_log_debug1(NT_LOG_DEBUG_STREAM, c->log, 0,
                       "stream proxy add preread buffer: %uz",
                       c->buffer->last - c->buffer->pos);

        cl = nt_chain_get_free_buf(c->pool, &u->free);
        if (cl == NULL) {
            nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        *cl->buf = *c->buffer;

        cl->buf->tag = (nt_buf_tag_t) &nt_stream_proxy_module;
        cl->buf->flush = 1;

        cl->next = u->upstream_out;
        u->upstream_out = cl;
    }

    if (u->proxy_protocol) {
        nt_log_debug0(NT_LOG_DEBUG_STREAM, c->log, 0,
                       "stream proxy add PROXY protocol header");

        cl = nt_chain_get_free_buf(c->pool, &u->free);
        if (cl == NULL) {
            nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        p = nt_pnalloc(c->pool, NT_PROXY_PROTOCOL_MAX_HEADER);
        if (p == NULL) {
            nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        cl->buf->pos = p;

        p = nt_proxy_protocol_write(c, p, p + NT_PROXY_PROTOCOL_MAX_HEADER);
        if (p == NULL) {
            nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        cl->buf->last = p;
        cl->buf->temporary = 1;
        cl->buf->flush = 0;
        cl->buf->last_buf = 0;
        cl->buf->tag = (nt_buf_tag_t) &nt_stream_proxy_module;

        cl->next = u->upstream_out;
        u->upstream_out = cl;

        u->proxy_protocol = 0;
    }

    u->upload_rate = nt_stream_complex_value_size(s, pscf->upload_rate, 0);
    u->download_rate = nt_stream_complex_value_size(s, pscf->download_rate, 0);

    u->connected = 1;

    pc->read->handler = nt_stream_proxy_upstream_handler;
    pc->write->handler = nt_stream_proxy_upstream_handler;

    if (pc->read->ready) {
        nt_post_event(pc->read, &nt_posted_events);
    }

    nt_stream_proxy_process(s, 0, 1);
}


#if (NT_STREAM_SSL)

static nt_int_t
nt_stream_proxy_send_proxy_protocol(nt_stream_session_t *s)
{
    u_char                       *p;
    ssize_t                       n, size;
    nt_connection_t             *c, *pc;
    nt_stream_upstream_t        *u;
    nt_stream_proxy_srv_conf_t  *pscf;
    u_char                        buf[NT_PROXY_PROTOCOL_MAX_HEADER];

    c = s->connection;

    nt_log_debug0(NT_LOG_DEBUG_STREAM, c->log, 0,
                   "stream proxy send PROXY protocol header");

    p = nt_proxy_protocol_write(c, buf, buf + NT_PROXY_PROTOCOL_MAX_HEADER);
    if (p == NULL) {
        nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return NT_ERROR;
    }

    u = s->upstream;

    pc = u->peer.connection;

    size = p - buf;

    n = pc->send(pc, buf, size);

    if (n == NT_AGAIN) {
        if (nt_handle_write_event(pc->write, 0) != NT_OK) {
            nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
            return NT_ERROR;
        }

        pscf = nt_stream_get_module_srv_conf(s, nt_stream_proxy_module);

        nt_add_timer(pc->write, pscf->timeout);

        pc->write->handler = nt_stream_proxy_connect_handler;

        return NT_AGAIN;
    }

    if (n == NT_ERROR) {
        nt_stream_proxy_finalize(s, NT_STREAM_OK);
        return NT_ERROR;
    }

    if (n != size) {

        /*
         * PROXY protocol specification:
         * The sender must always ensure that the header
         * is sent at once, so that the transport layer
         * maintains atomicity along the path to the receiver.
         */

        nt_log_error(NT_LOG_ERR, c->log, 0,
                      "could not send PROXY protocol header at once");

        nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);

        return NT_ERROR;
    }

    return NT_OK;
}


static char *
nt_stream_proxy_ssl_password_file(nt_conf_t *cf, nt_command_t *cmd,
    void *conf)
{
    nt_stream_proxy_srv_conf_t *pscf = conf;

    nt_str_t  *value;

    if (pscf->ssl_passwords != NT_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    value = cf->args->elts;

    pscf->ssl_passwords = nt_ssl_read_password_file(cf, &value[1]);

    if (pscf->ssl_passwords == NULL) {
        return NT_CONF_ERROR;
    }

    return NT_CONF_OK;
}


static void
nt_stream_proxy_ssl_init_connection(nt_stream_session_t *s)
{
    nt_int_t                     rc;
    nt_connection_t             *pc;
    nt_stream_upstream_t        *u;
    nt_stream_proxy_srv_conf_t  *pscf;

    u = s->upstream;

    pc = u->peer.connection;

    pscf = nt_stream_get_module_srv_conf(s, nt_stream_proxy_module);

    if (nt_ssl_create_connection(pscf->ssl, pc, NT_SSL_BUFFER|NT_SSL_CLIENT)
        != NT_OK)
    {
        nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    if (pscf->ssl_server_name || pscf->ssl_verify) {
        if (nt_stream_proxy_ssl_name(s) != NT_OK) {
            nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }
    }

    if (pscf->ssl_session_reuse) {
        pc->ssl->save_session = nt_stream_proxy_ssl_save_session;

        if (u->peer.set_session(&u->peer, u->peer.data) != NT_OK) {
            nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }
    }

    s->connection->log->action = "SSL handshaking to upstream";

    rc = nt_ssl_handshake(pc);

    if (rc == NT_AGAIN) {

        if (!pc->write->timer_set) {
            nt_add_timer(pc->write, pscf->connect_timeout);
        }

        pc->ssl->handler = nt_stream_proxy_ssl_handshake;
        return;
    }

    nt_stream_proxy_ssl_handshake(pc);
}


static void
nt_stream_proxy_ssl_handshake(nt_connection_t *pc)
{
    long                          rc;
    nt_stream_session_t         *s;
    nt_stream_upstream_t        *u;
    nt_stream_proxy_srv_conf_t  *pscf;

    s = pc->data;

    pscf = nt_stream_get_module_srv_conf(s, nt_stream_proxy_module);

    if (pc->ssl->handshaked) {

        if (pscf->ssl_verify) {
            rc = SSL_get_verify_result(pc->ssl->connection);

            if (rc != X509_V_OK) {
                nt_log_error(NT_LOG_ERR, pc->log, 0,
                              "upstream SSL certificate verify error: (%l:%s)",
                              rc, X509_verify_cert_error_string(rc));
                goto failed;
            }

            u = s->upstream;

            if (nt_ssl_check_host(pc, &u->ssl_name) != NT_OK) {
                nt_log_error(NT_LOG_ERR, pc->log, 0,
                              "upstream SSL certificate does not match \"%V\"",
                              &u->ssl_name);
                goto failed;
            }
        }

        if (pc->write->timer_set) {
            nt_del_timer(pc->write);
        }

        nt_stream_proxy_init_upstream(s);

        return;
    }

failed:

    nt_stream_proxy_next_upstream(s);
}


static void
nt_stream_proxy_ssl_save_session(nt_connection_t *c)
{
    nt_stream_session_t   *s;
    nt_stream_upstream_t  *u;

    s = c->data;
    u = s->upstream;

    u->peer.save_session(&u->peer, u->peer.data);
}


static nt_int_t
nt_stream_proxy_ssl_name(nt_stream_session_t *s)
{
    u_char                       *p, *last;
    nt_str_t                     name;
    nt_stream_upstream_t        *u;
    nt_stream_proxy_srv_conf_t  *pscf;

    pscf = nt_stream_get_module_srv_conf(s, nt_stream_proxy_module);

    u = s->upstream;

    if (pscf->ssl_name) {
        if (nt_stream_complex_value(s, pscf->ssl_name, &name) != NT_OK) {
            return NT_ERROR;
        }

    } else {
        name = u->ssl_name;
    }

    if (name.len == 0) {
        goto done;
    }

    /*
     * ssl name here may contain port, strip it for compatibility
     * with the http module
     */

    p = name.data;
    last = name.data + name.len;

    if (*p == '[') {
        p = nt_strlchr(p, last, ']');

        if (p == NULL) {
            p = name.data;
        }
    }

    p = nt_strlchr(p, last, ':');

    if (p != NULL) {
        name.len = p - name.data;
    }

    if (!pscf->ssl_server_name) {
        goto done;
    }

#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME

    /* as per RFC 6066, literal IPv4 and IPv6 addresses are not permitted */

    if (name.len == 0 || *name.data == '[') {
        goto done;
    }

    if (nt_inet_addr(name.data, name.len) != INADDR_NONE) {
        goto done;
    }

    /*
     * SSL_set_tlsext_host_name() needs a null-terminated string,
     * hence we explicitly null-terminate name here
     */

    p = nt_pnalloc(s->connection->pool, name.len + 1);
    if (p == NULL) {
        return NT_ERROR;
    }

    (void) nt_cpystrn(p, name.data, name.len + 1);

    name.data = p;

    nt_log_debug1(NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                   "upstream SSL server name: \"%s\"", name.data);

    if (SSL_set_tlsext_host_name(u->peer.connection->ssl->connection,
                                 (char *) name.data)
        == 0)
    {
        nt_ssl_error(NT_LOG_ERR, s->connection->log, 0,
                      "SSL_set_tlsext_host_name(\"%s\") failed", name.data);
        return NT_ERROR;
    }

#endif

done:

    u->ssl_name = name;

    return NT_OK;
}

#endif


static void
nt_stream_proxy_downstream_handler(nt_event_t *ev)
{
    nt_stream_proxy_process_connection(ev, ev->write);
}


static void
nt_stream_proxy_resolve_handler(nt_resolver_ctx_t *ctx)
{
    nt_stream_session_t            *s;
    nt_stream_upstream_t           *u;
    nt_stream_proxy_srv_conf_t     *pscf;
    nt_stream_upstream_resolved_t  *ur;

    s = ctx->data;

    u = s->upstream;
    ur = u->resolved;

    nt_log_debug0(NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                   "stream upstream resolve");

    if (ctx->state) {
        nt_log_error(NT_LOG_ERR, s->connection->log, 0,
                      "%V could not be resolved (%i: %s)",
                      &ctx->name, ctx->state,
                      nt_resolver_strerror(ctx->state));

        nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ur->naddrs = ctx->naddrs;
    ur->addrs = ctx->addrs;

#if (NT_DEBUG)
    {
    u_char      text[NT_SOCKADDR_STRLEN];
    nt_str_t   addr;
    nt_uint_t  i;

    addr.data = text;

    for (i = 0; i < ctx->naddrs; i++) {
        addr.len = nt_sock_ntop(ur->addrs[i].sockaddr, ur->addrs[i].socklen,
                                 text, NT_SOCKADDR_STRLEN, 0);

        nt_log_debug1(NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                       "name was resolved to %V", &addr);
    }
    }
#endif

    if (nt_stream_upstream_create_round_robin_peer(s, ur) != NT_OK) {
        nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    nt_resolve_name_done(ctx);
    ur->ctx = NULL;

    u->peer.start_time = nt_current_msec;

    pscf = nt_stream_get_module_srv_conf(s, nt_stream_proxy_module);

    if (pscf->next_upstream_tries
        && u->peer.tries > pscf->next_upstream_tries)
    {
        u->peer.tries = pscf->next_upstream_tries;
    }

    nt_stream_proxy_connect(s);
}


static void
nt_stream_proxy_upstream_handler(nt_event_t *ev)
{
    nt_stream_proxy_process_connection(ev, !ev->write);
}


static void
nt_stream_proxy_process_connection(nt_event_t *ev, nt_uint_t from_upstream)
{
    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_session_t         *s;
    nt_stream_upstream_t        *u;
    nt_stream_proxy_srv_conf_t  *pscf;

    c = ev->data;
    s = c->data;
    u = s->upstream;

    if (c->close) {
        nt_log_error(NT_LOG_INFO, c->log, 0, "shutdown timeout");
        nt_stream_proxy_finalize(s, NT_STREAM_OK);
        return;
    }

    c = s->connection;
    pc = u->peer.connection;

    pscf = nt_stream_get_module_srv_conf(s, nt_stream_proxy_module);

    if (ev->timedout) {
        ev->timedout = 0;

        if (ev->delayed) {
            ev->delayed = 0;

            if (!ev->ready) {
                if (nt_handle_read_event(ev, 0) != NT_OK) {
                    nt_stream_proxy_finalize(s,
                                              NT_STREAM_INTERNAL_SERVER_ERROR);
                    return;
                }

                if (u->connected && !c->read->delayed && !pc->read->delayed) {
                    nt_add_timer(c->write, pscf->timeout);
                }

                return;
            }

        } else {
            if (s->connection->type == SOCK_DGRAM) {

                if (pscf->responses == NT_MAX_INT32_VALUE
                    || (u->responses >= pscf->responses * u->requests))
                {

                    /*
                     * successfully terminate timed out UDP session
                     * if expected number of responses was received
                     */

                    handler = c->log->handler;
                    c->log->handler = NULL;

                    nt_log_error(NT_LOG_INFO, c->log, 0,
                                  "udp timed out"
                                  ", packets from/to client:%ui/%ui"
                                  ", bytes from/to client:%O/%O"
                                  ", bytes from/to upstream:%O/%O",
                                  u->requests, u->responses,
                                  s->received, c->sent, u->received,
                                  pc ? pc->sent : 0);

                    c->log->handler = handler;

                    nt_stream_proxy_finalize(s, NT_STREAM_OK);
                    return;
                }

                nt_connection_error(pc, NT_ETIMEDOUT, "upstream timed out");

                pc->read->error = 1;

                nt_stream_proxy_finalize(s, NT_STREAM_BAD_GATEWAY);

                return;
            }

            nt_connection_error(c, NT_ETIMEDOUT, "connection timed out");

            nt_stream_proxy_finalize(s, NT_STREAM_OK);

            return;
        }

    } else if (ev->delayed) {

        nt_log_debug0(NT_LOG_DEBUG_STREAM, c->log, 0,
                       "stream connection delayed");

        if (nt_handle_read_event(ev, 0) != NT_OK) {
            nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        }

        return;
    }

    if (from_upstream && !u->connected) {
        return;
    }

    nt_stream_proxy_process(s, from_upstream, ev->write);
}


static void
nt_stream_proxy_connect_handler(nt_event_t *ev)
{
    nt_connection_t      *c;
    nt_stream_session_t  *s;

    c = ev->data;
    s = c->data;

    if (ev->timedout) {
        nt_log_error(NT_LOG_ERR, c->log, NT_ETIMEDOUT, "upstream timed out");
        nt_stream_proxy_next_upstream(s);
        return;
    }

    nt_del_timer(c->write);

    nt_log_debug0(NT_LOG_DEBUG_STREAM, c->log, 0,
                   "stream proxy connect upstream");

    if (nt_stream_proxy_test_connect(c) != NT_OK) {
        nt_stream_proxy_next_upstream(s);
        return;
    }

    nt_stream_proxy_init_upstream(s);
}


static nt_int_t
nt_stream_proxy_test_connect(nt_connection_t *c)
{
    int        err;
    socklen_t  len;

#if (NT_HAVE_KQUEUE)

    if (nt_event_flags & NT_USE_KQUEUE_EVENT)  {
        err = c->write->kq_errno ? c->write->kq_errno : c->read->kq_errno;

        if (err) {
            (void) nt_connection_error(c, err,
                                    "kevent() reported that connect() failed");
            return NT_ERROR;
        }

    } else
#endif
    {
        err = 0;
        len = sizeof(int);

        /*
         * BSDs and Linux return 0 and set a pending error in err
         * Solaris returns -1 and sets errno
         */

        if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len)
            == -1)
        {
            err = nt_socket_errno;
        }

        if (err) {
            (void) nt_connection_error(c, err, "connect() failed");
            return NT_ERROR;
        }
    }

    return NT_OK;
}


static void
nt_stream_proxy_process(nt_stream_session_t *s, nt_uint_t from_upstream,
    nt_uint_t do_write)
{
    char                         *recv_action, *send_action;
    off_t                        *received, limit;
    size_t                        size, limit_rate;
    ssize_t                       n;
    nt_buf_t                    *b;
    nt_int_t                     rc;
    nt_uint_t                    flags, *packets;
    nt_msec_t                    delay;
    nt_chain_t                  *cl, **ll, **out, **busy;
    nt_connection_t             *c, *pc, *src, *dst;
    nt_log_handler_pt            handler;
    nt_stream_upstream_t        *u;
    nt_stream_proxy_srv_conf_t  *pscf;

    u = s->upstream;

    c = s->connection;
    pc = u->connected ? u->peer.connection : NULL;

    if (c->type == SOCK_DGRAM && (nt_terminate || nt_exiting)) {

        /* socket is already closed on worker shutdown */

        handler = c->log->handler;
        c->log->handler = NULL;

        nt_log_error(NT_LOG_INFO, c->log, 0, "disconnected on shutdown");

        c->log->handler = handler;

        nt_stream_proxy_finalize(s, NT_STREAM_OK);
        return;
    }

    pscf = nt_stream_get_module_srv_conf(s, nt_stream_proxy_module);

    if (from_upstream) {
        src = pc;
        dst = c;
        b = &u->upstream_buf;
        limit_rate = u->download_rate;
        received = &u->received;
        packets = &u->responses;
        out = &u->downstream_out;
        busy = &u->downstream_busy;
        recv_action = "proxying and reading from upstream";
        send_action = "proxying and sending to client";

    } else {
        src = c;
        dst = pc;
        b = &u->downstream_buf;
        limit_rate = u->upload_rate;
        received = &s->received;
        packets = &u->requests;
        out = &u->upstream_out;
        busy = &u->upstream_busy;
        recv_action = "proxying and reading from client";
        send_action = "proxying and sending to upstream";
    }

    for ( ;; ) {

        if (do_write && dst) {

            if (*out || *busy || dst->buffered) {
                c->log->action = send_action;

                rc = nt_stream_top_filter(s, *out, from_upstream);

                if (rc == NT_ERROR) {
                    nt_stream_proxy_finalize(s, NT_STREAM_OK);
                    return;
                }

                nt_chain_update_chains(c->pool, &u->free, busy, out,
                                      (nt_buf_tag_t) &nt_stream_proxy_module);

                if (*busy == NULL) {
                    b->pos = b->start;
                    b->last = b->start;
                }
            }
        }

        size = b->end - b->last;

        if (size && src->read->ready && !src->read->delayed
            && !src->read->error)
        {
            if (limit_rate) {
                limit = (off_t) limit_rate * (nt_time() - u->start_sec + 1)
                        - *received;

                if (limit <= 0) {
                    src->read->delayed = 1;
                    delay = (nt_msec_t) (- limit * 1000 / limit_rate + 1);
                    nt_add_timer(src->read, delay);
                    break;
                }

                if (c->type == SOCK_STREAM && (off_t) size > limit) {
                    size = (size_t) limit;
                }
            }

            c->log->action = recv_action;

            n = src->recv(src, b->last, size);

            if (n == NT_AGAIN) {
                break;
            }

            if (n == NT_ERROR) {
                src->read->eof = 1;
                n = 0;
            }

            if (n >= 0) {
                if (limit_rate) {
                    delay = (nt_msec_t) (n * 1000 / limit_rate);

                    if (delay > 0) {
                        src->read->delayed = 1;
                        nt_add_timer(src->read, delay);
                    }
                }

                if (from_upstream) {
                    if (u->state->first_byte_time == (nt_msec_t) -1) {
                        u->state->first_byte_time = nt_current_msec
                                                    - u->start_time;
                    }
                }

                for (ll = out; *ll; ll = &(*ll)->next) { /* void */ }

                cl = nt_chain_get_free_buf(c->pool, &u->free);
                if (cl == NULL) {
                    nt_stream_proxy_finalize(s,
                                              NT_STREAM_INTERNAL_SERVER_ERROR);
                    return;
                }

                *ll = cl;

                cl->buf->pos = b->last;
                cl->buf->last = b->last + n;
                cl->buf->tag = (nt_buf_tag_t) &nt_stream_proxy_module;

                cl->buf->temporary = (n ? 1 : 0);
                cl->buf->last_buf = src->read->eof;
                cl->buf->flush = 1;

                (*packets)++;
                *received += n;
                b->last += n;
                do_write = 1;

                continue;
            }
        }

        break;
    }

    c->log->action = "proxying connection";

    if (nt_stream_proxy_test_finalize(s, from_upstream) == NT_OK) {
        return;
    }

    flags = src->read->eof ? NT_CLOSE_EVENT : 0;

    if (nt_handle_read_event(src->read, flags) != NT_OK) {
        nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    if (dst) {
        if (nt_handle_write_event(dst->write, 0) != NT_OK) {
            nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        if (!c->read->delayed && !pc->read->delayed) {
            nt_add_timer(c->write, pscf->timeout);

        } else if (c->write->timer_set) {
            nt_del_timer(c->write);
        }
    }
}


static nt_int_t
nt_stream_proxy_test_finalize(nt_stream_session_t *s,
    nt_uint_t from_upstream)
{
    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_upstream_t        *u;
    nt_stream_proxy_srv_conf_t  *pscf;

    pscf = nt_stream_get_module_srv_conf(s, nt_stream_proxy_module);

    c = s->connection;
    u = s->upstream;
    pc = u->connected ? u->peer.connection : NULL;

    if (c->type == SOCK_DGRAM) {

        if (pscf->requests && u->requests < pscf->requests) {
            return NT_DECLINED;
        }

        if (pscf->requests) {
            nt_delete_udp_connection(c);
        }

        if (pscf->responses == NT_MAX_INT32_VALUE
            || u->responses < pscf->responses * u->requests)
        {
            return NT_DECLINED;
        }

        if (pc == NULL || c->buffered || pc->buffered) {
            return NT_DECLINED;
        }

        handler = c->log->handler;
        c->log->handler = NULL;

        nt_log_error(NT_LOG_INFO, c->log, 0,
                      "udp done"
                      ", packets from/to client:%ui/%ui"
                      ", bytes from/to client:%O/%O"
                      ", bytes from/to upstream:%O/%O",
                      u->requests, u->responses,
                      s->received, c->sent, u->received, pc ? pc->sent : 0);

        c->log->handler = handler;

        nt_stream_proxy_finalize(s, NT_STREAM_OK);

        return NT_OK;
    }

    /* c->type == SOCK_STREAM */

    if (pc == NULL
        || (!c->read->eof && !pc->read->eof)
        || (!c->read->eof && c->buffered)
        || (!pc->read->eof && pc->buffered))
    {
        return NT_DECLINED;
    }

    handler = c->log->handler;
    c->log->handler = NULL;

    nt_log_error(NT_LOG_INFO, c->log, 0,
                  "%s disconnected"
                  ", bytes from/to client:%O/%O"
                  ", bytes from/to upstream:%O/%O",
                  from_upstream ? "upstream" : "client",
                  s->received, c->sent, u->received, pc ? pc->sent : 0);

    c->log->handler = handler;

    nt_stream_proxy_finalize(s, NT_STREAM_OK);

    return NT_OK;
}


static void
nt_stream_proxy_next_upstream(nt_stream_session_t *s)
{
    nt_msec_t                    timeout;
    nt_connection_t             *pc;
    nt_stream_upstream_t        *u;
    nt_stream_proxy_srv_conf_t  *pscf;

    nt_log_debug0(NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                   "stream proxy next upstream");

    u = s->upstream;
    pc = u->peer.connection;

    if (pc && pc->buffered) {
        nt_log_error(NT_LOG_ERR, s->connection->log, 0,
                      "buffered data on next upstream");
        nt_stream_proxy_finalize(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    if (s->connection->type == SOCK_DGRAM) {
        u->upstream_out = NULL;
    }

    if (u->peer.sockaddr) {
        u->peer.free(&u->peer, u->peer.data, NT_PEER_FAILED);
        u->peer.sockaddr = NULL;
    }

    pscf = nt_stream_get_module_srv_conf(s, nt_stream_proxy_module);

    timeout = pscf->next_upstream_timeout;

    if (u->peer.tries == 0
        || !pscf->next_upstream
        || (timeout && nt_current_msec - u->peer.start_time >= timeout))
    {
        nt_stream_proxy_finalize(s, NT_STREAM_BAD_GATEWAY);
        return;
    }

    if (pc) {
        nt_log_debug1(NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                       "close proxy upstream connection: %d", pc->fd);

#if (NT_STREAM_SSL)
        if (pc->ssl) {
            pc->ssl->no_wait_shutdown = 1;
            pc->ssl->no_send_shutdown = 1;

            (void) nt_ssl_shutdown(pc);
        }
#endif

        u->state->bytes_received = u->received;
        u->state->bytes_sent = pc->sent;

        nt_close_connection(pc);
        u->peer.connection = NULL;
    }

    nt_stream_proxy_connect(s);
}


static void
nt_stream_proxy_finalize(nt_stream_session_t *s, nt_uint_t rc)
{
    nt_uint_t              state;
    nt_connection_t       *pc;
    nt_stream_upstream_t  *u;

    nt_log_debug1(NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                   "finalize stream proxy: %i", rc);

    u = s->upstream;

    if (u == NULL) {
        goto noupstream;
    }

    if (u->resolved && u->resolved->ctx) {
        nt_resolve_name_done(u->resolved->ctx);
        u->resolved->ctx = NULL;
    }

    pc = u->peer.connection;

    if (u->state) {
        if (u->state->response_time == (nt_msec_t) -1) {
            u->state->response_time = nt_current_msec - u->start_time;
        }

        if (pc) {
            u->state->bytes_received = u->received;
            u->state->bytes_sent = pc->sent;
        }
    }

    if (u->peer.free && u->peer.sockaddr) {
        state = 0;

        if (pc && pc->type == SOCK_DGRAM
            && (pc->read->error || pc->write->error))
        {
            state = NT_PEER_FAILED;
        }

        u->peer.free(&u->peer, u->peer.data, state);
        u->peer.sockaddr = NULL;
    }

    if (pc) {
        nt_log_debug1(NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                       "close stream proxy upstream connection: %d", pc->fd);

#if (NT_STREAM_SSL)
        if (pc->ssl) {
            pc->ssl->no_wait_shutdown = 1;
            (void) nt_ssl_shutdown(pc);
        }
#endif

        nt_close_connection(pc);
        u->peer.connection = NULL;
    }

noupstream:

    nt_stream_finalize_session(s, rc);
}


static u_char *
nt_stream_proxy_log_error(nt_log_t *log, u_char *buf, size_t len)
{
    u_char                 *p;
    nt_connection_t       *pc;
    nt_stream_session_t   *s;
    nt_stream_upstream_t  *u;

    s = log->data;

    u = s->upstream;

    p = buf;

    if (u->peer.name) {
        p = nt_snprintf(p, len, ", upstream: \"%V\"", u->peer.name);
        len -= p - buf;
    }

    pc = u->peer.connection;

    p = nt_snprintf(p, len,
                     ", bytes from/to client:%O/%O"
                     ", bytes from/to upstream:%O/%O",
                     s->received, s->connection->sent,
                     u->received, pc ? pc->sent : 0);

    return p;
}


static void *
nt_stream_proxy_create_srv_conf(nt_conf_t *cf)
{
    nt_stream_proxy_srv_conf_t  *conf;

    conf = nt_pcalloc(cf->pool, sizeof(nt_stream_proxy_srv_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by nt_pcalloc():
     *
     *     conf->ssl_protocols = 0;
     *     conf->ssl_ciphers = { 0, NULL };
     *     conf->ssl_name = NULL;
     *     conf->ssl_trusted_certificate = { 0, NULL };
     *     conf->ssl_crl = { 0, NULL };
     *     conf->ssl_certificate = { 0, NULL };
     *     conf->ssl_certificate_key = { 0, NULL };
     *
     *     conf->upload_rate = NULL;
     *     conf->download_rate = NULL;
     *     conf->ssl = NULL;
     *     conf->upstream = NULL;
     *     conf->upstream_value = NULL;
     */

    conf->connect_timeout = NT_CONF_UNSET_MSEC;
    conf->timeout = NT_CONF_UNSET_MSEC;
    conf->next_upstream_timeout = NT_CONF_UNSET_MSEC;
    conf->buffer_size = NT_CONF_UNSET_SIZE;
    conf->requests = NT_CONF_UNSET_UINT;
    conf->responses = NT_CONF_UNSET_UINT;
    conf->next_upstream_tries = NT_CONF_UNSET_UINT;
    conf->next_upstream = NT_CONF_UNSET;
    conf->proxy_protocol = NT_CONF_UNSET;
    conf->local = NT_CONF_UNSET_PTR;
    conf->socket_keepalive = NT_CONF_UNSET;

#if (NT_STREAM_SSL)
    conf->ssl_enable = NT_CONF_UNSET;
    conf->ssl_session_reuse = NT_CONF_UNSET;
    conf->ssl_server_name = NT_CONF_UNSET;
    conf->ssl_verify = NT_CONF_UNSET;
    conf->ssl_verify_depth = NT_CONF_UNSET_UINT;
    conf->ssl_passwords = NT_CONF_UNSET_PTR;
#endif

    return conf;
}


static char *
nt_stream_proxy_merge_srv_conf(nt_conf_t *cf, void *parent, void *child)
{
    nt_stream_proxy_srv_conf_t *prev = parent;
    nt_stream_proxy_srv_conf_t *conf = child;

    nt_conf_merge_msec_value(conf->connect_timeout,
                              prev->connect_timeout, 60000);

    nt_conf_merge_msec_value(conf->timeout,
                              prev->timeout, 10 * 60000);

    nt_conf_merge_msec_value(conf->next_upstream_timeout,
                              prev->next_upstream_timeout, 0);

    nt_conf_merge_size_value(conf->buffer_size,
                              prev->buffer_size, 16384);

    if (conf->upload_rate == NULL) {
        conf->upload_rate = prev->upload_rate;
    }

    if (conf->download_rate == NULL) {
        conf->download_rate = prev->download_rate;
    }

    nt_conf_merge_uint_value(conf->requests,
                              prev->requests, 0);

    nt_conf_merge_uint_value(conf->responses,
                              prev->responses, NT_MAX_INT32_VALUE);

    nt_conf_merge_uint_value(conf->next_upstream_tries,
                              prev->next_upstream_tries, 0);

    nt_conf_merge_value(conf->next_upstream, prev->next_upstream, 1);

    nt_conf_merge_value(conf->proxy_protocol, prev->proxy_protocol, 0);

    nt_conf_merge_ptr_value(conf->local, prev->local, NULL);

    nt_conf_merge_value(conf->socket_keepalive,
                              prev->socket_keepalive, 0);

#if (NT_STREAM_SSL)

    nt_conf_merge_value(conf->ssl_enable, prev->ssl_enable, 0);

    nt_conf_merge_value(conf->ssl_session_reuse,
                              prev->ssl_session_reuse, 1);

    nt_conf_merge_bitmask_value(conf->ssl_protocols, prev->ssl_protocols,
                              (NT_CONF_BITMASK_SET|NT_SSL_TLSv1
                               |NT_SSL_TLSv1_1|NT_SSL_TLSv1_2));

    nt_conf_merge_str_value(conf->ssl_ciphers, prev->ssl_ciphers, "DEFAULT");

    if (conf->ssl_name == NULL) {
        conf->ssl_name = prev->ssl_name;
    }

    nt_conf_merge_value(conf->ssl_server_name, prev->ssl_server_name, 0);

    nt_conf_merge_value(conf->ssl_verify, prev->ssl_verify, 0);

    nt_conf_merge_uint_value(conf->ssl_verify_depth,
                              prev->ssl_verify_depth, 1);

    nt_conf_merge_str_value(conf->ssl_trusted_certificate,
                              prev->ssl_trusted_certificate, "");

    nt_conf_merge_str_value(conf->ssl_crl, prev->ssl_crl, "");

    nt_conf_merge_str_value(conf->ssl_certificate,
                              prev->ssl_certificate, "");

    nt_conf_merge_str_value(conf->ssl_certificate_key,
                              prev->ssl_certificate_key, "");

    nt_conf_merge_ptr_value(conf->ssl_passwords, prev->ssl_passwords, NULL);

    if (conf->ssl_enable && nt_stream_proxy_set_ssl(cf, conf) != NT_OK) {
        return NT_CONF_ERROR;
    }

#endif

    return NT_CONF_OK;
}


#if (NT_STREAM_SSL)

static nt_int_t
nt_stream_proxy_set_ssl(nt_conf_t *cf, nt_stream_proxy_srv_conf_t *pscf)
{
    nt_pool_cleanup_t  *cln;

    pscf->ssl = nt_pcalloc(cf->pool, sizeof(nt_ssl_t));
    if (pscf->ssl == NULL) {
        return NT_ERROR;
    }

    pscf->ssl->log = cf->log;

    if (nt_ssl_create(pscf->ssl, pscf->ssl_protocols, NULL) != NT_OK) {
        return NT_ERROR;
    }

    cln = nt_pool_cleanup_add(cf->pool, 0);
    if (cln == NULL) {
        nt_ssl_cleanup_ctx(pscf->ssl);
        return NT_ERROR;
    }

    cln->handler = nt_ssl_cleanup_ctx;
    cln->data = pscf->ssl;

    if (pscf->ssl_certificate.len) {

        if (pscf->ssl_certificate_key.len == 0) {
            nt_log_error(NT_LOG_EMERG, cf->log, 0,
                          "no \"proxy_ssl_certificate_key\" is defined "
                          "for certificate \"%V\"", &pscf->ssl_certificate);
            return NT_ERROR;
        }

        if (nt_ssl_certificate(cf, pscf->ssl, &pscf->ssl_certificate,
                                &pscf->ssl_certificate_key, pscf->ssl_passwords)
            != NT_OK)
        {
            return NT_ERROR;
        }
    }

    if (nt_ssl_ciphers(cf, pscf->ssl, &pscf->ssl_ciphers, 0) != NT_OK) {
        return NT_ERROR;
    }

    if (pscf->ssl_verify) {
        if (pscf->ssl_trusted_certificate.len == 0) {
            nt_log_error(NT_LOG_EMERG, cf->log, 0,
                      "no proxy_ssl_trusted_certificate for proxy_ssl_verify");
            return NT_ERROR;
        }

        if (nt_ssl_trusted_certificate(cf, pscf->ssl,
                                        &pscf->ssl_trusted_certificate,
                                        pscf->ssl_verify_depth)
            != NT_OK)
        {
            return NT_ERROR;
        }

        if (nt_ssl_crl(cf, pscf->ssl, &pscf->ssl_crl) != NT_OK) {
            return NT_ERROR;
        }
    }

    if (nt_ssl_client_session_cache(cf, pscf->ssl, pscf->ssl_session_reuse)
        != NT_OK)
    {
        return NT_ERROR;
    }

    return NT_OK;
}

#endif


static char *
nt_stream_proxy_pass(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    nt_stream_proxy_srv_conf_t *pscf = conf;

    nt_url_t                            u;
    nt_str_t                           *value, *url;
    nt_stream_complex_value_t           cv;
    nt_stream_core_srv_conf_t          *cscf;
    nt_stream_compile_complex_value_t   ccv;

    if (pscf->upstream || pscf->upstream_value) {
        return "is duplicate";
    }

    cscf = nt_stream_conf_get_module_srv_conf(cf, nt_stream_core_module);

    cscf->handler = nt_stream_proxy_handler;

    value = cf->args->elts;

    url = &value[1];

    nt_memzero(&ccv, sizeof(nt_stream_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = url;
    ccv.complex_value = &cv;

    if (nt_stream_compile_complex_value(&ccv) != NT_OK) {
        return NT_CONF_ERROR;
    }

    if (cv.lengths) {
        pscf->upstream_value = nt_palloc(cf->pool,
                                          sizeof(nt_stream_complex_value_t));
        if (pscf->upstream_value == NULL) {
            return NT_CONF_ERROR;
        }

        *pscf->upstream_value = cv;

        return NT_CONF_OK;
    }

    nt_memzero(&u, sizeof(nt_url_t));

    u.url = *url;
    u.no_resolve = 1;

    pscf->upstream = nt_stream_upstream_add(cf, &u, 0);
    if (pscf->upstream == NULL) {
        return NT_CONF_ERROR;
    }

    return NT_CONF_OK;
}


static char *
nt_stream_proxy_bind(nt_conf_t *cf, nt_command_t *cmd, void *conf)
{
    nt_stream_proxy_srv_conf_t *pscf = conf;

    nt_int_t                            rc;
    nt_str_t                           *value;
    nt_stream_complex_value_t           cv;
    nt_stream_upstream_local_t         *local;
    nt_stream_compile_complex_value_t   ccv;

    if (pscf->local != NT_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (cf->args->nelts == 2 && nt_strcmp(value[1].data, "off") == 0) {
        pscf->local = NULL;
        return NT_CONF_OK;
    }

    nt_memzero(&ccv, sizeof(nt_stream_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &cv;

    if (nt_stream_compile_complex_value(&ccv) != NT_OK) {
        return NT_CONF_ERROR;
    }

    local = nt_pcalloc(cf->pool, sizeof(nt_stream_upstream_local_t));
    if (local == NULL) {
        return NT_CONF_ERROR;
    }

    pscf->local = local;

    if (cv.lengths) {
        local->value = nt_palloc(cf->pool, sizeof(nt_stream_complex_value_t));
        if (local->value == NULL) {
            return NT_CONF_ERROR;
        }

        *local->value = cv;

    } else {
        local->addr = nt_palloc(cf->pool, sizeof(nt_addr_t));
        if (local->addr == NULL) {
            return NT_CONF_ERROR;
        }

        rc = nt_parse_addr_port(cf->pool, local->addr, value[1].data,
                                 value[1].len);

        switch (rc) {
        case NT_OK:
            local->addr->name = value[1];
            break;

        case NT_DECLINED:
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "invalid address \"%V\"", &value[1]);
            /* fall through */

        default:
            return NT_CONF_ERROR;
        }
    }

    if (cf->args->nelts > 2) {
        if (nt_strcmp(value[2].data, "transparent") == 0) {
#if (NT_HAVE_TRANSPARENT_PROXY)
            nt_core_conf_t  *ccf;

            ccf = (nt_core_conf_t *) nt_get_conf(cf->cycle->conf_ctx,
                                                   nt_core_module);

            ccf->transparent = 1;
            local->transparent = 1;
#else
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "transparent proxying is not supported "
                               "on this platform, ignored");
#endif
        } else {
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "invalid parameter \"%V\"", &value[2]);
            return NT_CONF_ERROR;
        }
    }

    return NT_CONF_OK;
}
