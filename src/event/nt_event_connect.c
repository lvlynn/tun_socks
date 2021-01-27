


#include <nt_core.h>
#include <nt_event.h>
#include <nt_event_connect.h>


#if (NT_HAVE_TRANSPARENT_PROXY)
static nt_int_t nt_event_connect_set_transparent(nt_peer_connection_t *pc,
    nt_socket_t s);
#endif


nt_int_t
nt_event_connect_peer(nt_peer_connection_t *pc)
{
    int                rc, type, value;
#if (NT_HAVE_IP_BIND_ADDRESS_NO_PORT || NT_LINUX)
    in_port_t          port;
#endif
    nt_int_t          event;
    nt_err_t          err;
    nt_uint_t         level;
    nt_socket_t       s;
    nt_event_t       *rev, *wev;
    nt_connection_t  *c;

    rc = pc->get(pc, pc->data);
    if (rc != NT_OK) {
        return rc;
    }

    type = (pc->type ? pc->type : SOCK_STREAM);

    s = nt_socket(pc->sockaddr->sa_family, type, 0);

    nt_log_debug2(NT_LOG_DEBUG_EVENT, pc->log, 0, "%s socket %d",
                   (type == SOCK_STREAM) ? "stream" : "dgram", s);

    if (s == (nt_socket_t) -1) {
        nt_log_error(NT_LOG_ALERT, pc->log, nt_socket_errno,
                      nt_socket_n " failed");
        return NT_ERROR;
    }


    c = nt_get_connection(s, pc->log);

    if (c == NULL) {
        if (nt_close_socket(s) == -1) {
            nt_log_error(NT_LOG_ALERT, pc->log, nt_socket_errno,
                          nt_close_socket_n " failed");
        }

        return NT_ERROR;
    }

    c->type = type;

    if (pc->rcvbuf) {
        if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
                       (const void *) &pc->rcvbuf, sizeof(int)) == -1)
        {
            nt_log_error(NT_LOG_ALERT, pc->log, nt_socket_errno,
                          "setsockopt(SO_RCVBUF) failed");
            goto failed;
        }
    }

    if (pc->so_keepalive) {
        value = 1;

        if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE,
                       (const void *) &value, sizeof(int))
            == -1)
        {
            nt_log_error(NT_LOG_ALERT, pc->log, nt_socket_errno,
                          "setsockopt(SO_KEEPALIVE) failed, ignored");
        }
    }

    if (nt_nonblocking(s) == -1) {
        nt_log_error(NT_LOG_ALERT, pc->log, nt_socket_errno,
                      nt_nonblocking_n " failed");

        goto failed;
    }

    if (pc->local) {

#if (NT_HAVE_TRANSPARENT_PROXY)
        if (pc->transparent) {
            if (nt_event_connect_set_transparent(pc, s) != NT_OK) {
                goto failed;
            }
        }
#endif

#if (NT_HAVE_IP_BIND_ADDRESS_NO_PORT || NT_LINUX)
        port = nt_inet_get_port(pc->local->sockaddr);
#endif

#if (NT_HAVE_IP_BIND_ADDRESS_NO_PORT)

        if (pc->sockaddr->sa_family != AF_UNIX && port == 0) {
            static int  bind_address_no_port = 1;

            if (bind_address_no_port) {
                if (setsockopt(s, IPPROTO_IP, IP_BIND_ADDRESS_NO_PORT,
                               (const void *) &bind_address_no_port,
                               sizeof(int)) == -1)
                {
                    err = nt_socket_errno;

                    if (err != NT_EOPNOTSUPP && err != NT_ENOPROTOOPT) {
                        nt_log_error(NT_LOG_ALERT, pc->log, err,
                                      "setsockopt(IP_BIND_ADDRESS_NO_PORT) "
                                      "failed, ignored");

                    } else {
                        bind_address_no_port = 0;
                    }
                }
            }
        }

#endif

#if (NT_LINUX)

        if (pc->type == SOCK_DGRAM && port != 0) {
            int  reuse_addr = 1;

            if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                           (const void *) &reuse_addr, sizeof(int))
                 == -1)
            {
                nt_log_error(NT_LOG_ALERT, pc->log, nt_socket_errno,
                              "setsockopt(SO_REUSEADDR) failed");
                goto failed;
            }
        }

