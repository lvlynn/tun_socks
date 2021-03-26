#include <nt_core.h>

#include "protocol.h"

static nt_int_t nt_udp_read_raw_send( nt_connection_t *c );
nt_int_t nt_upstream_socks_writev( nt_connection_t *c, struct iovec *iov );

//检查跟上游的sock5服务器的连接是否已断开
nt_int_t nt_socks5_test_connection( nt_connection_t *c )
{

    //如果eof 为 1，或
    if( c == NULL || ( !c->read->eof )
        || ( !c->read->eof && c->buffered ) ) {
        return NT_DECLINED;
    }

    debug( "free connection" );
    //需要释放连接。

    nt_acc_session_t *s;
    s = c->data;
    tcp_close_client( s );
    nt_sock5_tcp_upstream_free( c );
    return NT_OK;
}


void nt_tun_upsteam_tcp_write_handler( nt_event_t *ev )
{
    debug( "start" );
}

/*
    连接超时后触发，用于检测该连接是否已经断开
*/
void nt_tun_upsteam_timeout_handler( nt_event_t *ev )
{
    debug( "start" );
}



void nt_tun_data_forward( nt_connection_t *c,  char *buf, size_t n )
{
    /* if( c->type == IPPROTO_UDP ) {
        nt_acc_session_t *s;
        s = c->data;

        if( s->ss->udp_conn ) {
            c->buffer->pos = c->buffer->start ;
             [>c->buffer->last += 10; <]
            c->buffer->last = nt_cpymem( c->buffer->start, buf, n );
        } else {
            //udp 连接未建立， 数据需要放在 buf 链表里
            nt_buf_t *b;
            b = nt_palloc( c->pool, n );

            [>b->last += 10;<]
            b->last = nt_cpymem( b->start, buf, n  );
            if( s->skb->buffer_chain ) {
                nt_skb_buf_t *buffer_chain = nt_palloc( c->pool, sizeof( nt_skb_buf_t )  );
                buffer_chain->b =b ;
                buffer_chain->next = NULL;
                s->skb->buffer_chain->next = buffer_chain ;
            } else {
                s->skb->buffer_chain = b;
            }
        }

    } else {

        c->buffer->pos = c->buffer->start ;
        c->buffer->last = nt_cpymem( c->buffer->start, buf, n );
    } */

    c->buffer->pos = c->buffer->start ;
    c->buffer->last = nt_cpymem( c->buffer->start, buf, n );


    c->read->handler( c->read );
}


void nt_tun_socks5_read_handler( nt_event_t *ev )
{

    nt_int_t ret;
    nt_connection_t *c;

    debug( "start" );
    c = ev->data;

    if( ev->write == 1 ) {
        return ;
    }

    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "-------------> fd=%d", c->fd );
    nt_upstream_socks_read( c );

    nt_acc_session_t *s ;
    /* nt_upstream_socks_t *ss; */

    s = c->data;
    ret = nt_tun_socks5_handle_phase( s );
    if( ret == NT_ERROR )
        return ;

    nt_socks5_test_connection( c );


    /* nt_upstream_socks_send( s->ss->tcp_conn ); */

}


void nt_tun_tcp_download_handler( nt_event_t *ev )
{

    nt_int_t ret;
    nt_connection_t *c;

    /* debug( "start" ); */
    c = ev->data;
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "-------------> fd=%d", c->fd );

    /* nt_upstream_socks_read( c ); */
    nt_downstream_raw_read_send( c );

    nt_socks5_test_connection( c );

}



/*
 * c->data = s
 * s->skb
 * s->skb->data = tcp
 *
 * */
