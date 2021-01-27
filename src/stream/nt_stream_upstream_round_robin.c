

#include <nt_core.h>
#include <nt_stream.h>


#define nt_stream_upstream_tries(p) ((p)->number                             \
                                      + ((p)->next ? (p)->next->number : 0))


static nt_stream_upstream_rr_peer_t *nt_stream_upstream_get_peer(
    nt_stream_upstream_rr_peer_data_t *rrp);
static void nt_stream_upstream_notify_round_robin_peer(
    nt_peer_connection_t *pc, void *data, nt_uint_t state);

#if (NT_STREAM_SSL)

static nt_int_t nt_stream_upstream_set_round_robin_peer_session(
    nt_peer_connection_t *pc, void *data);
static void nt_stream_upstream_save_round_robin_peer_session(
    nt_peer_connection_t *pc, void *data);
static nt_int_t nt_stream_upstream_empty_set_session(
    nt_peer_connection_t *pc, void *data);
static void nt_stream_upstream_empty_save_session(nt_peer_connection_t *pc,
    void *data);

#endif


nt_int_t
nt_stream_upstream_init_round_robin(nt_conf_t *cf,
    nt_stream_upstream_srv_conf_t *us)
{
    nt_url_t                        u;
    nt_uint_t                       i, j, n, w;
    nt_stream_upstream_server_t    *server;
    nt_stream_upstream_rr_peer_t   *peer, **peerp;
    nt_stream_upstream_rr_peers_t  *peers, *backup;

    us->peer.init = nt_stream_upstream_init_round_robin_peer;

    if (us->servers) {
        server = us->servers->elts;

        n = 0;
        w = 0;

        for (i = 0; i < us->servers->nelts; i++) {
            if (server[i].backup) {
                continue;
            }

            n += server[i].naddrs;
            w += server[i].naddrs * server[i].weight;
        }

        if (n == 0) {
            nt_log_error(NT_LOG_EMERG, cf->log, 0,
                          "no servers in upstream \"%V\" in %s:%ui",
                          &us->host, us->file_name, us->line);
            return NT_ERROR;
        }

        peers = nt_pcalloc(cf->pool, sizeof(nt_stream_upstream_rr_peers_t));
        if (peers == NULL) {
            return NT_ERROR;
        }

        peer = nt_pcalloc(cf->pool, sizeof(nt_stream_upstream_rr_peer_t) * n);
        if (peer == NULL) {
            return NT_ERROR;
        }

        peers->single = (n == 1);
        peers->number = n;
        peers->weighted = (w != n);
        peers->total_weight = w;
        peers->name = &us->host;

        n = 0;
        peerp = &peers->peer;

        for (i = 0; i < us->servers->nelts; i++) {
            if (server[i].backup) {
                continue;
            }

            for (j = 0; j < server[i].naddrs; j++) {
                peer[n].sockaddr = server[i].addrs[j].sockaddr;
                peer[n].socklen = server[i].addrs[j].socklen;
                peer[n].name = server[i].addrs[j].name;
                peer[n].weight = server[i].weight;
                peer[n].effective_weight = server[i].weight;
                peer[n].current_weight = 0;
                peer[n].max_conns = server[i].max_conns;
                peer[n].max_fails = server[i].max_fails;
                peer[n].fail_timeout = server[i].fail_timeout;
                peer[n].down = server[i].down;
                peer[n].server = server[i].name;

                *peerp = &peer[n];
                peerp = &peer[n].next;
                n++;
            }
        }

        us->peer.data = peers;

        /* backup servers */

        n = 0;
        w = 0;

        for (i = 0; i < us->servers->nelts; i++) {
            if (!server[i].backup) {
                continue;
            }

            n += server[i].naddrs;
            w += server[i].naddrs * server[i].weight;
        }

        if (n == 0) {
            return NT_OK;
        }

        backup = nt_pcalloc(cf->pool, sizeof(nt_stream_upstream_rr_peers_t));
        if (backup == NULL) {
            return NT_ERROR;
        }

        peer = nt_pcalloc(cf->pool, sizeof(nt_stream_upstream_rr_peer_t) * n);
        if (peer == NULL) {
            return NT_ERROR;
        }

        peers->single = 0;
        backup->single = 0;
        backup->number = n;
        backup->weighted = (w != n);
        backup->total_weight = w;
        backup->name = &us->host;

        n = 0;
        peerp = &backup->peer;

        for (i = 0; i < us->servers->nelts; i++) {
            if (!server[i].backup) {
                continue;
            }

            for (j = 0; j < server[i].naddrs; j++) {
                peer[n].sockaddr = server[i].addrs[j].sockaddr;
                peer[n].socklen = server[i].addrs[j].socklen;
                peer[n].name = server[i].addrs[j].name;
                peer[n].weight = server[i].weight;
                peer[n].effective_weight = server[i].weight;
                peer[n].current_weight = 0;
                peer[n].max_conns = server[i].max_conns;
                peer[n].max_fails = server[i].max_fails;
                peer[n].fail_timeout = server[i].fail_timeout;
                peer[n].down = server[i].down;
                peer[n].server = server[i].name;

                *peerp = &peer[n];
                peerp = &peer[n].next;
                n++;
            }
        }

        peers->next = backup;

        return NT_OK;
    }


    /* an upstream implicitly defined by proxy_pass, etc. */

    if (us->port == 0) {
        nt_log_error(NT_LOG_EMERG, cf->log, 0,
                      "no port in upstream \"%V\" in %s:%ui",
                      &us->host, us->file_name, us->line);
        return NT_ERROR;
    }

    nt_memzero(&u, sizeof(nt_url_t));

    u.host = us->host;
    u.port = us->port;

    if (nt_inet_resolve_host(cf->pool, &u) != NT_OK) {
        if (u.err) {
            nt_log_error(NT_LOG_EMERG, cf->log, 0,
                          "%s in upstream \"%V\" in %s:%ui",
                          u.err, &us->host, us->file_name, us->line);
        }

        return NT_ERROR;
    }

    n = u.naddrs;

    peers = nt_pcalloc(cf->pool, sizeof(nt_stream_upstream_rr_peers_t));
    if (peers == NULL) {
        return NT_ERROR;
    }

    peer = nt_pcalloc(cf->pool, sizeof(nt_stream_upstream_rr_peer_t) * n);
    if (peer == NULL) {
        return NT_ERROR;
    }

    peers->single = (n == 1);
    peers->number = n;
    peers->weighted = 0;
    peers->total_weight = n;
    peers->name = &us->host;

    peerp = &peers->peer;

    for (i = 0; i < u.naddrs; i++) {
        peer[i].sockaddr = u.addrs[i].sockaddr;
        peer[i].socklen = u.addrs[i].socklen;
        peer[i].name = u.addrs[i].name;
        peer[i].weight = 1;
        peer[i].effective_weight = 1;
        peer[i].current_weight = 0;
        peer[i].max_conns = 0;
        peer[i].max_fails = 1;
        peer[i].fail_timeout = 10;
        *peerp = &peer[i];
        peerp = &peer[i].next;
    }

    us->peer.data = peers;

    /* implicitly defined upstream has no backup servers */

    return NT_OK;
}


