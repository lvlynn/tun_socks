#include "rbtree.h"
#include "tun.h"
#include "protocol.h"
#include "nt_event.h"


nt_init_connection( nt_connection_t *c, nt_log_t *log )
{

    nt_event_t       *rev, *wev;

    rev = c->read;
    wev = c->write;
    nt_memzero( c, sizeof( nt_connection_t ) );

    c->read = rev;
    c->write = wev;

    c->fd = 0 ;
    c->log = log;

    nt_memzero( rev, sizeof( nt_event_t ) );
    nt_memzero( wev, sizeof( nt_event_t ) );

    rev->index = NT_INVALID_INDEX;
    wev->index = NT_INVALID_INDEX;

    rev->data = c;
    wev->data = c;

    rev->log = log;
    wev->log = log;


    wev->write = 1;

    c->recv = nt_unix_read;
    c->send = nt_unix_write;
}


void do_write()
{
}


//u_char *buffer ;

void test_read( nt_event_t *ev )
{
   // printf( "test_read\n" );


    nt_connection_t *c ;
    nt_buf_t   *b;
    nt_buf_t   buf ;
    size_t  size;
    ssize_t n;

    c = ev->data;

    b = c->buffer;

    b->last = b->start;
    b->pos = b->start;

    for( ;; ) {
        size = b->end - b->last;
   //     printf( "size=%d\n", size );

        if( size == 0 )
            break;

//        n = read( c->fd, b->last, size );
        n = c->recv( c, b->last, size );
        debug( "n=%d\n", n );

        // 如果不可读，或者已经读完
        // break结束for循环
        if( n == NT_AGAIN ) {
            break;
        }

        // 出错，标记为eof
        if( n == NT_ERROR ) {
            //                    src->write->eof = 1;
            //                    n = 0;
            break;
        }

        if( n >= 0 ) {
            // 缓冲区的pos指针移动，表示发送了n字节新数据
            b->last += n;
            break;
        }

        break;
    }

    nt_log_debug1( NT_LOG_DEBUG_EVENT, c->log, 0,
                   "receive=%d", b->last - b->start );



    /* if( nt_handle_read_event( ev, NT_CLOSE_EVENT ) != NT_OK ) {
        printf( "[%s] read event NT_ERROR\n", __func__ );
    } else
        printf( "[%s] read event NT_OK\n", __func__ ); */

    /*
        c->write->active = 0;
        c->write->ready = 0;
        if( nt_handle_write_event( c->write, 0 ) != NT_OK ) {
            printf( "[%s] write event NT_ERROR\n", __func__ );
        } else
            printf( "[%s] write event NT_OK\n", __func__ );
    */

}

void test_write( nt_event_t *ev )
{
    printf( "test_write\n" );

    nt_connection_t *c ;
    nt_buf_t   *b;
    nt_buf_t   buf ;
    size_t  size;
    ssize_t n;

    c = ev->data;


    b = c->buffer;

    char msg[128] = "hello world\n";
    b->last = nt_copy( b->start, msg, 13 );

    for( ;; ) {
        size = b->last - b->pos;

        if( !size )
            break;
        n = c->send( c, b->pos, size );

        // 如果不可读，或者已经读完
        // break结束for循环
        if( n == NT_AGAIN ) {
            break;
        }

        // 出错，标记为eof
        if( n == NT_ERROR ) {
            //                    src->write->eof = 1;
            //                    n = 0;
            break;
        }

        if( n >= 0 ) {
            // 缓冲区的pos指针移动，表示发送了n字节新数据
            b->pos += n;
            continue;
        }

        break;
    }


    if( nt_handle_write_event( ev, 0 ) != NT_OK ) {
        printf( "[%s] write event NT_ERROR\n", __func__ );
    } else
        printf( "[%s] write event NT_OK\n", __func__ );


    c->read->ready = 1;
    if( nt_handle_read_event( c->read, NT_READ_EVENT ) != NT_OK ) {
        printf( "[%s] read event NT_ERROR\n", __func__ );
    } else
        printf( "[%s] read event NT_OK\n", __func__ );

}

void listen_fd_hander( nt_event_t *ev )
{
    nt_connection_t *conn ;
    nt_buf_t *b;
    conn = ev->data;
    b = conn->buffer;

    //连接进入，解析内容
//    debug( "listen_fd_hander\n" );
    test_read( ev );

    ssize_t size = b->last - b->start ;

//   debug( "size=%d", size );
    if( size <= 0 )
        return;

    ip_input( conn );    
    //  protocol_parse(  );
    //  nt_add_event( ev, NT_READ_EVENT, NT_LEVEL_EVENT );

}





