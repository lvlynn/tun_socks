

#ifndef _NT_STREAM_UPSTREAM_H_INCLUDED_
#define _NT_STREAM_UPSTREAM_H_INCLUDED_


#include <nt_core.h>
#include <nt_stream.h>
#include <nt_event_connect.h>


#define NT_STREAM_UPSTREAM_CREATE        0x0001
#define NT_STREAM_UPSTREAM_WEIGHT        0x0002
#define NT_STREAM_UPSTREAM_MAX_FAILS     0x0004
#define NT_STREAM_UPSTREAM_FAIL_TIMEOUT  0x0008
#define NT_STREAM_UPSTREAM_DOWN          0x0010
#define NT_STREAM_UPSTREAM_BACKUP        0x0020
#define NT_STREAM_UPSTREAM_MAX_CONNS     0x0100


#define NT_STREAM_UPSTREAM_NOTIFY_CONNECT     0x1


typedef struct {
    nt_array_t                        upstreams;
                                           /* nt_stream_upstream_srv_conf_t */
} nt_stream_upstream_main_conf_t;


typedef struct nt_stream_upstream_srv_conf_s  nt_stream_upstream_srv_conf_t;


typedef nt_int_t (*nt_stream_upstream_init_pt)(nt_conf_t *cf,
    nt_stream_upstream_srv_conf_t *us);
typedef nt_int_t (*nt_stream_upstream_init_peer_pt)(nt_stream_session_t *s,
    nt_stream_upstream_srv_conf_t *us);


typedef struct {
    nt_stream_upstream_init_pt        init_upstream;
    nt_stream_upstream_init_peer_pt   init;
    void                              *data;
} nt_stream_upstream_peer_t;


typedef struct {
    nt_str_t                          name;
    nt_addr_t                        *addrs;
    nt_uint_t                         naddrs;
    nt_uint_t                         weight;
    nt_uint_t                         max_conns;
    nt_uint_t                         max_fails;
    time_t                             fail_timeout;
    nt_msec_t                         slow_start;
    nt_uint_t                         down;

    unsigned                           backup:1;

    NT_COMPAT_BEGIN(4)
    NT_COMPAT_END
} nt_stream_upstream_server_t;


struct nt_stream_upstream_srv_conf_s {
    nt_stream_upstream_peer_t         peer;
    void                             **srv_conf;

    nt_array_t                       *servers;
                                              /* nt_stream_upstream_server_t */

    nt_uint_t                         flags;
    nt_str_t                          host;
    u_char                            *file_name;
    nt_uint_t                         line;
    in_port_t                          port;
    nt_uint_t                         no_port;  /* unsigned no_port:1 */

#if (NT_STREAM_UPSTREAM_ZONE)
    nt_shm_zone_t                    *shm_zone;
#endif
};


typedef struct {
    nt_msec_t                         response_time;
    nt_msec_t                         connect_time;
    nt_msec_t                         first_byte_time;
    off_t                              bytes_sent;
    off_t                              bytes_received;

    nt_str_t                         *peer;
} nt_stream_upstream_state_t;


typedef struct {
    nt_str_t                          host;
    in_port_t                          port;
    nt_uint_t                         no_port; /* unsigned no_port:1 */

    nt_uint_t                         naddrs;
    nt_resolver_addr_t               *addrs;

    struct sockaddr                   *sockaddr;
    socklen_t                          socklen;
    nt_str_t                          name;

    nt_resolver_ctx_t                *ctx;
} nt_stream_upstream_resolved_t;


typedef struct {
    nt_peer_connection_t              peer;

    nt_buf_t                          downstream_buf;
    nt_buf_t                          upstream_buf;

    nt_chain_t                       *free;
    nt_chain_t                       *upstream_out;
    nt_chain_t                       *upstream_busy;
    nt_chain_t                       *downstream_out;
    nt_chain_t                       *downstream_busy;

    off_t                              received;
    time_t                             start_sec;
    nt_uint_t                         requests;
    nt_uint_t                         responses;
    nt_msec_t                         start_time;

    size_t                             upload_rate;
    size_t                             download_rate;

    nt_str_t                          ssl_name;

    nt_stream_upstream_srv_conf_t    *upstream;
    nt_stream_upstream_resolved_t    *resolved;
    nt_stream_upstream_state_t       *state;
    unsigned                           connected:1;
    unsigned                           proxy_protocol:1;

#if (T_NT_MULTI_UPSTREAM)
    unsigned                           multi:1;
#endif
} nt_stream_upstream_t;


nt_stream_upstream_srv_conf_t *nt_stream_upstream_add(nt_conf_t *cf,
    nt_url_t *u, nt_uint_t flags);


#define nt_stream_conf_upstream_srv_conf(uscf, module)                       \
    uscf->srv_conf[module.ctx_index]


extern nt_module_t  nt_stream_upstream_module;


#endif /* _NT_STREAM_UPSTREAM_H_INCLUDED_ */
