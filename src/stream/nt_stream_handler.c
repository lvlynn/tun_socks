

#include <nt_core.h>
#include <nt_event.h>
#include <nt_stream.h>


static void nt_stream_log_session(nt_stream_session_t *s);
static void nt_stream_close_connection(nt_connection_t *c);
static u_char *nt_stream_log_error(nt_log_t *log, u_char *buf, size_t len);
static void nt_stream_proxy_protocol_handler(nt_event_t *rev);


void
nt_stream_init_connection(nt_connection_t *c)
{
    debug( "start" );
    u_char                        text[NT_SOCKADDR_STRLEN];
    size_t                        len;
    nt_uint_t                    i;
    nt_time_t                   *tp;
    nt_event_t                  *rev;
    struct sockaddr              *sa;
    nt_stream_port_t            *port;
    struct sockaddr_in           *sin;
    nt_stream_in_addr_t         *addr;
    nt_stream_session_t         *s;
    nt_stream_addr_conf_t       *addr_conf;
#if (NT_HAVE_INET6)
    struct sockaddr_in6          *sin6;
    nt_stream_in6_addr_t        *addr6;
#endif
    nt_stream_core_srv_conf_t   *cscf;
    nt_stream_core_main_conf_t  *cmcf;

    /* find the server configuration for the address:port */

    port = c->listening->servers;

    if (port->naddrs > 1) {

        /*
         * There are several addresses on this port and one of them
         * is the "*:port" wildcard so getsockname() is needed to determine
         * the server address.
         *
         * AcceptEx() and recvmsg() already gave this address.
         */

        if (nt_connection_local_sockaddr(c, NULL, 0) != NT_OK) {
            nt_stream_close_connection(c);
            return;
        }

        sa = c->local_sockaddr;

        switch (sa->sa_family) {

#if (NT_HAVE_INET6)
        case AF_INET6:
            sin6 = (struct sockaddr_in6 *) sa;

            addr6 = port->addrs;

            /* the last address is "*" */

            for (i = 0; i < port->naddrs - 1; i++) {
                if (nt_memcmp(&addr6[i].addr6, &sin6->sin6_addr, 16) == 0) {
                    break;
                }
            }

            addr_conf = &addr6[i].conf;

            break;
#endif

        default: /* AF_INET */
            sin = (struct sockaddr_in *) sa;

            addr = port->addrs;

            /* the last address is "*" */

            for (i = 0; i < port->naddrs - 1; i++) {
                if (addr[i].addr == sin->sin_addr.s_addr) {
                    break;
                }
            }

            addr_conf = &addr[i].conf;

            break;
        }

    } else {
        switch (c->local_sockaddr->sa_family) {

#if (NT_HAVE_INET6)
        case AF_INET6:
            addr6 = port->addrs;
            addr_conf = &addr6[0].conf;
            break;
#endif

        default: /* AF_INET */
            addr = port->addrs;
            addr_conf = &addr[0].conf;
            break;
        }
    }

    s = nt_pcalloc(c->pool, sizeof(nt_stream_session_t));
    if (s == NULL) {
        nt_stream_close_connection(c);
        return;
    }

    s->signature = NT_STREAM_MODULE;
    s->main_conf = addr_conf->ctx->main_conf;
    s->srv_conf = addr_conf->ctx->srv_conf;
    debug( "s->srv_conf=%p", s->srv_conf );

#if (NT_STREAM_SNI)
    s->addr_conf = addr_conf;
    s->main_conf = ((nt_stream_core_srv_conf_t*)addr_conf->default_server)->ctx->main_conf;
    s->srv_conf = ((nt_stream_core_srv_conf_t*)addr_conf->default_server)->ctx->srv_conf;
#endif

#if (NT_STREAM_SSL)
    s->ssl = addr_conf->ssl;
#endif

    if (c->buffer) {
        s->received += c->buffer->last - c->buffer->pos;
    }

    s->connection = c;
    c->data = s;

    cscf = nt_stream_get_module_srv_conf(s, nt_stream_core_module);

    nt_set_connection_log(c, cscf->error_log);

    len = nt_sock_ntop(c->sockaddr, c->socklen, text, NT_SOCKADDR_STRLEN, 1);

    nt_log_error(NT_LOG_INFO, c->log, 0, "*%uA %sclient %*s connected to %V",
                  c->number, c->type == SOCK_DGRAM ? "udp " : "",
                  len, text, &addr_conf->addr_text);

    c->log->connection = c->number;
    c->log->handler = nt_stream_log_error;
    c->log->data = s;
    c->log->action = "initializing session";
    c->log_error = NT_ERROR_INFO;

    s->ctx = nt_pcalloc(c->pool, sizeof(void *) * nt_stream_max_module);
    if (s->ctx == NULL) {
        nt_stream_close_connection(c);
        return;
    }

    cmcf = nt_stream_get_module_main_conf(s, nt_stream_core_module);

    s->variables = nt_pcalloc(s->connection->pool,
                               cmcf->variables.nelts
                               * sizeof(nt_stream_variable_value_t));

    if (s->variables == NULL) {
        nt_stream_close_connection(c);
        return;
    }

    tp = nt_timeofday();
    s->start_sec = tp->sec;
    s->start_msec = tp->msec;

    rev = c->read;
    rev->handler = nt_stream_session_handler;

    if (addr_conf->proxy_protocol) {
        c->log->action = "reading PROXY protocol";

        rev->handler = nt_stream_proxy_protocol_handler;

        if (!rev->ready) {
            nt_add_timer(rev, cscf->proxy_protocol_timeout);

            if (nt_handle_read_event(rev, 0) != NT_OK) {
                nt_stream_finalize_session(s,
                                            NT_STREAM_INTERNAL_SERVER_ERROR);
            }

            return;
        }
    }

    //如果使用多连接
    if (nt_use_accept_mutex) {
        nt_post_event(rev, &nt_posted_events);
        return;
    }

    rev->handler(rev);
}


