#include "protocol.h"
#include "rbtree.h"

unsigned short tcp_chk( const unsigned short *data, int size )
{
    unsigned long sum;
    pshead ph;
    struct iphdr *ih = ( struct iphdr * )data;
    ph.saddr = ih->saddr;
    ph.daddr = ih->daddr;
    ph.mbz = 0;
    ph.ptcl = 6;
    ph.tcpl = htons( size - ( ih->ihl << 2 ) );
    struct tcphdr * tcp = ( struct tcphdr * )( ih + 1 );
    sum = chk( ( unsigned short * )&ph, sizeof( pshead ) );
    sum += chk( ( unsigned short * )tcp, ntohs( ph.tcpl ) );
    sum -= tcp->check;
    sum = ( sum >> 16 ) + ( sum & 0xffff );
    sum += ( sum >> 16 );
    return ( unsigned short )( ~sum );

}

u_int16_t tcp_get_port( char *pkg, u_int8_t direction )
{
    u_int16_t port ;

    struct iphdr *ih;
    struct tcphdr *th;
    ih = ( struct iphdr * )pkg;
    th = ( struct tcphdr * )( ih + 1 );

    if( direction == TCP_SRC )
        port = th->source;
    else
        port = th->dest;

    return  port  ;
    /* return ntohs( port ) ; */
}



/*  解析tcp 头中的option部分，主用来提取mss
 *   Kind | Length | Name           | RFC     | 描述
 *   0    | 1      | EOL            | RFC0793 | 结束TCP选项列表
 *   1    | 1      | NOP            | RFC0793 | 空操作填充TCP选项
 *   2    | 4      | MSS            | RFC0793 | 最大Segment长度
 *   3    | 3      | WSOPT          | RFC1323 | 窗口扩大系数
 *   4    | 2      | SACK-Premitted | RFC2018 | 表明支持SACK
 *   5    | 可变   | SACK           | RFC2018 | SACK Block（收到乱序数据
 *   8    | 10     | TSOPT          | RFC1323 | Timestamps
 *   19   | 18     | TCP-MD5        | RFC2385 | MD5认证
 *   28   | 4      | UTO            | RFC5482 | User Timeout（超过一定闲置时间后拆除连接）
 *   29   | 可变   | TCP-AO         | RFC5925 | 认证
 * */
void tcp_parse_option( char *data )
{
    const unsigned char *ptr;
    struct iphdr *ih = ( struct iphdr * )data;
    const struct tcphdr *th = ( struct tcphdr * )( ih + 1 );    //取得tcp的头部
    int length = ( th->doff * 4 ) - sizeof( struct tcphdr ); //取得除tcp头部的的长度

    ptr = ( const unsigned char * )( th + 1 ); //跳过tcp的头部

    while( length > 0 ) {
        int opcode = *ptr++;  //选项kind
        int opsize;  //选项长度，包括kind和length

        switch( opcode ) {
        case TCPOPT_EOL: //End of options ,options的结束标志
            return;
        case TCPOPT_NOP:    /* Ref: RFC 793 section 3.1 */ //填充标志，为了字节对齐
            length--;
            continue;
        default:
            opsize = *ptr++;
            if( opsize < 2 ) /* "silly options" */ //选项大小小于2个字节，直接退出
                return;
            if( opsize > length )
                return;    /* don't parse partial options */
            switch( opcode ) {
            case TCPOPT_MSS: { //对端发送的长度的限制，整个tcp传输segment的过程中不能大于这个值
                if( opsize == TCPOLEN_MSS && th->syn ) {
                    uint16_t maxseg = ntohs( *( uint16_t * )ptr );
                    debug( "maxseg=%u ", maxseg );
                }
                break;
            }
            case TCPOPT_WINDOW:  //窗口扩大选项
                if( opsize == TCPOLEN_WINDOW && th->syn ) {
                    debug( "wscale=%u ", *( uint8_t * )ptr );
                }
                break;
            case TCPOPT_SACK_PERM:
                if( opsize == TCPOLEN_SACK_PERM && th->syn ) {
                    debug( "TCPOPT_SACK_PERM" );
                }
                break;
            case TCPOPT_SACK:
                debug( "TCPOPT_SACK opsize=%d", opsize );
                break;
            case TCPOPT_TIMESTAMP:
                debug( "TCPOPT_TIMESTAMP" );
                break;
            case TCPOPT_MD5SIG:
                debug( "TCPOPT_MD5SIG" );
                break;
            default:
                break;
            }
        }

        ptr += opsize - 2;
        length -= opsize;   //减去一个option的大小，接着再进入while循环，进行下一个选项的解析。
    }

}

void tcp_free( nt_connection_t *c )
{

    nt_acc_session_t *s;

    s = c->data;

    close( s->fd );

    rcv_conn_del( &acc_tcp_tree, c );

    debug( "<<<<<<<<<<<<<<conn free<<<<<<<<<<<<<<<<<<<<<<" );

}

