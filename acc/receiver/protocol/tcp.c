#include "protocol.h"

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

    return ntohs( port ) ;
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

//判断该tcp包处于哪个阶段
uint8_t tcp_phase( nt_skb_tcp_t *tcp, struct tcphdr *th )
{
    uint8_t phase ;
    if( tcp->data_len == 0 ) {

        if( th->syn == 1 && th->ack == 0 )
            tcp->phase = TCP_PHASE_SYN;

        if( th->ack == 1 && th->syn == 1 )
            tcp->phase = TCP_PHASE_SYN_ACK;

        //ack 第二包的ack或 FIN的ack
        if( th->ack == 1 && th->syn == 0 && th->fin == 0 ) {
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

    memset( b->start , 0 , b->end - b->start );
    tcp = skb->data;

    ip_create( skb, ih );

//    struct iphdr * pkg_ih  = ( struct iphdr * ) b->start ;
//    b->last += sizeof( struct iphdr );
    struct tcphdr * pkg_th = ( struct tcphdr * ) b->last;

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
        //接收payload阶段，结束
        pkg_th->doff = 0x5;
        pkg_th->ack  = 1;
        pkg_th->seq = th->ack_seq;
        pkg_th->ack_seq = htonl( ntohl( th->seq ) + payload_len );
        break;

    case TCP_PHASE_SEND_PSH:
        //发送payload阶段，未结束
        pkg_th->doff = 0x5;
        pkg_th->ack  = 1;
        debug( "TCP_SEND_PAYLOAD seq=%u", ntohl( tcp->seq  ) );           
        pkg_th->seq = htonl( ntohl(tcp->seq) + tcp->payload_len )  ;                             // tcp seq   number
        debug( "TCP_SEND_PAYLOAD seq=%u", ntohl( tcp->seq  ) ); 
        debug( "TCP_SEND_PAYLOAD conn->odt->payload_len=%d", tcp->payload_len );
        pkg_th->ack_seq = htonl( ntohl( th->seq  ) + payload_len  );       // tcp ack number
        //memcpy( pkg_th + 1, tcp->data, tcp->data_len   ); //发送payload
        b->last = nt_cpymem( b->last , tcp->data, tcp->data_len );
        //        b->last += tcp->data_len;
        break;
    case TCP_PHASE_SEND_PSH_END:
        //发送payload阶段，结束
        pkg_th->doff = 0x5;
        pkg_th->ack  = 1;
        pkg_th->psh  = 1;
        pkg_th->seq = htonl( ntohl( tcp->seq ) + tcp->payload_len )  ;                          // tcp seq number
        debug( "TCP_SEND_PAYLOAD_END seq=%u", ntohl( tcp->seq ) );
        debug( "TCP_SEND_PAYLOAD_END conn->odt->payload_len=%d", tcp->payload_len );
        pkg_th->ack_seq = htonl( ntohl( th->seq ) + payload_len );       // tcp ack number
       // memcpy( pkg_th + 1, tcp->data, tcp->data_len  ); //发送payload
      //  b->last += tcp->data_len;
        b->last = nt_cpymem( b->last , tcp->data, tcp->data_len );

        break;
    case TCP_PHASE_SEND_FIN: //主动回 FIN 
    case TCP_PHASE_SEND_FIN_ACK:  //收到FIN ACK, 回应 ACK
        debug( " TCP_FIN_ACK " );
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
    tcp->payload_len = tcp->data_len;

    tcp->seq =  pkg_th->seq ;
    tcp->ack_seq =  pkg_th->ack_seq ;

    pkg_th->check     = chksum( b->start, skb->buf_len, TCP_CHK );

}

//对进入的tcp数据做处理。
//需进行三次握手的欺骗
int tcp_input( nt_connection_t *c )
{

    nt_rev_connection_t *rc ;
    nt_skb_t *skb;
    nt_skb_tcp_t *tcp;
    nt_buf_t *b;
    struct iphdr *ih;
    struct tcphdr *th;
    u_int8_t ret;

    b = c->buffer;
    rc = c->data;
    skb = rc->skb;
    tcp = skb->data;

    ih = ( struct iphdr * )b->start;
    th = ( struct tcphdr * )( ih + 1 );

    

//    uint16_t iphdr_len = ih->ihl << 2;
//    uint16_t tcphdr_len = th->doff << 2;
//    u_int16_t payload_len = tcp_len -  tcphdr_len;
    u_int16_t tcp_len = skb->skb_len - skb->iphdr_len;

    tcp->hdr_len = th->doff << 2;
    tcp->data_len = tcp_len - tcp->hdr_len;


    debug( "tcp->hdr_len=%d", tcp->hdr_len );
    debug( "packet len=%d, playload_len=%d", tcp_len, tcp->data_len );

    ret = tcp_phase( tcp, th );
    if( ret != NT_OK )
        return NT_ERROR;


    debug( "tcp phase=%d", tcp->phase );


    //执行回复动作
    tcp_phase_handle( c );
    

}


int tcp_direct_server( nt_connection_t *c )
{

    nt_rev_connection_t *rc ;
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


//    char buf[1452] = {0};
    char buf[568] = {0};
    int len = 0;

    int have_send_ack = 0;
//    in.sin_family = AF_INET;

    tcp->payload_len = 0;

    do {
        ret = recv( rfd, buf, sizeof( buf ), 0 );
        debug( "ret=%d, buf=%s", ret, buf );

        //send playload
        if( 0 >= ret ) {
            if( ( 0 > ret ) && ( EAGAIN == errno ) )
                return 0;
            else
                return -1;
        } else {
            tcp->data = buf ;
            tcp->data_len = ret;

            debug( "ret= %d, buf size=%d", ret, sizeof( buf  )  );
            if( ret != sizeof( buf ) ) {
                tcp->phase = TCP_PHASE_SEND_PSH_END;
            } else
                tcp->phase = TCP_PHASE_SEND_PSH;

            //tcp_phase_send_response( c  );
            tcp_create( skb, ih , th );
            ssize_t size = skb->buffer->last - skb->buffer->start;

            debug( "size=%d",  size);
            write( c->fd, skb->buffer->start  , size);

            if( ret != sizeof( buf ) )
                break ;
        }


    } while( ret > 0 );

    close( rfd );
    debug( "+++++++++++++++end+++++++++++++" );


}

int tcp_phase_proxy_socks( nt_connection_t *c ){
    debug( "++++++++++++++tcp_phase_proxy_socks+++++++++++++"  );
    nt_rev_connection_t *rc ;
    nt_skb_t *skb;
    nt_skb_tcp_t *tcp;
    int ret;
    const char path[]="/tmp/nt.socks5.sock";
    int fd;
    struct sockaddr_un server_addr;
    ssize_t size ;

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
   
    fd = socket(AF_UNIX,SOCK_STREAM,0);

    server_addr.sun_family=AF_UNIX;
    strcpy(server_addr.sun_path,path);


    if(connect(fd,(struct sockaddr *)&server_addr,sizeof(server_addr)) == -1){
        debug(" connect to server fail. errno=%d", errno);
        return TCP_PHASE_NULL;
    }

    nt_tcp_socks_t  tcp_socks;    
    tcp_socks.type = 2;
    tcp_socks.domain = 4;
    tcp_socks.protocol = 1;

    debug( "seq=%u", ntohl( tcp->seq ) );
    debug( "ack=%u", ntohl( tcp->ack_seq ) );

    tcp_socks.seq = tcp->seq;
    tcp_socks.ack = tcp->ack_seq;

    tcp_socks.server_ip = inet_addr("172.16.254.157");
    tcp_socks.server_port = htons( "1080" );

    strcpy( tcp_socks.user , "test" );
    strcpy( tcp_socks.password , "test" );

    //源码ip打印出来不对
    struct sockaddr_in *addr;
    addr = ( struct sockaddr_in *)tcp->src;
    tcp_socks.sip = addr->sin_addr.s_addr; 
    //tcp_socks.sip = ih->saddr; 
    tcp_socks.sport =  addr->sin_port;


    addr = ( struct sockaddr_in *)tcp->dst;
    tcp_socks.dip = addr->sin_addr.s_addr; 
    tcp_socks.dport = addr->sin_port;


    debug( "sip=%u.%u.%u.%u:%d", IP4_STR( tcp_socks.sip   ), ntohs( tcp_socks.sport   ));

    tcp_socks.data_len =  tcp->data_len ;
    debug(" size  = %d",  tcp_socks.data_len);
    tcp->data = c->buffer->start + skb->iphdr_len + tcp->hdr_len ;
    memcpy( tcp_socks.data , tcp->data,  tcp_socks.data_len );
  //  tcp_socks.data = tcp->data ;
    debug(" buf  = %s",  tcp_socks.data);


    size = sizeof( nt_tcp_socks_t  ) - 1500  + tcp_socks.data_len;
    debug(" size  = %d", size);

    ret = send( fd, (void *)&tcp_socks , size, 0 );

    debug(" ret  = %d", ret);
    if( ret < 0 )
        return TCP_PHASE_NULL;



//    char buf[1452] = {0};
    char buf[568] = {0};
    int len = 0;

    int have_send_ack = 0;
//    in.sin_family = AF_INET;

    tcp->payload_len = 0;

    do {
        ret = recv( fd, buf, sizeof( buf ), 0 );
        debug( "ret=%d, buf=%s", ret, buf );

        //send playload
        if( 0 >= ret ) {
            if( ( 0 > ret ) && ( EAGAIN == errno ) )
                return 0;
            else
                return -1;
        } else {
            tcp->data = buf ;
            tcp->data_len = ret;

            debug( "ret= %d, buf size=%d", ret, sizeof( buf  )  );
            if( ret != sizeof( buf ) ) {
                tcp->phase = TCP_PHASE_SEND_PSH_END;
            } else
                tcp->phase = TCP_PHASE_SEND_PSH;

            //tcp_phase_send_response( c  );
            tcp_create( skb, ih , th );
            ssize_t size = skb->buffer->last - skb->buffer->start;

            debug( "size=%d",  size);
            write( c->fd, skb->buffer->start  , size);

            if( ret != sizeof( buf ) )
                break ;
        }


    } while( ret > 0 );

    close( fd );
    debug( "+++++++++++++++end+++++++++++++" );


    return TCP_PHASE_NULL;
}

int tcp_phase_send_response( nt_connection_t *c ){
    debug( "++++++++++++++tcp_phase_send_response+++++++++++++"  );
    nt_rev_connection_t *rc ;
    nt_skb_t *skb;
    nt_skb_tcp_t *tcp;
    nt_buf_t *b;
    struct iphdr *ih;
    struct tcphdr *th;
    u_int8_t ret;

    b = c->buffer;
    rc = c->data;
    skb = rc->skb;
    tcp = skb->data;
    ih = ( struct iphdr * )b->start;
    th = ( struct tcphdr * )( ih + 1 );

    struct sockaddr_in *addr;
    addr = ( struct sockaddr_in *)tcp->src;
    debug( "sip=%u.%u.%u.%u:%d", IP4_STR( addr->sin_addr.s_addr   ), ntohs( addr->sin_port   ));


    //创建回复数据包, 回复SYN ACK
    tcp_create( skb, ih, th );

    addr = ( struct sockaddr_in *)tcp->src;
    debug( "sip=%u.%u.%u.%u:%d", IP4_STR( addr->sin_addr.s_addr   ), ntohs( addr->sin_port   ));


    ret = tcp_output( c );

    if( ret < 0   ){
        debug( "tcp_phase_send_response ret =%d \n", ret );
        ret = tcp_output( c );
    }


    if( ret < 0   )
        return TCP_PHASE_NULL;

    if( tcp->phase == TCP_PHASE_SEND_PSH_ACK)
        /* return TCP_PHASE_PROXY_DIRECT; */
        return TCP_PHASE_PROXY_SOCKS;
    else if ( tcp->phase == TCP_PHASE_SEND_FIN_ACK )
        return TCP_PHASE_SEND_FIN;
    else
        return TCP_PHASE_NULL;

}



int tcp_phase_handle( nt_connection_t *c )
{
    nt_rev_connection_t *rc ;
    nt_skb_t *skb;
    nt_skb_tcp_t *tcp;
    nt_buf_t *b;
    struct iphdr *ih;
    struct tcphdr *th;
    u_int8_t ret;

    b = c->buffer;
    rc = c->data;
    skb = rc->skb;
    tcp = skb->data;
    ih = ( struct iphdr * )b->start;
    th = ( struct tcphdr * )( ih + 1 );


    
    while( tcp->phase ){

        debug( "tcp->phase=%d", tcp->phase );

        switch( tcp->phase ) {
        case TCP_PHASE_SYN:
            tcp_parse_option( b->start );

            //接收到SYN包，把阶段改为发送SYN ACK
            tcp->phase = TCP_PHASE_SEND_SYN_ACK;
        case TCP_PHASE_SEND_SYN_ACK:
            tcp->phase = tcp_phase_send_response( c );
          
            break;
        case TCP_PHASE_PSH:
            //回复client
            break;
        case TCP_PHASE_PSH_END:
            //接收到PSH 结束，回复 PSH ACK;
            tcp->phase = TCP_PHASE_SEND_PSH_ACK;
        case TCP_PHASE_SEND_PSH_ACK:
            //回复PSH ACK
            tcp->phase = tcp_phase_send_response( c );

            break;
        case TCP_PHASE_PROXY_DIRECT:
            //发送payload 到 server
            tcp->phase = tcp_direct_server( c );
            break;
        case TCP_PHASE_PROXY_SOCKS:
            //发送payload 到 server
            tcp->phase = tcp_phase_proxy_socks( c );
            break;
        case TCP_PHASE_FIN:
            //收到FIN 回应FIN ACK
            tcp->phase = TCP_PHASE_SEND_FIN_ACK ;
            break;
        case TCP_PHASE_SEND_FIN_ACK:
            tcp->phase = tcp_phase_send_response( c  );
            break;
        case TCP_PHASE_SEND_FIN:
            tcp->phase = tcp_phase_send_response( c  );
            break;
        default:
            tcp->phase = TCP_PHASE_NULL;
            break;
        }
    }

    return tcp->phase;
}





int  tcp_output( nt_connection_t *c )
{
    nt_rev_connection_t *rc ;
    nt_skb_t *skb;
    nt_skb_tcp_t *tcp;
    int ret;

    rc = c->data;
    skb = rc->skb;
    tcp = skb->data;

    nt_buf_t *b;
    b = skb->buffer;
    ssize_t size = b->last - b->start;
    debug( "send size=%d", size );
    debug( "phase=%d", tcp->phase );

    print_pkg( b->start );
    ret = write( c->fd, b->start, size );

  
    debug( "ret=%d", ret );
}




