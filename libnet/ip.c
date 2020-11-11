#include <stdio.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <time.h>


#include "./connection.h"
#include "./tcp.h"
#include "./ip.h"
#include "./log.h"



void print_pkg( char *data )
{
    struct iphdr *ih = ( struct iphdr * )data;
    switch( ih->protocol ) {
    case IPPROTO_TCP: {
        struct tcphdr *tcp = ( struct tcphdr * )( ih + 1 );
        debug( "src = %d.%d.%d.%d:%d, dst = %d.%d.%d.%d: %d",
               IP4_STR( ih->saddr ), ntohs( tcp->source ),
               IP4_STR( ih->daddr ), ntohs( tcp->dest ) );
        break;
    }
    case IPPROTO_UDP:
        //udp_print( data, pkg );
        break;
    }
}


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

static unsigned short ip_chk( const unsigned short *data, int size )
{
    unsigned long sum = 0;
    struct iphdr *ih = ( struct iphdr * )data;
    sum =  chk( data, size ) - ih->check;
    sum = ( sum >> 16 ) + ( sum & 0xffff );
    sum += ( sum >> 16 );
    return ( unsigned short )( ~sum );
}
static unsigned short tcp_chk( const unsigned short *data, int size )
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

unsigned short chksum( const char *data, int size, int type )
{
    unsigned short sum = 0;
    unsigned short *udata = ( unsigned short * )data;
    switch( type ) {
    case IP_CHK:
        sum = ip_chk( udata, size );
        break;
    case TCP_CHK:
        sum = tcp_chk( udata, size );
        break;
    default:
        return ( unsigned short )( ~sum );
    }
    return sum;
}

int ip_create( nt_conn_t *conn )
{
    nt_skb_t *ndt = conn->ndt;
    uint8_t type = ndt->type;
    //tcp ack
    //tcp payload
    //udp payload
    if( type == TCP_SYN_ACK ) {  //包含option的长度
        ndt->pkg_len = ndt->iphdr_len +  ndt->tcp_info->hdr_len ;
//        ndt->pkg =( char *  ) malloc(  ndt->iphdr_len +  ndt->tcp_info->hdr_len  );
    } else if( type == TCP_PAYLOAD_ACK || type == TCP_FIN_ACK || type == TCP_SEND_FIN_ACK ) //不包含option的长度
        ndt->pkg_len = ndt->iphdr_len +  20 ;
    else if( type == TCP_SEND_PAYLOAD || type == TCP_SEND_PAYLOAD_END )
        ndt->pkg_len = ndt->iphdr_len +  20 + ndt->tcp_info->payload_len ;

//        ndt->pkg =( char * ) malloc(  ndt->iphdr_len +  ndt->tcp_info->hdr_len + ndt->tcp_info->payload_len );

    ndt->pkg = ( char * ) malloc( ndt->pkg_len );
    memset( ndt->pkg, 0, ndt->pkg_len );

    struct iphdr *pkg_ih = ( struct iphdr * )ndt->pkg;
    struct iphdr *ih = ( struct iphdr * )ndt->skb ;

    pkg_ih->version = 0x4;
    pkg_ih->ihl = sizeof( struct iphdr ) / 4;
    pkg_ih->tos = ih->tos;
    pkg_ih->tot_len = htons( ndt->pkg_len );
    pkg_ih->id = 0;
    pkg_ih->frag_off = ih->frag_off;
    pkg_ih->ttl = 64;
    pkg_ih->protocol = ih->protocol;
    pkg_ih->daddr = ih->saddr;
    pkg_ih->saddr = ih->daddr;
    pkg_ih->check = 0;
    pkg_ih->check = chksum( ndt->pkg, sizeof( struct iphdr ), IP_CHK );
    return 0;
}


static void udp_create( const char *data, char *pkg )
{

}



int create_pkg( nt_conn_t *conn )
{
    debug( "##############start###############" );
    switch( conn->ndt->protocol ) {
    case IPPROTO_TCP:
        ip_create( conn );
        tcp_create( conn );
        break;
    case IPPROTO_UDP:
//        udp_create( conn, pkg );
        break;
    default:
        return 0;
    }
    debug( "##############end###############" );
    return 0;
}


int ipv4_input( nt_conn_t *conn )
{
    nt_skb_t *ndt = conn->ndt;

    struct iphdr *ih = ( struct iphdr * )ndt->skb;
//    ndt->ih = ih;

    //头部长度：通常20字节，有选项时更长，总共不超过60字节。
    //tcp头部长度 + data长度
    debug( "ih->tot_len=%d", ntohs( ih->tot_len ) );

    ndt->skb_len = ntohs( ih->tot_len )  ;
    ndt->iphdr_len = ih->ihl << 2 ;

    conn->pcb->ipv4_src.ip = ih->saddr ;
    conn->pcb->ipv4_dst.ip = ih->daddr ;

    ndt->protocol = ih->protocol;
    //data长度
    //TCP头部选项是一个可变长的信息，这部分最多包含40字节，因为TCP头部最长60字节，（其中还包含前面20字节的固定部分）。
    switch( ih->protocol ) {
    case IPPROTO_TCP:
        tcp_input( conn );
        break;
    }
}

int ip_input( const char *data )
{

    struct iphdr *ih = ( struct iphdr * )data;
    if( ih->version == 0x6 ) {
        debug( "ipv6 pkg" );
        return 0;
    }

    nt_conn_t *conn = ( nt_conn_t * ) malloc( sizeof( nt_conn_t ) );
    conn->ndt = ( nt_skb_t * )malloc( sizeof( nt_skb_t ) );
    conn->ndt->tcp_info = ( nt_tcp_info_t * )malloc( sizeof( nt_tcp_info_t ) );
    conn->pcb = ( nt_tpcb_t * ) malloc( sizeof( nt_tpcb_t ) );
    conn->odt = ( nt_last_t * ) malloc( sizeof( nt_last_t ) );

    conn->ndt->ip_version = ih->version ;

    conn->ndt->skb = ( char * )data;

    ipv4_input( conn );
    return 0;
}
