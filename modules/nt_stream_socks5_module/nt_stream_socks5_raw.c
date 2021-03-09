#include <nt_core.h>
#include "nt_stream_socks5_raw.h"

void nt_print_pkg( char *data )
{
    struct iphdr *ih = ( struct iphdr * )data;
    switch( ih->protocol ) {
    case IPPROTO_TCP: {
        struct tcphdr *tcp = ( struct tcphdr * )( ih + 1 );
        debug(  "src = %d.%d.%d.%d:%d, dst = %d.%d.%d.%d: %d",
               IP4_STR( ih->saddr ), ntohs( tcp->source ),
               IP4_STR( ih->daddr ), ntohs( tcp->dest ) );
        break;
    }
    case IPPROTO_UDP:
        //udp_print( data, pkg );
        break;
    }
}

unsigned long nt_chk( const unsigned  short * data, int size )
{

    unsigned long sum = 0;
    while( size > 1 ) {
        sum += *data++;
        size -= sizeof( unsigned short );
    }
    if( size ) {
        sum += *( unsigned * )data;
    }
    return sum;
}


unsigned short nt_ip_chk( const unsigned short *data, int size )
{
    unsigned long sum = 0;
    struct iphdr *ih = ( struct iphdr * )data;
    sum =  nt_chk( data, size ) - ih->check;
    sum = ( sum >> 16 ) + ( sum & 0xffff );
    sum += ( sum >> 16 );
    return ( unsigned short )( ~sum );

}


unsigned short nt_udp_chk( const unsigned short *data, int size  )
{
    unsigned long sum;
    pshead ph; 
    struct iphdr *ih = ( struct iphdr *  )data;
    ph.saddr = ih->saddr;
    ph.daddr = ih->daddr;
    ph.mbz = 0;
    ph.ptcl = 17; 
    ph.tcpl = htons( size - ( ih->ihl << 2  )  );
    struct udphdr * udp = ( struct udphdr *  )( ih + 1  );
    sum = nt_chk( ( unsigned short *  )&ph, sizeof( pshead  )  );
    sum += nt_chk( ( unsigned short *  )udp, ntohs( ph.tcpl  )  );
    sum -= udp->check;
    sum = ( sum >> 16  ) + ( sum & 0xffff  );
    sum += ( sum >> 16  );
    return ( unsigned short  )( ~sum  );
}

static unsigned short nt_chksum( const char *data, int size, int type )
{
    unsigned short sum = 0;
    unsigned short *udata = ( unsigned short * )data;
    switch( type ) {
    case IP_CHK:
        sum = nt_ip_chk( udata, size );
        break;
    case TCP_CHK:
        sum = nt_tcp_chk( udata, size );
        break;
    case UDP_CHK:
        sum = nt_udp_chk( udata, size  );
        break;
    default:
        return ( unsigned short )( ~sum );

    }
    return sum;

}


/*
    ip头部构造，需要构造整个数据包长度

*/
int tcp_ip_create( nt_buf_t *b, nt_tcp_raw_socket_t *rs )
{

    switch( rs->phase ) {
    case TCP_PHASE_SEND_PSH:
    case TCP_PHASE_SEND_PSH_END:
    default:
        rs->tot_len = 20 + 20 +  rs->data_len;
        break;
    }

    debug(  "skb->buf_len=%d", rs->tot_len );


    struct iphdr *pkg_ih = ( struct iphdr * )b->last;
    b->last += sizeof( struct iphdr );

    pkg_ih->version = 0x4;
    pkg_ih->ihl = sizeof( struct iphdr ) / 4;
    pkg_ih->tos = 0x10;
    pkg_ih->tot_len = htons( rs->tot_len );
    pkg_ih->id = 0;
    pkg_ih->frag_off = 0;
    pkg_ih->ttl = 64;
    pkg_ih->protocol = 0x6;
    pkg_ih->daddr = rs->sip;
    pkg_ih->saddr = rs->dip;
    pkg_ih->check = 0;
    pkg_ih->check = nt_chksum( b->start, sizeof( struct iphdr ), IP_CHK );
    return 0;
}

int udp_ip_create( nt_buf_t *b, nt_udp_raw_socket_t *rs )
{
    rs->tot_len = 20 + 8 +  rs->data_len;

    debug(  "skb->buf_len=%d", rs->tot_len );

    struct iphdr *pkg_ih = ( struct iphdr * )b->last;
    b->last += sizeof( struct iphdr );

    pkg_ih->version = 0x4;
    pkg_ih->ihl = sizeof( struct iphdr ) / 4;
    pkg_ih->tos = 0x10;
    pkg_ih->tot_len = htons( rs->tot_len );
    pkg_ih->id = 0;
    pkg_ih->frag_off = 0;
    pkg_ih->ttl = 64;
    pkg_ih->protocol = 0x11;
    pkg_ih->daddr = rs->sip;
    pkg_ih->saddr = rs->dip;
    pkg_ih->check = 0;
    pkg_ih->check = nt_chksum( b->start, sizeof( struct iphdr ), IP_CHK );
    return 0;
}