static void
nt_stream_proxy_protocol_handler(nt_event_t *rev)
{
    u_char                      *p, buf[NT_PROXY_PROTOCOL_MAX_HEADER];
    size_t                       size;
    ssize_t                      n;
    nt_err_t                    err;
    nt_connection_t            *c;
    nt_stream_session_t        *s;
    nt_stream_core_srv_conf_t  *cscf;

    c = rev->data;
    s = c->data;

    nt_log_debug0(NT_LOG_DEBUG_STREAM, c->log, 0,
                   "stream PROXY protocol handler");

    if (rev->timedout) {
        nt_log_error(NT_LOG_INFO, c->log, NT_ETIMEDOUT, "client timed out");
        nt_stream_finalize_session(s, NT_STREAM_OK);
        return;
    }

    n = recv(c->fd, (char *) buf, sizeof(buf), MSG_PEEK);

    err = nt_socket_errno;

    nt_log_debug1(NT_LOG_DEBUG_STREAM, c->log, 0, "recv(): %z", n);

    if (n == -1) {
        if (err == NT_EAGAIN) {
            rev->ready = 0;

            if (!rev->timer_set) {
                cscf = nt_stream_get_module_srv_conf(s,
                                                      nt_stream_core_module);

                nt_add_timer(rev, cscf->proxy_protocol_timeout);
            }

            if (nt_handle_read_event(rev, 0) != NT_OK) {
                nt_stream_finalize_session(s,
                                            NT_STREAM_INTERNAL_SERVER_ERROR);
            }

            return;
        }

        nt_connection_error(c, err, "recv() failed");

        nt_stream_finalize_session(s, NT_STREAM_OK);
        return;
    }

    if (rev->timer_set) {
        nt_del_timer(rev);
    }

    p = nt_proxy_protocol_read(c, buf, buf + n);

    if (p == NULL) {
        nt_stream_finalize_session(s, NT_STREAM_BAD_REQUEST);
        return;
    }

    size = p - buf;

    if (c->recv(c, buf, size) != (ssize_t) size) {
        nt_stream_finalize_session(s, NT_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    c->log->action = "initializing session";

    nt_stream_session_handler(rev);
}


void
nt_stream_session_handler(nt_event_t *rev)
{
    debug( "start" );
    nt_connection_t      *c;
    nt_stream_session_t  *s;

    c = rev->data;
    s = c->data;

    nt_stream_core_run_phases(s);
}


void
nt_stream_finalize_session(nt_stream_session_t *s, nt_uint_t rc)
{
    nt_log_debug1(NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                   "finalize stream session: %i", rc);

    s->status = rc;

    nt_stream_log_session(s);

    nt_stream_close_connection(s->connection);
}


static void
nt_stream_log_session(nt_stream_session_t *s)
{
    nt_uint_t                    i, n;
    nt_stream_handler_pt        *log_handler;
    nt_stream_core_main_conf_t  *cmcf;

    cmcf = nt_stream_get_module_main_conf(s, nt_stream_core_module);

    log_handler = cmcf->phases[NT_STREAM_LOG_PHASE].handlers.elts;
    n = cmcf->phases[NT_STREAM_LOG_PHASE].handlers.nelts;

    for (i = 0; i < n; i++) {
        log_handler[i](s);
    }
}


static void
nt_stream_close_connection(nt_connection_t *c)
{
    nt_pool_t  *pool;

    nt_log_debug1(NT_LOG_DEBUG_STREAM, c->log, 0,
                   "close stream connection: %d", c->fd);

#if (NT_STREAM_SSL)

    if (c->ssl) {
        if (nt_ssl_shutdown(c) == NT_AGAIN) {
            c->ssl->handler = nt_stream_close_connection;
            return;
        }
    }

#endif

#if (NT_STAT_STUB)
    (void) nt_atomic_fetch_add(nt_stat_active, -1);
#endif

    pool = c->pool;

    nt_close_connection(c);

    nt_destroy_pool(pool);
}


static u_char *
nt_stream_log_error(nt_log_t *log, u_char *buf, size_t len)
{
    u_char                *p;
    nt_stream_session_t  *s;

    if (log->action) {
        p = nt_snprintf(buf, len, " while %s", log->action);
        len -= p - buf;
        buf = p;
    }

    s = log->data;

    p = nt_snprintf(buf, len, ", %sclient: %V, server: %V",
                     s->connection->type == SOCK_DGRAM ? "udp " : "",
                     &s->connection->addr_text,
                     &s->connection->listening->addr_text);
    len -= p - buf;
    buf = p;

    if (s->log_handler) {
        p = s->log_handler(log, buf, len);
    }

    return p;
}
