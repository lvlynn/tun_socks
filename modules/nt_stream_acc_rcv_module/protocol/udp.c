#include <nt_core.h>
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


void udp_create( nt_skb_t *skb, struct iphdr *ih, struct udphdr *uh )
{
    nt_skb_udp_t *udp;
    nt_buf_t *b;

    //初始化
    b = skb->buffer;  //
    b->last = b->start;
    b->pos = b->start;

    memset( b->start, 0, b->end - b->start );
    udp = skb->data;

    ip_create( skb, ih );

    struct udphdr * pkg_uh = ( struct udphdr * ) b->last;

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

int udp_direct_server( nt_connection_t *c )
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

    ih = ( struct iphdr * )b->start;
    uh = ( struct udphdr * )( ih + 1 );

    int rfd = socket( AF_INET, SOCK_DGRAM, 0 );


    struct sockaddr_in *to;
    to = ( struct sockaddr_in * )udp->dst;
    int addr_len = sizeof( struct sockaddr );
    //udp_create( skb, ih, uh );

    size_t n = sendto( rfd, udp->data, udp->data_len, 0, ( struct sockaddr* )to,  addr_len );

    debug( "sebdto n=%d", n );

    if( n <= 0 ) {
        debug( "send fail" );
        return -1;
    }

    char buf[1600] = {0};
    struct sockaddr_in dst;
    //n = recvfrom( rfd, buf, sizeof( buf  ), 0, (struct sockaddr*) to, (socklen_t*) &addr_len);
    n = recvfrom( rfd, buf, sizeof( buf ), 0, ( struct sockaddr* ) to,  &addr_len );
    debug( "recv =%d", n );


    udp->data = buf;
    udp->data_len = n;

    skb->buf_len = skb->iphdr_len + sizeof( struct udphdr ) + udp->data_len;
    udp_create( skb, ih, uh );

    b = skb->buffer;

    size_t size = b->last - b->start;
    write( c->fd, b->start, size );

    debug( "ret=%d, errno=%d", n, errno );
}


int udp_proxy_socks( nt_connection_t *c )
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
    int fist_connect = 0;

    s = c->data;
    skb = s->skb;
    udp = skb->data;

    ih = ( struct iphdr * )b->start;
    uh = ( struct udphdr * )( ih + 1 );


    //未发起过连接
    if( s->fd == 0 ) {

        fd = socket( AF_UNIX, SOCK_STREAM, 0 );

        server_addr.sun_family = AF_UNIX;
        strcpy( server_addr.sun_path, path );


        if( connect( fd, ( struct sockaddr * )&server_addr, sizeof( server_addr ) ) == -1 ) {
            debug( " connect to server fail. errno=%d", errno );
            return -1;
        }
        s->fd = fd;
        fist_connect = 1;
    } else
        fd = s->fd;


    //先发送连接信息，再发送data内容

    struct iovec iov[2];
    nt_acc_socks5_auth_t asa;
    if( fist_connect ) {
        //填充连接信息
        asa.type = 1;     //发起连接
        asa.domain = 4;   //ipv4
        asa.protocol = 2; // 1 tcp


        #if 0
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
        addr = ( struct sockaddr_in * )udp->src;
        asa.sip = addr->sin_addr.s_addr;
        asa.sport =  addr->sin_port;

        addr = ( struct sockaddr_in * )udp->dst;
        asa.dip = addr->sin_addr.s_addr;
        asa.dport = addr->sin_port;

        iov[0].iov_base = &asa;
        iov[0].iov_len = sizeof( nt_acc_socks5_auth_t );

    }

    //发送
    if( fist_connect ) {
        debug( " fist_connect " );
        iov[1].iov_base = udp->data;
        iov[1].iov_len = udp->data_len;
        debug( " data_len  = %d",  udp->data_len );
        //首次认证和data一起发送
        //不然nginx处理可能有问题
        ret =  writev( fd, iov, 2 );
    } else {
        debug( " no fist_connect " );

        char nil[10] = {0};
        iov[0].iov_base = nil;
        iov[0].iov_len = 10;

        iov[1].iov_base = udp->data;
        iov[1].iov_len = udp->data_len;
        debug( " data_len  = %d",  udp->data_len );
        //首次认证和data一起发送
        //不然nginx处理可能有问题
        ret =  writev( fd, iov, 2 );
        //  ret = send( fd, ( void * )udp->data, udp->data_len, 0 );
    }

    debug( " ret  = %d", ret );
    if( ret < 0 )
        return TCP_PHASE_NULL;

    return TCP_PHASE_NULL;


}



