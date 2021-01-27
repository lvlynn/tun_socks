
#include <nt_core.h>


#ifndef _NT_RESOLVER_H_INCLUDED_
#define _NT_RESOLVER_H_INCLUDED_


#define NT_RESOLVE_A         1
#define NT_RESOLVE_CNAME     5
#define NT_RESOLVE_PTR       12
#define NT_RESOLVE_MX        15
#define NT_RESOLVE_TXT       16
#if (NT_HAVE_INET6)
#define NT_RESOLVE_AAAA      28
#endif
#define NT_RESOLVE_SRV       33
#define NT_RESOLVE_DNAME     39

#define NT_RESOLVE_FORMERR   1
#define NT_RESOLVE_SERVFAIL  2
#define NT_RESOLVE_NXDOMAIN  3
#define NT_RESOLVE_NOTIMP    4
#define NT_RESOLVE_REFUSED   5
#define NT_RESOLVE_TIMEDOUT  NT_ETIMEDOUT


#define NT_NO_RESOLVER       (void *) -1

#define NT_RESOLVER_MAX_RECURSION    50


typedef struct nt_resolver_s  nt_resolver_t;


typedef struct {
    nt_connection_t         *udp;
    nt_connection_t         *tcp;
    struct sockaddr          *sockaddr;
    socklen_t                 socklen;
    nt_str_t                 server;
    nt_log_t                 log;
    nt_buf_t                *read_buf;
    nt_buf_t                *write_buf;
    nt_resolver_t           *resolver;
} nt_resolver_connection_t;


typedef struct nt_resolver_ctx_s  nt_resolver_ctx_t;

typedef void (*nt_resolver_handler_pt)(nt_resolver_ctx_t *ctx);


typedef struct {
    struct sockaddr          *sockaddr;
    socklen_t                 socklen;
    nt_str_t                 name;
    u_short                   priority;
    u_short                   weight;
} nt_resolver_addr_t;


typedef struct {
    nt_str_t                 name;
    u_short                   priority;
    u_short                   weight;
    u_short                   port;
} nt_resolver_srv_t;


typedef struct {
    nt_str_t                 name;
    u_short                   priority;
    u_short                   weight;
    u_short                   port;

    nt_resolver_ctx_t       *ctx;
    nt_int_t                 state;

    nt_uint_t                naddrs;
    nt_addr_t               *addrs;
} nt_resolver_srv_name_t;


typedef struct {
    nt_rbtree_node_t         node;
    nt_queue_t               queue;

    /* PTR: resolved name, A: name to resolve */
    u_char                   *name;

#if (NT_HAVE_INET6)
    /* PTR: IPv6 address to resolve (IPv4 address is in rbtree node key) */
    struct in6_addr           addr6;
#endif

    u_short                   nlen;
    u_short                   qlen;

    u_char                   *query;
#if (NT_HAVE_INET6)
    u_char                   *query6;
#endif

    union {
        in_addr_t             addr;
        in_addr_t            *addrs;
        u_char               *cname;
        nt_resolver_srv_t   *srvs;
    } u;

    u_char                    code;
    u_short                   naddrs;
    u_short                   nsrvs;
    u_short                   cnlen;

#if (NT_HAVE_INET6)
    union {
        struct in6_addr       addr6;
        struct in6_addr      *addrs6;
    } u6;

    u_short                   naddrs6;
#endif

    time_t                    expire;
    time_t                    valid;
    uint32_t                  ttl;

    unsigned                  tcp:1;
#if (NT_HAVE_INET6)
    unsigned                  tcp6:1;
#endif

    nt_uint_t                last_connection;

    nt_resolver_ctx_t       *waiting;
} nt_resolver_node_t;


struct nt_resolver_s {
    /* has to be pointer because of "incomplete type" */
    nt_event_t              *event;
    void                     *dummy;
    nt_log_t                *log;

    /* event ident must be after 3 pointers as in nt_connection_t */
    nt_int_t                 ident;

    /* simple round robin DNS peers balancer */
    nt_array_t               connections;
    nt_uint_t                last_connection;

    nt_rbtree_t              name_rbtree;
    nt_rbtree_node_t         name_sentinel;

    nt_rbtree_t              srv_rbtree;
    nt_rbtree_node_t         srv_sentinel;

    nt_rbtree_t              addr_rbtree;
    nt_rbtree_node_t         addr_sentinel;

    nt_queue_t               name_resend_queue;
    nt_queue_t               srv_resend_queue;
    nt_queue_t               addr_resend_queue;

    nt_queue_t               name_expire_queue;
    nt_queue_t               srv_expire_queue;
    nt_queue_t               addr_expire_queue;

#if (NT_HAVE_INET6)
    nt_uint_t                ipv6;                 /* unsigned  ipv6:1; */
    nt_rbtree_t              addr6_rbtree;
    nt_rbtree_node_t         addr6_sentinel;
    nt_queue_t               addr6_resend_queue;
    nt_queue_t               addr6_expire_queue;
#endif

    time_t                    resend_timeout;
    time_t                    tcp_timeout;
    time_t                    expire;
    time_t                    valid;

    nt_uint_t                log_level;
};


struct nt_resolver_ctx_s {
    nt_resolver_ctx_t       *next;
    nt_resolver_t           *resolver;
    nt_resolver_node_t      *node;

    /* event ident must be after 3 pointers as in nt_connection_t */
    nt_int_t                 ident;

    nt_int_t                 state;
    nt_str_t                 name;
    nt_str_t                 service;

    time_t                    valid;
    nt_uint_t                naddrs;
    nt_resolver_addr_t      *addrs;
    nt_resolver_addr_t       addr;
    struct sockaddr_in        sin;

    nt_uint_t                count;
    nt_uint_t                nsrvs;
    nt_resolver_srv_name_t  *srvs;

    nt_resolver_handler_pt   handler;
    void                     *data;
    nt_msec_t                timeout;

    unsigned                  quick:1;
    unsigned                  async:1;
    unsigned                  cancelable:1;
    nt_uint_t                recursion;
    nt_event_t              *event;
};


#if (T_NT_RESOLVER_FILE)
nt_int_t nt_resolver_read_resolv_file(nt_conf_t *cf, nt_str_t *filename,
    nt_str_t **names, nt_uint_t *n);
#endif
nt_resolver_t *nt_resolver_create(nt_conf_t *cf, nt_str_t *names,
    nt_uint_t n);
nt_resolver_ctx_t *nt_resolve_start(nt_resolver_t *r,
    nt_resolver_ctx_t *temp);
nt_int_t nt_resolve_name(nt_resolver_ctx_t *ctx);
void nt_resolve_name_done(nt_resolver_ctx_t *ctx);
nt_int_t nt_resolve_addr(nt_resolver_ctx_t *ctx);
void nt_resolve_addr_done(nt_resolver_ctx_t *ctx);
char *nt_resolver_strerror(nt_int_t err);


#endif /* _NT_RESOLVER_H_INCLUDED_ */
