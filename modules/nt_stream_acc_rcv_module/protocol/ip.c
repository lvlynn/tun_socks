#include "protocol.h"

//https://blog.csdn.net/KgdYsg/article/details/89106024

unsigned long chk( const unsigned  short * data, int size )
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


/*
 *计算前把Checksum字段置0；
 将IP Header中每两个连续的字节当成一个16bit数，对所有的16bit数进行求和，在求和过程中，任何溢出16bit数范围的求和结果都需要进行回卷——将溢出的高16bit和求和结果的低16bit相加；
 对最终的求和结果按位取反，即可得到IP Header Checksum
 * */
unsigned short ip_chk( const unsigned short *data, int size )
{
    unsigned long sum = 0;
    struct iphdr *ih = ( struct iphdr * )data;
    sum =  chk( data, size ) - ih->check;
    sum = ( sum >> 16 ) + ( sum & 0xffff );
    sum += ( sum >> 16 );
    return ( unsigned short )( ~sum );

}

void print_pkg( char *data )
{
    struct iphdr *ih = ( struct iphdr * )data;
    switch( ih->protocol ) {
    case IPPROTO_TCP: {
        struct tcphdr *tcp = ( struct tcphdr * )( ih + 1 );
        debug( "src = %d.%d.%d.%d:%d, dst = %d.%d.%d.%d:%d",
               IP4_STR( ih->saddr ), ntohs( tcp->source ),
               IP4_STR( ih->daddr ), ntohs( tcp->dest ) );
        break;
    }
    case IPPROTO_UDP: {
        debug("udp ih id=%d", ih->id );
        debug("udp ih frag=%d", ntohs(ih->frag_off )& IP_MF);
        struct udphdr *udp = ( struct udphdr * )( ih + 1 );
        debug( "src = %d.%d.%d.%d:%d, dst = %d.%d.%d.%d:%d",
               IP4_STR( ih->saddr ), ntohs( udp->source ),
               IP4_STR( ih->daddr ), ntohs( udp->dest ) );

        //udp_print( data, pkg );
        break;
    }    
    }
}



int ip_create( nt_skb_t *skb, struct iphdr *ih )
{
    nt_skb_tcp_t *tcp;
    nt_buf_t *b;

    tcp = skb->data;

    debug( "tcp phase=%d", tcp->phase );
    switch( tcp->phase ) {
    case TCP_PHASE_SEND_SYN_ACK:
        skb->buf_len = skb->iphdr_len + tcp->hdr_len;
        break;
    case TCP_PHASE_ACK:
    case TCP_PHASE_SEND_FIN:
    case TCP_PHASE_SEND_FIN_ACK:
    case TCP_PHASE_SEND_PSH_ACK:
        skb->buf_len = skb->iphdr_len + 20;
        break;
    case TCP_PHASE_SEND_PSH:
    case TCP_PHASE_SEND_PSH_END:
        skb->buf_len = skb->iphdr_len + 20 +  tcp->data_len;
        break;
    }

    debug( "skb->buf_len=%d", skb->buf_len );

    b = skb->buffer;
    /* b->last = b->start;
    b->pos = b->start; */

//     memset( b->start, 0, skb->buf_len   );
    struct iphdr *pkg_ih = ( struct iphdr * )b->last;
    b->last += sizeof( struct iphdr );

    pkg_ih->version = 0x4;
    pkg_ih->ihl = sizeof( struct iphdr ) / 4;
    pkg_ih->tos = ih->tos;
    pkg_ih->tot_len = htons( skb->buf_len );
    pkg_ih->id = 0;
    pkg_ih->frag_off = ih->frag_off;
    pkg_ih->ttl = 64;
    pkg_ih->protocol = ih->protocol;

    pkg_ih->daddr = ih->saddr;
    pkg_ih->saddr = ih->daddr;

    /* pkg_ih->daddr = ih->daddr;
    pkg_ih->saddr = ih->saddr;
 */

    pkg_ih->check = 0;
    pkg_ih->check = chksum( b->start, sizeof( struct iphdr ), IP_CHK );
    return 0;
}


int acc_tcp_ip_create(   nt_buf_t *b, nt_skb_tcp_t *tcp )
{

    tcp->tot_len = 20 + 20 +  tcp->data_len;

    /* debug( "skb->buf_len=%d", tcp->tot_len ); */

    /* b = skb->buffer; */
    /* b->last = b->start;
    b->pos = b->start; */

//     memset( b->start, 0, skb->buf_len   );
    struct iphdr *pkg_ih = ( struct iphdr * )b->start;
    /* b->last += sizeof( struct iphdr ); */

    pkg_ih->version = 0x4;
    pkg_ih->ihl = sizeof( struct iphdr ) / 4;
    pkg_ih->tos = 0x10;
    pkg_ih->tot_len = htons( tcp->tot_len );
    pkg_ih->id = 0;
    pkg_ih->frag_off = 0;
    pkg_ih->ttl = 64;
    pkg_ih->protocol = 0x6;

    pkg_ih->daddr = tcp->sip;
    pkg_ih->saddr = tcp->dip;

    /* pkg_ih->daddr = ih->daddr;
    pkg_ih->saddr = ih->saddr;
 */

    pkg_ih->check = 0;
    pkg_ih->check = chksum( b->start, sizeof( struct iphdr ), IP_CHK );
    return 0;
}





nt_connection_t* ipv4_input( char *data )
{
//    debug( "ipv4 pkg" );
    struct iphdr *ih;
    nt_connection_t *c;

    ih = ( struct iphdr * )data;

    /* debug( "ih->protocol=%d", ih->protocol ) ; */
    switch( ih->protocol ) {
    case IPPROTO_TCP:
        c = acc_tcp_input( data );
        break;
    case IPPROTO_UDP:
        /* udp_input( c ); */
        break;
    }

    return c;
}

nt_connection_t*  ip_input( char *data )
{
    //判断ipv4 还是ipv6
    struct iphdr *ih = ( struct iphdr * )data;

    if( ih->version == 0x6 ) {
        debug( "ipv6 pkg" );
        return NULL;
    }
    struct tcphdr *th = ( struct tcphdr *)(ih + 1);
    debug( "ack=%d,fin=%d", th->ack, th->fin  );
    return ipv4_input( data );

}

//返回连接
nt_connection_t *ip_lookup_connection( char *data ){

    nt_connection_t *c;
    return ip_input( data ); 

}

