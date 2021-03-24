
#include <nt_core.h>

#include "protocol.h"
/* #include "socks5.h" */


int socks5_method_request( nt_buf_t *buf )
//int socks5_method_request( int fd )
{
    buf->last = buf->start;
    buf->pos = buf->start;

    *buf->last++ = 0x05;   //ver
    *buf->last++ = 0x02;   //methods
    *buf->last++ = 0;      //method[0]
    *buf->last++ = 2;      //method[1]

    return 0;
}


/* Build authentication packet in buf,
    * It set subnegotiation version to 0x01
    * It get uname/password from c->config.cli->username/password
    * It copy Socks5Auth struct in buf->data.
    * And set buf->a to zero and buf->b to 3 + req.ulen + req.plen
    * Return:
    *  -1, error no username or password
    *   0, success
    *
    * From RFC1929:
    * Once the SOCKS V5 server has started, and the client has selected the
    * Username/Password Authentication protocol, the Username/Password
    * subnegotiation begins.  This begins with the client producing a
    * Username/Password request:
    *
    * +----+------+----------+------+----------+
    * |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
    * +----+------+----------+------+----------+
    * | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
    * +----+------+----------+------+----------+
    *
    * The VER field contains the current version of the subnegotiation,
    * which is X'01'
    */
int socks5_auth_request( nt_buf_t *buf, char *uname, char *upswd )
{
    buf->last = buf->start;
    buf->pos = buf->start;

    size_t c = 0;
    uint32_t ulen = strlen( uname );
    uint32_t plen = strlen( upswd );

    *buf->last++ = 0x01; //ver;

    c = ulen & 0xff;
    *buf->last++ = c;    //存入ulen

    memcpy( buf->last, uname, c ); //存入uname
    buf->last += c;

    c = plen & 0xff;
    *buf->last++ = c;     //存入plen

    memcpy( buf->last, upswd, c ); //存入upswd
    buf->last += c;

    return 0;
}

/*
   *
   *
   SOCKS请求如下表所示:
    +----+-----+-------+------+----------+----------+
    | VER| CMD | RSV   | ATYP |  DST.ADDR|  DST.PORT|
    +----+-----+-------+------+----------+----------+
    | 1  | 1   | X'00' | 1    | variable |      2   |
    +----+-----+-------+------+----------+----------+
  ``
    各个字段含义如下:
    VER  版本号X'05'
    CMD：
         1. CONNECT X'01'
         2. BIND    X'02'
         3. UDP ASSOCIATE X'03'
    RSV  保留字段
    ATYP IP类型
         1.IPV4 X'01'
         2.DOMAINNAME X'03'
         3.IPV6 X'04'
    DST.ADDR 目标地址
         1.如果是IPv4地址，这里是big-endian序的4字节数据
         2.如果是FQDN，比如"www.nsfocus.net"，这里将是:
           0F 77 77 77 2E 6E 73 66 6F 63 75 73 2E 6E 65 74
           注意，没有结尾的NUL字符，非ASCIZ串，第一字节是长度域
         3.如果是IPv6地址，这里是16字节数据。
    DST.PORT 目标端口（按网络次序排列）
  **sock5响应如下:**
   OCKS Server评估来自SOCKS Client的转发请求并发送响应报文:
   +----+-----+-------+------+----------+----------+
   |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
   +----+-----+-------+------+----------+----------+
   | 1  |  1  | X'00' |  1   | Variable |    2     |
   +----+-----+-------+------+----------+----------+
   VER  版本号X'05'
   REP
        1. 0x00        成功
        2. 0x01        一般性失败
        3. 0x02        规则不允许转发
        4. 0x03        网络不可达
        5. 0x04        主机不可达
        6. 0x05        连接拒绝
        7. 0x06        TTL超时
        8. 0x07        不支持请求包中的CMD
        9. 0x08        不支持请求包中的ATYP
        10. 0x09-0xFF   unassigned
   RSV         保留字段，必须为0x00
   ATYP        用于指明BND.ADDR域的类型
   BND.ADDR    CMD相关的地址信息，不要为BND所迷惑
   BND.PORT    CMD相关的端口信息，big-endian序的2字节数据

   *
   * */
