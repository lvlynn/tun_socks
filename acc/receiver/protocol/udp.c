#include "protocol.h"
#include "rbtree.h"

 unsigned short udp_chk( const unsigned short *data, int size )
{
    unsigned long sum;
    pshead ph;
    struct iphdr *ih = ( struct iphdr * )data;
    ph.saddr = ih->saddr;
    ph.daddr = ih->daddr;
    ph.mbz = 0;
    ph.ptcl = 17;
    ph.tcpl = htons( size - ( ih->ihl << 2 ) );
    struct udphdr * udp = ( struct udphdr * )( ih + 1 );
    sum = chk( ( unsigned short * )&ph, sizeof( pshead ) );
    sum += chk( ( unsigned short * )udp, ntohs( ph.tcpl ) );
    sum -= udp->check;
    sum = ( sum >> 16 ) + ( sum & 0xffff );
    sum += ( sum >> 16 );
    return ( unsigned short )( ~sum );

}


void udp_create( nt_skb_t *skb, struct iphdr *ih, struct udphdr *uh  ){
	nt_skb_udp_t *udp;
	nt_buf_t *b;

	//初始化
	b = skb->buffer;  //
	b->last = b->start;
	b->pos = b->start;

	memset( b->start, 0, b->end - b->start );
	udp = skb->data;

	ip_create( skb, ih );

	struct udphdr * pkg_uh = ( struct udphdr *  ) b->last;

	b->last += sizeof( struct udphdr );
    pkg_uh->source  = uh->dest;
    pkg_uh->dest = uh->source;
	/* pkg_uh->source  = uh->source;
	pkg_uh->dest = uh->dest;
 */
	pkg_uh->len = htons( sizeof( struct udphdr ) + udp->data_len ) ;
	
	b->last = nt_cpymem( b->last, udp->data, udp->data_len );

    pkg_uh->check = 0;

    debug( "skb->buf_len=%d", skb->buf_len );
	pkg_uh->check     = chksum( b->start, skb->buf_len, UDP_CHK );

    debug( "checksum=%#4x", pkg_uh->check );

}

int udp_direct_server( nt_connection_t *c  )
{
    nt_buf_t *b;
	nt_skb_t *skb;
    b = c->buffer;

    struct iphdr *ih;
    struct udphdr *uh;
	nt_acc_session_t *s;
    nt_skb_udp_t *udp;

	s = c->data;
	skb = s->skb;
    udp = skb->data;

    ih = ( struct iphdr *  )b->start;
    uh = ( struct udphdr *  )( ih + 1  );

    int rfd = socket( AF_INET, SOCK_DGRAM, 0  );


    struct sockaddr_in *to;
    to = ( struct sockaddr_in * )udp->dst;
    int addr_len = sizeof(struct sockaddr);
	//udp_create( skb, ih, uh );
    
    size_t n = sendto(rfd, udp->data , udp->data_len, 0, (struct sockaddr*)to,  addr_len);

    debug( "sebdto n=%d", n );

    if( n <= 0 ){
        debug( "send fail" );
        return -1;
    }

    char buf[1600] = {0};
    struct sockaddr_in dst;
    //n = recvfrom( rfd, buf, sizeof( buf  ), 0, (struct sockaddr*) to, (socklen_t*) &addr_len);
    n = recvfrom( rfd, buf, sizeof( buf  ), 0, (struct sockaddr*) to,  &addr_len);
    debug( "recv =%d", n );


    udp->data = buf;
    udp->data_len = n;

    skb->buf_len = skb->iphdr_len + sizeof( struct udphdr  ) + udp->data_len;
    udp_create( skb, ih, uh  );

    b = skb->buffer;

    size_t size = b->last - b->start;
    write( c->fd, b->start, size );

    debug( "ret=%d, errno=%d", n , errno);
}


