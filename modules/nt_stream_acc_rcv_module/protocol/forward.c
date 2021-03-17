#include <nt_core.h>

#include "protocol.h"



void nt_tun_socks5_read_handler( nt_event_t *ev )
{

    nt_int_t ret;
    nt_connection_t *c;

    debug( "start" );
    c = ev->data;
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "-------------> fd=%d", c->fd );
    nt_upstream_socks_read( c );

    nt_acc_session_t *s ;
    /* nt_upstream_socks_t *ss; */

    s = c->data;
    ret = nt_tun_socks5_handle_phase( s );
    if( ret == NT_ERROR )
        return ;


    /* nt_upstream_socks_send( s->ss->tcp_conn ); */

}


void nt_tun_tcp_download_handler( nt_event_t *ev )
{

    nt_int_t ret;
    nt_connection_t *c;

    debug( "start" );
    c = ev->data;
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "-------------> fd=%d", c->fd );

    /* nt_upstream_socks_read( c ); */
    nt_downstream_raw_read_send( c );

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


    nt_skb_tcp_t *tcp;

    b = c->buffer ;
    s = c->data;
    tcp  = s->skb->data;

    ssize_t size = s->skb->skb_len - tcp->data_len ;

    debug( "size=%d", size );
    debug( "s->skb->skb_len=%d", s->skb->skb_len );
    debug( "tcp->data_len=%d", tcp->data_len );

    debug( "buf = %s", b->start + size );
    b->pos += size;

    debug( "s =%d", ( b->last - b->pos ) );
    debug( "s =%d", ( b->last - b->start ) );

    nt_upstream_socks_send( c );
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

nt_int_t nt_upstream_socks_send( nt_connection_t *c )
{

    size_t size;

    ssize_t      n;
    nt_buf_t    *b;
    nt_uint_t                    flags;
    char  *send_action;

    b = c->buffer ;

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

#define SEND_USE_TUN_FD 1

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


#ifdef SEND_USE_TUN_FD
    int tun_fd = s->fd; 
    debug( "tun_fd=%d", tun_fd );
#else

    struct sockaddr_in *addr;
    addr = ( struct sockaddr_in * )tcp->src ;

    int sock = socket( AF_INET, SOCK_RAW, IPPROTO_TCP );
    int on = 1;
    if( setsockopt( sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof( on ) ) == -1 ) {
        perror( "setsockopt() failed" );
        return -1;
    } else {
        /*printf( "setsockopt() ok\n"  );*/
    }
  
    //size_t size ;
    struct sockaddr_in to;
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = addr->sin_addr.s_addr;
    to.sin_port = addr->sin_port;

#endif



    for( ;; ) {

        if( do_write ) {

            size = b->last - b->pos;

            if( size ) {

                c->log->action = send_action;

            //    n = c->send( c, b->pos, size );
                debug( " raw send size =%d", size );

                if( size == 1500 )
                    tcp->phase = TCP_PHASE_SEND_PSH;
                else
                    tcp->phase = TCP_PHASE_SEND_PSH_END;

                acc_tcp_psh_create( b,  tcp );

                int i = 0;
                for( i=20; i< 41;i++  ){
                debug( "create i=%d", *( b->pos + i ));
                }
#ifdef SEND_USE_TUN_FD
                n = write( tun_fd, b->pos, size );
#else
                n = sendto( sock, b->pos,  size, 0, ( struct sockaddr* ) &to, sizeof( to ) );
#endif

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
                    b->pos += n;
                    continue;
                }


            }

            if( size == 0  ) {
                b->pos = b->start;
                b->last = b->start;
            }
        }


        // size是缓冲区的剩余可用空间
        size = b->end - b->last;
        if( size && c->read->ready && ! c->read->delayed
            && !c->read->error ) {

            c->log->action = send_action;

            b->last += 40;
            n = c->recv( c, b->last, size - 40 );

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

            if( n == 0 ){
                break;
            }

            debug( "buf = %s",  b->pos );
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

    flags = c->read->eof ? NT_CLOSE_EVENT : 0;
    if( nt_handle_read_event( c->read, flags  ) != NT_OK  ) {

    }

    return NT_OK;
}