#if 0
void tcp_init( nt_connection_t *c )
{
    u_int16_t sport ;
    nt_rbtree_node_t *node;
    nt_acc_session_t *s;
    nt_skb_t *skb;
    nt_buf_t *b;
    struct iphdr *ih;

    b = c->buffer;  //存放read进来的数据包
    ih = ( struct iphdr * )b->start;


    sport = tcp_get_port( b->start, TCP_SRC );

    //查询该连接是否已经在红黑树中，如果没有就添加一个新的 nt_acc_session_t 对象
    //并把源IP，目的IP存入其中
    //把这个新的 nt_acc_session_t 添加到红黑树
    node = rcv_conn_search( &acc_tcp_tree, sport );
    if( node != NULL ) {
//        debug( "node in tree" );
        s = ( nt_acc_session_t * )node->key;
        skb = s->skb;
        skb->protocol = ih->protocol ;

        skb->skb_len = b->last - b->start;
        skb->iphdr_len = ih->ihl << 2;


        s->conn = c;
        c->data = s ;

        nt_skb_tcp_t *skb_tcp = skb->data;

        struct sockaddr_in *addr2;
        addr2 = ( struct sockaddr_in * )skb_tcp->src;
        debug( "src = %d.%d.%d.%d:%d", IP4_STR( addr2->sin_addr.s_addr ), ntohs( addr2->sin_port ) );

        addr2 = ( struct sockaddr_in * )skb_tcp->dst;
        debug( "dst = %d.%d.%d.%d:%d", IP4_STR( addr2->sin_addr.s_addr ), ntohs( addr2->sin_port ) );

    } else {
        debug( "该连接是第一个数据包" );
        s = nt_pcalloc( c->pool, sizeof( nt_acc_session_t ) );

        //生成一个新的skb
        skb = nt_pcalloc( c->pool, sizeof( nt_skb_t ) );

        skb->protocol = ih->protocol ;
        skb->skb_len = b->last - b->start;
        skb->iphdr_len = ih->ihl << 2;

        //申请一个回复用的buf
        skb->buffer = nt_create_temp_buf( c->pool, 1500 );


        if( ih->protocol == IPPROTO_TCP ) {
            nt_skb_tcp_t *skb_tcp = nt_pcalloc( c->pool, sizeof( nt_skb_tcp_t ) );


            struct sockaddr *src = ( struct sockaddr * ) nt_pcalloc( c->pool, sizeof( struct sockaddr ) );
            struct sockaddr *dst = ( struct sockaddr * ) nt_pcalloc( c->pool, sizeof( struct sockaddr ) );



            skb_tcp->data = NULL;
            skb_tcp->data_len = 0;

            skb_tcp->payload_len = 0;
            skb_tcp->seq = ntohl( 1 );
            skb_tcp->ack_seq = ntohl( 1 );


            skb->data = skb_tcp;

            /* debug( "src = %d.%d.%d.%d, dst = %d.%d.%d.%d",
                   IP4_STR( ih->saddr ),
                   IP4_STR( ih->daddr ) );
            */
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = ih->saddr;
            addr.sin_port = htons( sport );
            nt_memcpy( src, ( struct sockaddr * )&addr, sizeof( struct sockaddr ) );
            skb_tcp->src = src;


            addr.sin_addr.s_addr = ih->daddr;
            addr.sin_port = htons( tcp_get_port( b->start, TCP_DST ) );
            nt_memcpy( dst, ( struct sockaddr * )&addr, sizeof( struct sockaddr ) );
            skb_tcp->dst = dst;

            struct sockaddr_in *addr2;
            addr2 = ( struct sockaddr_in * )skb_tcp->src;
            debug( "src = %d.%d.%d.%d:%d", IP4_STR( addr2->sin_addr.s_addr ), ntohs( addr2->sin_port ) );


            addr2 = ( struct sockaddr_in * )skb_tcp->dst;
            debug( "dst = %d.%d.%d.%d:%d", IP4_STR( addr2->sin_addr.s_addr ), ntohs( addr2->sin_port ) );



        }

        s->port = sport;
        s->conn = c;
        s->skb = skb;
        s->fd = 0;

        c->data = s ;
        //把新的s添加到红黑树;
        nt_rbtree_node_t *node = ( nt_rbtree_node_t * ) nt_palloc( c->pool, sizeof( nt_rbtree_node_t ) );
        rcv_conn_add( &acc_tcp_tree, s, node );
    }

}


//判断该tcp包处于哪个阶段
uint8_t tcp_phase( nt_skb_tcp_t *tcp, struct tcphdr *th )
{
    uint8_t phase ;
    /* debug( "tcp phase len=%d", tcp->data_len ); */

    if( tcp->data_len == 0 ) {

        if( th->syn == 1 && th->ack == 0 )
            tcp->phase = TCP_PHASE_SYN;

        if( th->ack == 1 && th->syn == 1 ) {
            tcp->phase = TCP_PHASE_SYN_ACK;
        }

        //ack 第二包的ack或 FIN的ack
        if( th->ack == 1 && th->syn == 0 && th->fin == 0 ) {
            /* debug( "tcp->phase ack %d", tcp->phase ); */
            if( tcp->phase == TCP_PHASE_CONN_FREE ) {
                return NT_DONE;
            }

            tcp->phase = TCP_PHASE_ACK;
            return NT_ERROR;
        }

        if( th->fin == 1 )
            tcp->phase = TCP_PHASE_FIN ;

    } else {
        debug( "th->psh=%d", th->psh );
        if( th->psh == 1 ) {
            tcp->phase = TCP_PHASE_PSH_END ;
        } else {
            tcp->phase = TCP_PHASE_PSH ;
        }
    }

    return NT_OK;
}
#endif