int udp_proxy_socks( nt_connection_t *c  )
{
    nt_buf_t *b;
	nt_skb_t *skb;
    b = c->buffer;

    struct iphdr *ih;
    struct udphdr *uh;
	nt_acc_session_t *s;
    nt_skb_udp_t *udp;
    int ret;
    int fd;
    const char path[] = "/tmp/nt.socks5.sock";
    struct sockaddr_un server_addr;
    ssize_t size ;

	s = c->data;
	skb = s->skb;
    udp = skb->data;

    ih = ( struct iphdr *  )b->start;
    uh = ( struct udphdr *  )( ih + 1  );


	//未发起过连接
      if( s->fd == 0){ 
  
          fd = socket( AF_UNIX, SOCK_STREAM, 0 );
  
          server_addr.sun_family = AF_UNIX;
          strcpy( server_addr.sun_path, path );
  
  
          if( connect( fd, ( struct sockaddr * )&server_addr, sizeof( server_addr ) ) == -1 ) { 
              debug( " connect to server fail. errno=%d", errno );
              return -1;
          }   
          s->fd = fd; 
      } else
          fd = s->fd; 

	
 //填充发送内容
      nt_udp_socks_t  us;
      us.type = 2;
      us.domain = 4;
      us.protocol = 2;
  
      us.server_ip = inet_addr( "172.16.254.157" );
      us.server_port = htons( "1080" );
  
      strcpy( us.user, "test" );
      strcpy( us.password, "test" );
  
      //源码ip打印出来不对
      struct sockaddr_in *addr;
      addr = ( struct sockaddr_in * )udp->src;
      us.sip = addr->sin_addr.s_addr;
      //tcp_socks.sip = ih->saddr;
      us.sport =  addr->sin_port;
  
  
	  addr = ( struct sockaddr_in * )udp->dst;
	  us.dip = addr->sin_addr.s_addr;
	  us.dport = addr->sin_port;

	  us.data_len =  udp->data_len ;
	  memcpy( us.data, udp->data,  us.data_len );


    int rfd = socket( AF_INET, SOCK_DGRAM, 0  );

	size = sizeof( nt_udp_socks_t ) - 1500  + us.data_len;

	//发送
	ret = send( fd, ( void * )&us, size, 0 );

	debug( " ret  = %d", ret );

	return 0;
}



int udp_init( nt_connection_t *c ){

	nt_acc_session_t *s;
    nt_skb_t *skb;

    struct iphdr *ih;
    nt_buf_t *b;
    b = c->buffer;

    ih = ( struct iphdr *  )b->start;

    u_int16_t sport ;
    nt_rbtree_node_t *node;
    sport = tcp_get_port( b->start, TCP_SRC  );

    node = rcv_conn_search( &acc_udp_tree, sport  );
    if( node != NULL ){
        s = ( nt_acc_session_t *  )node->key;
        skb = s->skb;
        skb->protocol = ih->protocol ;
        skb->skb_len = b->last - b->start;
        skb->iphdr_len = ih->ihl << 2;

        s->conn = c;
        c->data = s ;
    } else {
        s = nt_pcalloc( c->pool, sizeof( nt_acc_session_t ) );
        skb = nt_pcalloc( c->pool, sizeof( nt_skb_t  )  );

        skb->protocol = ih->protocol ;
        skb->skb_len = b->last - b->start;
        debug( "skb->skb_len=%d", skb->skb_len );
        skb->iphdr_len = ih->ihl << 2;

        skb->buffer = nt_create_temp_buf( c->pool, 1500  );

        nt_skb_udp_t *udp = nt_pcalloc( c->pool, sizeof( nt_skb_udp_t  )  );

        struct sockaddr *src = ( struct sockaddr *  ) nt_pcalloc( c->pool, sizeof( struct   sockaddr  )  );
        struct sockaddr *dst = ( struct sockaddr *  ) nt_pcalloc( c->pool, sizeof( struct   sockaddr  )  );

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = ih->saddr;
        addr.sin_port = htons( tcp_get_port( b->start, TCP_SRC   )   );
        nt_memcpy( src, ( struct sockaddr *  )&addr, sizeof( struct sockaddr  )  );
        udp->src = src;

        addr.sin_addr.s_addr = ih->daddr;
        addr.sin_port = htons( tcp_get_port( b->start, TCP_DST  )  );
        nt_memcpy( dst, ( struct sockaddr *  )&addr, sizeof( struct sockaddr  )  );
        udp->dst = dst;

        udp->data = b->start + skb->iphdr_len + sizeof( struct udphdr );
        udp->data_len = b->last - udp->data ;

        debug( "udp data len=%d", udp->data_len  );

        skb->data = udp;
        s->conn = c;
        s->skb = skb;
        c->data = s;
    }


    debug( "skb->skb_len=%d", skb->skb_len );
}

int udp_input( nt_connection_t *c ){


    nt_buf_t *b;
	nt_skb_t *skb;
    struct iphdr *ih;
    struct udphdr *uh;
	nt_acc_session_t *s;
    nt_skb_udp_t *udp;

#if 0
    udp_init( c );

    b = c->buffer;

    rc = c->data;
	skb = rc->skb;

    
    udp = skb->data;

    ih = ( struct iphdr *  )b->start;
    uh = ( struct udphdr *  )( ih + 1  );


    debug( "checksum=%#4x", uh->check );

    debug( "skb->iphdr_len=%d", skb->iphdr_len );

    debug( "skb->skb_len=%d", skb->skb_len );

//    skb->buf_len = skb->skb_len;
//    udp_create( skb, ih, uh );
    //unsigned long sum = chksum( b->start, skb->skb_len , UDP_CHK  );
    //debug( "checksum2=%#4x", sum );
    //udp_create( skb, ih, uh );
#endif 
    b = c->buffer;

    debug( "udp_input" );
    print_pkg( b->start );

    
    udp_init( c );

//    udp_direct_server(c);

    udp_proxy_socks(c);
}



int udp_output( nt_connection_t *c ){


}
