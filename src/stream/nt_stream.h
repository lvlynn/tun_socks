

#ifndef _NT_STREAM_H_INCLUDED_
#define _NT_STREAM_H_INCLUDED_


#include <nt_core.h>

#if (NT_STREAM_SSL)
#include <nt_stream_ssl_module.h>
#endif


typedef struct nt_stream_session_s  nt_stream_session_t;


#include <nt_stream_variables.h>
#include <nt_stream_script.h>
#include <nt_stream_upstream.h>
#include <nt_stream_upstream_round_robin.h>


#define NT_STREAM_OK                        200
#define NT_STREAM_BAD_REQUEST               400
#define NT_STREAM_FORBIDDEN                 403
#define NT_STREAM_INTERNAL_SERVER_ERROR     500
#define NT_STREAM_BAD_GATEWAY               502
#define NT_STREAM_SERVICE_UNAVAILABLE       503


typedef struct {
    void                         **main_conf;
    void                         **srv_conf;
} nt_stream_conf_ctx_t;


typedef struct {
    struct sockaddr               *sockaddr;
    socklen_t                      socklen;
    nt_str_t                      addr_text;

    /* server ctx */
    nt_stream_conf_ctx_t         *ctx;

    unsigned                       bind:1;
    unsigned                       wildcard:1;
    unsigned                       ssl:1;
#if (NT_HAVE_INET6)
    unsigned                       ipv6only:1;
#endif
    unsigned                       reuseport:1;
    unsigned                       so_keepalive:2;
    unsigned                       proxy_protocol:1;
#if (NT_HAVE_KEEPALIVE_TUNABLE)
    int                            tcp_keepidle;
    int                            tcp_keepintvl;
    int                            tcp_keepcnt;
#endif
    int                            backlog;
    int                            rcvbuf;
    int                            sndbuf;
    int                            type;
#if (NT_STREAM_SNI)
    unsigned                       default_server;
#endif
} nt_stream_listen_t;


typedef struct {
    nt_stream_conf_ctx_t         *ctx;
    nt_str_t                      addr_text;
    unsigned                       ssl:1;
    unsigned                       proxy_protocol:1;
#if (NT_STREAM_SNI)
    void                          *default_server;
    void                          *virtual_names;
#endif
} nt_stream_addr_conf_t;

typedef struct {
    in_addr_t                      addr;
    nt_stream_addr_conf_t         conf;
} nt_stream_in_addr_t;


#if (NT_HAVE_INET6)

typedef struct {
    struct in6_addr                addr6;
    nt_stream_addr_conf_t         conf;
} nt_stream_in6_addr_t;

#endif


typedef struct {
    /* nt_stream_in_addr_t or nt_stream_in6_addr_t */
    void                          *addrs;
    nt_uint_t                     naddrs;
} nt_stream_port_t;


typedef struct {
    int                            family;
    int                            type;
    in_port_t                      port;
    nt_array_t                    addrs; /* array of nt_stream_conf_addr_t */
} nt_stream_conf_port_t;


typedef struct {
    nt_stream_listen_t            opt;
#if (NT_STREAM_SNI)
    void                          *default_server;
    nt_array_t                    servers;  /* array of nt_stream_core_srv_conf_t */
    nt_hash_t                     hash;
    nt_hash_wildcard_t           *wc_head;
    nt_hash_wildcard_t           *wc_tail;
#endif
} nt_stream_conf_addr_t;


typedef enum {
    NT_STREAM_POST_ACCEPT_PHASE = 0,
    NT_STREAM_PREACCESS_PHASE,
    NT_STREAM_ACCESS_PHASE,
    NT_STREAM_SSL_PHASE,
    NT_STREAM_PREREAD_PHASE,
    NT_STREAM_CONTENT_PHASE,
    NT_STREAM_LOG_PHASE
} nt_stream_phases;


typedef struct nt_stream_phase_handler_s  nt_stream_phase_handler_t;

typedef nt_int_t (*nt_stream_phase_handler_pt)(nt_stream_session_t *s,
    nt_stream_phase_handler_t *ph);
typedef nt_int_t (*nt_stream_handler_pt)(nt_stream_session_t *s);
typedef void (*nt_stream_content_handler_pt)(nt_stream_session_t *s);


struct nt_stream_phase_handler_s {
    nt_stream_phase_handler_pt    checker;
    nt_stream_handler_pt          handler;
    nt_uint_t                     next;
};


typedef struct {
    nt_stream_phase_handler_t    *handlers;
} nt_stream_phase_engine_t;


typedef struct {
    nt_array_t                    handlers;
} nt_stream_phase_t;


typedef struct {
    nt_array_t                    servers;     /* nt_stream_core_srv_conf_t */
    nt_array_t                    listen;      /* nt_stream_listen_t */

    nt_stream_phase_engine_t      phase_engine;

    nt_hash_t                     variables_hash;

    nt_array_t                    variables;        /* nt_stream_variable_t */
    nt_array_t                    prefix_variables; /* nt_stream_variable_t */
    nt_uint_t                     ncaptures;

    nt_uint_t                     variables_hash_max_size;
    nt_uint_t                     variables_hash_bucket_size;

    nt_hash_keys_arrays_t        *variables_keys;

    nt_stream_phase_t             phases[NT_STREAM_LOG_PHASE + 1];

#if (NT_STREAM_SNI)
    nt_uint_t                     server_names_hash_max_size;
    nt_uint_t                     server_names_hash_bucket_size;
#endif
} nt_stream_core_main_conf_t;