int socks5_dest_request( nt_buf_t *buf, uint32_t dest_ip, uint16_t dest_port, uint8_t type )
{
    buf->last = buf->start;
    buf->pos = buf->start;


    *buf->last++ =  0x05;  //ver

    if( type == 2 )
        *buf->last++ =  0x03;  // cmd  // 1 tcp  ,3 udp
    else
        *buf->last++ =  0x01;  // cmd  // 1 tcp  ,3 udp

    *buf->last++ =  0;     //reserved

    *buf->last++ =  1;     //ipv4

    memcpy( buf->last, &dest_ip, 4 );
    buf->last += 4;

    memcpy( buf->last, &dest_port, 2 );
    buf->last += 2;

    return 0;
}


/*释放上游连接*/
nt_int_t nt_sock5_tcp_upstream_free( nt_connection_t *c )
{
    if( c )
        nt_close_connection( c );
}


nt_int_t nt_acc_session_finalize( nt_acc_session_t *s ){
    
    nt_upstream_socks_t *ss;

    nt_sock5_tcp_upstream_free( s->ss->tcp_conn );

}

nt_int_t nt_stream_init_socks5_upstream( nt_connection_t *c )
{
    nt_int_t ret;
    nt_connection_t  *tcp_conn;
    nt_event_t  *rev, *wev;
    nt_acc_session_t *s;
    nt_upstream_socks_t *ss;

//    c = s->connection;
    s = c->data;

    ss = nt_palloc( c->pool, sizeof( nt_upstream_socks_t ) );

    s->ss = ss;
//先根据节点类型从服务器获取ip和端口。

//这里先写死。
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr( "172.16.254.157" );
    addr.sin_port =  htons( 1080 );
//生成一个新的fd;
    int fd = socket( AF_INET, SOCK_STREAM, 0 );

    debug( "init new socks5 connection fd:%d", fd );
    ret = connect( fd, ( struct sockaddr * )&addr, sizeof( struct sockaddr ) );
    if( ret == -1 ) {
        return NT_ERROR;
    }

    //生成一个新的connection
    tcp_conn = nt_get_connection( fd, c->log );
    if( tcp_conn == NULL ) {
        return NT_ERROR;
    }

    /* c->fd = fd; */
    tcp_conn->data = s;

    ss->tcp_conn = tcp_conn;

    tcp_conn->pool = c->pool;
    tcp_conn->type = SOCK_STREAM;

    tcp_conn->log = c->log;
    tcp_conn->buffer = nt_create_temp_buf( c->pool, 1500 );
    /* tcp_conn->buffer = nt_create_temp_buf( c->pool, 8192 ); */

    if( tcp_conn->buffer == NULL )
        return NT_ERROR;

//设置连接读写回调
    tcp_conn->recv = nt_recv;
    tcp_conn->send = nt_send;
//  c->send_chain = nt_send_chain;

    rev = tcp_conn->read;
    wev = tcp_conn->write;

    rev->active = 0;
    rev->ready = 0;
    wev->ready = 1;



    rev->log = c->log;
    wev->log = c->log;

    tcp_conn->read->handler = nt_tun_socks5_read_handler;
    tcp_conn->write->handler = nt_tun_socks5_read_handler;

    nt_add_timer( tcp_conn->write, 60 );

    nt_add_event( tcp_conn->read, NT_READ_EVENT,  0 );

    #if 1
    nt_skb_tcp_t *tcp;
    tcp = s->skb->data;

    #ifdef SEND_USE_TUN_FD
        tcp->fd = c->fd;
    #else
    /* struct sockaddr_in *a; */
    /* a = ( struct sockaddr_in * )tcp->src ; */

    int sock = socket( AF_INET, SOCK_RAW, IPPROTO_TCP );
    int on = 1;
    if( setsockopt( sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof( on ) ) == -1 ) {
        debug( "setsockopt() failed" );
        return -1;
    } else {
        /*printf( "setsockopt() ok\n"  );*/
    }
    debug( "new tcp raw fd=%d", sock );
    //size_t size ;
    /* struct sockaddr_in to;
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = addr->sin_addr.s_addr;
    to.sin_port = addr->sin_port; */

        tcp->fd = sock;
    #endif
    #endif


    return NT_OK;
}


