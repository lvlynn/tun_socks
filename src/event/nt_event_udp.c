
#include <nt_core.h>
#include <nt_event.h>


#if !(NT_WIN32)

struct nt_udp_connection_s {
    nt_rbtree_node_t   node;
    nt_connection_t   *connection;
    nt_buf_t          *buffer;
};


static void nt_close_accepted_udp_connection(nt_connection_t *c);
static ssize_t nt_udp_shared_recv(nt_connection_t *c, u_char *buf,
    size_t size);
static nt_int_t nt_insert_udp_connection(nt_connection_t *c);
static nt_connection_t *nt_lookup_udp_connection(nt_listening_t *ls,
    struct sockaddr *sockaddr, socklen_t socklen,
    struct sockaddr *local_sockaddr, socklen_t local_socklen);

/*
 * udp 流程走向
 * https://blog.csdn.net/sdghchj/article/details/89385915
 *
 * */
void
nt_event_recvmsg(nt_event_t *ev)
{
    ssize_t            n;
    nt_buf_t          buf;
    nt_log_t         *log;
    nt_err_t          err;
    socklen_t          socklen, local_socklen;
    nt_event_t       *rev, *wev;
    struct iovec       iov[1];
    struct msghdr      msg;
    nt_sockaddr_t     sa, lsa;
    struct sockaddr   *sockaddr, *local_sockaddr;
    nt_listening_t   *ls;
    nt_event_conf_t  *ecf;
    nt_connection_t  *c, *lc;
    static u_char      buffer[65535];

#if (NT_HAVE_MSGHDR_MSG_CONTROL)

#if (NT_HAVE_IP_RECVDSTADDR)
    u_char             msg_control[CMSG_SPACE(sizeof(struct in_addr))];
#elif (NT_HAVE_IP_PKTINFO)
    u_char             msg_control[CMSG_SPACE(sizeof(struct in_pktinfo))];
#endif

#if (NT_HAVE_INET6 && NT_HAVE_IPV6_RECVPKTINFO)
    u_char             msg_control6[CMSG_SPACE(sizeof(struct in6_pktinfo))];
#endif

#endif

    if (ev->timedout) {
        if (nt_enable_accept_events((nt_cycle_t *) nt_cycle) != NT_OK) {
            return;
        }

        ev->timedout = 0;
    }

    ecf = nt_event_get_conf(nt_cycle->conf_ctx, nt_event_core_module);

    if (!(nt_event_flags & NT_USE_KQUEUE_EVENT)) {
        ev->available = ecf->multi_accept;
    }

    lc = ev->data;
    ls = lc->listening;
    ev->ready = 0;

    nt_log_debug2(NT_LOG_DEBUG_EVENT, ev->log, 0,
                   "recvmsg on %V, ready: %d", &ls->addr_text, ev->available);

    do {
        nt_memzero(&msg, sizeof(struct msghdr));

        iov[0].iov_base = (void *) buffer;
        iov[0].iov_len = sizeof(buffer);

        msg.msg_name = &sa;
        msg.msg_namelen = sizeof(nt_sockaddr_t);
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

#if (NT_HAVE_MSGHDR_MSG_CONTROL)

        if (ls->wildcard) {

#if (NT_HAVE_IP_RECVDSTADDR || NT_HAVE_IP_PKTINFO)
            if (ls->sockaddr->sa_family == AF_INET) {
                msg.msg_control = &msg_control;
                msg.msg_controllen = sizeof(msg_control);
            }
#endif

#if (NT_HAVE_INET6 && NT_HAVE_IPV6_RECVPKTINFO)
            if (ls->sockaddr->sa_family == AF_INET6) {
                msg.msg_control = &msg_control6;
                msg.msg_controllen = sizeof(msg_control6);
            }
#endif
        }

#endif

        n = recvmsg(lc->fd, &msg, 0);

        if (n == -1) {
            err = nt_socket_errno;

            if (err == NT_EAGAIN) {
                nt_log_debug0(NT_LOG_DEBUG_EVENT, ev->log, err,
                               "recvmsg() not ready");
                return;
            }

            nt_log_error(NT_LOG_ALERT, ev->log, err, "recvmsg() failed");

            return;
        }

#if (NT_HAVE_MSGHDR_MSG_CONTROL)
        if (msg.msg_flags & (MSG_TRUNC|MSG_CTRUNC)) {
            nt_log_error(NT_LOG_ALERT, ev->log, 0,
                          "recvmsg() truncated data");
            continue;
        }
#endif

        sockaddr = msg.msg_name;
        socklen = msg.msg_namelen;

        if (socklen > (socklen_t) sizeof(nt_sockaddr_t)) {
            socklen = sizeof(nt_sockaddr_t);
        }

        if (socklen == 0) {

            /*
             * on Linux recvmsg() returns zero msg_namelen
             * when receiving packets from unbound AF_UNIX sockets
             */

            socklen = sizeof(struct sockaddr);
            nt_memzero(&sa, sizeof(struct sockaddr));
            sa.sockaddr.sa_family = ls->sockaddr->sa_family;
        }

        local_sockaddr = ls->sockaddr;
        local_socklen = ls->socklen;

#if (NT_HAVE_MSGHDR_MSG_CONTROL)

        if (ls->wildcard) {
            struct cmsghdr  *cmsg;

            nt_memcpy(&lsa, local_sockaddr, local_socklen);
            local_sockaddr = &lsa.sockaddr;

            for (cmsg = CMSG_FIRSTHDR(&msg);
                 cmsg != NULL;
                 cmsg = CMSG_NXTHDR(&msg, cmsg))
            {

#if (NT_HAVE_IP_RECVDSTADDR)

                if (cmsg->cmsg_level == IPPROTO_IP
                    && cmsg->cmsg_type == IP_RECVDSTADDR
                    && local_sockaddr->sa_family == AF_INET)
                {
                    struct in_addr      *addr;
                    struct sockaddr_in  *sin;

                    addr = (struct in_addr *) CMSG_DATA(cmsg);
                    sin = (struct sockaddr_in *) local_sockaddr;
                    sin->sin_addr = *addr;

                    break;
                }

#elif (NT_HAVE_IP_PKTINFO)

                if (cmsg->cmsg_level == IPPROTO_IP
                    && cmsg->cmsg_type == IP_PKTINFO
                    && local_sockaddr->sa_family == AF_INET)
                {
                    struct in_pktinfo   *pkt;
                    struct sockaddr_in  *sin;

                    pkt = (struct in_pktinfo *) CMSG_DATA(cmsg);
                    sin = (struct sockaddr_in *) local_sockaddr;
                    sin->sin_addr = pkt->ipi_addr;

                    break;
                }

#endif

#if (NT_HAVE_INET6 && NT_HAVE_IPV6_RECVPKTINFO)

                if (cmsg->cmsg_level == IPPROTO_IPV6
                    && cmsg->cmsg_type == IPV6_PKTINFO
                    && local_sockaddr->sa_family == AF_INET6)
                {
                    struct in6_pktinfo   *pkt6;
                    struct sockaddr_in6  *sin6;

                    pkt6 = (struct in6_pktinfo *) CMSG_DATA(cmsg);
                    sin6 = (struct sockaddr_in6 *) local_sockaddr;
                    sin6->sin6_addr = pkt6->ipi6_addr;

                    break;
                }

#endif

            }
        }

#endif

        c = nt_lookup_udp_connection(ls, sockaddr, socklen, local_sockaddr,
                                      local_socklen);

        if (c) {

#if (NT_DEBUG)
            if (c->log->log_level & NT_LOG_DEBUG_EVENT) {
                nt_log_handler_pt  handler;

                handler = c->log->handler;
                c->log->handler = NULL;

                nt_log_debug2(NT_LOG_DEBUG_EVENT, c->log, 0,
                               "recvmsg: fd:%d n:%z", c->fd, n);

                c->log->handler = handler;
            }
#endif

            nt_memzero(&buf, sizeof(nt_buf_t));

            buf.pos = buffer;
            buf.last = buffer + n;

            rev = c->read;

            c->udp->buffer = &buf;

            rev->ready = 1;
            rev->active = 0;

            rev->handler(rev);

            if (c->udp) {
                c->udp->buffer = NULL;
            }

            rev->ready = 0;
            rev->active = 1;

            goto next;
        }

#if (NT_STAT_STUB)
        (void) nt_atomic_fetch_add(nt_stat_accepted, 1);
#endif

        nt_accept_disabled = nt_cycle->connection_n / 8
                              - nt_cycle->free_connection_n;

        c = nt_get_connection(lc->fd, ev->log);
        if (c == NULL) {
            return;
        }

        c->shared = 1;
        c->type = SOCK_DGRAM;
        c->socklen = socklen;

#if (NT_STAT_STUB)
        (void) nt_atomic_fetch_add(nt_stat_active, 1);
#endif

        c->pool = nt_create_pool(ls->pool_size, ev->log);
        if (c->pool == NULL) {
            nt_close_accepted_udp_connection(c);
            return;
        }

        c->sockaddr = nt_palloc(c->pool, socklen);
        if (c->sockaddr == NULL) {
            nt_close_accepted_udp_connection(c);
            return;
        }

        nt_memcpy(c->sockaddr, sockaddr, socklen);

        log = nt_palloc(c->pool, sizeof(nt_log_t));
        if (log == NULL) {
            nt_close_accepted_udp_connection(c);
            return;
        }

        *log = ls->log;

        c->recv = nt_udp_shared_recv;
        c->send = nt_udp_send;
        c->send_chain = nt_udp_send_chain;

        c->log = log;
        c->pool->log = log;
        c->listening = ls;

        if (local_sockaddr == &lsa.sockaddr) {
            local_sockaddr = nt_palloc(c->pool, local_socklen);
            if (local_sockaddr == NULL) {
                nt_close_accepted_udp_connection(c);
                return;
            }

            nt_memcpy(local_sockaddr, &lsa, local_socklen);
        }

        c->local_sockaddr = local_sockaddr;
        c->local_socklen = local_socklen;

        c->buffer = nt_create_temp_buf(c->pool, n);
        if (c->buffer == NULL) {
            nt_close_accepted_udp_connection(c);
            return;
        }

        c->buffer->last = nt_cpymem(c->buffer->last, buffer, n);

        rev = c->read;
        wev = c->write;

        rev->active = 1;
        wev->ready = 1;

        rev->log = log;
        wev->log = log;

        /*
         * TODO: MT: - nt_atomic_fetch_add()
         *             or protection by critical section or light mutex
         *
         * TODO: MP: - allocated in a shared memory
         *           - nt_atomic_fetch_add()
         *             or protection by critical section or light mutex
         */

        c->number = nt_atomic_fetch_add(nt_connection_counter, 1);

#if (NT_STAT_STUB)
        (void) nt_atomic_fetch_add(nt_stat_handled, 1);
#endif

        if (ls->addr_ntop) {
            c->addr_text.data = nt_pnalloc(c->pool, ls->addr_text_max_len);
            if (c->addr_text.data == NULL) {
                nt_close_accepted_udp_connection(c);
                return;
            }

            c->addr_text.len = nt_sock_ntop(c->sockaddr, c->socklen,
                                             c->addr_text.data,
                                             ls->addr_text_max_len, 0);
            if (c->addr_text.len == 0) {
                nt_close_accepted_udp_connection(c);
                return;
            }
        }

#if (NT_DEBUG)
        {
        nt_str_t  addr;
        u_char     text[NT_SOCKADDR_STRLEN];

        nt_debug_accepted_connection(ecf, c);

        if (log->log_level & NT_LOG_DEBUG_EVENT) {
            addr.data = text;
            addr.len = nt_sock_ntop(c->sockaddr, c->socklen, text,
                                     NT_SOCKADDR_STRLEN, 1);

            nt_log_debug4(NT_LOG_DEBUG_EVENT, log, 0,
                           "*%uA recvmsg: %V fd:%d n:%z",
                           c->number, &addr, c->fd, n);
        }

        }
#endif

        if (nt_insert_udp_connection(c) != NT_OK) {
            nt_close_accepted_udp_connection(c);
            return;
        }

        log->data = NULL;
        log->handler = NULL;

        ls->handler(c);

    next:

        if (nt_event_flags & NT_USE_KQUEUE_EVENT) {
            ev->available -= n;
        }

    } while (ev->available);
}


