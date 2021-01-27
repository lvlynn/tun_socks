
#include <nt_core.h>
#include <nt_event.h>


void test()
{

    printf( "test\n" );
}
/*
 *  //ev事件的重要标志位
 *
 *  ready ,在 accept进入一个新事件的时候设定 rev->ready为1， wev->ready为1
 *  在ngx_get_connection中设定 wev->write = 1 ， rev->write 默认为0
 *  也就是说如果ev->write 1 我们可以认定为当前事件为write事件
 *  error
 *  active 
 *
 *
 * */




nt_new_connection( nt_connection_t *c, nt_log_t *log )
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


u_char *buffer ;
nt_connection_t  *c_stdin ;
nt_connection_t  *c_stdout ;


void test_read( nt_event_t *ev )
{
    printf( "test_read\n" );


    nt_connection_t *c ;
    nt_buf_t   *b;
    nt_buf_t   buf ;
    size_t  size;
    ssize_t n;

    c = ev->data;

    u_char *p = buffer;
    if( p == NULL )
        return NULL;

    printf( "p=%#x\n", p );

    buf.start = p;
    buf.end = p + 1500;
    buf.pos = p;
    buf.last = p;

    b = &buf;
    for( ;; ) {
        size = b->end - b->last;
        printf( "size=%d\n", size );

        if( size == 0 )
            break;

//        n = read( c->fd, b->last, size );
        n = c->recv( c, b->last, size );
        printf( "n=%d\n", n );

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
                   "buf=%s", b->start );



    if( nt_handle_read_event( ev, NT_CLOSE_EVENT ) != NT_OK ) {
        printf( "[%s] read event NT_ERROR\n", __func__ );
    } else
        printf( "[%s] read event NT_OK\n", __func__ );


    c_stdout->write->active = 0;
    c_stdout->write->ready = 0;
    if( nt_handle_write_event( c_stdout->write, 0 ) != NT_OK ) {
        printf( "[%s] write event NT_ERROR\n", __func__ );
    } else
        printf( "[%s] write event NT_OK\n", __func__ );


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
    u_char *p = buffer;

    buf.start = p;
    buf.end = p + 1500;
    buf.pos = p;
    buf.last = p;

    b = &buf;

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
    if( nt_handle_read_event( c_stdin->read, NT_READ_EVENT ) != NT_OK ) {
        printf( "[%s] read event NT_ERROR\n", __func__ );
    } else
        printf( "[%s] read event NT_OK\n", __func__ );

}

void test_ev_hander( nt_event_t *ev )
{

    printf( "test_ev_hander\n" );

    nt_add_event( ev, NT_READ_EVENT, NT_LEVEL_EVENT );


}





int main()
{

    signal( SIGUSR1, do_write );

    nt_log_t *log ;
    nt_pool_t *pool;
    nt_cycle_t *cycle;


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


    cycle->log = log;

    // event 初始化
    nt_event_init( cycle );
    nt_event_process_init( cycle );

    nt_socket_t s = STDOUT_FILENO ;

    nt_event_t *ev ;

    //    cycle->connections[0];
    c_stdin = &cycle->connections[0];
    c_stdout = &cycle->connections[1];


    nt_new_connection( c_stdin, log );
    nt_new_connection( c_stdout, log );


    c_stdin->pool = pool;
    c_stdin->read->handler = test_read;
    c_stdin->write->handler = NULL;

    c_stdout->pool = pool;
    c_stdout->read->handler = NULL;
    c_stdout->write->handler = test_write;




    ev = ( nt_event_t * ) malloc( sizeof( nt_event_t ) );
    ev->data = ( void * )c_stdin;

    ev->handler = test_ev_hander;
    ev->index = NT_INVALID_INDEX ;
    ev->write = 0;
    ev->log = log;

    int ret =  nt_add_event( c_stdin->read,  NT_READ_EVENT,  0 );

    printf( "ret = %d\n", ret );

    buffer = nt_pnalloc( pool, 1500 );

    for( ;; ) {
        nt_msec_t  timer, delta;
        nt_uint_t  flags;

        timer = nt_event_find_timer();
        flags = NT_UPDATE_TIME;

        delta = nt_current_msec;
        printf( "timer = %d\n", timer );
        nt_event_process( cycle, timer, flags );

        //统计本次wait事件的耗时
        delta = nt_current_msec - delta;
        nt_log_debug1( NT_LOG_DEBUG_EVENT, cycle->log, 0,
                        "timer delta: %M", delta );


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
