
#include <nt_core.h>
#include <nt_stream.h>
#include "nt_stream_socks5_module.h"
#include "nt_stream_socks5_raw.h"

// 重做 inet_ntoa， 可直接打印 uint32_t 的ip，而不用重新构造 struct sin_addr
// #define nt_inet_ntoa( a )  inet_ntoa( *( struct in_addr *)(&a))

/*
//            c->read->ready = 1;
//          nt_handle_read_event( c->read, NT_READ_EVENT );
//先获取客户端发过来的payload
//        nt_add_read_event( c->read ,  );
//nt_handle_read_event( c->read, NT_WRITE_EVENT  );
//        nt_add_event( c->read, NT_WRITE_EVENT ,  NT_CLEAR_EVENT );

//主动触发第二次接收payload
//不用自己主动触发， tun发过来的auth 跟payload如果分开了
//会自己触发read

//            nt_add_event( c->read, NT_WRITE_EVENT ,  NT_CLEAR_EVENT  );

 *
 * */

/*
 * socks5 阶段处理出入口
 *
 * */
nt_int_t nt_stream_socks5_handle_phase( nt_event_t *ev )
{
    nt_int_t ret;
    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_session_t         *s;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;
    nt_stream_socks5_session_t *ss;

    // 连接、会话、上游
    c = ev->data;
    s = c->data;
    u = s->upstream;
    ss = s->data;
    nt_buf_t *b;

    
    c = s->connection;
    s = c->data;
    u = s->upstream;
    ss = s->data; 

    nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> [auth handle_phase] c=%d, u=%d", s->connection->fd , u->peer.connection->fd );

    b = &u->upstream_buf;

    switch( ss->socks5_phase ) {
    case SOCKS5_VERSION_REQ: //ver send size 4
        //     nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "[socks5][send] %s 版本验证", __func__ );
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", "[socks5 auth]  [1] request" );
        socks5_method_request( b ) ;
        ss->socks5_phase = SOCKS5_VERSION_RESP ;
        break;
    case SOCKS5_USER_REQ: //user/passwd send
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", "[socks5 auth]  [2] request" );
        if( socks5_auth_request( b, "test", "test" ) < 0 ) {
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", "[socks5 auth]  [2] request fail" );
        }
        ss->socks5_phase = SOCKS5_USER_RESP ;
        break;
    case SOCKS5_SERVER_REQ: //dip/dport send
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", "[socks5 auth]  [3] request " );

        //socks5_dest_request( b, inet_addr( "14.215.177.39" ), htons( 80 ) ) ;
        if( ss->type == 2 )
            socks5_dest_request( b, INADDR_ANY, 0,  ss->type ) ;
        else{
            nt_tcp_raw_socket_t *raw;
            raw = ss->raw;
            socks5_dest_request( b, raw->dip, raw->dport, ss->type ) ;
        }

        ss->socks5_phase = SOCKS5_SERVER_RESP ;
        break;
    case SOCKS5_VERSION_RESP: //ver send ack size 2
        if( ( b->last - b->pos ) == 0 ) {
            return NT_ERROR;
        }

        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", "[socks5 auth]  [1] response" );
        if( *b->start == 0x05 && *( b->start + 1 ) == 0x02 ) {
            ss->socks5_phase = SOCKS5_USER_REQ;
        } else {
            ss->socks5_phase = SOCKS5_ERROR;
        }
        break;
        #if 1
    case SOCKS5_USER_RESP: //user/passwd send ack size 2
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> [socks5 auth]  [2] response b.size=%d", b->last - b->pos );
        if( ( b->last - b->pos ) == 0 ) {
            return NT_ERROR;
        }

        if( *b->start == 0x01 && *( b->start + 1 ) == 0x00 ) {
            ss->socks5_phase = SOCKS5_SERVER_REQ;
        } else {
            nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> [socks5 auth]  [2] response , b[1]=%d, b[2]=%d", *b->start, *( b->start + 1 ) );
            ss->socks5_phase = SOCKS5_ERROR;
        }

        break;
    case SOCKS5_SERVER_RESP: //dip/dport send ack size 2
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", "[socks5 auth]  [3] response" );
        if( ( b->last - b->pos ) == 0 ) {
            return NT_ERROR;
        }


        if( *b->start == 0x05 && *( b->start + 1 ) == 0x00 ) {
            ss->socks5_phase = SOCKS5_DATA_FORWARD;

            if( ss->type == 2 ) {

                struct sockaddr_in a;

                // 看服务器返回的地址。
                uint32_t udp_proxy_ip =  *( in_addr_t * )( b->start + 4 );
                uint16_t udp_proxy_port = *( short * )( b->start + 8 ) ;
                //*(short*)( &abyUdpAssociateBuf[8]  )
                a.sin_addr.s_addr = udp_proxy_ip;

                nt_log_debug3( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s %s:%d", "[socks5 auth]  [3] response udp ", nt_inet_ntoa( ss->proxy_addr ), ntohs( udp_proxy_port ) );

                nt_stream_socks5_change_ip( ss->udp, ss->proxy_addr , udp_proxy_port );
            }

            nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s%d", "[socks5 auth]  [3] response, size=", ss->data_len );
            if( ss->data_len ) {
                nt_stream_socks5_handle_phase( ev );
            } else {
                //主动触发
                nt_stream_socks5_downstream_first_read_handler( c->read );
            }


        } else {
            ss->socks5_phase = SOCKS5_ERROR;
        }

        break;
    case SOCKS5_DATA_FORWARD: //payload send
        //不建议用改变指针的方式。
        // sctx->upstream_buf = sctx->downstream_buf;

        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "[socks5][发送payload]", __func__ );

        /* b->last = b->start ;
        b->pos = b->start ; */

        ss = s->data ;
        if( ss->type == 2 ) {
            nt_stream_session_t *udp;
            nt_stream_upstream_t        *uu;
            udp = ss->udp;
            uu = udp->upstream;


            nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "-ss->connect == %d",  ss->connect );
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "- uu->connected  == %d",   uu->connected  );
            //连接udp
            if( ss->connect == 2 && uu->connected == 0 ) {
                nt_stream_socks5_connect( ss->udp );

                if( uu->connected == 0 ){
                    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "-%s", "发起udp 连接失败" );
                    ss->socks5_phase = SOCKS5_ERROR;
                    break;
                }

                nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "-%s", "发起udp 连接成功" );
            }

            if( ss->connect == 0 )
                break;

            if( uu->connected ){
                ss->connect = 0;
            }

            //  c->fd = uu->peer.connection->fd ;
            // 这里需要将ev改为udp 的ev;
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "new udp fd=%d",  uu->peer.connection->fd );
            ev = uu->peer.connection->write ;
            uu->peer.connection->read->handler = nt_stream_socks5_u2d_handler;
            uu->peer.connection->write->handler = nt_stream_socks5_upstream_write_handler;

            //注册 上游的read事件
            //nt_add_event( uu->peer.connection->read, NT_READ_EVENT,  NT_CLEAR_EVENT );

            udp->data = ss ;
            b = &uu->upstream_buf;
        }

        //避免指针位置被偏移
        //b->last = nt_copy( b->start, sctx->downstream_buf.start,  sctx->downstream_buf.last - sctx->downstream_buf.start );
        //payload 从pos开始

        b->last = nt_copy( b->start, u->downstream_buf.pos,  u->downstream_buf.last - u->downstream_buf.pos );
        u->downstream_buf.last = u->downstream_buf.start;
        u->downstream_buf.pos = u->downstream_buf.start;
        //不加+ ，可测试错误情况
        /* sctx->upstream_buf.last +=  sctx->downstream_buf.last - sctx->downstream_buf.start ; */



        //如果是udp 需要增加一个 udp 类型的upstream;

        //发送payload
        ret = nt_stream_socks5_pre_forward( ev, 1, 1 );
        if( ret != NT_OK )
            return NT_ERROR;

        nt_log_debug0( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> 设置rw 新回调" );
        /* nt_stream_socks5_session_t *ss; */

        // c是下游连接，pc是上游连接
        c = s->connection;
        pc = u->peer.connection;

        //完成socks5的验证过程后，改回原来的回调函数，用来发送接收payload， 不增加socks5头


        c->write->handler = nt_stream_socks5_u2d_handler;
        c->read->handler = nt_stream_socks5_downstream_read_handler;

        if( ss->type == 2 ) { //UDP
            pc->read->handler = nt_stream_socks5_u2d_handler;
            pc->write->handler = nt_stream_socks5_upstream_write_handler;
        } else { //TCP
            pc->read->handler = nt_stream_socks5_u2d_handler;
            pc->write->handler = nt_stream_socks5_upstream_write_handler;
        }


        //测试代码， 此处不能这么设置
        /* if( nt_handle_write_event( c->read, 0 ) != NT_OK ) {
            nt_stream_socks5_finalize( s,
                                        NT_STREAM_INTERNAL_SERVER_ERROR );
            return NT_DONE;
        } */


        //触发upstream payload 转发
        //先 触发upstream 的write ，把第一次携带的payload发给 socks服务器
        //  nt_stream_socks5_process_test( s, 1, 1 );
        return NT_OK;

        break;
        #endif
    }

    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> socks5_phase=%d", ss->socks5_phase );
    nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "########--> %s, %s", __func__, "end" );
    if( ss->socks5_phase == SOCKS5_ERROR ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return NT_ERROR;
    }

    return NT_OK;
}