nt_int_t
nt_stream_upstream_init_round_robin_peer(nt_stream_session_t *s,
    nt_stream_upstream_srv_conf_t *us)
{
    nt_uint_t                           n;
    nt_stream_upstream_rr_peer_data_t  *rrp;

    rrp = s->upstream->peer.data;

    if (rrp == NULL) {
        rrp = nt_palloc(s->connection->pool,
                         sizeof(nt_stream_upstream_rr_peer_data_t));
        if (rrp == NULL) {
            return NT_ERROR;
        }

        s->upstream->peer.data = rrp;
    }

    rrp->peers = us->peer.data;
    rrp->current = NULL;
    rrp->config = 0;

    n = rrp->peers->number;

    if (rrp->peers->next && rrp->peers->next->number > n) {
        n = rrp->peers->next->number;
    }

    if (n <= 8 * sizeof(uintptr_t)) {
        rrp->tried = &rrp->data;
        rrp->data = 0;

    } else {
        n = (n + (8 * sizeof(uintptr_t) - 1)) / (8 * sizeof(uintptr_t));

        rrp->tried = nt_pcalloc(s->connection->pool, n * sizeof(uintptr_t));
        if (rrp->tried == NULL) {
            return NT_ERROR;
        }
    }

    s->upstream->peer.get = nt_stream_upstream_get_round_robin_peer;
    s->upstream->peer.free = nt_stream_upstream_free_round_robin_peer;
    s->upstream->peer.notify = nt_stream_upstream_notify_round_robin_peer;
    s->upstream->peer.tries = nt_stream_upstream_tries(rrp->peers);
#if (NT_STREAM_SSL)
    s->upstream->peer.set_session =
                             nt_stream_upstream_set_round_robin_peer_session;
    s->upstream->peer.save_session =
                             nt_stream_upstream_save_round_robin_peer_session;
#endif

    return NT_OK;
}