int main()
{

//    signal( SIGUSR1, do_write );
//     signal( SIGPIPE, NULL  );

     struct sigaction sa;
     sa.sa_handler = SIG_IGN;
     sigaction( SIGPIPE, &sa, 0  );

     nt_log_t *log ;
    nt_pool_t *pool;
    nt_cycle_t *cycle;
    int rcv_fd;

    nt_time_init();
    log = nt_log_init( NULL );
    log->log_level = NT_LOG_DEBUG_ALL;

    nt_log_debug1( NT_LOG_DEBUG_EVENT, log, 0,
                   "timer delta: %M", nt_current_msec );



    //初始化一个内存池
    pool = nt_create_pool( NT_CYCLE_POOL_SIZE, log );
    if( pool == NULL ) {
        return NULL;
    }
    pool->log = log;

    //从内存池中申请cycle
    cycle = nt_pcalloc( pool, sizeof( nt_cycle_t ) );
    if( cycle == NULL ) {
        nt_destroy_pool( pool );
        return NULL;
    }

    //创建接收器的fd
    rcv_fd = tun_init();
    if( rcv_fd < 0 )
        return -1;

    cycle->log = log;
    
    printf( "rcv_fd = %d\n", rcv_fd );

    // event 初始化
    nt_event_init( cycle );

    //这里会初始化连接池
    nt_event_process_init( cycle );

    nt_socket_t s = rcv_fd ;
    nt_event_t *ev ;

    //    cycle->connections[0];
    nt_connection_t  *c_rcv ;
    //取连接池中的第一个连接作为 tun fd的监听连接
    c_rcv = &cycle->connections[0];
    nt_init_connection( c_rcv, log );

    c_rcv->pool = pool;
    //c_rcv->read->handler = listen_fd_hander;
    c_rcv->read->handler = listen_fd_hander;
    c_rcv->write->handler = NULL;
    c_rcv->fd = rcv_fd;

    /*    ev = ( nt_event_t * ) malloc( sizeof( nt_event_t ) );
        ev->data = ( void * )c_rcv;

        ev->handler = listen_fd_hander;
        ev->index = NT_INVALID_INDEX ;
        ev->write = 0;
        ev->log = log;
    */
    c_rcv->buffer = nt_pnalloc( pool, sizeof( nt_buf_t ) );
    //udp 不能超过1500 MTU 否则nginx 构造出来会超过1500 而被丢弃
    u_char *p  = nt_pnalloc( pool, 1490 );
    c_rcv->buffer->start = p;
    c_rcv->buffer->end = p + 1490;
    c_rcv->buffer->pos = p;
    c_rcv->buffer->last = p;



    c_rcv->read->ready = 1;
//   int ret =  nt_add_event( c_rcv->read,  NT_READ_EVENT,  0 );
    int ret =  nt_add_event( c_rcv->read,  NT_READ_EVENT,  0 );
    printf( "ret = %d\n", ret );


    //初始化接收器用的红黑树
    nt_rbtree_init( &acc_tcp_tree, &g_sentinel, nt_rbtree_insert_conn_handle );
    nt_rbtree_init( &acc_udp_tree, &g_sentinel, nt_rbtree_insert_conn_handle );
    nt_rbtree_init( &acc_udp_id_tree, &g_sentinel, nt_rbtree_insert_udp_id_handle );

    for( ;; ) {

        debug("-----------------for-------------------");
        nt_msec_t  timer, delta;
        nt_uint_t  flags;

        timer = nt_event_find_timer();
        flags = NT_UPDATE_TIME;

        delta = nt_current_msec;
    //    printf( "timer = %d\n", timer );
        nt_event_process( cycle, timer, flags );

        //统计本次wait事件的耗时
        delta = nt_current_msec - delta;
    //    nt_log_debug1( NT_LOG_DEBUG_EVENT, cycle->log, 0,
    //                   "timer delta: %M", delta );

        //连接首次进入需要先 accept
        //nt_event_process_posted( cycle, &nt_posted_accept_events  );
        /*
         *  delta是之前统计的耗时，存在毫秒级的耗时，就对所有时间的timer进行检查，
         *  果timeout 就从time rbtree中删除到期的timer，同时调用相应事件的handler函数处理
         */
        if( delta ) {
            nt_event_expire_timers();
        }



        nt_event_process_posted( cycle, &nt_posted_events );
//        sleep( 3 );
    }

    free( cycle );

    return 0;
}