/*
 * 解析客户端发送过来的认证数据以及载荷
 *
 * */
nt_int_t nt_stream_socks5_parse_client_data( nt_stream_session_t *s, nt_buf_t *b )
{

    nt_connection_t    *c;
    nt_stream_socks5_session_t *ss;

    debug(  "socks5_parse_client_data" );
    /*
        //先获取客户端发过来的payload
        nt_stream_socks5_ctx_t *sctx = nt_stream_get_module_ctx( s, nt_stream_socks5_module );
        nt_buf_t *b;
        b = &sctx->downstream_buf;
    */
    /* return ; */
    if( !b )
        return NT_ERROR;

    size_t size = b->last - b->start ;

    /* nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> parse_client_data size = %d", size); */
    if( !size )
        return NT_ERROR;

    c = s->connection ;
    ss = s->data;

    // 1为auth connect 阶段。
    if( ss->connect == 1 ) {
        nt_acc_socks5_auth_t *asa = ( nt_acc_socks5_auth_t* )  b->start;
        ss->type = asa->protocol;

        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s ", "解析本地转发过来的数据" );
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> user= %s",  asa->user );
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> password= %s",  asa->password );
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> server_ip= %s",  nt_inet_ntoa( asa->server_ip ) );
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> server_port= %d", ntohs( asa->server_port ) );
        debug(   "sip=%u.%u.%u.%u:%d", IP4_STR( asa->sip ), ntohs( asa->sport ) );
        debug(   "dip=%u.%u.%u.%u:%d", IP4_STR( asa->dip ), ntohs( asa->dport ) );
        ss->proxy_addr = asa->server_ip ;
        ss->proxy_port = asa->server_port ;

        struct in_addr addr;
        //memcpy(&addr, &asa->dip, 4);
        uint32_t ddd;
        ddd = inet_addr( "119.29.29.29" );
        // memcpy(&addr, &ddd, 4);
        memcpy( &addr, &asa->dip, 4 );
        debug(  "111udp set dst ip=%s", inet_ntoa( addr ) );

        

        ss->type = asa->protocol;
        //下面为解析认证信息
        //tcp
        if( asa->protocol == 1 ) {
            nt_tcp_raw_socket_t *raw;
            raw = ( nt_tcp_raw_socket_t * ) nt_palloc( c->pool,  sizeof( nt_tcp_raw_socket_t ) );
            raw->sip = asa->sip;
            raw->sport = asa->sport;

            raw->dip = asa->dip;
            raw->dport = asa->dport;
            ss->raw = raw ;

            nt_stream_socks5_change_ip( s, asa->server_ip, asa->server_port );

            if( size != sizeof( nt_acc_socks5_auth_t ) ) {
                //data 一起过来了。可以直接读取
                nt_acc_tcp_t *at = ( nt_acc_tcp_t* )( b->start + sizeof( nt_acc_socks5_auth_t ) );
                raw->seq = at->seq;
                raw->ack_seq = at->ack  ;
                raw->payload_len =  0;
                nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> seq=%d, ack=%d",  ntohl( at->seq ), ntohl( at->ack ) );


                b->pos = b->start + sizeof( nt_acc_socks5_auth_t ) +  sizeof( nt_acc_tcp_t ) - 1500;
                ss->data_len = b->last - b->pos;
                nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "---->conn buf= %s, len=%d",  b->pos, ss->data_len );
            } else {
                b->last = b->start ;
                b->pos = b->start ;
                ss->data_len = 0;
            }

        } else if( asa->protocol == 2 ) {
            nt_udp_raw_socket_t *raw;
            raw = ( nt_udp_raw_socket_t * ) nt_palloc( c->pool,  sizeof( nt_udp_raw_socket_t ) );
            raw->sip = asa->sip;
            raw->sport = asa->sport;

            raw->dip = asa->dip;
            raw->dport = asa->dport;
            nt_stream_socks5_change_ip( s, asa->server_ip, asa->server_port );

            if( size != sizeof( nt_acc_socks5_auth_t ) ) {
                //data 一起过来了。可以直接读取
                //构造socks5的udp 数据头部分
                b->pos = b->start + sizeof( nt_acc_socks5_auth_t ) - 10;
#pragma pack(1)
                struct socks5_udp_header {
                    uint16_t rsv;
                    uint8_t frag;
                    uint8_t atyp;
                    uint32_t ip;
                    uint16_t port;
                };
#pragma pack()

                nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "---->struct socks5_udp_header=%d", sizeof( struct socks5_udp_header ) );
                struct socks5_udp_header *suh = ( struct socks5_udp_header * )b->pos;
                suh->rsv = 0;
                suh->frag = 0;
                suh->atyp = 1;
                suh->ip = raw->dip;
                suh->port = raw->dport;

                struct sockaddr_in a;
                //a.sin_addr.s_addr = *( uint32_t * )( b->pos + 4 );
                a.sin_addr.s_addr = raw->dip;
                debug(  "udp set dst ip=%s", inet_ntoa( a.sin_addr ) );

                memcpy( &addr, &raw->dip, 4 );
                debug(  "111udp set dst ip=%s", inet_ntoa( addr ) );


                nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "---->to ip= %s", inet_ntoa( a.sin_addr ) );

                ss->data_len = b->last - b->pos;
                nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "---->conn buf= %s, len=%d",  b->pos, ss->data_len );
            } else {
                b->last = b->start ;
                b->pos = b->start ;
                ss->data_len = 0;
            }
            ss->raw = raw ;
        }


    } else {
        //udp 不用解析payload
        if( ss->type == 2 ) {
            nt_udp_raw_socket_t *raw;
            raw = ss->raw;
            //  b->pos = b->start + sizeof( nt_acc_socks5_auth_t ) - 10;
#pragma pack(1)
            struct socks5_udp_header {
                uint16_t rsv;
                uint8_t frag;
                uint8_t atyp;
                uint32_t ip;
                uint16_t port;
            };
#pragma pack()

            nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "---->struct socks5_udp_header=%d", sizeof( struct socks5_udp_header ) );
            struct socks5_udp_header *suh = ( struct socks5_udp_header * )b->pos;
            suh->rsv = 0;
            suh->frag = 0;
            suh->atyp = 1;
            suh->ip = raw->dip;
            suh->port = raw->dport;

            ss->data_len = b->last - b->pos;
            nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "---->conn buf= %s, len=%d",  b->pos, ss->data_len );

            return NT_OK;
        }

        //tcp
        nt_acc_tcp_t *at = ( nt_acc_tcp_t* )  b->start;
        nt_tcp_raw_socket_t *raw;
        raw = ss->raw;
        raw->seq = at->seq;
        raw->ack_seq = at->ack  ;
        raw->payload_len =  0;
        b->pos = b->start +  sizeof( nt_acc_tcp_t ) - 1500;
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "---->接收到的data len= %d", b->last -  b->pos );
//        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "---->data buf= %s",  b->pos  );
    }

    return NT_OK;


}