nt_int_t
nt_stream_upstream_create_round_robin_peer(nt_stream_session_t *s,
    nt_stream_upstream_resolved_t *ur)
{
    u_char                              *p;
    size_t                               len;
    socklen_t                            socklen;
    nt_uint_t                           i, n;
    struct sockaddr                     *sockaddr;
    nt_stream_upstream_rr_peer_t       *peer, **peerp;
    nt_stream_upstream_rr_peers_t      *peers;
    nt_stream_upstream_rr_peer_data_t  *rrp;

    rrp = s->upstream->peer.data;

    if (rrp == NULL) {
        rrp = nt_palloc(s->connection->pool,
                         sizeof(nt_stream_upstream_rr_peer_data_t));
        if (rrp == NULL) {
            return NT_ERROR;
        }

        s->upstream->peer.data = rrp;
    }

    peers = nt_pcalloc(s->connection->pool,
                        sizeof(nt_stream_upstream_rr_peers_t));
    if (peers == NULL) {
        return NT_ERROR;
    }

    peer = nt_pcalloc(s->connection->pool,
                       sizeof(nt_stream_upstream_rr_peer_t) * ur->naddrs);
    if (peer == NULL) {
        return NT_ERROR;
    }

    peers->single = (ur->naddrs == 1);
    peers->number = ur->naddrs;
    peers->name = &ur->host;

    if (ur->sockaddr) {
        peer[0].sockaddr = ur->sockaddr;
        peer[0].socklen = ur->socklen;
        peer[0].name = ur->name;
        peer[0].weight = 1;
        peer[0].effective_weight = 1;
        peer[0].current_weight = 0;
        peer[0].max_conns = 0;
        peer[0].max_fails = 1;
        peer[0].fail_timeout = 10;
        peers->peer = peer;

    } else {
        peerp = &peers->peer;

        for (i = 0; i < ur->naddrs; i++) {

            socklen = ur->addrs[i].socklen;

            sockaddr = nt_palloc(s->connection->pool, socklen);
            if (sockaddr == NULL) {
                return NT_ERROR;
            }

            nt_memcpy(sockaddr, ur->addrs[i].sockaddr, socklen);
            nt_inet_set_port(sockaddr, ur->port);

            p = nt_pnalloc(s->connection->pool, NT_SOCKADDR_STRLEN);
            if (p == NULL) {
                return NT_ERROR;
            }

            len = nt_sock_ntop(sockaddr, socklen, p, NT_SOCKADDR_STRLEN, 1);

            peer[i].sockaddr = sockaddr;
            peer[i].socklen = socklen;
            peer[i].name.len = len;
            peer[i].name.data = p;
            peer[i].weight = 1;
            peer[i].effective_weight = 1;
            peer[i].current_weight = 0;
            peer[i].max_conns = 0;
            peer[i].max_fails = 1;
            peer[i].fail_timeout = 10;
            *peerp = &peer[i];
            peerp = &peer[i].next;
        }
    }

    rrp->peers = peers;
    rrp->current = NULL;
    rrp->config = 0;

    if (rrp->peers->number <= 8 * sizeof(uintptr_t)) {
        rrp->tried = &rrp->data;
        rrp->data = 0;

    } else {
        n = (rrp->peers->number + (8 * sizeof(uintptr_t) - 1))
                / (8 * sizeof(uintptr_t));

        rrp->tried = nt_pcalloc(s->connection->pool, n * sizeof(uintptr_t));
        if (rrp->tried == NULL) {
            return NT_ERROR;
        }
    }

    s->upstream->peer.get = nt_stream_upstream_get_round_robin_peer;
    s->upstream->peer.free = nt_stream_upstream_free_round_robin_peer;
    s->upstream->peer.tries = nt_stream_upstream_tries(rrp->peers);
#if (NT_STREAM_SSL)
    s->upstream->peer.set_session = nt_stream_upstream_empty_set_session;
    s->upstream->peer.save_session = nt_stream_upstream_empty_save_session;
#endif

    return NT_OK;
}