#if 0 
//对进入的tcp数据做处理。
//需进行三次握手的欺骗
int tcp_input( nt_connection_t *c )
{

    nt_acc_session_t *s ;
    nt_skb_t *skb;
    nt_skb_tcp_t *tcp;
    nt_buf_t *b;
    struct iphdr *ih;
    struct tcphdr *th;
    int8_t ret;

    tcp_init( c );

    b = c->buffer;
    s = c->data;
    skb = s->skb;
    tcp = skb->data;

    ih = ( struct iphdr * )b->start;
    th = ( struct tcphdr * )( ih + 1 );


//    uint16_t iphdr_len = ih->ihl << 2;
//    uint16_t tcphdr_len = th->doff << 2;
//    u_int16_t payload_len = tcp_len -  tcphdr_len;
    u_int16_t tcp_len = skb->skb_len - skb->iphdr_len;

    tcp->hdr_len = th->doff << 2;
    tcp->data_len = tcp_len - tcp->hdr_len;

    /*
        debug( "skb->skb_len=%d", skb->skb_len );
        debug( "skb->iphdr_len=%d", skb->iphdr_len );
        debug( "tcp->hdr_len=%d", tcp->hdr_len );
        debug( "packet len=%d, playload_len=%d", tcp_len, tcp->data_len );
    */

    ret = tcp_phase( tcp, th );
    if( ret == NT_ERROR )
        return NT_ERROR;

    debug( "tcp phase ret =%d", ret );
    if( ret == NT_DONE ) {
        tcp_free( c );
        return NT_DONE;
    }

    debug( "tcp phase=%d", tcp->phase );


    //执行回复动作
    tcp_phase_handle( c, tcp );
}
#endif

int tcp_direct_server( nt_connection_t *c )
{

    nt_acc_session_t *rc ;
    nt_skb_t *skb;
    nt_skb_tcp_t *tcp;
    int ret;


    rc = c->data;
    skb = rc->skb;
    tcp = skb->data;

    nt_buf_t *b;
    //ssize_t size = b->last - b->start;
    b = c->buffer;

    struct iphdr *ih;
    struct tcphdr *th;
    ih = ( struct iphdr * )b->start;
    th = ( struct tcphdr * )( ih + 1 );

    /* struct sockaddr_in *daddr; */
    /* daddr = (struct sockaddr_in *)tcp->dst; */

    //连接到目的server
    int rfd = socket( AF_INET, SOCK_STREAM, 0 );
//    struct sockaddr_in remote ;
//    remote.sin_family = AF_INET;

//    remote.sin_addr.s_addr =  daddr->sin_addr.s_addr;

    debug( "tcp direct connect \n" );
    ret = connect( rfd, tcp->dst, sizeof( struct sockaddr ) );

    if( ret < 0 ) {
        debug( "connect error\n" );
        return 0;
    }

    debug( "tcp direct server=%d", ret );
    char *payload = c->buffer->start + skb->iphdr_len + tcp->hdr_len ;
    int payload_len = tcp->data_len;

    ret = send( rfd, payload, payload_len, 0 );
    if( ret < 0 ) {
        debug( "ret=%d", ret );
        return -1;
    }


    char buf[1452] = {0};
//    char buf[568] = {0};
    int len = 0;

    int have_send_ack = 0;
//    in.sin_family = AF_INET;

    tcp->last_len = 0;

/* #define SEND_USE_TUN_FD 1 */

#ifndef SEND_USE_TUN_FD
    int sock = socket( AF_INET, SOCK_RAW, IPPROTO_TCP );
    int on = 1;
    if( setsockopt( sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof( on ) ) == -1 ) {
        perror( "setsockopt() failed" );
        return -1;
    } else {
        /*printf( "setsockopt() ok\n"  );*/
    }
    size_t size ;
    struct sockaddr_in to;
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = ih->saddr;
    to.sin_port = th->source;
#endif

    do {
        ret = recv( rfd, buf, sizeof( buf ), 0 );
      //  debug( "ret=%d, buf=%s", ret, buf );

        //send playload
        if( 0 >= ret ) {
            if( ( 0 > ret ) && ( EAGAIN == errno ) )
                return 0;
            else
                return -1;
        } else {
            tcp->data = buf ;
            tcp->data_len = ret;

            debug( "ret= %d, buf size=%d", ret, sizeof( buf ) );
            if( ret != sizeof( buf ) ) {
                tcp->phase = TCP_PHASE_SEND_PSH_END;
            } else
                tcp->phase = TCP_PHASE_SEND_PSH;

            tcp_create( skb, ih, th );
            ssize_t size = skb->buffer->last - skb->buffer->start;

            debug( "size=%d",  size );
            #ifdef SEND_USE_TUN_FD
            //tun直接转回去
            write( c->fd, skb->buffer->start, size );
            #else
            //使用原始套接字
            sendto( sock, skb->buffer->start,  size, 0, ( struct sockaddr* ) &to, sizeof( to ) );
            #endif

            if( ret != sizeof( buf ) )
                break ;
        }


    } while( ret > 0 );

    close( rfd );
    debug( "+++++++++++++++end+++++++++++++" );


}



// socks connect
int tcp_phase_socks_connect(  nt_connection_t *c ){

    //先进行 上游 socks5的认证连接
    nt_stream_init_socks5_upstream( c );
}