/*
 * 返回NGCX_DONE，表示连接结束
 *
 * */
//参考 nt_stream_socks5_process_connection
//第一个连接触发的认证转发
nt_int_t nt_stream_socks5_pre_forward( nt_event_t *ev, nt_int_t from_upstream, nt_uint_t do_write )
{
    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_session_t         *s;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    // 连接、会话、上游
    c = ev->data;
    s = c->data;
    u = s->upstream;
    /* nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s, from_upstream=%d", __func__, from_upstream ); */

    if( c->close ) {
        nt_log_error( NT_LOG_INFO, c->log, 0, "shutdown timeout" );
        nt_stream_socks5_finalize( s, NT_STREAM_OK );
        return NT_DONE;
    }

    // c是下游连接，pc是上游连接
    c = s->connection;
    pc = u->peer.connection;

    pscf = nt_stream_get_module_srv_conf( s, nt_stream_socks5_module );

    // 超时处理，没有delay则失败
    if( ev->timedout ) {
        ev->timedout = 0;

        if( ev->delayed ) {
            ev->delayed = 0;

            if( !ev->ready ) {
                if( nt_handle_read_event( ev, 0 ) != NT_OK ) {
                    nt_stream_socks5_finalize( s,
                                                NT_STREAM_INTERNAL_SERVER_ERROR );
                    return NT_DONE;
                }

                if( u->connected && !c->read->delayed && !pc->read->delayed ) {
                    nt_add_timer( c->write, pscf->timeout );
                }

                return NT_AGAIN;
            }

        } else {
            if( s->connection->type == SOCK_DGRAM ) {

                if( pscf->responses == NT_MAX_INT32_VALUE
                    || ( u->responses >= pscf->responses * u->requests ) ) {

                    /*
                     * successfully terminate timed out UDP session
                     * if expected number of responses was received
                     */

                    handler = c->log->handler;
                    c->log->handler = NULL;

                    nt_log_error( NT_LOG_INFO, c->log, 0,
                                   "udp timed out"
                                   ", packets from/to client:%ui/%ui"
                                   ", bytes from/to client:%O/%O"
                                   ", bytes from/to upstream:%O/%O",
                                   u->requests, u->responses,
                                   s->received, c->sent, u->received,
                                   pc ? pc->sent : 0 );

                    c->log->handler = handler;

                    nt_stream_socks5_finalize( s, NT_STREAM_OK );
                    return NT_DONE;
                }

                nt_connection_error( pc, NT_ETIMEDOUT, "upstream timed out" );

                pc->read->error = 1;

                nt_stream_socks5_finalize( s, NT_STREAM_BAD_GATEWAY );

                return NT_DONE;
            }

            nt_connection_error( c, NT_ETIMEDOUT, "connection timed out" );

            nt_stream_socks5_finalize( s, NT_STREAM_OK );

            return NT_DONE;
        }

    } else if( ev->delayed ) {

        nt_log_debug0( NT_LOG_DEBUG_STREAM, c->log, 0,
                        "stream connection delayed" );

        if( nt_handle_read_event( ev, 0 ) != NT_OK ) {
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        }

        return NT_DONE;
    }

    if( from_upstream && !u->connected ) {
        return NT_DONE;
    }

    // 核心处理函数，处理两个连接的数据收发
    return nt_stream_socks5_forward( s, from_upstream, do_write );
}