nt_int_t
nt_stream_upstream_get_round_robin_peer(nt_peer_connection_t *pc, void *data)
{
    nt_stream_upstream_rr_peer_data_t *rrp = data;

    nt_int_t                        rc;
    nt_uint_t                       i, n;
    nt_stream_upstream_rr_peer_t   *peer;
    nt_stream_upstream_rr_peers_t  *peers;

    nt_log_debug1(NT_LOG_DEBUG_STREAM, pc->log, 0,
                   "get rr peer, try: %ui", pc->tries);

    pc->connection = NULL;

    peers = rrp->peers;
    nt_stream_upstream_rr_peers_wlock(peers);

    if (peers->single) {
        peer = peers->peer;

        if (peer->down) {
            goto failed;
        }

        if (peer->max_conns && peer->conns >= peer->max_conns) {
            goto failed;
        }

        rrp->current = peer;

    } else {

        /* there are several peers */

        peer = nt_stream_upstream_get_peer(rrp);

        if (peer == NULL) {
            goto failed;
        }

        nt_log_debug2(NT_LOG_DEBUG_STREAM, pc->log, 0,
                       "get rr peer, current: %p %i",
                       peer, peer->current_weight);
    }

    pc->sockaddr = peer->sockaddr;
    pc->socklen = peer->socklen;
    pc->name = &peer->name;

    peer->conns++;

    nt_stream_upstream_rr_peers_unlock(peers);

    return NT_OK;

failed:

    if (peers->next) {

        nt_log_debug0(NT_LOG_DEBUG_STREAM, pc->log, 0, "backup servers");

        rrp->peers = peers->next;

        n = (rrp->peers->number + (8 * sizeof(uintptr_t) - 1))
                / (8 * sizeof(uintptr_t));

        for (i = 0; i < n; i++) {
            rrp->tried[i] = 0;
        }

        nt_stream_upstream_rr_peers_unlock(peers);

        rc = nt_stream_upstream_get_round_robin_peer(pc, rrp);

        if (rc != NT_BUSY) {
            return rc;
        }

        nt_stream_upstream_rr_peers_wlock(peers);
    }

    nt_stream_upstream_rr_peers_unlock(peers);

    pc->name = peers->name;

    return NT_BUSY;
}


static nt_stream_upstream_rr_peer_t *
nt_stream_upstream_get_peer(nt_stream_upstream_rr_peer_data_t *rrp)
{
    time_t                          now;
    uintptr_t                       m;
    nt_int_t                       total;
    nt_uint_t                      i, n, p;
    nt_stream_upstream_rr_peer_t  *peer, *best;

    now = nt_time();

    best = NULL;
    total = 0;

#if (NT_SUPPRESS_WARN)
    p = 0;
#endif

    for (peer = rrp->peers->peer, i = 0;
         peer;
         peer = peer->next, i++)
    {
        n = i / (8 * sizeof(uintptr_t));
        m = (uintptr_t) 1 << i % (8 * sizeof(uintptr_t));

        if (rrp->tried[n] & m) {
            continue;
        }

        if (peer->down) {
            continue;
        }

        if (peer->max_fails
            && peer->fails >= peer->max_fails
            && now - peer->checked <= peer->fail_timeout)
        {
            continue;
        }

        if (peer->max_conns && peer->conns >= peer->max_conns) {
            continue;
        }

        peer->current_weight += peer->effective_weight;
        total += peer->effective_weight;

        if (peer->effective_weight < peer->weight) {
            peer->effective_weight++;
        }

        if (best == NULL || peer->current_weight > best->current_weight) {
            best = peer;
            p = i;
        }
    }

    if (best == NULL) {
        return NULL;
    }

    rrp->current = best;

    n = p / (8 * sizeof(uintptr_t));
    m = (uintptr_t) 1 << p % (8 * sizeof(uintptr_t));

    rrp->tried[n] |= m;

    best->current_weight -= total;

    if (now - best->checked > best->fail_timeout) {
        best->checked = now;
    }

    return best;
}