int tcp_phase_proxy_socks( nt_connection_t *c )
{
    debug( "++++++++++++++tcp_phase_proxy_socks+++++++++++++" );
    nt_acc_session_t *s ;
    nt_skb_t *skb;
    nt_skb_tcp_t *tcp;
    int ret;
    int fd;

    s = c->data;
    if( s->ss == NULL ){
        tcp_phase_socks_connect(c);
        s->ss->phase = SOCKS5_VERSION_REQ ; 
    }



        
    nt_tun_socks5_handle_phase( s );



    return NT_OK;

#if 0
    const char path[] = "/tmp/nt.socks5.sock";
    struct sockaddr_un server_addr;
#else
    struct sockaddr_in server_addr;

    server_addr.sin_family =AF_INET;
    server_addr.sin_addr.s_addr = inet_addr( "172.16.254.157"  );
    server_addr.sin_port =  htons( 1080  );
#endif 
    ssize_t size ;

    s = c->data;
    skb = s->skb;
    tcp = skb->data;

    nt_buf_t *b;
    //ssize_t size = b->last - b->start;
    b = c->buffer;

    struct iphdr *ih;
    struct tcphdr *th;
    ih = ( struct iphdr * )b->start;
    th = ( struct tcphdr * )( ih + 1 );

    int fist_connect = 0;
    //未发起过连接
    if( s->fd == 0 ) {

        fd = socket( AF_INET, SOCK_STREAM, 0 );

        debug( "new fd = %d", fd );


        if( connect( fd, ( struct sockaddr * )&server_addr, sizeof( server_addr ) ) == -1 ) {
            debug( " connect to server fail. errno=%d", errno );
            return TCP_PHASE_NULL;
        }
        s->fd = fd;
        fist_connect = 1;
    } else {
        fd = s->fd;
    }

    //先发送连接信息，再发送data内容
    //现在tcp存在一个问题，就是发送到nginx后，有可能两次发送会被合并成一次。这是不对的。

    struct iovec iov[2];
    nt_acc_socks5_auth_t asa;
    if( fist_connect ) {
        //填充连接信息
        asa.type = 1;     //发起连接
        asa.domain = 4;   //ipv4
        asa.protocol = 1; // 1 tcp

#if 1
        asa.server_ip = inet_addr( "172.16.254.157" );
        asa.server_port = htons( 1080 );
        strcpy( asa.user, "test" );
        strcpy( asa.password, "test" );

#else
        asa.server_ip = inet_addr( "162.14.16.22" );
        asa.server_port = htons( 18000 );
        strcpy( asa.user, "18206086619" );
        strcpy( asa.password, "ICtOe0rhCVt5i6pF" );
#endif


        struct sockaddr_in *addr;
        addr = ( struct sockaddr_in * )tcp->src;
        asa.sip = addr->sin_addr.s_addr;
        asa.sport =  addr->sin_port;

        addr = ( struct sockaddr_in * )tcp->dst;
        asa.dip = addr->sin_addr.s_addr;
        asa.dport = addr->sin_port;

        iov[0].iov_base = &asa;
        iov[0].iov_len = sizeof( nt_acc_socks5_auth_t );

    }

    /* ret = send( fd, ( void * )&asa,  sizeof( nt_acc_socks5_auth_t ) , 0 );
    debug( " ret  = %d", ret );
    if( ret < 0 )
        return TCP_PHASE_NULL; */

    /* sleep(1); */
    //构造tcp 的seq ack 和载荷， 并进行发送
    nt_acc_tcp_t at;
    at.seq = tcp->last_seq;
    at.ack = tcp->last_ack;
    debug( "seq=%u, ack=%u", ntohl( at.seq ), ntohl( at.ack ) );

    at.data_len =  tcp->data_len ;
    debug( " size  = %d",  at.data_len );
    tcp->data = c->buffer->start + skb->iphdr_len + tcp->hdr_len ;
    memcpy( at.data, tcp->data,  at.data_len );
    //  tcp_socks.data = tcp->data ;
    /* debug( " buf  = %s",  tcp_socks.data ); */
    debug( " 本次载荷大小为：%d",   at.data_len );

    size = sizeof( nt_acc_tcp_t ) - 1500  + at.data_len;
    debug( " size  = %d", size );

    //发送
    if( fist_connect ) {
        debug( " fist_connect " );
        iov[1].iov_base = &at;
        iov[1].iov_len = size;
        //首次认证和data一起发送
        //不然nginx处理可能有问题
        ret =  writev( fd, iov, 2 );
    } else {
        debug( " no fist_connect " );
        ret = send( fd, ( void * )&at, size, 0 );
    }

    debug( " ret  = %d", ret );
    if( ret < 0 )
        return TCP_PHASE_NULL;

}

