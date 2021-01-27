

#ifndef _NT_STREAM_UPSTREAM_ROUND_ROBIN_H_INCLUDED_
#define _NT_STREAM_UPSTREAM_ROUND_ROBIN_H_INCLUDED_


#include <nt_core.h>
#include <nt_stream.h>


typedef struct nt_stream_upstream_rr_peer_s   nt_stream_upstream_rr_peer_t;

struct nt_stream_upstream_rr_peer_s {
    struct sockaddr                 *sockaddr;
    socklen_t                        socklen;
    nt_str_t                        name;
    nt_str_t                        server;

    nt_int_t                        current_weight;
    nt_int_t                        effective_weight;
    nt_int_t                        weight;

    nt_uint_t                       conns;
    nt_uint_t                       max_conns;

    nt_uint_t                       fails;
    time_t                           accessed;
    time_t                           checked;

    nt_uint_t                       max_fails;
    time_t                           fail_timeout;
    nt_msec_t                       slow_start;
    nt_msec_t                       start_time;

    nt_uint_t                       down;

    void                            *ssl_session;
    int                              ssl_session_len;

#if (NT_STREAM_UPSTREAM_ZONE)
    nt_atomic_t                     lock;
#endif

    nt_stream_upstream_rr_peer_t   *next;

    NT_COMPAT_BEGIN(25)
    NT_COMPAT_END
};


typedef struct nt_stream_upstream_rr_peers_s  nt_stream_upstream_rr_peers_t;

struct nt_stream_upstream_rr_peers_s {
    nt_uint_t                       number;

#if (NT_STREAM_UPSTREAM_ZONE)
    nt_slab_pool_t                 *shpool;
    nt_atomic_t                     rwlock;
    nt_stream_upstream_rr_peers_t  *zone_next;
#endif

    nt_uint_t                       total_weight;

    unsigned                         single:1;
    unsigned                         weighted:1;

    nt_str_t                       *name;

    nt_stream_upstream_rr_peers_t  *next;

    nt_stream_upstream_rr_peer_t   *peer;
};


#if (NT_STREAM_UPSTREAM_ZONE)

#define nt_stream_upstream_rr_peers_rlock(peers)                             \
                                                                              \
    if (peers->shpool) {                                                      \
        nt_rwlock_rlock(&peers->rwlock);                                     \
    }

#define nt_stream_upstream_rr_peers_wlock(peers)                             \
                                                                              \
    if (peers->shpool) {                                                      \
        nt_rwlock_wlock(&peers->rwlock);                                     \
    }

#define nt_stream_upstream_rr_peers_unlock(peers)                            \
                                                                              \
    if (peers->shpool) {                                                      \
        nt_rwlock_unlock(&peers->rwlock);                                    \
    }


#define nt_stream_upstream_rr_peer_lock(peers, peer)                         \
                                                                              \
    if (peers->shpool) {                                                      \
        nt_rwlock_wlock(&peer->lock);                                        \
    }

#define nt_stream_upstream_rr_peer_unlock(peers, peer)                       \
                                                                              \
    if (peers->shpool) {                                                      \
        nt_rwlock_unlock(&peer->lock);                                       \
    }

#else

#define nt_stream_upstream_rr_peers_rlock(peers)
#define nt_stream_upstream_rr_peers_wlock(peers)
#define nt_stream_upstream_rr_peers_unlock(peers)
#define nt_stream_upstream_rr_peer_lock(peers, peer)
#define nt_stream_upstream_rr_peer_unlock(peers, peer)

#endif


typedef struct {
    nt_uint_t                       config;
    nt_stream_upstream_rr_peers_t  *peers;
    nt_stream_upstream_rr_peer_t   *current;
    uintptr_t                       *tried;
    uintptr_t                        data;
} nt_stream_upstream_rr_peer_data_t;


nt_int_t nt_stream_upstream_init_round_robin(nt_conf_t *cf,
    nt_stream_upstream_srv_conf_t *us);
nt_int_t nt_stream_upstream_init_round_robin_peer(nt_stream_session_t *s,
    nt_stream_upstream_srv_conf_t *us);
nt_int_t nt_stream_upstream_create_round_robin_peer(nt_stream_session_t *s,
    nt_stream_upstream_resolved_t *ur);
nt_int_t nt_stream_upstream_get_round_robin_peer(nt_peer_connection_t *pc,
    void *data);
void nt_stream_upstream_free_round_robin_peer(nt_peer_connection_t *pc,
    void *data, nt_uint_t state);


#endif /* _NT_STREAM_UPSTREAM_ROUND_ROBIN_H_INCLUDED_ */