/*
 * from_upstream =1 , do_write = 0 , 上游的read
 * from_upstream =1 , do_write = 1 , 上游的write
 *
 * from_upstream =0 , do_write = 0 , 下游的read
 * from_upstream =0 , do_write = 1 , 下游的write
 *
 * */
//参考 nt_stream_socks5_process
nt_int_t nt_stream_socks5_forward( nt_stream_session_t *s, nt_uint_t from_upstream, nt_uint_t do_write )
{

    nt_log_debug3( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s, up=%d, do_write=%d", __func__, from_upstream, do_write );
    char                         *recv_action, *send_action;
    off_t                        *received, limit;
    size_t                        size, limit_rate;
    ssize_t                       n;
    nt_buf_t                    *b;
    nt_int_t                     rc;
    nt_uint_t                    flags, *packets;
    nt_msec_t                    delay;
    nt_chain_t                  *cl, **ll, **out, **busy;
    nt_connection_t             *c, *pc, *src, *dst;
    nt_connection_t              *r, *w;
    nt_log_handler_pt            handler;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    // u是上游结构体
    u = s->upstream;

    // c是下游的连接
    c = s->connection;

    // pc是上游的连接，如果连接失败就是nullptr
    pc = u->connected ? u->peer.connection : NULL;

    // nginx处于即将停止状态，连接是udp
    // 使用连接的log记录日志
    if( c->type == SOCK_DGRAM && ( nt_terminate || nt_exiting ) ) {

        /* socket is already closed on worker shutdown */

        handler = c->log->handler;
        c->log->handler = NULL;

        nt_log_error( NT_LOG_INFO, c->log, 0, "disconnected on shutdown" );

        c->log->handler = handler;

        nt_stream_socks5_finalize( s, NT_STREAM_OK );
        return NT_DONE;
    }

    // 取proxy模块的配置
    pscf = nt_stream_get_module_srv_conf( s, nt_stream_socks5_module );
    nt_stream_socks5_ctx_t *sctx = nt_stream_get_module_ctx( s, nt_stream_socks5_module );
    nt_stream_socks5_session_t *ss = s->data;

    // 根据上下游状态决定来源和目标
    // 以及缓冲区、限速等
    // 注意使用的缓冲区指针
    if( from_upstream ) {
        // 数据下行i
        /*
            r = src = pc;  //上游
            w = dst = c;   //下游
                //上游读
            //上游 --> 下游
                //下游写
        */
        r = src = pc;  //上游
        w = dst = pc;   //下游
        //上游读
        //上游 --> 下游
        //下游写

        nt_log_debug2( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> connect=%d, type=%d", ss->connect, ss->type  );
        //是udp , 并且处于转发payload阶段
        if( ss->connect == 0 && ss->type == 2 ) {
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", "当前需要转发udp 数据" );
            pc->recv = nt_udp_recv;
            pc->send = nt_udp_send;
        } else {
            pc->recv = nt_recv;
            pc->send = nt_send;
        }

        // 缓冲区是upstream_buf，即上游来的数据
        //    b = &sctx->upstream_buf;
        b = &u->upstream_buf;
        limit_rate = u->download_rate;
        received = &u->received;
        packets = &u->responses;
        out = &u->downstream_out;
        busy = &u->downstream_busy;
        recv_action = "socks5ing and reading from upstream";
        send_action = "socks5ing and sending to client";

    } else {
        // 数据上行
        /*
            src = c;   //下游
            dst = pc;  //上游
                //下游读
            //下游 --> 上游
                //上游写
        */
        r = src = c;  //上游
        w = dst = c;   //下游

        // 缓冲区是downstream_buf，即下游来的数据
        // 早期downstream_buf直接就是客户端连接的buffer
        // 现在是一个正常分配的buffer
        //   b = &sctx->downstream_buf;
        b = &u->downstream_buf;
        limit_rate = u->upload_rate;
        received = &s->received;
        packets = &u->requests;
        out = &u->upstream_out;
        busy = &u->upstream_busy;
        recv_action = "socks5ing and reading from client";
        send_action = "socks5ing and sending to upstream";
    }

    if( do_write ) { //处理send

        nt_log_debug2( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> fd=%d, %s", dst->fd, "do send" );
        for( ;; ) {
            size = b->last - b->pos;
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> size=%d", size );

            if( size ) {
//            if( size && src->write->ready && !src->write->delayed
//                && !src->write->error ) {
                c->log->action = send_action;
                n = src->send( dst, b->pos, size );

                nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> n=%d", n );

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
                    ( *packets )++;
                    // 缓冲区的pos指针移动，表示发送了n字节新数据
                    b->pos += n;
                    continue;
                }
            }
            break;
        }

        //发送完毕需要取消write 事件，否则非epoll ET模式 会一直触发
        /**
         * 调用此函数的nt_handle_write_event的意思是
         * dst->write ， 可写、已就绪，就删除写事件
         * 不可写、未就绪，就添加写事件
         */
            
        /* if( nt_handle_write_event( dst->write, 0 ) != NT_OK ) {
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return NT_DONE;
        } */

    } else { //处理recv

        // b指向当前需要操作的缓冲区
        // 死循环操作，直到出错或者again
        for( ;; ) {
            // size是缓冲区的剩余可用空间
            size = b->end - b->last;

            /* nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> size=%d", size );
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> src->read->ready=%d", src->read->ready );
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> src->read->delayed=%d",  src->read->delayed );
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> src->read->error=%d",  src->read->error );
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> src->read->eof=%d",  src->read->eof ); */

            // 如果缓冲区有剩余，且src还可以读数据
            if( size && src->read->ready && !src->read->delayed
                && !src->read->error ) {

                c->log->action = recv_action;

                // 尽量读满缓冲区
                n = src->recv( src, b->last, size );

                nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> n=%d", n );
                // 如果不可读，或者已经读完
                // break结束for循环
                if( n == NT_AGAIN ) {
                    break;
                }

                // 出错，标记为eof
                if( n == NT_ERROR ) {
                    src->read->eof = 1;
                    n = 0;
                }

                // 读取了n字节的数据
                if( n >= 0 ) {
                    ( *packets )++;
                    // 增加接收的数据字节数
                    *received += n;
                    // 缓冲区的末尾指针移动，表示收到了n字节新数据
                    b->last += n;

                    // 读数据部分结束
                    continue;
                }
            }

            break;
        }

        c->log->action = "socks5ing connection";
        // 这时应该是src已经读完，数据也发送完
        // 读取出错也会有eof标志
        int ret = 0;
        if( ( ret = nt_stream_socks5_test_finalize( s, from_upstream ) ) == NT_OK ) {
            return NT_DONE;
        }

        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> ret=%d", ret );

        // 如果eof就要关闭读事件
        //my_test 取消该写法，改用直接赋值
#if 1
        flags = src->read->eof ? NT_CLOSE_EVENT : 0;
        if( nt_handle_read_event( src->read, flags ) != NT_OK ) {
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return;
        }
#endif

    }
    
    nt_log_debug2( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> c->active=%d, c->ready=%d", c->read->active, c->read->ready );


    nt_log_debug0( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> forward end" );
    return NT_OK;
}




/*
 * 连接首次过来必须经过这里，以便发起sock5认证
 * 从tun接收数据。 接收后，需要先判断数据类型，然后发起sock5连接
 * 然后转发payload
 * */
void nt_stream_socks5_downstream_first_read_handler( nt_event_t *ev )
{
    nt_int_t ret;
    nt_buf_t *b;
    nt_connection_t             *c;
    nt_stream_session_t         *s;
    nt_stream_upstream_t        *u;
    c = ev->data;
    s = c->data;
    u = s->upstream;
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "#################--> %s", __func__ );


    //先获取客户端发过来的payload
    /* nt_stream_socks5_ctx_t *sctx = nt_stream_get_module_ctx( s, nt_stream_socks5_module );
    b = &sctx->downstream_buf; */


    b = &u->downstream_buf ;
    b->last = b->start ;
    b->pos = b->start ;

    //接收tun 客户端过来的数据
    ret = nt_stream_socks5_pre_forward( ev, 0, 0 );
    if( ret != NT_OK )
        return;

//    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", "start parse" );

    nt_stream_socks5_parse_client_data( s,  b );

    //这里需要先把client方向过来的read 取消监听，不然在认证过程会直接触发data内容的接收。

    c->read->active = 0;
    //同等于 设置 c->read->active = 0;
    //nt_del_event( c->read, NT_READ_EVENT, NT_CLOSE_EVENT );

    nt_stream_socks5_session_t *ss = s->data;

    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> ss->connect=%d", ss->connect );
    if( ss->connect == 1 ) {
        /*
         * 客户端发送首次连接信息到这里， nginx发起到socks5 server的tcp连接。
         * 必须等连接完成才能触发 upstream_first_write
         * 所以必须把upstream_first_write的触发写在 nt_stream_socks5_init_upstream 的最结尾
         * */
        nt_stream_socks5_connect( s );
        c->read->active = 1;

        if( ss->type == 2 ) {
            ss->connect = 2;
        } else
            ss->connect = 0;


    } else {
        //接收payload部分
        nt_stream_socks5_handle_phase( ev );
    }

    nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> [handle_phase] c=%d, u=%d", c->fd, u->peer.connection->fd );
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> size=%d", b->last - b->start );


    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "##########%s", "===============================================================" );
}


/*
 * 正常不用触发回应tun客户端
 * */
void nt_stream_socks5_downstream_first_write_handler( nt_event_t *ev )
{

    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_session_t         *s;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    // 连接、会话、上游
    c = ev->data;
    s = c->data;
    u = s->upstream;
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "##########%s", "===============================================================" );

}


/*
 * 该回调用来接收socks 认证时 接收server过来的数据
 * 数据过来后自己触发read 
 * */
void
nt_stream_socks5_upstream_first_read_handler( nt_event_t *ev )
{
    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_session_t         *s;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    // 连接、会话、上游
    c = ev->data;
    s = c->data;
    u = s->upstream;
    c = s->connection;  //这个才是client 的

    int fd = u->peer.connection->fd ;


    // nt_add_event( c->read, NT_READ_EVENT,  NT_CLEAR_EVENT );
    // nt_add_event( c->write, NT_READ_EVENT,  NT_CLEAR_EVENT );

    int ret = 0;
    nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "########--> %s, fd=%d", __func__, fd );
    nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> [handle_phase] c=%d, u=%d", c->fd, u->peer.connection->fd );

    char buf[128];

    nt_stream_socks5_ctx_t *sctx = nt_stream_get_module_ctx( s, nt_stream_socks5_module );
    nt_buf_t *b;
    /* b = &sctx->upstream_buf; */
    b = &u->upstream_buf;

    b->last = b->start ;
    b->pos = b->start ;

    // my_test ,测试read之后，为什么自己触发了write
    pc = u->peer.connection;
    //   int len = read_n_bytes( fd, buf, sizeof( buf ) ) ;
    //    int len = u->peer.connection->recv( u->peer.connection, buf, 128 );
    /* nt_del_event(  pc->write, NT_READ_EVENT, 0  );
       return;
       */
    //从上游接收数据
    ret = nt_stream_socks5_pre_forward( ev, 1, 0 );
    if( ret != NT_OK )
        return;

    /* nt_del_event(  pc->write, NT_WRITE_EVENT, 0  );
    return; */

    //分析回应结果
    ret = nt_stream_socks5_handle_phase( ev );

    if( ret !=  NT_OK )
        return;

    nt_stream_socks5_session_t *ss;
    ss = s->data;
    if( ss->socks5_phase == SOCKS5_DATA_FORWARD )
        return;

    if( ss->socks5_phase == SOCKS5_ERROR )
        return;

    //触发upstream write 函数
    nt_log_debug0( NT_LOG_DEBUG_STREAM,  c->log, 0, "########--> 调用 upstream first write" );
    #if 0
    if( nt_handle_write_event( u->peer.connection->write, 0 ) != NT_OK ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return;
    }
    #else
    nt_stream_socks5_upstream_first_write_handler( u->peer.connection->write );
    #endif



    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "##########%s", "===============================================================" );
    return ;

}