int tcp_phase_send_response( nt_connection_t *c )
{
    debug( "++++++++++++++tcp_phase_send_response+++++++++++++" );
    nt_acc_session_t *s ;
    nt_skb_t *skb;
    nt_skb_tcp_t *tcp;
    nt_buf_t *b;
    struct iphdr *ih;
    struct tcphdr *th;
    u_int8_t ret;

    b = c->buffer;
    s = c->data;
    skb = s->skb;
    tcp = skb->data;
    ih = ( struct iphdr * )b->start;
    th = ( struct tcphdr * )( ih + 1 );

    struct sockaddr_in *addr;
    addr = ( struct sockaddr_in * )tcp->src;


    //创建回复数据包, 回复SYN ACK
    tcp_create( skb, ih, th );

    addr = ( struct sockaddr_in * )tcp->src;
    debug( "sip=%u.%u.%u.%u:%d", IP4_STR( addr->sin_addr.s_addr ), ntohs( addr->sin_port ) );


    ret = tcp_output( s );

    if( ret < 0 ) {
        debug( "tcp_phase_send_response ret =%d \n", ret );
        ret = tcp_output( s );
    }

    if( ret < 0 )
        return TCP_PHASE_NULL;


    debug( "tcp->close=%d", tcp->close );
    if( (tcp->close == 1) && (tcp->phase == TCP_PHASE_SEND_FIN )){
        return TCP_PHASE_NULL ;
    }

    if( (tcp->close == 1) && (tcp->phase == TCP_PHASE_SEND_FIN_ACK )){
        return TCP_PHASE_CONN_FREE ;
    }



    if( tcp->phase == TCP_PHASE_SEND_PSH_ACK )
        /* return TCP_PHASE_PROXY_DIRECT; */
    return TCP_PHASE_PROXY_SOCKS;

    else if( tcp->phase == TCP_PHASE_SEND_FIN_ACK )
        return TCP_PHASE_SEND_FIN;
    else if( tcp->phase == TCP_PHASE_SEND_FIN ) {
        debug( "return TCP_PHASE_CONN_FREE" );
        return TCP_PHASE_CONN_FREE;
    } else
        return TCP_PHASE_NULL;

}


int tcp_close_client( nt_acc_session_t *s ){

    nt_connection_t *c;
    nt_skb_tcp_t *tcp;

    c = s->connection ;
    tcp = s->skb->data;

    //主动关闭， 所以需要设置为1
    tcp->close = 1;
    tcp->phase = TCP_PHASE_SEND_FIN ;
    
    tcp_phase_handle( c, tcp );
} 

int tcp_phase_handle( nt_connection_t *c , nt_skb_tcp_t *tcp)
{
    u_int8_t ret;
    nt_acc_session_t *s;

    while( tcp->phase ) {

        /* debug( "tcp->phase=%d", tcp->phase ); */

        switch( tcp->phase ) {
        case TCP_PHASE_SYN:
            //   tcp_parse_option( b->start );
            debug( "TCP_PHASE_SYN" );
            //接收到SYN包，把阶段改为发送SYN ACK
            tcp->phase = TCP_PHASE_SEND_SYN_ACK;
            break;
        case TCP_PHASE_SEND_SYN_ACK:
            debug( "TCP_PHASE_SEND_SYN_ACK" );
            tcp->phase = tcp_phase_send_response( c );

            break;
        case TCP_PHASE_PSH:
            debug( "TCP_PHASE_PSH" );
            //不用回ACK， 直接中转给socks
            tcp->phase = TCP_PHASE_PROXY_SOCKS;
            break;
        case TCP_PHASE_PSH_END:
            debug( "TCP_PHASE_PSH_END" );
            //接收到PSH 结束，回复 PSH ACK;
            tcp->phase = TCP_PHASE_SEND_PSH_ACK;
            break;
        case TCP_PHASE_SEND_PSH_ACK:
            debug( "TCP_PHASE_SEND_PSH_ACK" );
            //回复PSH ACK
            tcp->phase = tcp_phase_send_response( c );

            break;
        case TCP_PHASE_PROXY_DIRECT:
            debug( "TCP_PHASE_PROXY_DIRECT" );
            //发送payload 到 server
            tcp->phase = tcp_direct_server( c );
            break;
        case TCP_PHASE_PROXY_SOCKS:
            debug( "TCP_PHASE_PROXY_SOCKS" );
            //发送payload 到 server
            tcp->phase = tcp_phase_proxy_socks( c );
            break;
        case TCP_PHASE_FIN:
            debug( "TCP_PHASE_FIN" );
            //收到FIN 回应FIN ACK
            tcp->phase = TCP_PHASE_SEND_FIN_ACK ;
            break;
        case TCP_PHASE_SEND_FIN_ACK:
            debug( "TCP_PHASE_SEND_FIN_ACK" );
            tcp->phase = tcp_phase_send_response( c );
            break;
        case TCP_PHASE_SEND_FIN:
            debug( "TCP_PHASE_SEND_FIN" );
            tcp->phase = tcp_phase_send_response( c );
            //发送完后需要进入free阶段
            // tcp->phase = TCP_PHASE_CONN_FREE;
            break;
        case TCP_PHASE_CONN_FREE:
            //
            debug( "TCP_PHASE_CONN_FREE" );
            //tcp_free( c );
            //tcp->phase = TCP_PHASE_NULL;
            
            nt_tun_close_session( c ); 
            return tcp->phase;
            break;
        case TCP_PHASE_RST:

            /* s =c->data; */
            /* nt_acc_session_finalize( s ); */
            nt_tun_close_session( c ); 

            return TCP_PHASE_CONN_FREE;
        default:
            /* debug( "TCP_PHASE_NULL" ); */
            tcp->phase = TCP_PHASE_NULL;
            break;
        }
    }

    return tcp->phase;
}





int  tcp_output( nt_acc_session_t *s )
{
    /* nt_acc_session_t *s ; */
    /* nt_skb_t *skb; */
    /* nt_skb_tcp_t *tcp; */
    int ret;

    /* s = c->data; */
    /* skb = s->skb; */
    /* tcp = skb->data; */

    nt_buf_t *b;
    b = s->skb->buffer;
    ssize_t size = b->last - b->start;
    debug( "send size=%d", size );
//    debug( "phase=%d", tcp->phase );

    /* print_pkg( b->start ); */
    //https://blog.csdn.net/zhang2010kang/article/details/47165801

    /* debug( "c->fd=%d",  c->fd ); */
    ret = write( s->fd, b->start, size );

/* #if SEND_USE_TUN_FD 
    ret = write( s->fd, b->start, size );
#else
    ret = sendto( tcp->fd, b->start,  size, 0, tcp->src, sizeof( struct sockaddr ) );
#endif */


    debug( "ret=%d", ret );
}