void
nt_stream_upstream_free_round_robin_peer(nt_peer_connection_t *pc, void *data,
    nt_uint_t state)
{
    nt_stream_upstream_rr_peer_data_t  *rrp = data;

    time_t                          now;
    nt_stream_upstream_rr_peer_t  *peer;

    nt_log_debug2(NT_LOG_DEBUG_STREAM, pc->log, 0,
                   "free rr peer %ui %ui", pc->tries, state);

    peer = rrp->current;

    nt_stream_upstream_rr_peers_rlock(rrp->peers);
    nt_stream_upstream_rr_peer_lock(rrp->peers, peer);

    if (rrp->peers->single) {
        peer->conns--;

        nt_stream_upstream_rr_peer_unlock(rrp->peers, peer);
        nt_stream_upstream_rr_peers_unlock(rrp->peers);

        pc->tries = 0;
        return;
    }

    if (state & NT_PEER_FAILED) {
        now = nt_time();

        peer->fails++;
        peer->accessed = now;
        peer->checked = now;

        if (peer->max_fails) {
            peer->effective_weight -= peer->weight / peer->max_fails;

            if (peer->fails >= peer->max_fails) {
                nt_log_error(NT_LOG_WARN, pc->log, 0,
                              "upstream server temporarily disabled");
            }
        }

        nt_log_debug2(NT_LOG_DEBUG_STREAM, pc->log, 0,
                       "free rr peer failed: %p %i",
                       peer, peer->effective_weight);

        if (peer->effective_weight < 0) {
            peer->effective_weight = 0;
        }

    } else {

        /* mark peer live if check passed */

        if (peer->accessed < peer->checked) {
            peer->fails = 0;
        }
    }

    peer->conns--;

    nt_stream_upstream_rr_peer_unlock(rrp->peers, peer);
    nt_stream_upstream_rr_peers_unlock(rrp->peers);

    if (pc->tries) {
        pc->tries--;
    }
}


static void
nt_stream_upstream_notify_round_robin_peer(nt_peer_connection_t *pc,
    void *data, nt_uint_t type)
{
    nt_stream_upstream_rr_peer_data_t  *rrp = data;

    nt_stream_upstream_rr_peer_t  *peer;

    peer = rrp->current;

    if (type == NT_STREAM_UPSTREAM_NOTIFY_CONNECT
        && pc->connection->type == SOCK_STREAM)
    {
        nt_stream_upstream_rr_peers_rlock(rrp->peers);
        nt_stream_upstream_rr_peer_lock(rrp->peers, peer);

        if (peer->accessed < peer->checked) {
            peer->fails = 0;
        }

        nt_stream_upstream_rr_peer_unlock(rrp->peers, peer);
        nt_stream_upstream_rr_peers_unlock(rrp->peers);
    }
}


#if (NT_STREAM_SSL)