unsigned short nt_tcp_chk( const unsigned short *data, int size )
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
    sum = nt_chk( ( unsigned short * )&ph, sizeof( pshead ) );
    sum += nt_chk( ( unsigned short * )tcp, ntohs( ph.tcpl ) );
    sum -= tcp->check;
    sum = ( sum >> 16 ) + ( sum & 0xffff );
    sum += ( sum >> 16 );
    return ( unsigned short )( ~sum );

}

/*
   * 构造回应数据包
   * */
void nt_tcp_create( nt_buf_t *b, nt_tcp_raw_socket_t *rs )
{
    debug(  "tcp_create" );
    b->last = b->start;
    b->pos = b->start;

    /* memset( b->start, 0, 1500 ); */

    tcp_ip_create( b, rs );

    //    struct iphdr * pkg_ih  = ( struct iphdr * ) b->start ;
    //    b->last += sizeof( struct iphdr );
    struct tcphdr * pkg_th = ( struct tcphdr * ) b->last;

    b->last += sizeof( struct tcphdr );
    pkg_th->source  = rs->dport;                            // tcp source port
    pkg_th->dest = rs->sport;                          // tcp dest port

    //标志位先置零
    pkg_th->urg = 0;
    pkg_th->ack = 0;
    pkg_th->psh = 0;
    pkg_th->rst = 0;
    pkg_th->syn = 0;
    pkg_th->fin = 0;

    /* uint8_t iphdr_len = 20;
    uint8_t tcphdr_len = 20;
    */
//    uint16_t payload_len = skb->skb_len - iphdr_len - tcphdr_len ;

    switch( rs->phase ) {

    case TCP_PHASE_SEND_PSH:
        //发送payload阶段，未结束
        pkg_th->doff = 0x5;
        pkg_th->ack  = 1;
        debug(  "TCP_SEND_PAYLOAD seq=%u", ntohl( rs->seq ) );
        pkg_th->seq = htonl( ntohl( rs->seq ) + rs->payload_len )  ;                           // tcp seq   number
        debug(  "TCP_SEND_PAYLOAD seq=%u", ntohl( rs->seq ) );
        debug(  "TCP_SEND_PAYLOAD conn->odt->payload_len=%d", rs->payload_len );
        pkg_th->ack_seq = rs->ack_seq ;         // tcp ack number
        //memcpy( pkg_th + 1, tcp->data, tcp->data_len   ); //发送payload
        b->last = nt_cpymem( b->last, rs->data, rs->data_len );
        memset( b->last, 0, b->end - b->last ); 
        //        b->last += tcp->data_len;
    debug(  "TCP CREATE 1") ;
        break;
    case TCP_PHASE_SEND_PSH_END:
        //发送payload阶段，结束
        pkg_th->doff = 0x5;
        pkg_th->ack  = 1;
        pkg_th->psh  = 1;
        pkg_th->seq = htonl( ntohl( rs->seq ) + rs->payload_len )  ;                          // tcp seq number
        debug(  "TCP_SEND_PAYLOAD_END seq=%u", ntohl( rs->seq ) );
        debug(  "TCP_SEND_PAYLOAD_END conn->odt->payload_len=%d", rs->payload_len );
        pkg_th->ack_seq = rs->ack_seq ;         // tcp ack number
        // memcpy( pkg_th + 1, tcp->data, tcp->data_len  ); //发送payload
        //  b->last += tcp->data_len;
        b->last = nt_cpymem( b->last, rs->data, rs->data_len );
        memset( b->last, 0, b->end - b->last ); 

        break;
    }
    pkg_th->window    = htons( -1 );
    pkg_th->check     = 0;
    pkg_th->urg_ptr   = 0;

    debug(  "TCP CREATE 2") ;
    //保存当前 seq , ack 和载荷长度，给下一次使用
    rs->payload_len = rs->data_len;
    rs->seq =  pkg_th->seq ;
    rs->ack_seq =  pkg_th->ack_seq ;

    debug(  "TCP CREATE 3") ;
    pkg_th->check     = nt_chksum( b->start, rs->tot_len, TCP_CHK );
    debug(  "TCP CREATE 4") ;

}


void nt_udp_create( nt_buf_t *b, nt_udp_raw_socket_t *rs )
{
    b->last = b->start;
    b->pos = b->start;

    /* memset( b->start, 0, 1500 ); */

    udp_ip_create( b, rs );

    struct udphdr * pkg_uh = ( struct udphdr *   ) b->last;

    b->last += sizeof( struct udphdr  );
    pkg_uh->source  = rs->dport;
    pkg_uh->dest = rs->sport;
    /* pkg_uh->source  = uh->source;
     *       pkg_uh->dest = uh->dest;
     *          */
    pkg_uh->len = htons( sizeof( struct udphdr  ) + rs->data_len  ) ; 

    b->last = nt_cpymem( b->last, rs->data, rs->data_len  );

    pkg_uh->check = 0;

    pkg_uh->check     = nt_chksum( b->start, rs->tot_len, UDP_CHK );
}