/*
 * 主动调用sock5 认证过程
 *  upstream send
 * */
void
nt_stream_socks5_upstream_first_write_handler( nt_event_t *ev )
{
    nt_int_t ret;

    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_session_t         *s;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    // 连接、会话、上游
    c = ev->data;       //这个是upstream 的
    s = c->data;
    c = s->connection;  //这个才是client 的
    u = s->upstream;
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "#########--> %s", __func__ );
    nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> [handle_phase] c=%d, u=%d", c->fd, u->peer.connection->fd );

//    nt_stream_socks5_ctx_t *sctx = nt_stream_get_module_ctx( s, nt_stream_socks5_module );
    nt_buf_t *b;
//    b = &sctx->upstream_buf;
    b = &u->upstream_buf;
    b->pos = b->start;
    b->last = b->start;

    nt_stream_socks5_session_t *ss;
    ss = s->data;

    //nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> [handle_phase] buf size=%d",   );

    ret = nt_stream_socks5_handle_phase( ev );
    if( ret != NT_OK )
        return;


//    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "[socks5][send] %s   call nt_stream_socks5_pre_forward", __func__ );
    //  u->peer.connection->send( u->peer.connection, b->pos, 4  );
    //nt_stream_socks5_pre_forward( ev, 1, 1 );
    //

    ret = nt_stream_socks5_pre_forward( ev, 1, 1 );
    if( ret != NT_OK )
        return;


    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "##########%s", "===============================================================" );

    //  nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "[socks5][send] %s   end", __func__ );

}