int udp_init( nt_connection_t *c )
{

    nt_acc_session_t *s;
    nt_skb_t *skb;

    struct iphdr *ih;
    nt_buf_t *b;
    b = c->buffer;

    ih = ( struct iphdr * )b->start;

    u_int16_t sport ;
    nt_rbtree_node_t *node;

    debug( "IP_MF=%d", ntohs( ih->frag_off )& IP_MF );

    debug( "IP_off=%d", ntohs( ih->frag_off )& IP_OFFMASK );

    //如果是第一个包，ip的分片偏移为0;
    //判断是否为第一个包，是的话，可以取到端口
    if( ( ntohs( ih->frag_off )& IP_OFFMASK ) == 0 ) {
        debug( "first data" );
        //是第一个包
        sport = tcp_get_port( b->start, TCP_SRC );
        node = rcv_conn_search( &acc_udp_tree, sport );

    } else {
        debug( "no first data" );
        //不是第一个包
        //取不到端口,从红黑树中获取信息
        node = rcv_conn_search( &acc_udp_id_tree, ih->id );
        if( node == NULL ) {

            debug( "node NULL" );
            return NT_ERROR;
        }
    }

    if( node != NULL ) {
        s = ( nt_acc_session_t * )node->key;
        skb = s->skb;
        skb->protocol = ih->protocol ;
        skb->skb_len = b->last - b->start;
        skb->iphdr_len = ih->ihl << 2;

        nt_skb_udp_t *udp = skb->data;

        if( ( ntohs( ih->frag_off )& IP_OFFMASK ) == 0 ) {
            udp->data = b->start + skb->iphdr_len + sizeof( struct udphdr );
        } else
            udp->data = b->start + skb->iphdr_len ;

        udp->data_len = b->last - udp->data ;
        udp->ip_id = ih->id;

        debug( "size=%d", udp->data_len );

        s->conn = c;
        c->data = s ;
    } else {
        debug( "first connect" );
        s = nt_pcalloc( c->pool, sizeof( nt_acc_session_t ) );
        skb = nt_pcalloc( c->pool, sizeof( nt_skb_t ) );

        skb->protocol = ih->protocol ;
        skb->skb_len = b->last - b->start;
        skb->iphdr_len = ih->ihl << 2;

        skb->buffer = nt_create_temp_buf( c->pool, 1500 );

        nt_skb_udp_t *udp = nt_pcalloc( c->pool, sizeof( nt_skb_udp_t ) );

        struct sockaddr *src = ( struct sockaddr * ) nt_pcalloc( c->pool, sizeof( struct   sockaddr ) );
        struct sockaddr *dst = ( struct sockaddr * ) nt_pcalloc( c->pool, sizeof( struct   sockaddr ) );

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = ih->saddr;
        addr.sin_port = htons( tcp_get_port( b->start, TCP_SRC ) );
        nt_memcpy( src, ( struct sockaddr * )&addr, sizeof( struct sockaddr ) );
        udp->src = src;

        addr.sin_addr.s_addr = ih->daddr;
        addr.sin_port = htons( tcp_get_port( b->start, TCP_DST ) );
        nt_memcpy( dst, ( struct sockaddr * )&addr, sizeof( struct sockaddr ) );
        udp->dst = dst;

        udp->data = b->start + skb->iphdr_len + sizeof( struct udphdr );
        udp->data_len = b->last - udp->data ;

        debug( "udp data len=%d", udp->data_len );
        udp->ip_id = ih->id;

        skb->data = udp;
        s->port = sport;
        s->conn = c;
        s->skb = skb;
        s->fd = 0;

        c->data = s ;

        //把新的s添加到红黑树;
        nt_rbtree_node_t *node = ( nt_rbtree_node_t * ) nt_palloc( c->pool, sizeof( nt_rbtree_node_t ) );
        rcv_conn_add( &acc_udp_tree, s, node );

    }

    //如果第一个包有分片需要添加到红黑树，以便后面的数据包可以找到该信息
    debug( "MF=%d", ( ntohs( ih->frag_off ) & IP_MF ) );
    if( ntohs( ih->frag_off )& IP_MF ) {
        debug( "第一个数据包被分片" );
        rcv_conn_add( &acc_udp_id_tree, s, node );
    }


    debug( "skb->skb_len=%d", skb->skb_len );
}

