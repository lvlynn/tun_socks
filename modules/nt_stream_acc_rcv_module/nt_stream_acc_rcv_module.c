#include <nt_core.h>
#include "rbtree.h"
#include "nt_tun_acc_handler.h"
#include <nt_stream.h>



struct nt_udp_connection_s {
    nt_rbtree_node_t   node;
    nt_connection_t   *connection;
    nt_buf_t          *buffer;

};



static ssize_t
nt_udp_shared_recv( nt_connection_t *c, u_char *buf, size_t size );
static nt_int_t
nt_insert_tun_connection( nt_connection_t *c );
static void
nt_tun_close_accepted_connection( nt_connection_t *c );
void nt_tun_delete_rbtee_node(void *data);

void
nt_tun_session_handler( nt_event_t *rev )
{
    debug( "start" );
    nt_connection_t      *c;
    nt_stream_session_t  *s;

    c = rev->data;
    s = c->data;
}


static u_char *
nt_tun_log_error( nt_log_t *log, u_char *buf, size_t len )
{
    u_char                *p;
    nt_stream_session_t  *s;

    if( log->action ) {
        p = nt_snprintf( buf, len, " while %s", log->action );
        len -= p - buf;
        buf = p;
    }

    s = log->data;

    p = nt_snprintf( buf, len, ", %sclient: %V, server: %V",
                     s->connection->type == SOCK_DGRAM ? "udp " : "",
                     &s->connection->addr_text,
                     &s->connection->listening->addr_text );
    len -= p - buf;
    buf = p;

    if( s->log_handler ) {
        p = s->log_handler( log, buf, len );
    }

    return p;
}


static void
nt_tun_close_connection( nt_connection_t *c )
{
    nt_pool_t  *pool;

    nt_log_debug1( NT_LOG_DEBUG_STREAM, c->log, 0,
                   "close stream connection: %d", c->fd );

    #if (NT_STREAM_SSL)

    if( c->ssl ) {
        if( nt_ssl_shutdown( c ) == NT_AGAIN ) {
            c->ssl->handler = nt_tun_close_connection;
            return;
        }
    }

    #endif

    #if (NT_STAT_STUB)
    ( void ) nt_atomic_fetch_add( nt_stat_active, -1 );
    #endif

    pool = c->pool;

    nt_close_connection( c );

    nt_destroy_pool( pool );
}


void
nt_tun_init_session( nt_connection_t *c )
{
    nt_acc_session_t         *s;
    nt_stream_core_srv_conf_t   *cscf;
    nt_time_t                   *tp;
    nt_event_t                  *rev;
    //申请一个加速session
    s = nt_pcalloc( c->pool, sizeof( nt_acc_session_t ) );
    if( s == NULL ) {
        nt_tun_close_connection( c );
        return;
    }

    //统计改session接收到的数据个数
    if( c->buffer ) {
        s->received += c->buffer->last - c->buffer->pos;
    }

    s->connection = c;
    c->data = s;

    /* s->fd = c->fd; */
    #if 0
    s->signature = NT_STREAM_MODULE;
    s->main_conf = addr_conf->ctx->main_conf;
    s->srv_conf = addr_conf->ctx->srv_conf;

    cscf = nt_stream_get_module_srv_conf( s, nt_stream_core_module );
    nt_set_connection_log( c, cscf->error_log );

    c->log->connection = c->number;
    c->log->handler = nt_tun_log_error;
    c->log->data = s;
    c->log->action = "initializing session";
    c->log_error = NT_ERROR_INFO;
    #endif

    tp = nt_timeofday();
    s->start_sec = tp->sec;
    s->start_msec = tp->msec;

    rev = c->read;
    rev->handler = nt_tun_accept_new_handler;

    //如果使用多连接
    if( nt_use_accept_mutex ) {
        nt_post_event( rev, &nt_posted_events );
        return;
    }

    rev->handler( rev );
}