/*
 * 构造回应数据包
 * */
void tcp_create( nt_skb_t *skb, struct iphdr *ih, struct tcphdr *th )
{
    nt_skb_tcp_t *tcp;
    nt_buf_t *b;

    //初始化
    b = skb->buffer;  //
    b->last = b->start;
    b->pos = b->start;


    /* memset( b->start , 0 , 1500 ); */
    //不知道为什么，不清空的话，会出现请求curl baidu.com 数据只有一半
    /* memset( b->start + 1329, 0, 1500 - 1329 ); */
    tcp = skb->data;

    ip_create( skb, ih );

//    struct iphdr * pkg_ih  = ( struct iphdr * ) b->start ;
//    b->last += sizeof( struct iphdr );
    struct tcphdr * pkg_th = ( struct tcphdr * ) b->last;

    memset( pkg_th, 0 ,  th->doff << 2 );
    b->last += sizeof( struct tcphdr );
    pkg_th->source  = th->dest;                            // tcp source port
    pkg_th->dest = th->source;                          // tcp dest port

    //标志位先置零
    pkg_th->urg = 0;
    pkg_th->ack = 0;
    pkg_th->psh = 0;
    pkg_th->rst = 0;
    pkg_th->syn = 0;
    pkg_th->fin = 0;

    uint8_t iphdr_len = ih->ihl << 2;
    uint8_t tcphdr_len = th->doff << 2;

    uint16_t payload_len = skb->skb_len - iphdr_len - tcphdr_len ;

    switch( tcp->phase ) {
    case TCP_PHASE_SYN:
    case TCP_PHASE_SEND_SYN_ACK:
        debug( "--ph--TCP_PHASE_SEND_SYN_ACK" );
        //SYN 回复 SYN ACK
        pkg_th->doff = ( skb->skb_len - iphdr_len ) / 4;
        pkg_th->syn  = 1;
        pkg_th->ack  = 1;
        pkg_th->seq = th->ack_seq;
        pkg_th->ack_seq = htonl( ntohl( th->seq ) + 1 );
        //memcpy( pkg_th + 1, th + 1, tcp->hdr_len - 20   );  //syn 回应需要带上option
        memcpy( pkg_th + 1, th + 1, tcphdr_len  - 20 );   //syn 回应需要带上option
        b->last += ( tcphdr_len  - 20 );
        break;
    case TCP_PHASE_PSH:
    //接收payload阶段，未结束
    case TCP_PHASE_PSH_END:
    case TCP_PHASE_SEND_PSH_ACK:
        debug( "--ph--TCP_PHASE_SEND_PSH_ACK" );
        //接收payload阶段，结束
        pkg_th->doff = 0x5;
        pkg_th->ack  = 1;
        pkg_th->seq = th->ack_seq;
        pkg_th->ack_seq = htonl( ntohl( th->seq ) + payload_len );
        break;

    case TCP_PHASE_SEND_PSH:
        debug( "--ph--TCP_PHASE_SEND_PSH" );
        //发送payload阶段，未结束
        pkg_th->doff = 0x5;
        pkg_th->ack  = 1;
        /* debug( "TCP_SEND_PAYLOAD seq=%u", ntohl( tcp->seq ) ); */
        pkg_th->seq = htonl( ntohl( tcp->last_seq ) + tcp->last_len )  ;                           // tcp seq   number
        /* debug( "TCP_SEND_PAYLOAD seq=%u", ntohl( tcp->seq ) ); */
        /* debug( "TCP_SEND_PAYLOAD conn->odt->payload_len=%d", tcp->payload_len ); */
        pkg_th->ack_seq = htonl( ntohl( th->seq ) + payload_len );         // tcp ack number
        //memcpy( pkg_th + 1, tcp->data, tcp->data_len   ); //发送payload
        b->last = nt_cpymem( b->last, tcp->data, tcp->data_len );
        //        b->last += tcp->data_len;
        break;
    case TCP_PHASE_SEND_PSH_END:
        debug( "--ph--TCP_PHASE_SEND_PSH_END" );
        //发送payload阶段，结束
        pkg_th->doff = 0x5;
        pkg_th->ack  = 1;
        pkg_th->psh  = 1;
        pkg_th->seq = htonl( ntohl( tcp->last_seq ) + tcp->last_len )  ;                          // tcp seq number
        /* debug( "TCP_SEND_PAYLOAD_END seq=%u", ntohl( tcp->seq ) ); */
        /* debug( "TCP_SEND_PAYLOAD_END conn->odt->payload_len=%d", tcp->payload_len ); */
        pkg_th->ack_seq = htonl( ntohl( th->seq ) + payload_len );       // tcp ack number
        // memcpy( pkg_th + 1, tcp->data, tcp->data_len  ); //发送payload
        //  b->last += tcp->data_len;
        b->last = nt_cpymem( b->last, tcp->data, tcp->data_len );
        memset( b->last, 0, b->end - b->last ); 
        break;
    case TCP_PHASE_SEND_FIN: //主动回 FIN
    case TCP_PHASE_SEND_FIN_ACK:  //收到FIN ACK, 回应 ACK
        debug( "--ph--TCP_PHASE_SEND_FIN_ACK" );
        pkg_th->doff = 0x5;
        pkg_th->ack  = 1;                                   // ack标识位
        if( tcp->phase == TCP_PHASE_SEND_FIN )
            pkg_th->fin  = 1;
        pkg_th->seq = th->ack_seq;                             // tcp seq number

        debug( "before-->pkg_th->seq=%u", ntohl( pkg_th->seq ) );
        debug( "before-->th->seq=%u", ntohl( th->seq ) );
        pkg_th->ack_seq = htonl( ntohl( th->seq ) + 1 );       // tcp ack number
        break;

    }

    pkg_th->window    = htons( -1 );
    pkg_th->check     = 0;
    pkg_th->urg_ptr   = 0;

    //保存当前 seq , ack 和载荷长度，给下一次使用
    /* tcp->payload_len = tcp->data_len; */

    tcp->last_len = 0;
    tcp->last_seq =  pkg_th->seq ;
    tcp->last_ack =  pkg_th->ack_seq ;

    pkg_th->check     = chksum( b->start, skb->buf_len, TCP_CHK );

}