/* 
 * 本回调必须是连接非首次发送数据包，连接以及完成过socks5 认证过程
 * ss->type 为SOCKS_DATA_FORWARD
 * 客户端发送过来的信息 read触发函数
 */
void nt_stream_socks5_downstream_read_handler( nt_event_t *ev )
{
    nt_int_t ret;
    nt_connection_t             *c;
    nt_stream_session_t         *s;
    nt_stream_upstream_t        *u;
    c = ev->data;
    s = c->data;
    u = s->upstream;
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "#################--> %s", __func__ );


    //先获取客户端发过来的payload
    nt_stream_socks5_ctx_t *sctx = nt_stream_get_module_ctx( s, nt_stream_socks5_module );
    nt_buf_t *db, *ub;
    db = &u->downstream_buf;
    ub = &u->upstream_buf;

    db->last = db->start ;
    db->pos = db->start ;

    //接收tun 客户端过来的数据
    //即 用down buf 接收数据
    ret = nt_stream_socks5_pre_forward( ev, 0, 0 );
    if( ret != NT_OK )
        return;


    debug(   "start parse" );
    //必须数据先接收完才解析
    if( nt_stream_socks5_parse_client_data( s,  db ) != NT_OK ) {
        return;
    }

//    nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "db buf=%s, len=%d", db->pos, db->last - db->pos );


    nt_stream_socks5_session_t *ss;
    ss = s->data;

    if( ss->type == 2 ) {
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", "当前需要转发udp 数据" );

        nt_stream_session_t *udp;
        udp = ss->udp;
        u = udp->upstream;

        nt_connection_t  *pc;
        pc = u->connected ? u->peer.connection : NULL;

        ub = &u->upstream_buf;
        //这里多了一步拷贝的动作，影响效率，后面再优化
        //把nt_stream_socks5_pre_forward函数改成可指定发送的buf变量即可
        ub->last = ub->start ;
        ub->pos = ub->start ;

        //    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "#################--> %d", db->last - db->pos );
        ub->last = nt_cpymem( ub->start, db->pos, db->last - db->pos );

 //       nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "ub buf=%s", ub->start );
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, " ub len = %d", ub->last - ub->pos );


        nt_stream_socks5_pre_forward( pc->write, 1, 1 );

    } else {

        //这里多了一步拷贝的动作，影响效率，后面再优化
        //把nt_stream_socks5_pre_forward函数改成可指定发送的buf变量即可
        ub->last = ub->start ;
        ub->pos = ub->start ;

        //    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "#################--> %d", db->last - db->pos );
        ub->last = nt_cpymem( ub->start, db->pos, db->last - db->pos );

        //nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "ub buf=%s", ub->start );
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, " ub len = %d", ub->last - ub->pos );
        //上游写 , 把downbuf发给上游


        nt_stream_socks5_pre_forward( ev, 1, 1 );
    }
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "##########%s", "===============================================================" );

}