int udp_input( nt_connection_t *c )
{


    nt_buf_t *b;
    nt_skb_t *skb;
    struct iphdr *ih;
    struct udphdr *uh;
    nt_acc_session_t *s;
    nt_skb_udp_t *udp;
    nt_int_t ret;

    #if 0
    udp_init( c );

    b = c->buffer;

    rc = c->data;
    skb = rc->skb;


    udp = skb->data;

    ih = ( struct iphdr * )b->start;
    uh = ( struct udphdr * )( ih + 1 );


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


    ret = udp_init( c );
    if( ret != NT_OK ) {
        return;
    }

//    udp_direct_server(c);

    udp_proxy_socks( c );
}



int udp_output( nt_connection_t *c )
{


}


void acc_udp_create( nt_buf_t *b, nt_skb_udp_t *udp )
{
    //初始化
    acc_udp_ip_create( b, udp );

    struct udphdr * pkg_uh = ( struct udphdr * )( b->start + 20 );

    b->last += sizeof( struct udphdr );
    pkg_uh->source  = udp->dport;
    pkg_uh->dest = udp->sport;

    pkg_uh->len = htons( sizeof( struct udphdr ) + udp->data_len ) ;

    pkg_uh->check = 0;

    pkg_uh->check     = chksum( b->start, udp->tot_len, UDP_CHK );

    debug( "checksum=%#4x", pkg_uh->check );
}



nt_connection_t* acc_udp_input( char *data )
{
    struct iphdr *ih;
    struct udphdr *uh;
    u_int16_t sport ;
    nt_rbtree_node_t *node;
    nt_acc_session_t *s;
    nt_skb_t *skb;

    nt_connection_t *c;

    ih = ( struct iphdr * )data;

    debug( "IP_MF=%d", ntohs( ih->frag_off )& IP_MF );

    debug( "IP_off=%d", ntohs( ih->frag_off )& IP_OFFMASK );

    //如果是第一个包，ip的分片偏移为0;
    //判断是否为第一个包，是的话，可以取到端口
    if( ( ntohs( ih->frag_off )& IP_OFFMASK ) == 0 ) {
        debug( "first data" );
        //是第一个包
        sport = tcp_get_port( data, TCP_SRC );
        node = rcv_conn_search( &acc_udp_tree, sport );

    } else {
        debug( "no first data" );
        //不是第一个包
        //取不到端口,从红黑树中获取信息
        node = rcv_conn_search( &acc_udp_id_tree, ih->id );
    }

    if( node ) {
        c = ( nt_acc_session_t * )node->key;
        return c;
    } else
        return NULL;
}


void nt_udp_data_parse( nt_acc_session_t *s )
{
    nt_buf_t *b;
    nt_buf_t *new;
    /* nt_acc_session_t *s; */
    nt_skb_udp_t *udp;
    nt_connection_t *c;

    c = s->connection ;
    b = c->buffer ;
    udp = s->skb->data;

    if( udp->phase == UDP_PHASE_SOCKS_CONNECT ){
        //udp 连接未建立， 数据需要放在 buf 链表里
        size_t n = b->last - b->start;
        debug( "buf len =%d", n );
        new = nt_create_temp_buf( c->pool, n );
        new->last = nt_cpymem( new->start, b->start, n );

        nt_skb_buf_t *buffer_chain ;
        nt_skb_buf_t *new_chain ;
        new_chain = nt_palloc( c->pool, sizeof( nt_skb_buf_t ) );

        new_chain->b = new;
        new_chain->next = NULL;
        debug( "head=%p", s->skb->buffer_chain );
        s->skb->chain_tail->next = new_chain;
        s->skb->chain_tail = new_chain;
        debug( "head=%p", s->skb->buffer_chain );
        debug( "head=%p", s->skb->chain_tail );
    }

    struct iphdr *ih;
    ih = ( struct iphdr * )b->start;

    s->skb->skb_len = b->last - b->start;
    s->skb->iphdr_len = ih->ihl << 2;

    if( ( ntohs( ih->frag_off )& IP_OFFMASK ) == 0 )
        udp->hdr_len = s->skb->iphdr_len + sizeof( struct udphdr );
    else
        udp->hdr_len = s->skb->iphdr_len;

    debug( " udp buf=%s", b->start + udp->hdr_len );
    //如果第一个包有分片需要添加到红黑树，以便后面的数据包可以找到该信息
    debug( "MF=%d", ( ntohs( ih->frag_off ) & IP_MF ) );
    if( ntohs( ih->frag_off )& IP_MF ) {
        debug( "第一个数据包被分片" );
        rcv_conn_add( &acc_udp_id_tree, c, s->node );
    }

}