typedef struct {
    nt_stream_content_handler_pt  handler;

    nt_stream_conf_ctx_t         *ctx;

    u_char                        *file_name;
    nt_uint_t                     line;

    nt_flag_t                     tcp_nodelay;
    size_t                         preread_buffer_size;
    nt_msec_t                     preread_timeout;

    nt_log_t                     *error_log;

    nt_msec_t                     resolver_timeout;
    nt_resolver_t                *resolver;

    nt_msec_t                     proxy_protocol_timeout;

    nt_uint_t                     listen;  /* unsigned  listen:1; */

#if (NT_STREAM_SNI)
    /* array of the nt_stream_server_name_t, "server_name" directive */
    nt_array_t                    server_names;
    nt_str_t                      server_name;
#endif
} nt_stream_core_srv_conf_t;

#if (NT_STREAM_SNI)
/* list of structures to find core_srv_conf quickly at run time */
typedef struct {
    nt_stream_core_srv_conf_t  *server;   /* virtual name server conf */
    nt_str_t                    name;
} nt_stream_server_name_t;

typedef struct {
    nt_hash_combined_t        names;
} nt_stream_virtual_names_t;
#endif

struct nt_stream_session_s {
    uint32_t                       signature;         /* "STRM" */

    nt_connection_t              *connection;

    off_t                          received;
    time_t                         start_sec;
    nt_msec_t                     start_msec;

    nt_log_handler_pt             log_handler;

    void                         **ctx;
    void                         **main_conf;
    void                         **srv_conf;

    nt_stream_upstream_t         *upstream;
    nt_array_t                   *upstream_states;
                                           /* of nt_stream_upstream_state_t */
    nt_stream_variable_value_t   *variables;

#if (NT_PCRE)
    nt_uint_t                     ncaptures;
    int                           *captures;
    u_char                        *captures_data;
#endif

    nt_int_t                      phase_handler;
    nt_uint_t                     status;

    unsigned                       ssl:1;

    unsigned                       stat_processing:1;

    unsigned                       health_check:1;
#if (NT_STREAM_SNI)
    nt_stream_addr_conf_t        *addr_conf;
#endif

#if (T_NT_MULTI_UPSTREAM)
    nt_queue_t                   *multi_item;
    nt_queue_t                   *backend_r;
    nt_queue_t                    waiting_queue;
    nt_flag_t                     waiting;
#endif
};


typedef struct {
    nt_int_t                    (*preconfiguration)(nt_conf_t *cf);
    nt_int_t                    (*postconfiguration)(nt_conf_t *cf);

    void                        *(*create_main_conf)(nt_conf_t *cf);
    char                        *(*init_main_conf)(nt_conf_t *cf, void *conf);

    void                        *(*create_srv_conf)(nt_conf_t *cf);
    char                        *(*merge_srv_conf)(nt_conf_t *cf, void *prev,
                                                   void *conf);
} nt_stream_module_t;


#define NT_STREAM_MODULE       0x4d525453     /* "STRM" */

#define NT_STREAM_MAIN_CONF    0x02000000
#define NT_STREAM_SRV_CONF     0x04000000
#define NT_STREAM_UPS_CONF     0x08000000


#define NT_STREAM_MAIN_CONF_OFFSET  offsetof(nt_stream_conf_ctx_t, main_conf)
#define NT_STREAM_SRV_CONF_OFFSET   offsetof(nt_stream_conf_ctx_t, srv_conf)


#define nt_stream_get_module_ctx(s, module)   (s)->ctx[module.ctx_index]
#define nt_stream_set_ctx(s, c, module)       s->ctx[module.ctx_index] = c;
#define nt_stream_delete_ctx(s, module)       s->ctx[module.ctx_index] = NULL;


#define nt_stream_get_module_main_conf(s, module)                             \
    (s)->main_conf[module.ctx_index]
#define nt_stream_get_module_srv_conf(s, module)                              \
    (s)->srv_conf[module.ctx_index]

#define nt_stream_conf_get_module_main_conf(cf, module)                       \
    ((nt_stream_conf_ctx_t *) cf->ctx)->main_conf[module.ctx_index]
#define nt_stream_conf_get_module_srv_conf(cf, module)                        \
    ((nt_stream_conf_ctx_t *) cf->ctx)->srv_conf[module.ctx_index]

#define nt_stream_cycle_get_module_main_conf(cycle, module)                   \
    (cycle->conf_ctx[nt_stream_module.index] ?                                \
        ((nt_stream_conf_ctx_t *) cycle->conf_ctx[nt_stream_module.index])   \
            ->main_conf[module.ctx_index]:                                     \
        NULL)


#define NT_STREAM_WRITE_BUFFERED  0x10


void nt_stream_core_run_phases(nt_stream_session_t *s);
nt_int_t nt_stream_core_generic_phase(nt_stream_session_t *s,
    nt_stream_phase_handler_t *ph);
nt_int_t nt_stream_core_preread_phase(nt_stream_session_t *s,
    nt_stream_phase_handler_t *ph);
nt_int_t nt_stream_core_content_phase(nt_stream_session_t *s,
    nt_stream_phase_handler_t *ph);


void nt_stream_init_connection(nt_connection_t *c);
void nt_stream_session_handler(nt_event_t *rev);
void nt_stream_finalize_session(nt_stream_session_t *s, nt_uint_t rc);


extern nt_module_t  nt_stream_module;
extern nt_uint_t    nt_stream_max_module;
extern nt_module_t  nt_stream_core_module;


typedef nt_int_t (*nt_stream_filter_pt)(nt_stream_session_t *s,
    nt_chain_t *chain, nt_uint_t from_upstream);


extern nt_stream_filter_pt  nt_stream_top_filter;


#endif /* _NT_STREAM_H_INCLUDED_ */