/*
 * 构造回应数据包
 * tcp 下载后直接回复
 *
 * */
void acc_tcp_psh_create( nt_buf_t *b, nt_skb_tcp_t *tcp )
{
    acc_tcp_ip_create(  b , tcp );

    struct tcphdr * pkg_th = ( struct tcphdr * ) ( b->start + sizeof( struct iphdr ));

    /* memset( pkg_th, 0 ,  20 ); */
    /* b->last += sizeof( struct tcphdr ); */
    pkg_th->source  = tcp->dport;                            // tcp source port
    pkg_th->dest = tcp->sport;                          // tcp dest port

    //标志位先置零
    pkg_th->urg = 0;
    pkg_th->ack = 0;
    pkg_th->psh = 0;
    pkg_th->rst = 0;
    pkg_th->syn = 0;
    pkg_th->fin = 0;


    /* debug( "seq=%d , tcp->payload_len=%d,  ack=%d" ,  ntohl( tcp->last_seq ) , tcp->last_len,  ntohl( tcp->last_ack )); */

    switch( tcp->phase ) {
    case TCP_PHASE_SEND_PSH:
        /* debug( "--ph--TCP_PHASE_SEND_PSH" ); */
        //发送payload阶段，未结束
        pkg_th->doff = 0x5;
        pkg_th->ack  = 1;
        pkg_th->seq = htonl( ntohl( tcp->last_seq ) + tcp->last_len )  ;                           // tcp seq   number
        pkg_th->ack_seq = tcp->last_ack ;

        /* pkg_th->ack_seq = htonl( ntohl( th->seq ) + payload_len );         // tcp ack number */
        /* b->last = nt_cpymem( b->last, tcp->data, tcp->data_len ); */
        break;
    case TCP_PHASE_SEND_PSH_END:
        /* debug( "--ph--TCP_PHASE_SEND_PSH_END" ); */
        //发送payload阶段，结束
        pkg_th->doff = 0x5;
        pkg_th->ack  = 1;
        pkg_th->psh  = 1;
        pkg_th->seq = htonl( ntohl( tcp->last_seq ) + tcp->last_len )  ;                           // tcp seq   number
        pkg_th->ack_seq = tcp->last_ack ;
        /* pkg_th->ack_seq = htonl( ntohl( th->seq ) + payload_len );       // tcp ack number */
        /* b->last = nt_cpymem( b->last, tcp->data, tcp->data_len );
        */
            memset( b->last, 0, b->end - b->last ); 
        break;
    }

    pkg_th->window    = htons( -1 );
    pkg_th->check     = 0;
    pkg_th->urg_ptr   = 0;

    //保存当前 seq , ack 和载荷长度，给下一次使用
    tcp->last_len = tcp->data_len;

    tcp->last_seq =  pkg_th->seq ;
    tcp->last_ack =  pkg_th->ack_seq ;

    pkg_th->check     = chksum( b->start, tcp->tot_len, TCP_CHK );

}




nt_connection_t* acc_tcp_input( char *data ){

    struct iphdr *ih;
    struct tcphdr *th;
    u_int16_t sport ;
    nt_rbtree_node_t *node;
    nt_acc_session_t *s;
    nt_skb_t *skb;

    nt_connection_t *c;

    ih = ( struct iphdr *  )data;

    sport = tcp_get_port( data, TCP_SRC  );
    
    node = rcv_conn_search( &acc_tcp_tree, sport  );

    if( node != NULL ){ //存在
        /* debug( "node exist" ); */
        c = ( nt_acc_session_t *  )node->key;
        return c;
    } else{//不存在
        debug( "node null" );
        return NULL;
    }
}