/* udp 数据分发 */
void nt_udp_data_dispatch_handler( nt_event_t *ev )
{

    nt_connection_t *c;
    nt_skb_udp_t *udp;
    nt_acc_session_t *s;
    c = ev->data;
    s = c->data;
    udp = s->skb->data ;

    switch( udp->phase ) {
    case UDP_PHASE_SOCKS_CONNECT:
        //udp 上游还在连接，不处理
        nt_udp_data_parse( s );
        break;
    case UDP_PHASE_DATA_FORWARD:
        //可正常通信
        nt_udp_data_parse( s );
        nt_tun_udp_data_send( s->connection );
        break;
    case UDP_PHASE_SOCKS_DOWN:
        //上游已经断线
        break;
    }

}

/*
 * udp 连接的首个数据包
 * */
void nt_tun_udp_accept_new( nt_connection_t *c )
{
    debug( "start" );
    nt_buf_t *b;
    struct iphdr *ih;
    struct udphdr *uh;
    nt_acc_session_t *s;
    nt_skb_t *skb;
    nt_skb_udp_t *udp;


    s = c->data;

    if( s->skb == NULL ) {
        b = c->buffer ;
        ih = ( struct iphdr * ) b->start;

        //该数据包没有 udp头
        if( ( ntohs( ih->frag_off )& IP_OFFMASK ) != 0 ) {
            return ;
        }


        uh = ( struct udphdr * )( ih + 1 );

        //申请一个skb
        skb = nt_palloc( c->pool, sizeof( nt_skb_t ) );
        s->skb = skb;
        skb->protocol = ih->protocol ;
        skb->skb_len = b->last - b->start;
        skb->iphdr_len = ih->ihl << 2;
        skb->buffer = nt_create_temp_buf( c->pool, 1500 );
        skb->buffer_chain = NULL;

        udp = ( nt_skb_udp_t * ) nt_pcalloc( c->pool, sizeof( nt_skb_udp_t ) );

        skb->data = udp;
        udp->ip_id = ih->id;

        struct sockaddr *src = ( struct sockaddr * ) nt_pcalloc( c->pool, sizeof( struct sockaddr ) );
        struct sockaddr *dst = ( struct sockaddr * ) nt_pcalloc( c->pool, sizeof( struct sockaddr ) );
        udp->src = src;
        udp->dst = dst;

        udp->sip = ih->saddr;
        udp->dip = ih->daddr;

        udp->sport = uh->source ;
        udp->dport = uh->dest ;

        //包含ip头 和 udp头
        udp->hdr_len = skb->iphdr_len + sizeof( struct udphdr );

        debug( " accept udp buf=%s", b->start + udp->hdr_len );

        udp->phase = UDP_PHASE_SOCKS_CONNECT;
        /* udp->data = b->start + skb->iphdr_len + sizeof( struct udphdr ); */
        /* udp->data_len = b->last - udp->data ; */

        nt_skb_buf_t *buffer_chain = nt_palloc( c->pool, sizeof( nt_skb_buf_t ) );
        buffer_chain->b = b;
        buffer_chain->next = NULL;
        s->skb->buffer_chain = buffer_chain;
        s->skb->chain_tail = buffer_chain;

        struct sockaddr_in *addr ;
        addr = ( struct sockaddr_in * ) src;
        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = ih->saddr;
        addr->sin_port = uh->source;

        addr = ( struct sockaddr_in * ) dst;
        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = ih->daddr;
        addr->sin_port = uh->dest;

        s->port = uh->source;
        s->fd = c->fd;
        debug( " udp ip=%s:%d", nt_inet_ntoa( udp->dip ), ntohs( udp->dport ) );
        c->read->handler = nt_udp_data_dispatch_handler;

        nt_rbtree_node_t *node = ( nt_rbtree_node_t * ) nt_palloc( c->pool, sizeof( nt_rbtree_node_t ) );
        s->node = node ;

        rcv_conn_add( &acc_udp_tree, c, node );
        /* nt_connection_rbtree_add( c ); */

        //如果第一个包有分片需要添加到红黑树，以便后面的数据包可以找到该信息
        debug( "MF=%d", ( ntohs( ih->frag_off ) & IP_MF ) );
        if( ntohs( ih->frag_off )& IP_MF ) {
            debug( "第一个数据包被分片" );
            rcv_conn_add( &acc_udp_id_tree, c, node );
        }

    }

    //udp 不同于tcp , 第一个数据包就需要建立连接并转发
    nt_tun_upstream_connect_handler( c->read );
}