/*
 * 本回调正常不调用
 * 
 * */
void
nt_stream_socks5_downstream_write_handler( nt_event_t *ev )
{

    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_session_t         *s;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    // 连接、会话、上游
    c = ev->data;
    s = c->data;
    u = s->upstream;

    debug(   "proxy #####下游###write#####" );
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "######下游###write#####----> %s", __func__ );

    return ;
    //这里添加，读取信息，并改变buf
    //
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", __func__ );

//    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0 ,"%s start", __func__ );
    nt_stream_socks5_process_connection( ev, ev->write );
}


/* 接收上游发过来的数据 */
void
nt_stream_socks5_upstream_read_handler( nt_event_t *ev )
{
    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_session_t         *s;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    // 连接、会话、上游
    c = ev->data;
    s = c->data;
    u = s->upstream;

    //这里添加，读取信息，并改变buf
    //


    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "######上游###read#####----> %s", __func__ );

//    c->write->active = 0;
//    c->write->ready = 0;
//    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0 ,"%s start", __func__ );
//

    //调用完接收数据后，要触发下游的发送
    //当前    ev->write = 0 ， 即from_upstream = 1; 从上游读数据
    //

    //先判断buf是否有可发送的内容，如果有调用send发往下游，
    //如果还是没发全，接着注册下游写事件,直到发成功为止。
    //调用 recv 读数据。再触发send

    // nt_stream_socks5_process_connection( ev, !ev->write );
}

/* 接收上游发过来的数据 */
void
nt_stream_socks5_upstream_write_handler( nt_event_t *ev )
{
    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_session_t         *s;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    // 连接、会话、上游
    c = ev->data;
    s = c->data;
    u = s->upstream;

    nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "##########----> %s , fd=%d", __func__, c->fd );

    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "##########----> ev->write=%d", ev->write );
    /* if( nt_stream_socks5_test_finalize( s, 1 ) == NT_OK ) {
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s ", "finalize OK" );
        return;
    } */

     nt_stream_socks5_process_connection( ev, !ev->write );
}




/* 接收上游发过来的数据 */
void
nt_stream_socks5_u2d_handler( nt_event_t *ev )
{

    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_session_t         *s;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    // 连接、会话、上游
    c = ev->data;
    s = c->data;
    u = s->upstream;

    nt_stream_socks5_u2d_forward( s, 1,  1 );


}

#if 1
nt_int_t
nt_stream_socks5_u2d_forward_to_client( nt_stream_session_t *s, nt_uint_t from_upstream,
        nt_uint_t do_write )
{

    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_upstream_t        *u;

    // u是上游结构体
    u = s->upstream;

    // c是下游的连接
    c = s->connection;

    // pc是上游的连接，如果连接失败就是nullptr
    pc = u->connected ? u->peer.connection : NULL;

    /* nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s start", __func__ ); */

    nt_buf_t *ub, *db;
    ub = &u->upstream_buf;
    db = &u->downstream_buf;
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> send size= %d", ub->last - ub->pos );


    nt_stream_socks5_session_t *ss;
    ss = s->data;
    int sock;

    if( ss->type == 2 ) {
        sock = socket( AF_INET, SOCK_RAW, IPPROTO_UDP );
    } else {
        sock = socket( AF_INET, SOCK_RAW, IPPROTO_TCP );
    }

    if( sock == -1 ) {
        perror( "socket() failed" );
        return -1;

    } else {
        /* printf( "socket() ok\n" ); */

    }

    int on = 1;
    if( setsockopt( sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof( on ) ) == -1 ) {
        perror( "setsockopt() failed" );
        return -1;

    } else {
        /* printf( "setsockopt() ok\n" ); */

    }


    //方式一 socket raw
    size_t size ;
    struct sockaddr_in to;
    to.sin_family = AF_INET;

    if( ss->type == 2 ) {
        nt_udp_raw_socket_t *raw = ss->raw;
        to.sin_addr.s_addr = raw->sip;
        to.sin_port = raw->sport;

        raw->data = ub->pos + 10 ;
        raw->data_len =  ub->last - ub->pos - 10;

        //    b->pos 每次发送最多以1400个大小发送，因此如果内容超过1400个需要分开发送；
        if( raw->data_len > 1400 ) {
            raw->data_len = 1400;
        }

        nt_udp_create( db,  raw );
        size = raw->tot_len;
    } else {
        nt_tcp_raw_socket_t *raw = ss->raw;
        raw->phase = TCP_PHASE_SEND_PSH;
        to.sin_addr.s_addr = raw->sip;
        to.sin_port = raw->sport;

        raw->data = ub->pos ;
        raw->data_len =  ub->last - ub->pos;

        //    b->pos 每次发送最多以1400个大小发送，因此如果内容超过1400个需要分开发送；
        if( raw->data_len > 1460 ) {
            raw->data_len = 1460;
        }
        nt_tcp_create( db,  raw );
        size = raw->tot_len;
    }


    /* int bytes = nt_unix_sendto( sock, ub->start,  raw->tot_len, 0, ( struct sockaddr*  ) &to, sizeof( to  ) ); */
    int bytes = sendto( sock, db->start,  size, 0, ( struct sockaddr* ) &to, sizeof( to ) );
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> bytes =%d", bytes );
    if( bytes == -1 ) {
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> raw send fail%s",  "" );
        perror( "sendto() failed" );
        return -1;
    } else {
        /* nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> raw send size=%d",  bytes ); */
        /* printf( "sendto() ok\n" ); */
    }


    close( sock );
    if( ss->type == 2 )
        return bytes  - 18;
    else
        return bytes - 40;

}
#endif