static void
nt_close_accepted_udp_connection(nt_connection_t *c)
{
    nt_free_connection(c);

    c->fd = (nt_socket_t) -1;

    if (c->pool) {
        nt_destroy_pool(c->pool);
    }

#if (NT_STAT_STUB)
    (void) nt_atomic_fetch_add(nt_stat_active, -1);
#endif
}


static ssize_t
nt_udp_shared_recv(nt_connection_t *c, u_char *buf, size_t size)
{
    ssize_t     n;
    nt_buf_t  *b;

    if (c->udp == NULL || c->udp->buffer == NULL) {
        return NT_AGAIN;
    }

    b = c->udp->buffer;

    n = nt_min(b->last - b->pos, (ssize_t) size);

    nt_memcpy(buf, b->pos, n);

    c->udp->buffer = NULL;

    c->read->ready = 0;
    c->read->active = 1;

    return n;
}


void
nt_udp_rbtree_insert_value(nt_rbtree_node_t *temp,
    nt_rbtree_node_t *node, nt_rbtree_node_t *sentinel)
{
    nt_int_t               rc;
    nt_connection_t       *c, *ct;
    nt_rbtree_node_t     **p;
    nt_udp_connection_t   *udp, *udpt;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            udp = (nt_udp_connection_t *) node;
            c = udp->connection;

            udpt = (nt_udp_connection_t *) temp;
            ct = udpt->connection;

            rc = nt_cmp_sockaddr(c->sockaddr, c->socklen,
                                  ct->sockaddr, ct->socklen, 1);

            if (rc == 0 && c->listening->wildcard) {
                rc = nt_cmp_sockaddr(c->local_sockaddr, c->local_socklen,
                                      ct->local_sockaddr, ct->local_socklen, 1);
            }

            p = (rc < 0) ? &temp->left : &temp->right;
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


static nt_int_t
nt_insert_udp_connection(nt_connection_t *c)
{
    uint32_t               hash;
    nt_pool_cleanup_t    *cln;
    nt_udp_connection_t  *udp;

    if (c->udp) {
        return NT_OK;
    }

    udp = nt_pcalloc(c->pool, sizeof(nt_udp_connection_t));
    if (udp == NULL) {
        return NT_ERROR;
    }

    udp->connection = c;

    nt_crc32_init(hash);
    nt_crc32_update(&hash, (u_char *) c->sockaddr, c->socklen);

    if (c->listening->wildcard) {
        nt_crc32_update(&hash, (u_char *) c->local_sockaddr, c->local_socklen);
    }

    nt_crc32_final(hash);

    udp->node.key = hash;

    cln = nt_pool_cleanup_add(c->pool, 0);
    if (cln == NULL) {
        return NT_ERROR;
    }

    cln->data = c;
    cln->handler = nt_delete_udp_connection;

    nt_rbtree_insert(&c->listening->rbtree, &udp->node);

    c->udp = udp;

    return NT_OK;
}


void
nt_delete_udp_connection(void *data)
{
    nt_connection_t  *c = data;

    if (c->udp == NULL) {
        return;
    }

    nt_rbtree_delete(&c->listening->rbtree, &c->udp->node);

    c->udp = NULL;
}


static nt_connection_t *
nt_lookup_udp_connection(nt_listening_t *ls, struct sockaddr *sockaddr,
    socklen_t socklen, struct sockaddr *local_sockaddr, socklen_t local_socklen)
{
    uint32_t               hash;
    nt_int_t              rc;
    nt_connection_t      *c;
    nt_rbtree_node_t     *node, *sentinel;
    nt_udp_connection_t  *udp;

#if (NT_HAVE_UNIX_DOMAIN)

    if (sockaddr->sa_family == AF_UNIX) {
        struct sockaddr_un *saun = (struct sockaddr_un *) sockaddr;

        if (socklen <= (socklen_t) offsetof(struct sockaddr_un, sun_path)
            || saun->sun_path[0] == '\0')
        {
            nt_log_debug0(NT_LOG_DEBUG_EVENT, nt_cycle->log, 0,
                           "unbound unix socket");
            return NULL;
        }
    }

#endif

    node = ls->rbtree.root;
    sentinel = ls->rbtree.sentinel;

    nt_crc32_init(hash);
    nt_crc32_update(&hash, (u_char *) sockaddr, socklen);

    if (ls->wildcard) {
        nt_crc32_update(&hash, (u_char *) local_sockaddr, local_socklen);
    }

    nt_crc32_final(hash);

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

        udp = (nt_udp_connection_t *) node;

        c = udp->connection;

        rc = nt_cmp_sockaddr(sockaddr, socklen,
                              c->sockaddr, c->socklen, 1);

        if (rc == 0 && ls->wildcard) {
            rc = nt_cmp_sockaddr(local_sockaddr, local_socklen,
                                  c->local_sockaddr, c->local_socklen, 1);
        }

        if (rc == 0) {
            return c;
        }

        node = (rc < 0) ? node->left : node->right;
    }

    return NULL;
}

#else

void
nt_delete_udp_connection(void *data)
{
    return;
}

#endif