// tcp 数据中转
void nt_tun_tcp_data_upload_handler( nt_connection_t *c )
{

    nt_buf_t *b;
    nt_acc_session_t *s;
    nt_connection_t *tcp_conn;

    nt_skb_tcp_t *tcp;

    s = c->data;
    tcp_conn = s->ss->tcp_conn;


    b = c->buffer ;
    tcp  = s->skb->data;

    debug( "tcp %p", tcp );
    ssize_t size = s->skb->skb_len - tcp->data_len ;

    debug( "size=%d", size );
    debug( "s->skb->skb_len=%d", s->skb->skb_len );
    debug( "tcp->data_len=%d", tcp->data_len );

    debug( "buf = %s", b->start + size );
    b->pos += size;

    debug( "s =%d", ( b->last - b->pos ) );
    debug( "s =%d", ( b->last - b->start ) );

    if( ( b->last - b->pos ) < 0 )
        return;

    nt_upstream_socks_send( tcp_conn, b );
    nt_socks5_test_connection( tcp_conn );
}


nt_int_t nt_upstream_socks_read( nt_connection_t *c )
{

    size_t size;

    ssize_t      n;
    nt_buf_t    *b;
    nt_uint_t                    flags;
    char  *send_action;

    b = c->buffer ;

    send_action = "socks5ing and sending to upstream";
    for( ;; ) {

        // size是缓冲区的剩余可用空间
        size = b->end - b->last;
        if( size && c->read->ready && ! c->read->delayed
            && !c->read->error ) {

            c->log->action = send_action;

            n = c->recv( c, b->last, size );

            debug( "read n=%d", n );
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> n=%d", n );

            // 如果不可读，或者已经读完
            // break结束for循环
            if( n == NT_AGAIN ) {
                break;
            }

            // 出错，标记为eof
            if( n == NT_ERROR ) {
                c->read->eof = 1;
                n = 0;
            }

            if( n >= 0 ) {
                /* ( *packets )++;  */
                // 缓冲区的pos指针移动，表示发送了n字节新数据
                b->last += n;
                break;
            }
        }
        break;
    }


    flags = c->read->eof ? NT_CLOSE_EVENT : 0;
    if( nt_handle_read_event( c->read, flags ) != NT_OK ) {

    }

    return NT_OK;
}

nt_int_t nt_upstream_socks_send( nt_connection_t *c, nt_buf_t *b )
{

    size_t size;

    ssize_t      n;
    /* nt_buf_t    *b; */
    nt_uint_t                    flags;
    char  *send_action;

    /* b = c->buffer ; */

    send_action = "socks5ing and sending to upstream";

    int send_status = 0;


    for( ;; ) {
        size = b->last - b->pos;

        if( size ) {

            c->log->action = send_action;

            n = c->send( c, b->pos, size );

            debug( "send n=%d", n );
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> n=%d", n );

            // 如果不可读，或者已经读完
            // break结束for循环
            if( n == NT_AGAIN ) {
                break;
            }

            // 出错，标记为eof
            if( n == NT_ERROR ) {
                //                    src->write->eof = 1;
                //                    n = 0;
                break;
            }

            if( n >= 0 ) {
                /* ( *packets )++;  */
                // 缓冲区的pos指针移动，表示发送了n字节新数据
                send_status = 1;
                b->pos += n;
                continue;
            }


        }

        if( size == 0 ) {
            b->pos = b->start;
            b->last = b->start;
        }


        break;
    }

    /* flags = c->read->eof ? NT_CLOSE_EVENT : 0;
    if( nt_handle_read_event( c->read, flags  ) != NT_OK  ) {

    }
    */
    return NT_OK;
}