#endif

        if (bind(s, pc->local->sockaddr, pc->local->socklen) == -1) {
            nt_log_error(NT_LOG_CRIT, pc->log, nt_socket_errno,
                          "bind(%V) failed", &pc->local->name);

            goto failed;
        }
    }

    if (type == SOCK_STREAM) {
        c->recv = nt_recv;
        c->send = nt_send;
        /* c->recv_chain = nt_recv_chain;
        c->send_chain = nt_send_chain; */

        c->sendfile = 1;

        if (pc->sockaddr->sa_family == AF_UNIX) {
            c->tcp_nopush = NT_TCP_NOPUSH_DISABLED;
            c->tcp_nodelay = NT_TCP_NODELAY_DISABLED;

#if (NT_SOLARIS)
            /* Solaris's sendfilev() supports AF_NCA, AF_INET, and AF_INET6 */
            c->sendfile = 0;
#endif
        }

    } else { /* type == SOCK_DGRAM */
        c->recv = nt_udp_recv;
        c->send = nt_send;
        /* c->send_chain = nt_udp_send_chain; */
    }

    c->log_error = pc->log_error;

    rev = c->read;
    wev = c->write;

    rev->log = pc->log;
    wev->log = pc->log;

    pc->connection = c;

    c->number = nt_atomic_fetch_add(nt_connection_counter, 1);

    if (nt_add_conn) {
        if (nt_add_conn(c) == NT_ERROR) {
            goto failed;
        }
    }

    nt_log_debug3(NT_LOG_DEBUG_EVENT, pc->log, 0,
                   "connect to %V, fd:%d #%uA", pc->name, s, c->number);

    rc = connect(s, pc->sockaddr, pc->socklen);

    if (rc == -1) {
        err = nt_socket_errno;


        if (err != NT_EINPROGRESS
#if (NT_WIN32)
            /* Winsock returns WSAEWOULDBLOCK (NT_EAGAIN) */
            && err != NT_EAGAIN
#endif
            )
        {
            if (err == NT_ECONNREFUSED
#if (NT_LINUX)
                /*
                 * Linux returns EAGAIN instead of ECONNREFUSED
                 * for unix sockets if listen queue is full
                 */
                || err == NT_EAGAIN
#endif
                || err == NT_ECONNRESET
                || err == NT_ENETDOWN
                || err == NT_ENETUNREACH
                || err == NT_EHOSTDOWN
                || err == NT_EHOSTUNREACH)
            {
                level = NT_LOG_ERR;

            } else {
                level = NT_LOG_CRIT;
            }

            nt_log_error(level, c->log, err, "connect() to %V failed",
                          pc->name);

            nt_close_connection(c);
            pc->connection = NULL;

            return NT_DECLINED;
        }
    }

    if (nt_add_conn) {
        if (rc == -1) {

            /* NT_EINPROGRESS */

            return NT_AGAIN;
        }

        nt_log_debug0(NT_LOG_DEBUG_EVENT, pc->log, 0, "connected");

        wev->ready = 1;

        return NT_OK;
    }

    if (nt_event_flags & NT_USE_IOCP_EVENT) {

        nt_log_debug1(NT_LOG_DEBUG_EVENT, pc->log, nt_socket_errno,
                       "connect(): %d", rc);

        if (nt_blocking(s) == -1) {
            nt_log_error(NT_LOG_ALERT, pc->log, nt_socket_errno,
                          nt_blocking_n " failed");
            goto failed;
        }

        /*
         * FreeBSD's aio allows to post an operation on non-connected socket.
         * NT does not support it.
         *
         * TODO: check in Win32, etc. As workaround we can use NT_ONESHOT_EVENT
         */

        rev->ready = 1;
        wev->ready = 1;

        return NT_OK;
    }

    if (nt_event_flags & NT_USE_CLEAR_EVENT) {

        /* kqueue */

        event = NT_CLEAR_EVENT;

    } else {

        /* select, poll, /dev/poll */

        event = NT_LEVEL_EVENT;
    }

    if (nt_add_event(rev, NT_READ_EVENT, event) != NT_OK) {
        goto failed;
    }

    if (rc == -1) {

        /* NT_EINPROGRESS */

        if (nt_add_event(wev, NT_WRITE_EVENT, event) != NT_OK) {
            goto failed;
        }

        return NT_AGAIN;
    }

    nt_log_debug0(NT_LOG_DEBUG_EVENT, pc->log, 0, "connected");

    wev->ready = 1;

    return NT_OK;

