#ifndef _NT_CONNECTION_H_
#define _NT_CONNECTION_H_

#include <nt_def.h>
#include <nt_core.h>


typedef struct nt_listening_s  nt_listening_t;

struct nt_listening_s {
    nt_socket_t        fd;

    struct sockaddr    *sockaddr;
    socklen_t           socklen;    /* size of sockaddr */
    size_t              addr_text_max_len;
    nt_str_t           addr_text;

    int                 type;

    int                 backlog;
    int                 rcvbuf;
    int                 sndbuf;
#if (NT_HAVE_KEEPALIVE_TUNABLE)
    int                 keepidle;
    int                 keepintvl;
    int                 keepcnt;
#endif

    /* handler of accepted connection */
    nt_connection_handler_pt   handler;

    void               *servers;  /* array of nt_http_in_addr_t, for example */

    nt_log_t           log;
    nt_log_t          *logp;

    size_t              pool_size;
    /* should be here because of the AcceptEx() preread */
    size_t              post_accept_buffer_size;
    /* should be here because of the deferred accept */
    nt_msec_t          post_accept_timeout;

    nt_listening_t    *previous;
    nt_connection_t   *connection;

    nt_rbtree_t        rbtree;
    nt_rbtree_node_t   sentinel;

    nt_uint_t          worker;

    unsigned            open:1;
    unsigned            remain:1;
    unsigned            ignore:1;

    unsigned            bound:1;       /* already bound */
    unsigned            inherited:1;   /* inherited from previous process */
    unsigned            nonblocking_accept:1;
    unsigned            listen:1;
    unsigned            nonblocking:1;
    unsigned            shared:1;    /* shared between threads or processes */
    unsigned            addr_ntop:1;
    unsigned            wildcard:1;

#if (NT_HAVE_INET6)
    unsigned            ipv6only:1;
#endif
    unsigned            reuseport:1;
    unsigned            add_reuseport:1;
    unsigned            keepalive:2;

    unsigned            deferred_accept:1;
    unsigned            delete_deferred:1;
    unsigned            add_deferred:1;
#if (NT_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    char               *accept_filter;
#endif
#if (NT_HAVE_SETFIB)
    int                 setfib;
#endif

#if (NT_HAVE_TCP_FASTOPEN)
    int                 fastopen;
#endif

};


typedef enum {
    NT_ERROR_ALERT = 0,
    NT_ERROR_ERR,
    NT_ERROR_INFO,
    NT_ERROR_IGNORE_ECONNRESET,
    NT_ERROR_IGNORE_EINVAL
} nt_connection_log_error_e;


typedef enum {
    NT_TCP_NODELAY_UNSET = 0,
    NT_TCP_NODELAY_SET,
    NT_TCP_NODELAY_DISABLED
} nt_connection_tcp_nodelay_e;


typedef enum {
    NT_TCP_NOPUSH_UNSET = 0,
    NT_TCP_NOPUSH_SET,
    NT_TCP_NOPUSH_DISABLED
} nt_connection_tcp_nopush_e;


#define NT_LOWLEVEL_BUFFERED  0x0f
#define NT_SSL_BUFFERED       0x01
#define NT_HTTP_V2_BUFFERED   0x02

/*用于存储一个新连进来的连接*/
struct nt_connection_s {

    //存储内容
    void               *data;

    #if (T_NT_MULTI_UPSTREAM)
    void               *multi_c;
    #endif
    //read event事件
    nt_event_t        *read;
    //write event事件
    nt_event_t        *write;
    #if (NT_SSL && NT_SSL_ASYNC)
    nt_event_t        *async;
    #endif

    nt_socket_t        fd;
    #if (NT_SSL && NT_SSL_ASYNC)
    nt_socket_t        async_fd;
    #endif

    //recv回调
    nt_recv_pt         recv;
    //send回调
    nt_send_pt         send;
//    nt_recv_chain_pt   recv_chain;
//    nt_send_chain_pt   send_chain;

    nt_listening_t    *listening;

    //发送个数
    off_t               sent;
    #if (T_NT_REQ_STATUS)
    off_t               received;
    #endif


    //日志
    nt_log_t           *log;

    nt_pool_t         *pool;

    int                 type;

    struct sockaddr    *sockaddr;
    socklen_t           socklen;
    nt_str_t           addr_text;

    nt_str_t           proxy_protocol_addr;
    in_port_t           proxy_protocol_port;

    #if (NT_SSL || NT_COMPAT)
    nt_ssl_connection_t  *ssl;
    #if (NT_SSL_ASYNC)
    nt_flag_t          async_enable;
    #endif
    #endif

  //  nt_udp_connection_t  *udp;

    struct sockaddr    *local_sockaddr;
    socklen_t           local_socklen;

    nt_buf_t          *buffer;

    nt_queue_t         queue;

    //nt_atomic_uint_t   number;

    nt_uint_t          requests;

    unsigned            buffered: 8;

    unsigned            log_error: 3;    /* nt_connection_log_error_e */

    unsigned            timedout: 1;
    unsigned            error: 1;
    unsigned            destroyed: 1;

    unsigned            idle: 1;
    unsigned            reusable: 1;
    unsigned            close: 1;
    unsigned            shared: 1;

    unsigned            sendfile: 1;
    unsigned            sndlowat: 1;
    unsigned            tcp_nodelay: 2;  /* nt_connection_tcp_nodelay_e */
    unsigned            tcp_nopush: 2;   /* nt_connection_tcp_nopush_e */

    unsigned            need_last_buf: 1;
    #if (NT_SSL && NT_SSL_ASYNC)
    unsigned            num_async_fds: 8;
    #endif

    #if (NT_HAVE_AIO_SENDFILE || NT_COMPAT)
    unsigned            busy_count: 2;
    #endif

    #if (NT_THREADS || NT_COMPAT)
    nt_thread_task_t  *sendfile_task;
    #endif

};


#define nt_set_connection_log(c, l)                                         \
                                                                             \
    c->log->file = l->file;                                                  \
    c->log->next = l->next;                                                  \
    c->log->writer = l->writer;                                              \
    c->log->wdata = l->wdata;                                                \
    if (!(c->log->log_level & NT_LOG_DEBUG_CONNECTION)) {                   \
        c->log->log_level = l->log_level;                                    \
    }


nt_listening_t *nt_create_listening(nt_conf_t *cf, struct sockaddr *sockaddr,
    socklen_t socklen);
nt_int_t nt_clone_listening(nt_cycle_t *cycle, nt_listening_t *ls);
nt_int_t nt_set_inherited_sockets(nt_cycle_t *cycle);
nt_int_t nt_open_listening_sockets(nt_cycle_t *cycle);
void nt_configure_listening_sockets(nt_cycle_t *cycle);
void nt_close_listening_sockets(nt_cycle_t *cycle);
void nt_close_connection(nt_connection_t *c);
void nt_close_idle_connections(nt_cycle_t *cycle);
nt_int_t nt_connection_local_sockaddr(nt_connection_t *c, nt_str_t *s,
    nt_uint_t port);
nt_int_t nt_tcp_nodelay(nt_connection_t *c);
nt_int_t nt_connection_error(nt_connection_t *c, nt_err_t err, char *text);

nt_connection_t *nt_get_connection(nt_socket_t s, nt_log_t *log);
void nt_free_connection(nt_connection_t *c);

void nt_reusable_connection(nt_connection_t *c, nt_uint_t reusable);

#endif