/*
 * 新回调接口，为了可以发送原始套接字
 *
 * 如果触发的是下游write的ev， 说明send没成功
 * 如果触发的是上游read的ev,说明数据来了，或者数据没读完
 * 从上游 read 到下游 write
 *
 * 下游write采用原始套接字发出去
 * */
void
nt_stream_socks5_u2d_forward( nt_stream_session_t *s, nt_uint_t from_upstream,
                               nt_uint_t do_write )
{
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );
    char                         *recv_action, *send_action;
    off_t                        *received, limit;
    size_t                        size, limit_rate;
    ssize_t                       n;
    nt_buf_t                    *b;
    nt_int_t                     rc;
    nt_uint_t                    flags, *packets;
    nt_msec_t                    delay;
    nt_chain_t                  *cl, **ll, **out, **busy;
    nt_connection_t             *c, *pc, *src, *dst;
    nt_log_handler_pt            handler;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;


    // u是上游结构体
    u = s->upstream;

    // c是下游的连接
    c = s->connection;

    // pc是上游的连接，如果连接失败就是nullptr
    pc = u->connected ? u->peer.connection : NULL;

    // nginx处于即将停止状态，连接是udp
    // 使用连接的log记录日志
    if( c->type == SOCK_DGRAM && ( nt_terminate || nt_exiting ) ) {

        /* socket is already closed on worker shutdown */

        handler = c->log->handler;
        c->log->handler = NULL;

        nt_log_error( NT_LOG_INFO, c->log, 0, "disconnected on shutdown" );

        c->log->handler = handler;

        nt_stream_socks5_finalize( s, NT_STREAM_OK );
        return;
    }

    // 取proxy模块的配置
    pscf = nt_stream_get_module_srv_conf( s, nt_stream_socks5_module );


    // 根据上下游状态决定来源和目标
    // 以及缓冲区、限速等
    // 注意使用的缓冲区指针

    //设定参数
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s 接收到上游", __func__ );

    // 数据下行
    //
    nt_stream_socks5_session_t *ss = s->data;
    #if 0
    //是udp , 并且处于转发payload阶段
    if( ss->type == 2 && ss->socks5_phase == SOCKS5_DATA_FORWARD ) {
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", "当前需要转发udp 数据" );

        nt_stream_session_t *udp;
        udp = ss->udp;
        u = udp->upstream;

        pc = u->connected ? u->peer.connection : NULL;

        w = dst = pc;
        pc->recv = nt_udp_recv;
        pc->send = nt_udp_send;
        //这里如果pc 是tcp 类型的话， 下面的recv 会无限死循环执行
        //
    } else
    #endif
        if( ss->type == 2 && ss->socks5_phase == SOCKS5_DATA_FORWARD ) {
            pc->recv = nt_udp_recv;
            pc->send = nt_udp_send;
        } else {
            pc->recv = nt_recv;
            pc->send = nt_send;
        }


    src = pc;
    dst = c;
    // 缓冲区是upstream_buf，即上游来的数据
    b = &u->upstream_buf;
    limit_rate = u->download_rate;
    received = &u->received;
    packets = &u->responses;
    out = &u->downstream_out;
    busy = &u->downstream_busy;
    recv_action = "socks5ing and reading from upstream";
    send_action = "socks5ing and sending to client";





    // b指向当前需要操作的缓冲区
    // 死循环操作，直到出错或者again
    for( ;; ) {

        // 如果要求写，那么把缓冲区里的数据发到dst
        if( do_write && dst ) {

            size = b->last - b->pos;

            /* nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> send size=%d", size ); */
            if( size ) {
                //            if( size && src->write->ready && !src->write->delayed
                //                && !src->write->error ) {
                c->log->action = send_action;
                //这个必须得改成原始套接字的fd，并且使用sendto
                // n = dst->send( dst, b->pos, size );
                n = nt_stream_socks5_u2d_forward_to_client( s, 1, 1 );
                nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> n=%d", n );

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

                if( n == 0 ) {
                    break;
                }

                if( n > 0 ) {
                    ( *packets )++;
                    // 缓冲区的pos指针移动，表示发送了n字节新数据
                    b->pos += n;
                    continue;
                }
            }

            if( size == 0 ) {
                b->pos = b->start;
                b->last = b->start;
            }

        }

        // size是缓冲区的剩余可用空间
        size = b->end - b->last;
        /* nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> can read size=%d", size ); */

        // 如果缓冲区有剩余，且src还可以读数据
        if( size && src->read->ready && !src->read->delayed
            && !src->read->error ) {

            c->log->action = recv_action;

            // 尽量读满缓冲区
            n = src->recv( src, b->last, size );

            /* nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> n=%d", n ); */
            // 如果不可读，或者已经读完
            // break结束for循环
            if( n == NT_AGAIN ) {
                break;
            }

            // 出错，标记为eof
            if( n == NT_ERROR ) {
                src->read->eof = 1;
                n = 0;
            }

            if( n == 0 ) {
                break;
            }


            // 读取了n字节的数据
            if( n > 0 ) {
                ( *packets )++;
                // 增加接收的数据字节数
                *received += n;
                // 缓冲区的末尾指针移动，表示收到了n字节新数据
                b->last += n;

                do_write = 1;
                // 读数据部分结束
//                nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> buf=%s", b->start );
                continue;
            }

        }

        break;
    }

    c->log->action = "socks5ing connection";
    // 这时应该是src已经读完，数据也发送完
    // 读取出错也会有eof标志
    if( nt_stream_socks5_test_finalize( s, 1 ) == NT_OK ) {
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s ", "finalize OK" );
        return;
    }

    // 如果eof就要关闭读事件
    flags = src->read->eof ? NT_CLOSE_EVENT : 0;
    #if 1
    if( nt_handle_read_event( src->read, flags ) != NT_OK ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return;
    }
    #endif




    nt_log_debug2( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> c->active=%d, c->ready=%d", c->read->active, c->read->ready );

    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s ", "forward end" );
}


