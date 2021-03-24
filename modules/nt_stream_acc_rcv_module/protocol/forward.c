#include <nt_core.h>

#include "protocol.h"


//检查跟上游的sock5服务器的连接是否已断开
nt_int_t nt_socks5_test_connection( nt_connection_t *c){

    //如果eof 为 1，或 
    if( c == NULL || ( !c->read->eof ) 
        || ( !c->read->eof && c->buffered  ) ){
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

void nt_tun_socks5_read_handler( nt_event_t *ev )
{

    nt_int_t ret;
    nt_connection_t *c;

    debug( "start" );
    c = ev->data;

    if( ev->write == 1 ){
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
    /* char buf[6556] = {0};
    int n = read( c->fd , buf , 6556 );

    debug( "n= %d", n );

    if( n > 0 )
    debug( "down buf=%s", buf ); */
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

    if(( b->last - b->pos) < 0 )
        return;

    nt_upstream_socks_send( tcp_conn , b );
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

nt_int_t nt_upstream_socks_send( nt_connection_t *c , nt_buf_t *b)
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

        if( size == 0  ) {
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
    /* int to_fd = tcp->fd; */
#else
/* 
    
    int sock = socket( AF_INET, SOCK_RAW, IPPROTO_TCP );
    int on = 1;
    if( setsockopt( sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof( on ) ) == -1 ) {
        perror( "setsockopt() failed" );
        return -1;
    } else {
    }
  
    */
    //size_t size ;
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

        usleep( 10 );
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

            if( n == 0 ){
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


    if( send_flag == 0  ) {
        b->pos = b->start;
        b->last = b->start;
    }


    /* debug( "c->read->eof=%d", c->read->eof ); */
    flags = c->read->eof ? NT_CLOSE_EVENT : 0;

    /* if( nt_handle_read_event( c->read, flags  ) != NT_OK  ) { */

    /* } */

    return NT_OK;
}