nt_int_t nt_downstream_raw_read_send( nt_connection_t *c )
{
    size_t size;

    ssize_t      n;
    nt_buf_t    *b;
    nt_uint_t                    flags;
    char  *send_action;
    nt_uint_t do_write = 0;

    b = c->buffer ;

    send_action = "socks5ing and sending to upstream";

    nt_acc_session_t *s ;

    s = c->data;


    nt_upstream_socks_t *ss;
    ss = s->ss;

    nt_connection_t *pc = s->connection;

    struct iphdr *ih;
    struct tcphdr *th;
    ih = ( struct iphdr * )pc->buffer->start;
    th = ( struct tcphdr * )( ih + 1 );
    nt_skb_t *skb;
    nt_skb_tcp_t *tcp ;

    skb = s->skb;
    tcp = skb->data;

    int to_fd ;
    #if 1
    #ifdef SEND_USE_TUN_FD
    to_fd = s->fd;
    #else
    struct sockaddr_in *addr;
    addr = ( struct sockaddr_in * )tcp->src ;

    struct sockaddr_in to;
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = addr->sin_addr.s_addr;
    to.sin_port = addr->sin_port;
    to_fd = tcp->fd;
    debug( "to_fd=%d", tcp->fd );
    debug( "s_fd=%d", s->fd );
    #endif
    #endif

    int send_flag = 0;
    for( ;; ) {

        if( do_write ) {
            size = b->last - b->pos;
            if( size ) {

                c->log->action = send_action;
                //    n = c->send( c, b->pos, size );
                /* debug( " raw send size =%d", size ); */
                nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> size=%d", size );

                if( size == 1500 )
                    tcp->phase = TCP_PHASE_SEND_PSH;
                else
                    tcp->phase = TCP_PHASE_SEND_PSH_END;

                acc_tcp_psh_create( b,  tcp );

                #ifdef SEND_USE_TUN_FD
                n = write( to_fd, b->pos, size );
                #else
                /* n = sendto( to_fd, b->pos,  size, 0, tcp->src, sizeof( struct sockaddr ) ); */
                n = sendto( to_fd, b->pos,  size, 0, ( struct sockaddr * )&to, sizeof( to ) );
                #endif

                /* debug( "send n=%d", n ); */
                nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> n=%d", n );

                // 如果不可读，或者已经读完
                // break结束for循环
                if( n == NT_AGAIN ) {
                    debug( "n == NT_AGAIN" );
                    break;
                }

                // 出错，标记为eof
                if( n == NT_ERROR ) {
                    debug( "n == NT_ERROR" );
                    //                    src->write->eof = 1;
                    //                    n = 0;
                    break;
                }

                if( n >= 0 ) {
                    /* ( *packets )++;  */
                    // 缓冲区的pos指针移动，表示发送了n字节新数据
                    b->pos += n;
                    /* continue; */
                    break;
                }


            }

        }

        // size是缓冲区的剩余可用空间
        size = b->end - b->last;
        /* debug( "c->read->ready=%d", c->read->ready ); */
        /* debug( "c->read->error=%d", c->read->error ); */
        if( size && c->read->ready && ! c->read->delayed
            && !c->read->error ) {

            c->log->action = send_action;

            b->last += 40;
            n = c->recv( c, b->last, 1460 );

            /* debug( "read n=%d", n ); */
            /* debug( "read data=%s", b->start +40 ); */
            /* nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> n=%d", n ); */

            // 如果不可读，或者已经读完
            // break结束for循环
            if( n == NT_AGAIN ) {
                debug( "n == NT_AGAIN" );
                break;
            }

            // 出错，标记为eof
            if( n == NT_ERROR ) {
                debug( "n == NT_ERROR" );
                c->read->eof = 1;
                n = 0;
            }

            if( n == 0 ) {
                debug( "n == 0" );

                break;
            }

            /* debug( "buf = %s",  b->pos ); */
            if( n >= 0 ) {
                /* ( *packets )++;  */
                // 缓冲区的pos指针移动，表示发送了n字节新数据
                tcp->data_len = n;
                b->last += n;
                do_write = 1;
                continue;
            }
        }


        break;
    }


    if( send_flag == 0 ) {
        b->pos = b->start;
        b->last = b->start;
    }


    /* debug( "c->read->eof=%d", c->read->eof ); */
    flags = c->read->eof ? NT_CLOSE_EVENT : 0;

    /* if( nt_handle_read_event( c->read, flags  ) != NT_OK  ) { */

    /* } */

    return NT_OK;
}

//              udp
//============================================================

//socks udp 头部
#pragma pack(1)
struct socks5_udp_header {
    uint16_t rsv;
    uint8_t frag;
    uint8_t atyp;
    uint32_t ip;
    uint16_t port;
};
#pragma pack()