static nt_int_t
nt_stream_upstream_set_round_robin_peer_session(nt_peer_connection_t *pc,
    void *data)
{
    nt_stream_upstream_rr_peer_data_t  *rrp = data;

    nt_int_t                        rc;
    nt_ssl_session_t               *ssl_session;
    nt_stream_upstream_rr_peer_t   *peer;
#if (NT_STREAM_UPSTREAM_ZONE)
    int                              len;
    const u_char                    *p;
    nt_stream_upstream_rr_peers_t  *peers;
    u_char                           buf[NT_SSL_MAX_SESSION_SIZE];
#endif

    peer = rrp->current;

#if (NT_STREAM_UPSTREAM_ZONE)
    peers = rrp->peers;

    if (peers->shpool) {
        nt_stream_upstream_rr_peers_rlock(peers);
        nt_stream_upstream_rr_peer_lock(peers, peer);

        if (peer->ssl_session == NULL) {
            nt_stream_upstream_rr_peer_unlock(peers, peer);
            nt_stream_upstream_rr_peers_unlock(peers);
            return NT_OK;
        }

        len = peer->ssl_session_len;

        nt_memcpy(buf, peer->ssl_session, len);

        nt_stream_upstream_rr_peer_unlock(peers, peer);
        nt_stream_upstream_rr_peers_unlock(peers);

        p = buf;
        ssl_session = d2i_SSL_SESSION(NULL, &p, len);

        rc = nt_ssl_set_session(pc->connection, ssl_session);

        nt_log_debug1(NT_LOG_DEBUG_STREAM, pc->log, 0,
                       "set session: %p", ssl_session);

        nt_ssl_free_session(ssl_session);

        return rc;
    }
#endif

    ssl_session = peer->ssl_session;

    rc = nt_ssl_set_session(pc->connection, ssl_session);

    nt_log_debug1(NT_LOG_DEBUG_STREAM, pc->log, 0,
                   "set session: %p", ssl_session);

    return rc;
}


static void
nt_stream_upstream_save_round_robin_peer_session(nt_peer_connection_t *pc,
    void *data)
{
    nt_stream_upstream_rr_peer_data_t  *rrp = data;

    nt_ssl_session_t               *old_ssl_session, *ssl_session;
    nt_stream_upstream_rr_peer_t   *peer;
#if (NT_STREAM_UPSTREAM_ZONE)
    int                              len;
    u_char                          *p;
    nt_stream_upstream_rr_peers_t  *peers;
    u_char                           buf[NT_SSL_MAX_SESSION_SIZE];
#endif

#if (NT_STREAM_UPSTREAM_ZONE)
    peers = rrp->peers;

    if (peers->shpool) {

        ssl_session = nt_ssl_get0_session(pc->connection);

        if (ssl_session == NULL) {
            return;
        }

        nt_log_debug1(NT_LOG_DEBUG_STREAM, pc->log, 0,
                       "save session: %p", ssl_session);

        len = i2d_SSL_SESSION(ssl_session, NULL);

        /* do not cache too big session */

        if (len > NT_SSL_MAX_SESSION_SIZE) {
            return;
        }

        p = buf;
        (void) i2d_SSL_SESSION(ssl_session, &p);

        peer = rrp->current;

        nt_stream_upstream_rr_peers_rlock(peers);
        nt_stream_upstream_rr_peer_lock(peers, peer);

        if (len > peer->ssl_session_len) {
            nt_shmtx_lock(&peers->shpool->mutex);

            if (peer->ssl_session) {
                nt_slab_free_locked(peers->shpool, peer->ssl_session);
            }

            peer->ssl_session = nt_slab_alloc_locked(peers->shpool, len);

            nt_shmtx_unlock(&peers->shpool->mutex);

            if (peer->ssl_session == NULL) {
                peer->ssl_session_len = 0;

                nt_stream_upstream_rr_peer_unlock(peers, peer);
                nt_stream_upstream_rr_peers_unlock(peers);
                return;
            }

            peer->ssl_session_len = len;
        }

        nt_memcpy(peer->ssl_session, buf, len);

        nt_stream_upstream_rr_peer_unlock(peers, peer);
        nt_stream_upstream_rr_peers_unlock(peers);

        return;
    }
#endif

    ssl_session = nt_ssl_get_session(pc->connection);

    if (ssl_session == NULL) {
        return;
    }

    nt_log_debug1(NT_LOG_DEBUG_STREAM, pc->log, 0,
                   "save session: %p", ssl_session);

    peer = rrp->current;

    old_ssl_session = peer->ssl_session;
    peer->ssl_session = ssl_session;

    if (old_ssl_session) {

        nt_log_debug1(NT_LOG_DEBUG_STREAM, pc->log, 0,
                       "old session: %p", old_ssl_session);

        /* TODO: may block */

        nt_ssl_free_session(old_ssl_session);
    }
}


static nt_int_t
nt_stream_upstream_empty_set_session(nt_peer_connection_t *pc, void *data)
{
    return NT_OK;
}


static void
nt_stream_upstream_empty_save_session(nt_peer_connection_t *pc, void *data)
{
    return;
}

#endif
