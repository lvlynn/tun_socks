#include "protocol.h"
#include "rbtree.h"

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
    case IPPROTO_UDP:
        //udp_print( data, pkg );
        break;
    }
}



int ip_create( nt_skb_t *skb , struct iphdr *ih)
{
	nt_skb_tcp_t *tcp;
	nt_buf_t *b;
	
	tcp = skb->data;

	switch( tcp->phase ){
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
    
    debug( "skb->buf_len=%d", skb->buf_len  );

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
	pkg_ih->check = 0;
	pkg_ih->check = chksum( b->start, sizeof( struct iphdr ), IP_CHK );
	return 0;
}



static int ipv4_input( nt_connection_t *c )
{
    debug( "ipv4 pkg" );
    nt_buf_t *b;
    struct iphdr *ih;
    u_int16_t sport ;
    nt_rbtree_node_t *node;
    nt_rev_connection_t *nrc;
    nt_skb_t *skb;


    b = c->buffer;  //存放read进来的数据包
    ih = ( struct iphdr * )b->start;


    //如果是tcp udp, 取端口,用于查询是否是新链接
    switch( ih->protocol ) {
    case IPPROTO_TCP:
        sport = tcp_get_port( b->start, TCP_SRC );
        break;
    case IPPROTO_UDP:
        sport = tcp_get_port( b->start, TCP_SRC );
        break;
    }


    debug( "sport=%d", sport );

    c->type = ih->protocol;

    //查询该连接是否已经在红黑树中，如果没有就添加一个新的 nt_rev_connection_t 对象
    //并把源IP，目的IP存入其中
    //把这个新的 nt_rev_connection_t 添加到红黑树
    node = rcv_conn_search( &tcp_udp_tree, sport );
    if( node != NULL ) {
        debug( "node in tree" );
       nrc = ( nt_rev_connection_t *)node->key;
       skb = nrc->skb;
       skb->protocol = ih->protocol ;
       skb->skb_len = b->last - b->start;
       skb->iphdr_len = ih->ihl << 2;

       c->data = nrc ; 
       nrc->conn = c;

       nt_skb_tcp_t *skb_tcp = skb->data;

       struct sockaddr_in *addr2;
       addr2 = ( struct sockaddr_in * )skb_tcp->src;
       debug( "src = %d.%d.%d.%d:%d", IP4_STR( addr2->sin_addr.s_addr ), ntohs( addr2->sin_port  ) );

       addr2 = ( struct sockaddr_in * )skb_tcp->dst;
       debug( "dst = %d.%d.%d.%d:%d", IP4_STR( addr2->sin_addr.s_addr ), ntohs( addr2->sin_port  ) );

    } else {
        debug( "node not in tree" );
        nrc = nt_pcalloc( c->pool, sizeof( nt_rev_connection_t ) );

        //生成一个新的skb
        skb = nt_pcalloc( c->pool, sizeof( nt_skb_t ) );

        skb->protocol = ih->protocol ;
        skb->skb_len = b->last - b->start;
        skb->iphdr_len = ih->ihl << 2;

        //申请一个回复用的buf
        skb->buffer = nt_create_temp_buf( c->pool, 1500 );


        if( ih->protocol == IPPROTO_TCP ) {
            nt_skb_tcp_t *skb_tcp = nt_pcalloc( c->pool, sizeof( nt_skb_tcp_t ) );


            debug( "skb_tcp=%p", skb_tcp );

            struct sockaddr *src = ( struct sockaddr *  ) nt_pcalloc( c->pool, sizeof( struct sockaddr  )  );
            struct sockaddr *dst = ( struct sockaddr *  ) nt_pcalloc( c->pool, sizeof( struct sockaddr  )  );
        
            debug( "src=%p, dst=%p", src, dst );


            skb_tcp->data = NULL;
            skb_tcp->data_len = 0;

            skb_tcp->payload_len = 0;
            skb_tcp->seq = ntohl(1);
            skb_tcp->ack_seq = ntohl(1);


            skb->data = skb_tcp;

            debug( "src = %d.%d.%d.%d, dst = %d.%d.%d.%d",
                   IP4_STR( ih->saddr ),
                   IP4_STR( ih->daddr ) );

            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = ih->saddr;
            addr.sin_port = htons(sport);
            nt_memcpy( src, ( struct sockaddr * )&addr, sizeof( struct sockaddr ) );
             skb_tcp->src = src;


            addr.sin_addr.s_addr = ih->daddr;
            addr.sin_port = htons(tcp_get_port( b->start, TCP_DST ));
            nt_memcpy( dst, ( struct sockaddr * )&addr, sizeof( struct sockaddr ) );
            skb_tcp->dst = dst;

            struct sockaddr_in *addr2;
            addr2 = ( struct sockaddr_in * )skb_tcp->src;
            debug( "src = %d.%d.%d.%d:%d", IP4_STR( addr2->sin_addr.s_addr ), ntohs( addr2->sin_port  ) );


            addr2 = ( struct sockaddr_in * )skb_tcp->dst;
            debug( "dst = %d.%d.%d.%d:%d", IP4_STR( addr2->sin_addr.s_addr ), ntohs( addr2->sin_port  ) );



        }

        nrc->port = sport;
        nrc->conn = c;
        nrc->skb = skb;

        c->data = nrc ;
        //把新的nrc添加到红黑树;
        rcv_conn_add( &tcp_udp_tree, c );
    }


    switch( ih->protocol ) {
    case IPPROTO_TCP:
        tcp_input( c );
        break;
    case IPPROTO_UDP:
        break;
    }

    return NT_OK;
}

int ip_input( nt_connection_t *c )
{

    nt_buf_t *b;
    b = c->buffer;

    //判断ipv4 还是ipv6
    struct iphdr *ih = ( struct iphdr * )b->start;

    if( ih->version == 0x6 ) {
        debug( "ipv6 pkg" );
        return ;
    }

    ipv4_input( c );

    return  0;
}