void nt_tun_udp_add_socks5_header( nt_acc_session_t *s, nt_buf_t *b )
{
    nt_skb_udp_t *udp;
    struct socks5_udp_header *suh;

    udp = s->skb->data;
    debug( "udp->hdr_len=%d", udp->hdr_len );
    b->pos += ( udp->hdr_len - sizeof( struct socks5_udp_header ) );
    suh = ( struct socks5_udp_header * )b->pos;

    debug( "sizeof suh=%d", sizeof( struct socks5_udp_header ) );
    suh->rsv = 0;
    suh->frag = 0;
    suh->atyp = 1;
    suh->ip = udp->dip;
    suh->port = udp->dport;
    debug( " udp ip=%s:%d", nt_inet_ntoa( udp->dip ), ntohs( udp->dport ) );

}


/* void nt_tun_udp_add_socks5_header( nt_acc_session_t *s, struct iovec *iov ){
    nt_skb_udp_t *udp;

    udp = s->skb->data;

    struct socks5_udp_header *suh = ( struct socks5_udp_header * ) s->skb->buffer ;
    suh->rsv = 0;
    suh->frag = 0;
    suh->atyp = 1;
    suh->ip = udp->dip;
    suh->port = udp->dport;
    debug( " udp ip=%s:%d", nt_inet_ntoa(  udp->dip  ), ntohs( udp->dport  )  );

    iov[0].iov_base = &suh ;
    iov[0].iov_base = sizeof( struct socks5_udp_header );

    iov[1].iov_base = udp->data;
    iov[1].iov_len = udp->data_len;


}  */


//上传 udp 数据
void nt_tun_udp_data_send( nt_connection_t *c )
{
    //先发送队列数据, 然后指向到正常发送模式
    nt_acc_session_t *s;
    nt_buf_t *b;

    s = c->data;

    nt_skb_udp_t *udp;
    udp = s->skb->data;

    if( udp->phase == UDP_PHASE_SOCKS_CONNECT ) {
        debug( "send chain" );
        //从 buffer_chain 从发送
        nt_skb_buf_t *q;
        q = s->skb->buffer_chain;
        debug( "head=%p", s->skb->buffer_chain );
        debug( "head=%p", q );

        while( q != NULL ) {
            debug( "p =%p", q );
            nt_tun_udp_add_socks5_header( s, q->b );
            nt_upstream_socks_send( s->ss->udp_conn, q->b );
            q = q->next ;
        }
//        s->skb->buffer_chain == NULL;
//
      udp->phase = UDP_PHASE_DATA_FORWARD;
      /* c->read->handler = ;       */
    } else {
        debug( "send buf" );
        //从 buf中发送
        b = c->buffer ;
        nt_tun_udp_add_socks5_header( s, b );

        nt_upstream_socks_send( s->ss->udp_conn, b );
        /* nt_upstream_socks_writev(  s->ss->udp_conn, iov ); */
    }
    /* exit(0); */
}


void nt_upstream_udp_read_handler( nt_event_t *ev )
{
    nt_int_t ret;
    nt_connection_t *c;

    /* debug( "start" ); */
    c = ev->data;
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "-------------> fd=%d", c->fd );

    nt_udp_read_raw_send( c );

    nt_socks5_test_connection( c );
}

nt_int_t nt_upstream_socks_writev( nt_connection_t *c, struct iovec *iov )
{
    size_t size;

    ssize_t      n;
    /* nt_buf_t    *b; */
    nt_uint_t                    flags;
    char  *send_action;

    /* b = c->buffer ; */

    send_action = "socks5ing and sending to upstream";

    int send_status = 0;
    n = c->send( c, iov, 2 );

    debug( "send n=%d", n );
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> n=%d", n );


    /* flags = c->read->eof ? NT_CLOSE_EVENT : 0;
    if( nt_handle_read_event( c->read, flags  ) != NT_OK  ) {

    }
    */
    return NT_OK;
}