nt_int_t nt_tun_socks5_handle_phase( nt_acc_session_t *s )
{
    nt_int_t ret;
    nt_connection_t *c;

    c = s->connection ;

    nt_buf_t *bb;
    bb = c->buffer ;
    debug( "s =%d", ( bb->last - bb->start ) );

    #if 1
    nt_upstream_socks_t *ss;
    nt_buf_t *b;
    ss = s->ss;

    b = ss->tcp_conn->buffer ;
    nt_skb_tcp_t *tcp;
    tcp = s->skb->data ;

    /* restart: */

    debug( "ss->phase=%d", ss->phase );
    debug( "tcp->data_len=%d", tcp->data_len );
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "ss->phase=%d", ss->phase );

    int exit = 1;
    while( exit ) {

        switch( ss->phase ) {
        case SOCKS5_VERSION_REQ: //ver send size 4
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", "[socks5 auth]  [1] request" );
            b->last = b->start;
            b->pos = b->start;

            socks5_method_request( b ) ;
            ret = nt_upstream_socks_send( s->ss->tcp_conn, b );
            if( ret == NT_OK ) {
                ss->phase = SOCKS5_VERSION_RESP ;
            } else {
                ss->phase = SOCKS5_ERROR;
            }
            exit = 0;
            break;
        case SOCKS5_USER_REQ: //user/passwd send
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", "[socks5 auth]  [2] request" );
            if( socks5_auth_request( b, "test", "test" ) < 0 ) {
                nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", "[socks5 auth]  [2] request fail" );
            }

            ret = nt_upstream_socks_send( s->ss->tcp_conn, b );
            if( ret == NT_OK ) {
                ss->phase = SOCKS5_USER_RESP ;
            } else {
                ss->phase = SOCKS5_ERROR;
            }
            exit = 0;
            break;
        case SOCKS5_SERVER_REQ: //dip/dport send
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", "[socks5 auth]  [3] request " );

            /* tcp = s->skb->data ; */

            /* socks5_dest_request( b, inet_addr( "14.215.177.39" ), htons( 80 ) , 1) ; */
            socks5_dest_request( b,  tcp->dip, tcp->dport, 1 ) ;
            /* if( ss->type == 2 )
                socks5_dest_request( b, INADDR_ANY, 0,  ss->type ) ;
            else {
                nt_tcp_raw_socket_t *raw;
                raw = ss->raw;
                socks5_dest_request( b, raw->dip, raw->dport, ss->type ) ;
            } */
            ret = nt_upstream_socks_send( s->ss->tcp_conn, b );
            if( ret == NT_OK ) {
                ss->phase = SOCKS5_SERVER_RESP ;
            } else {
                ss->phase = SOCKS5_ERROR;
            }

            exit = 0;
            break;

        case SOCKS5_VERSION_RESP: //ver send ack size 2
            if( ( b->last - b->pos ) == 0 ) {
                return NT_ERROR;

            }

            nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", "[socks5 auth]  [1] response" );
            if( *b->start == 0x05 && *( b->start + 1 ) == 0x02 ) {
                ss->phase = SOCKS5_USER_REQ;
            } else {
                ss->phase = SOCKS5_ERROR;
                exit = 0;
            }
            break;

        case SOCKS5_USER_RESP: //user/passwd send ack size 2
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> [socks5 auth]  [2] response b.size=%d", b->last - b->pos );
            if( ( b->last - b->pos ) == 0 ) {
                return NT_ERROR;

            }

            if( *b->start == 0x01 && *( b->start + 1 ) == 0x00 ) {
                ss->phase = SOCKS5_SERVER_REQ;
            } else {
                nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> [socks5 auth]  [2] response , b[1]=%d, b[2]=%d", *b->start, *  ( b->start + 1 ) );
                ss->phase = SOCKS5_ERROR;
                exit = 0;
            }

            break;

        case SOCKS5_SERVER_RESP: //dip/dport send ack size 2
            nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", "[socks5 auth]  [3] response" );
            if( ( b->last - b->pos ) == 0 ) {
                return NT_ERROR;
            }

            if( *b->start == 0x05 && *( b->start + 1 ) == 0x00 ) {
                ss->phase = SOCKS5_DATA_FORWARD;

                b->last = b->start;
                b->pos = b->start;

            } else {
                ss->phase = SOCKS5_ERROR;
                exit = 0;
            }

            break;
        case SOCKS5_DATA_FORWARD:

            nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", "[socks5 auth]  [4] data forward" );

            ss->tcp_conn->read->handler = nt_tun_tcp_download_handler;
            /* nt_tun_tcp_data_upload_handler( ss->tcp_conn ); */

            debug( "tcp %p", tcp );
            debug( "tcp->data_len=%d", tcp->data_len );
            nt_tun_tcp_data_upload_handler( c );

            exit = 0;
            break;
        }

    }
    #endif


    if( ss->phase == SOCKS5_ERROR )
        return NT_ERROR;
}