//判断该tcp包处于哪个阶段
uint8_t acc_tcp_phase( nt_skb_tcp_t *tcp, struct tcphdr *th )
{
	uint8_t phase ;
    debug( "tcp phase len=%d", tcp->data_len );

	if( tcp->data_len == 0 ) {

        debug( "ack=%d,fin=%d, window=%d", th->ack, th->fin, th->window );
        /* printf( "ack=%d,fin=%d, window=%d\n", th->ack, th->fin, th->window ); */

        //客户端 read 处理不过来
        //curl 下载大数据会出现
        // 可以发窗口探测包, 其中数据为0
        if( th->window == 0 ){
        }

        if( th->syn == 1 && th->ack == 0 ){
            debug( "TCP_PHASE_SYN" );
			tcp->phase = TCP_PHASE_SYN;
        }

		if( th->ack == 1 && th->syn == 1 ) {
			tcp->phase = TCP_PHASE_SYN_ACK;
		}

		//ack 第二包的ack或 FIN的ack
		if( th->ack == 1 && th->syn == 0 && th->fin == 0 ) {
			/* debug( "tcp->phase ack %d", tcp->phase ); */
			if( tcp->phase == TCP_PHASE_CONN_FREE ) {
				return NT_DONE;
			}

			tcp->phase = TCP_PHASE_ACK;
			return NT_ERROR;
		}

        if( th->fin == 1 ){
            debug( "client FIN close" );
			tcp->phase = TCP_PHASE_FIN ;
        }

        if( th->rst == 1 ){
            debug( "client RST close" );
            tcp->phase = TCP_PHASE_RST;
            return NT_DONE;
        }


	} else {
		debug( "th->psh=%d", th->psh );
		if( th->psh == 1 ) {
			tcp->phase = TCP_PHASE_PSH_END ;
		} else {
			tcp->phase = TCP_PHASE_PSH ;
		}
	}    

	return NT_OK;
}



/* 
	先进行tcp的3次握手
*/
nt_connection_t* acc_tcp_handshake( nt_acc_session_t *s ){

	nt_buf_t *b;
	nt_connection_t *c;
	struct iphdr *ih;
    struct tcphdr *th;
	nt_skb_tcp_t tmp_tcp;
	nt_skb_tcp_t *tcp;
	nt_int_t ret;
    nt_skb_t *skb;

	c =s->connection ;
	b = c->buffer ;

	ih = ( struct iphdr *) b->start;
	th = ( struct tcphdr * )( ih + 1 );

    u_int16_t tcp_len = (b->last - b->start)  - (ih->ihl << 2);
    tmp_tcp.data_len = tcp_len - (th->doff << 2);

    /* debug( "tcp data len=%d" , tmp_tcp.data_len ); */

    tmp_tcp.phase = 0;
    ret = acc_tcp_phase( &tmp_tcp, th);
    if( ret == NT_ERROR ){
        return NT_ERROR;
    }



    //进行首次的初始化
    if( s->skb == NULL ){
        
        skb = nt_palloc( c->pool, sizeof( nt_skb_t ) );

        s->skb = skb;
        skb->buffer = nt_create_temp_buf( c->pool, 1500  );

        tcp = ( nt_skb_tcp_t *) nt_pcalloc( c->pool, sizeof( nt_skb_tcp_t ) );
        skb->data = tcp;

        struct sockaddr *src = ( struct sockaddr *  ) nt_pcalloc( c->pool, sizeof( struct sockaddr  )  );
        struct sockaddr *dst = ( struct sockaddr *  ) nt_pcalloc( c->pool, sizeof( struct sockaddr  )  );
        tcp->src = src;
        tcp->dst = dst;

        tcp->sip = ih->saddr;
        tcp->dip = ih->daddr;

        tcp->sport = th->source ;
        tcp->dport = th->dest ;

        struct sockaddr_in *addr ;
        addr =( struct sockaddr_in * ) src; 
        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = ih->saddr;
        addr->sin_port = th->source;

        addr =( struct sockaddr_in * ) dst; 
        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = ih->daddr;
        addr->sin_port = th->dest;
        
        tcp->hdr_len = th->doff << 2;
        tcp->data = NULL;
        tcp->data_len = 0;
        tcp->last_len = 0;
        tcp->last_seq = ntohl( 1  );
        tcp->last_ack = ntohl( 1  );
        skb->protocol = ih->protocol ;
        skb->skb_len = b->last - b->start;
        skb->iphdr_len = ih->ihl << 2;

        s->port = th->source;
        nt_connection_rbtree_add( c );
        s->fd = c->fd;
        c->type = IPPROTO_TCP ;
    } else {
        tcp = s->skb->data;
        tcp->data_len = tmp_tcp.data_len;
        skb = s->skb;
        skb->skb_len = b->last - b->start;
        skb->iphdr_len = ih->ihl << 2;


        /* 
         * tcp->seq = th->seq;
         * tcp->ack_seq = th->ack_seq;
         */
    }

    tcp->phase = tmp_tcp.phase;

    if( tcp->phase == TCP_PHASE_PSH ||  tcp->phase == TCP_PHASE_PSH_END ){

        /* if(  tcp->phase == TCP_PHASE_PSH ){
            tcp->last_len = 0;
        } */

        tcp->last_seq = th->seq;
        tcp->last_ack = th->ack_seq;
        /* debug( " payload=%s", b->start + ( skb->skb_len - tmp_tcp.data_len)) ; */
    }

    /* debug( "c->fd=%d", c->fd ); */
	tcp_phase_handle( c , tcp );

    /* debug( "end" ); */
	/* //从tun 流入的握手阶段的 syn
	if( th->syn == 1 && th->ack == 0 ){
		[>tcp->phase = TCP_PHASE_SYN;<]
		// 需回复 
	}

	//从tun 流入的握手阶段的 syn 后的ack
	if( th->ack == 1 && th->syn == 1 ) {
		[>tcp->phase = TCP_PHASE_SYN_ACK;<]
	} */
}