void
nt_tun_init_connection( nt_connection_t *c )
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

    if( port->naddrs > 1 ) {

        /*
         * There are several addresses on this port and one of them
         * is the "*:port" wildcard so getsockname() is needed to determine
         * the server address.
         *
         * AcceptEx() and recvmsg() already gave this address.
         */

        if( nt_connection_local_sockaddr( c, NULL, 0 ) != NT_OK ) {
            nt_tun_close_connection( c );
            return;
        }

        sa = c->local_sockaddr;

        switch( sa->sa_family ) {

            #if (NT_HAVE_INET6)
        case AF_INET6:
            sin6 = ( struct sockaddr_in6 * ) sa;

            addr6 = port->addrs;

            /* the last address is "*" */

            for( i = 0; i < port->naddrs - 1; i++ ) {
                if( nt_memcmp( &addr6[i].addr6, &sin6->sin6_addr, 16 ) == 0 ) {
                    break;
                }
            }

            addr_conf = &addr6[i].conf;

            break;
            #endif

        default: /* AF_INET */
            sin = ( struct sockaddr_in * ) sa;

            addr = port->addrs;

            /* the last address is "*" */

            for( i = 0; i < port->naddrs - 1; i++ ) {
                if( addr[i].addr == sin->sin_addr.s_addr ) {
                    break;
                }
            }

            addr_conf = &addr[i].conf;

            break;
        }

    } else {
        switch( c->local_sockaddr->sa_family ) {

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

    s = nt_pcalloc( c->pool, sizeof( nt_stream_session_t ) );
    if( s == NULL ) {
        nt_tun_close_connection( c );
        return;
    }

    s->signature = NT_STREAM_MODULE;
    s->main_conf = addr_conf->ctx->main_conf;
    s->srv_conf = addr_conf->ctx->srv_conf;
    debug( "s->srv_conf=%p", s->srv_conf );

    #if (NT_STREAM_SNI)
    s->addr_conf = addr_conf;
    s->main_conf = ( ( nt_stream_core_srv_conf_t* )addr_conf->default_server )->ctx->main_conf;
    s->srv_conf = ( ( nt_stream_core_srv_conf_t* )addr_conf->default_server )->ctx->srv_conf;
    #endif

    #if (NT_STREAM_SSL)
    s->ssl = addr_conf->ssl;
    #endif

    if( c->buffer ) {
        s->received += c->buffer->last - c->buffer->pos;
    }

    s->connection = c;
    c->data = s;

    cscf = nt_stream_get_module_srv_conf( s, nt_stream_core_module );

    nt_set_connection_log( c, cscf->error_log );

    len = nt_sock_ntop( c->sockaddr, c->socklen, text, NT_SOCKADDR_STRLEN, 1 );

    nt_log_error( NT_LOG_INFO, c->log, 0, "*%uA %sclient %*s connected to %V",
                  c->number, c->type == SOCK_DGRAM ? "udp " : "",
                  len, text, &addr_conf->addr_text );

    c->log->connection = c->number;
    c->log->handler = nt_tun_log_error;
    c->log->data = s;
    c->log->action = "initializing session";
    c->log_error = NT_ERROR_INFO;

    s->ctx = nt_pcalloc( c->pool, sizeof( void * ) * nt_stream_max_module );
    if( s->ctx == NULL ) {
        nt_tun_close_connection( c );
        return;
    }

    cmcf = nt_stream_get_module_main_conf( s, nt_stream_core_module );

    s->variables = nt_pcalloc( s->connection->pool,
                               cmcf->variables.nelts
                               * sizeof( nt_stream_variable_value_t ) );

    if( s->variables == NULL ) {
        nt_tun_close_connection( c );
        return;
    }

    tp = nt_timeofday();
    s->start_sec = tp->sec;
    s->start_msec = tp->msec;

    rev = c->read;
    rev->handler = nt_tun_session_handler;

    //如果使用多连接
    if( nt_use_accept_mutex ) {
        nt_post_event( rev, &nt_posted_events );
        return;
    }

    rev->handler( rev );

}



int nt_open_tun_socket( nt_listening_t *ls )
{
    nt_int_t fd;

    debug( "name=%s", ls->sockaddr->sa_data );
    fd = tun_init( ls->sockaddr->sa_data );

    if( fd < 0 ) {
        debug( "fd=%d", fd );

        return NT_ERROR;
    }

    //初始化接收器用的红黑树
    nt_rbtree_init( &acc_tcp_tree, &g_sentinel, nt_rbtree_insert_conn_handle );
    nt_rbtree_init( &acc_udp_tree, &g_sentinel, nt_rbtree_insert_conn_handle );
    nt_rbtree_init( &acc_udp_id_tree, &g_sentinel, nt_rbtree_insert_udp_id_handle );

    ls->fd = fd;

    if( !( nt_event_flags & NT_USE_IOCP_EVENT ) ) {
        if( nt_nonblocking( fd ) == -1 ) {
            /* nt_log_error( NT_LOG_EMERG, log, nt_socket_errno, */
            /* nt_nonblocking_n " %V failed", */
            /* &ls[i].addr_text ); */

            if( nt_close_socket( fd ) == -1 ) {
                /* nt_log_error( NT_LOG_EMERG, log, nt_socket_errno, */
                /* nt_close_socket_n " %V failed", */
                /* &ls[i].addr_text ); */
            }

            return NT_ERROR;
        }
    }

    return NT_OK;
}


//数据分发
// 数据分 要加速的
//        不要加速的
//        要丢弃的
//  NT_TUN_ACC_DROP
//  NT_TUN_ACC_LOCAL
//  NT_TUN_ACC_PROXY
int nt_tun_data_dispatch( const char* data )
{

    // 先区分协议，只流入 tcp udp , 其他协议 goto NT_TUN_ACC_LOCAL
    // 由于需要流回local ,所以queue比较适合本方案
    nt_int_t ret;
    nt_connection_t  *c, *lc;


    struct iphdr *ih = ( struct iphdr * )data;

    if( ih->version == 0x6 ) {
        return NT_TUN_ACC_LOCAL;
    }


    switch( ih->protocol ) {
    case IPPROTO_TCP:
        return NT_TUN_ACC_PROXY;

    case IPPROTO_UDP:
        return NT_TUN_ACC_PROXY;

    default:
        return NT_TUN_ACC_LOCAL;
    }

    //判断是否需要加速。
    return ret;
}


//处理tun数据流入 入口，参考udp 的accept回调 nt_event_recvmsg
/*
 * tun fd本身占用一个 conn,
 * tun 流入的每个连接占用一个新连接
 *
 * */
void nt_event_accept_tun( nt_event_t *ev )
{

    /* debug( "%s", "start" ); */

    ssize_t            n;
    nt_listening_t   *ls;
    nt_event_conf_t  *ecf;
    nt_sockaddr_t     sa;
    nt_connection_t  *c, *lc;
    nt_int_t ret;
    nt_event_t       *rev, *wev;
    nt_err_t          err;
    nt_log_t         *log;
    socklen_t          socklen, local_socklen;
    ssize_t size;
    nt_pool_cleanup_t    *cln;

    //取数据
    static u_char      buffer[65535];

    //处理timeout
    if( ev->timedout ) {
        if( nt_enable_accept_events( ( nt_cycle_t * ) nt_cycle ) != NT_OK ) {
            return;

        }

        ev->timedout = 0;
    }

    ecf = nt_event_get_conf( nt_cycle->conf_ctx, nt_event_core_module );

    // nt_event_flags 不为NT_USE_KQUEUE_EVENT的时候需要赋值
    /* if( !( nt_event_flags & NT_USE_KQUEUE_EVENT ) ) {
        debug( "ecf->multi_accept=%d", ecf->multi_accept );
        ev->available = ecf->multi_accept;
    } */
    ev->available = ecf->multi_accept;

    lc = ev->data;
    ls = lc->listening;
    ev->ready = 0;

    nt_log_debug2( NT_LOG_DEBUG_EVENT, ev->log, 0,
                   "accept on %V, ready: %d", &ls->addr_text, ev->available );

    /* debug( "lc->fd=%d", lc->fd ); */
    do {

        //读取本次过来的数据
        /* debug( "call read" ); */
        //该 fd 必须为 非阻塞模式，否则一个worker 无法同时处理多连接
        n = read( lc->fd, buffer, sizeof( buffer ) );
        debug( "n=%d", n );

        if( n == -1 ) {
            err = nt_socket_errno;

            if( err == NT_EAGAIN ) {
                nt_log_debug0( NT_LOG_DEBUG_EVENT, ev->log, err,
                               "tun read not ready" );
                return;

            }

            nt_log_error( NT_LOG_ALERT, ev->log, err, "read() failed" );
            return;
        }


        //分析数据是哪种协议，进行中转处理
        c =  ip_input( buffer );
        if( c ) {
            /* c->buffer->pos = c->buffer->start ;
            c->buffer->last = nt_cpymem( c->buffer->start, buffer, n );

            c->read->handler( c->read ); */

            nt_tun_data_forward( c, buffer, n );

        } else {
            debug( "该连接为新连接" );
            //该连接为新连接，做新连接的初始化

            //原子性操作， 全局变量统计，存在共享内存中
            #if (NT_STAT_STUB)
            ( void ) nt_atomic_fetch_add( nt_stat_accepted, 1 );
            #endif
            //accept 关闭变量计数；
            nt_accept_disabled = nt_cycle->connection_n / 8
                                 - nt_cycle->free_connection_n;

            //从空闲连接池中取一个连接
            c = nt_get_connection( lc->fd, ev->log );
            //获取失败
            if( c == NULL ) {
                return NT_ERROR;
            }

            //设置参数
            //shared表示该连接是共享的fd , 一半是继承自监听的fd。 设置为1后，在调用nt_close_connection函数不会直接close(fd)
            c->shared = 1;
            //这个类型决定 upstream 的连接类型
            /* c->type = SOCK_STREAM; */
            c->socklen = socklen;
            nt_tun_set_connection_type( c , buffer );


            #if (NT_STAT_STUB)
            ( void ) nt_atomic_fetch_add( nt_stat_active, 1 );
            #endif

            //申请内存给当前连接用；
            /* debug( "ls->pool_size=%d", ls->pool_size ); */

            /* c->pool = nt_create_pool( ls->pool_size, ev->log ); */
            c->pool = nt_create_pool( 8192, ev->log );
            if( c->pool == NULL ) {
                //申请失败，释放连接
                nt_tun_close_accepted_connection( c );
                return NT_ERROR;
            }

            //该变量指向 socks5_pass 的上游server
            //该变量指向连接的目的地址
            c->sockaddr = nt_palloc( c->pool, socklen );
            if( c->sockaddr == NULL ) {
                nt_tun_close_accepted_connection( c );
                return;

            }

            nt_memcpy( c->sockaddr, &sa, socklen );

            //申请一个log 用的空间
            log = nt_palloc( c->pool, sizeof( nt_log_t ) );
            if( log == NULL ) {
                nt_tun_close_accepted_connection( c );
                return;
            }

            *log = ls->log;
            //设置连接读写回调
            /* c->recv = nt_udp_shared_recv; */
            c->recv = nt_recv;
            c->send = nt_send;
            c->send_chain = nt_udp_send_chain;

            //继承log, 主监听 ls
            c->log = log;
            c->pool->log = log;
            c->listening = ls;

            //申请一个buffer空间
            c->buffer = nt_create_temp_buf( c->pool, 1500 );
            if( c->buffer == NULL ) {
                nt_tun_close_accepted_connection( c );
                return NT_ERROR;
            }

            //把当前读取到的内容拷贝到当前连接下；
            if( c->type == IPPROTO_TCP ){
                c->buffer->last = nt_cpymem( c->buffer->last, buffer, n );
            } else{
                /* c->buffer->last += 10; */
                c->buffer->last = nt_cpymem( c->buffer->last, buffer, n  );
            }

            //设置参数
            rev = c->read;
            wev = c->write;

            rev->active = 1;
            wev->ready = 1;

            rev->log = log;
            wev->log = log;

            //统计连接数
            c->number = nt_atomic_fetch_add( nt_connection_counter, 1 );

            #if (NT_STAT_STUB)
            ( void ) nt_atomic_fetch_add( nt_stat_handled, 1 );
            #endif


            //把新连接插入红黑树中
            /* if( nt_insert_tun_connection( c ) != NT_OK ) {
                nt_tun_close_accepted_connection( c );
                return NT_ERROR;
            } */


            log->data = NULL;
            log->handler = NULL;

            //执行监听函数回调入口；
            //stream 为 调用nt_stream_init_connection();
            debug( " ls->handler 调用nt_stream_init_connection" );
            /* ls->handler( c ); */
            /* nt_tun_init_connection( c ); */

            //添加内存回收后触发的回调 ( nt_destroy_pool )
            cln = nt_pool_cleanup_add(c->pool, 0);
            if (cln == NULL) {
                return NT_ERROR;
            }

            cln->data = c;
            cln->handler = nt_tun_delete_rbtee_node;

            nt_tun_init_session( c );

        }

        /* ret = nt_tun_data_dispatch( data );
        if( ret ==  NT_ERROR ){
            return;
        } */
/* next: */

        //只有 是 NT_USE_KQUEUE_EVENT 类型的时候才需要减掉
        /* if( nt_event_flags & NT_USE_KQUEUE_EVENT ) {
            ev->available -= n;
        } */

        /* debug( "ev->available=%d", ev->available ); */
        //启用了 一个worker可以同时接受多个连接， 问题在于如何退出， 因为ev->available 一直是1
    } while( ev->available );

    /* debug( "end" ); */
}


static ssize_t
nt_udp_shared_recv( nt_connection_t *c, u_char *buf, size_t size )
{
    ssize_t     n;
    nt_buf_t  *b;

    if( c->udp == NULL || c->udp->buffer == NULL ) {
        return NT_AGAIN;
    }

    b = c->udp->buffer;

    n = nt_min( b->last - b->pos, ( ssize_t ) size );

    nt_memcpy( buf, b->pos, n );

    c->udp->buffer = NULL;

    c->read->ready = 0;
    c->read->active = 1;

    return n;
}


void nt_tun_delete_rbtee_node(void *data)
{
    /* nt_connection_t  *c = data;

    if (c->udp == NULL) {
        return;

    }

    nt_rbtree_delete(&c->listening->rbtree, &c->udp->node);

    c->udp = NULL; */

} 

static nt_int_t
nt_insert_tun_connection( nt_connection_t *c )
{

    #if 0
    uint32_t               hash;
    nt_pool_cleanup_t    *cln;
    nt_udp_connection_t  *udp;

    if( c->udp ) {
        return NT_OK;
    }

    udp = nt_pcalloc( c->pool, sizeof( nt_udp_connection_t ) );
    if( udp == NULL ) {
        return NT_ERROR;
    }

    udp->connection = c;

    nt_crc32_init( hash );
    nt_crc32_update( &hash, ( u_char * ) c->sockaddr, c->socklen );

    if( c->listening->wildcard ) {
        nt_crc32_update( &hash, ( u_char * ) c->local_sockaddr, c->local_socklen );
    }

    nt_crc32_final( hash );

    udp->node.key = hash;

    cln = nt_pool_cleanup_add( c->pool, 0 );
    if( cln == NULL ) {
        return NT_ERROR;
    }

    cln->data = c;
    cln->handler = nt_delete_udp_connection;

    nt_rbtree_insert( &c->listening->rbtree, &udp->node );

    c->udp = udp;
    #endif
    return NT_OK;
}


static void
nt_tun_close_accepted_connection( nt_connection_t *c )
{
    debug( "start" );
    nt_free_connection( c );

    c->fd = ( nt_socket_t ) -1;

    if( c->pool ) {
        nt_destroy_pool( c->pool );
    }

    #if (NT_STAT_STUB)
    ( void ) nt_atomic_fetch_add( nt_stat_active, -1 );
    #endif
}