static nt_int_t nt_udp_read_raw_send( nt_connection_t *c )
{
    size_t size;

    ssize_t      n;
    nt_buf_t    *b;
    nt_uint_t                    flags;
    char  *send_action;
    nt_uint_t do_write = 0;


    send_action = "socks5ing and sending to upstream";

    nt_acc_session_t *s ;

    s = c->data;


    nt_upstream_socks_t *ss;
    ss = s->ss;

    nt_connection_t *pc = s->connection;

    nt_skb_t *skb;
    nt_skb_udp_t *udp ;

    udp = s->skb->data;

    b = s->ss->udp_conn->buffer ;

    debug( "size=%d", b->last - b->start );
    int to_fd ;
    #if 1
    #ifdef SEND_USE_TUN_FD
    to_fd = s->fd;
    #else
    to_fd = tcp->fd;
    debug( "to_fd=%d", tcp->fd );
    debug( "s_fd=%d", s->fd );
    #endif
    #endif

    int send_flag = 0;
    for( ;; ) {

        if( do_write ) {
            size = b->last - b->pos;
            if( size ) {

                c->log->action = send_action;
                //    n = c->send( c, b->pos, size );
                /* debug( " raw send size =%d", size ); */
                nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> size=%d", size );


                acc_udp_create( b, udp );

                #ifdef SEND_USE_TUN_FD
                n = write( to_fd, b->pos, size );
                #else
                /* n = sendto( to_fd, b->pos,  size, 0, tcp->src, sizeof( struct sockaddr ) ); */
                n = sendto( to_fd, b->pos,  size, 0, ( struct sockaddr * )&to, sizeof( to ) );
                #endif

                debug( "send n=%d", n );
                nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> n=%d", n );

                // 如果不可读，或者已经读完
                // break结束for循环
                if( n == NT_AGAIN ) {
                    debug( "n == NT_AGAIN" );
                    break;
                }

                // 出错，标记为eof
                if( n == NT_ERROR ) {
                    debug( "n == NT_ERROR" );
                    //                    src->write->eof = 1;
                    //                    n = 0;
                    break;
                }

                if( n >= 0 ) {
                    /* ( *packets )++;  */
                    // 缓冲区的pos指针移动，表示发送了n字节新数据
                    b->pos += n;
                    /* continue; */
                    break;
                }


            }

        }

        // size是缓冲区的剩余可用空间
        size = b->end - b->last;
        /* debug( "c->read->ready=%d", c->read->ready ); */
        /* debug( "c->read->error=%d", c->read->error ); */
        if( size && c->read->ready && ! c->read->delayed
            && !c->read->error ) {

            c->log->action = send_action;

            b->last += 18;
            n = c->recv( c, b->last, 1460 );

            debug( "read n=%d", n );
            /* debug( "read data=%s", b->start +40 ); */
            /* nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> n=%d", n ); */

            // 如果不可读，或者已经读完
            // break结束for循环
            if( n == NT_AGAIN ) {
                debug( "n == NT_AGAIN" );
                break;
            }

            // 出错，标记为eof
            if( n == NT_ERROR ) {
                debug( "n == NT_ERROR" );
                c->read->eof = 1;
                n = 0;
            }

            if( n == 0 ) {
                debug( "n == 0" );

                break;
            }

            /* debug( "buf = %s",  b->pos ); */
            if( n >= 0 ) {
                /* ( *packets )++;  */
                // 缓冲区的pos指针移动，表示发送了n字节新数据
                udp->data_len = n - 10;
                b->last += n;
                do_write = 1;
                continue;
            }
        }


        break;
    }


    if( send_flag == 0 ) {
        b->pos = b->start;
        b->last = b->start;
    }


    /* debug( "c->read->eof=%d", c->read->eof ); */
    flags = c->read->eof ? NT_CLOSE_EVENT : 0;

    /* if( nt_handle_read_event( c->read, flags  ) != NT_OK  ) { */

    /* } */

    return NT_OK;
}