failed:

    nt_close_connection(c);
    pc->connection = NULL;

    return NT_ERROR;
}


#if (NT_HAVE_TRANSPARENT_PROXY)

static nt_int_t
nt_event_connect_set_transparent(nt_peer_connection_t *pc, nt_socket_t s)
{
    int  value;

    value = 1;

#if defined(SO_BINDANY)

    if (setsockopt(s, SOL_SOCKET, SO_BINDANY,
                   (const void *) &value, sizeof(int)) == -1)
    {
        nt_log_error(NT_LOG_ALERT, pc->log, nt_socket_errno,
                      "setsockopt(SO_BINDANY) failed");
        return NT_ERROR;
    }

#else

    switch (pc->local->sockaddr->sa_family) {

    case AF_INET:

#if defined(IP_TRANSPARENT)

        if (setsockopt(s, IPPROTO_IP, IP_TRANSPARENT,
                       (const void *) &value, sizeof(int)) == -1)
        {
            nt_log_error(NT_LOG_ALERT, pc->log, nt_socket_errno,
                          "setsockopt(IP_TRANSPARENT) failed");
            return NT_ERROR;
        }

#elif defined(IP_BINDANY)

        if (setsockopt(s, IPPROTO_IP, IP_BINDANY,
                       (const void *) &value, sizeof(int)) == -1)
        {
            nt_log_error(NT_LOG_ALERT, pc->log, nt_socket_errno,
                          "setsockopt(IP_BINDANY) failed");
            return NT_ERROR;
        }

#endif

        break;

#if (NT_HAVE_INET6)

    case AF_INET6:

#if defined(IPV6_TRANSPARENT)

        if (setsockopt(s, IPPROTO_IPV6, IPV6_TRANSPARENT,
                       (const void *) &value, sizeof(int)) == -1)
        {
            nt_log_error(NT_LOG_ALERT, pc->log, nt_socket_errno,
                          "setsockopt(IPV6_TRANSPARENT) failed");
            return NT_ERROR;
        }

#elif defined(IPV6_BINDANY)

        if (setsockopt(s, IPPROTO_IPV6, IPV6_BINDANY,
                       (const void *) &value, sizeof(int)) == -1)
        {
            nt_log_error(NT_LOG_ALERT, pc->log, nt_socket_errno,
                          "setsockopt(IPV6_BINDANY) failed");
            return NT_ERROR;
        }

#else

        nt_log_error(NT_LOG_ALERT, pc->log, 0,
                      "could not enable transparent proxying for IPv6 "
                      "on this platform");

        return NT_ERROR;

#endif

        break;

#endif /* NT_HAVE_INET6 */

    }

#endif /* SO_BINDANY */

    return NT_OK;
}

#endif


nt_int_t
nt_event_get_peer(nt_peer_connection_t *pc, void *data)
{
    return NT_OK;
}