ssize_t
nt_unix_writev( nt_connection_t *c, struct iovec *iov, size_t size )
{
    ssize_t       n;
    nt_err_t     err;
    nt_event_t  *wev;

    wev = c->write;

    for( ;; ) {
        n = writev( c->fd, iov, size );

        nt_log_debug3( NT_LOG_DEBUG_EVENT, c->log, 0,
                       "send: fd:%d %z of %uz", c->fd, n, size );

        if( n > 0 ) {
            if( n < ( ssize_t ) size ) {
                wev->ready = 0;
            }

            c->sent += n;

            return n;
        }

        err = nt_socket_errno;

        if( n == 0 ) {
            nt_log_error( NT_LOG_ALERT, c->log, err, "send() returned zero" );
            wev->ready = 0;
            return n;
        }

        if( err == NT_EAGAIN || err == NT_EINTR ) {
            wev->ready = 0;

            nt_log_debug0( NT_LOG_DEBUG_EVENT, c->log, err,
                           "send() not ready" );

            if( err == NT_EAGAIN ) {
                return NT_AGAIN;
            }

        } else {
            wev->error = 1;
            ( void ) nt_connection_error( c, err, "send() failed" );
            return NT_ERROR;
        }
    }
}


//发起udp socks连接
nt_int_t nt_tun_udp_upstream_connect( nt_connection_t *c )
{
    nt_int_t ret;
    nt_acc_session_t *s;
    nt_connection_t *tcp_conn;
    nt_connection_t *udp_conn ;
    nt_event_t  *rev, *wev;
    s = c->data;


    int fd = socket( AF_INET, SOCK_DGRAM, 0 );

    nt_buf_t *b;
    tcp_conn = s->ss->tcp_conn ;
    b = tcp_conn->buffer;

    uint32_t udp_proxy_ip =  *( in_addr_t * )( b->start + 4 );
    uint16_t udp_proxy_port = *( short * )( b->start + 8 ) ;

    debug( " udp ip=%s:%d", nt_inet_ntoa( udp_proxy_ip ), ntohs( udp_proxy_port ) );

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = udp_proxy_ip;
    addr.sin_port =  udp_proxy_port;

    debug( "init new socks5 udp connection fd:%d", fd );
    ret = connect( fd, ( struct sockaddr * )&addr, sizeof( struct sockaddr ) );
    if( ret == -1 ) {
        debug( "udp connect fail" );
        return NT_ERROR;
    }


    //生成一个新的connection
    udp_conn = nt_get_connection( fd, c->log );
    if( udp_conn == NULL ) {
        return NT_ERROR;
    }

    /* c->fd = fd; */
    udp_conn->data = s;
    s->ss->udp_conn = udp_conn;
    udp_conn->pool = c->pool;


    udp_conn->type = IPPROTO_TCP;

    udp_conn->log = c->log;
    udp_conn->buffer = nt_create_temp_buf( c->pool, 1500 );
    /* udp_conn->buffer = nt_create_temp_buf( c->pool, 8192 ); */

    if( udp_conn->buffer == NULL )
        return NT_ERROR;

    //设置连接读写回调
    udp_conn->recv = nt_udp_recv;
    udp_conn->send = nt_udp_send;
    /* udp_conn->send = nt_unix_writev; */
    //  c->send_chain = nt_send_chain;

    rev = udp_conn->read;
    wev = udp_conn->write;

    rev->active = 0;
    rev->ready = 0;
    wev->ready = 1;

    rev->log = c->log;
    wev->log = c->log;

    udp_conn->read->handler = nt_upstream_udp_read_handler;
    udp_conn->write->handler = nt_tun_upsteam_timeout_handler;

    nt_add_timer( udp_conn->write, 60 * 1000 );
    nt_add_event( udp_conn->read, NT_READ_EVENT,  0 );

    return NT_OK;
}


