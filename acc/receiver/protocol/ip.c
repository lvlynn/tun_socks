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



int ip_create( nt_skb_t *skb , struct iphdr *ih)
{
	nt_skb_tcp_t *skb_data;
	nt_buf_t *b;
	
	skb_data = skb->data;

	switch( skb_data->phase ){
	case TCP_PHASE_SEND_SYN_ACK:
		skb->buf_len = skb->iphdr_len + skb_data->hdr_len;
		break;
	case TCP_PHASE_ACK:
	case TCP_PHASE_SEND_FIN:     
	case TCP_PHASE_SEND_FIN_ACK:
	case TCP_PHASE_SEND_PSH_ACK:
		skb->buf_len = skb->iphdr_len + 20;
		break;
	case TCP_PHASE_SEND_PSH:
	case TCP_PHASE_SEND_PSH_END:
		skb->buf_len = skb->iphdr_len + 20 +  skb_data->data_len;
		break;
	} 
    
    debug( "skb->buf_len=%d", skb->buf_len  );

	b = skb->buffer;
	b->last = b->start;
	b->pos = b->start;

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


    b = c->buffer;
    ih = ( struct iphdr * )b->start;


    //?????????tcp udp, ?????????,??????????????????????????????
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

    //?????????????????????????????????????????????????????????????????????????????? nt_rev_connection_t ??????
    //?????????IP?????????IP????????????
    //??????????????? nt_rev_connection_t ??????????????????
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

    } else {
        debug( "node not in tree" );
        nrc = nt_palloc( c->pool, sizeof( nt_rev_connection_t ) );

        //??????????????????skb
        skb = nt_palloc( c->pool, sizeof( nt_skb_t ) );

        skb->protocol = ih->protocol ;
        skb->skb_len = b->last - b->start;
        skb->iphdr_len = ih->ihl << 2;

        if( ih->protocol == IPPROTO_TCP ) {
            nt_skb_tcp_t *skb_tcp = nt_palloc( c->pool, sizeof( nt_skb_t ) );
            skb_tcp->src = ( struct sockaddr * ) nt_palloc( c->pool, sizeof( struct sockaddr ) );
            skb_tcp->dst = ( struct sockaddr * ) nt_palloc( c->pool, sizeof( struct sockaddr ) );

            skb->data = skb_tcp;

            debug( "src = %d.%d.%d.%d, dst = %d.%d.%d.%d",
                   IP4_STR( ih->saddr ),
                   IP4_STR( ih->daddr ) );

            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = ih->saddr;
            addr.sin_port = htons(sport);
            nt_memcpy( skb_tcp->src, ( struct sockaddr * )&addr, sizeof( struct sockaddr ) );

            addr.sin_addr.s_addr = ih->daddr;
            addr.sin_port = htons(tcp_get_port( b->start, TCP_DST ));
            nt_memcpy( skb_tcp->dst, ( struct sockaddr * )&addr, sizeof( struct sockaddr ) );


            struct sockaddr_in *addr2;
            addr2 = ( struct sockaddr_in * )skb_tcp->src;
            debug( "src = %d.%d.%d.%d:%d", IP4_STR( addr2->sin_addr.s_addr ), ntohs( addr2->sin_port  ) );


            addr2 = ( struct sockaddr_in * )skb_tcp->dst;
            debug( "dst = %d.%d.%d.%d:%d", IP4_STR( addr2->sin_addr.s_addr ), ntohs( addr2->sin_port  ) );


            //????????????????????????buf
            skb->buffer = nt_create_temp_buf( c->pool, 1500 );

        }

        nrc->port = sport;
        nrc->conn = c;
        nrc->skb = skb;

        c->data = nrc ;
        //?????????nrc??????????????????;
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

    //??????ipv4 ??????ipv6
    struct iphdr *ih = ( struct iphdr * )b->start;

    if( ih->version == 0x6 ) {
        debug( "ipv6 pkg" );
        return ;
    }

    ipv4_input( c );

    return  0;
    /*
    conn->local_sockaddr = ih->saddr ;  //???ip


    struct sockaddr_in tmp_addr;
    tmp_addr.sin_family = AF_INET;
    tmp_addr.sin_addr.s_addr = ih->saddr;
    conn->sockaddr = (struct sockaddr *)&tmp_addr ;  //???ip

    debug( "src = %d.%d.%d.%d, dst = %d.%d.%d.%d",
           IP4_STR( ih->saddr  ),
           IP4_STR( ih->daddr  ) );

    //????????????????????????????????????

    //?????????????????????tcp??? udp???icmp
    //tcp ??? ??????3???????????????
    //udp ,  ????????????
    switch( ih->protocol  ) {
        case IPPROTO_TCP:
           // tcp_input( conn  );
            break;
    }
    */


}
