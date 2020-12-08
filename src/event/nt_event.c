#include <nt_def.h>
#include <nt_core.h>
#include <nt_event.h>

//处理select、 poll、 epoll 网络模型
//定义在modules/nt_select.c中
extern nt_module_t nt_select_module;
nt_int_t nt_event_process_set_socket( nt_cycle_t *cycle );

//event的flags值
nt_uint_t            nt_event_flags;

nt_uint_t             nt_accept_mutex_held;
nt_uint_t             nt_use_accept_mutex;

//event的回调函数集合
nt_event_actions_t   nt_event_actions;

#if 0
nt_module_t  nt_event_core_module = {
    NT_MODULE_V1,
    &nt_event_core_module_ctx,            /* module context */
    nt_event_core_commands,               /* module directives */
    NT_EVENT_MODULE,                      /* module type */
    NULL,                                  /* init master */
    nt_event_module_init,                 /* init module */
    nt_event_process_init,                /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NT_MODULE_V1_PADDING
};
#endif

/*确定使用哪个网络模型*/
nt_event_init( nt_cycle_t *cycle )
{

    nt_module_t        *module;
    //这里使用select模型
    module = &nt_select_module;

    nt_event_module_t  *event_module = module->ctx;
//    nt_event_module_t *ctx = ( nt_event_module_t * )(module->index->ctx);


    //调用select的init函数
    event_module->actions.init( cycle );

    //完成回调地址关联
    //nt_event_actions = event_module->actions;

//    测试
    //nt_done_events( cycle );
    //设置初始值
}


/*
   * worker进程创建时调用
   * */
nt_int_t nt_event_process_init( nt_cycle_t *cycle )
{
    cycle->connection_n = 512;
    nt_uint_t           m, i;
    nt_event_t         *rev, *wev;
    #if (NT_SSL && NT_SSL_ASYNC)
    nt_event_t         *aev;
    #endif
    //nt_listening_t     *ls;
    nt_connection_t    *c, *next, *old;
//      nt_core_conf_t     *ccf;
//      nt_event_conf_t    *ecf;
    nt_event_module_t  *module;

    nt_queue_init( &nt_posted_accept_events );
    nt_queue_init( &nt_posted_events );
    //初始化计数器，此处将会创建一颗红黑树，来维护计时器，之后会详细讲解
    if( nt_event_timer_init( cycle->log ) == NT_ERROR ) {
        return NT_ERROR;
    }

//创建全局的nt_connection_t数组，保存所有的connection
    //由于这个过程是在各个worker进程中执行的，所以每个worker都有自己的connection数组
    cycle->connections =
        nt_alloc( sizeof( nt_connection_t ) * cycle->connection_n, cycle->log );
    if( cycle->connections == NULL ) {
        return NT_ERROR;
    }

    c = cycle->connections;


    //创建一个读事件数组
    cycle->read_events = nt_alloc( sizeof( nt_event_t ) * cycle->connection_n,
                                   cycle->log );
    if( cycle->read_events == NULL ) {
        return NT_ERROR;
    }

    rev = cycle->read_events;
    for( i = 0; i < cycle->connection_n; i++ ) {
        rev[i].closed = 1;
        rev[i].instance = 1;
    }

    //创建一个写事件数组
    cycle->write_events = nt_alloc( sizeof( nt_event_t ) * cycle->connection_n,
                                    cycle->log );
    if( cycle->write_events == NULL ) {
        return NT_ERROR;
    }

    wev = cycle->write_events;
    for( i = 0; i < cycle->connection_n; i++ ) {
        wev[i].closed = 1;
    }



    #if (NT_SSL && NT_SSL_ASYNC)
    cycle->async_events = nt_alloc( sizeof( nt_event_t ) * cycle->connection_n,
                                    cycle->log );
    if( cycle->async_events == NULL ) {
        return NT_ERROR;
    }

    aev = cycle->async_events;
    for( i = 0; i < cycle->connection_n; i++ ) {
        aev[i].closed = 1;
        aev[i].instance = 1;
    }
    #endif

    //初始化整个connection数组
    do {
        i--;
        //connection的data 指向下一个connection
        c[i].data = next;
        //read ev
        c[i].read = &cycle->read_events[i];
        //write ev
        c[i].write = &cycle->write_events[i];
        //fd
        c[i].fd = ( nt_socket_t ) - 1;
        #if (NT_SSL && NT_SSL_ASYNC)
        c[i].async = &cycle->async_events[i];
        c[i].async_fd = ( nt_socket_t ) - 1;
        #endif

        next = &c[i];
    } while( i );

    cycle->free_connections = next;
    cycle->free_connection_n = cycle->connection_n;

	return nt_event_process_set_socket( cycle );
}


nt_int_t
nt_event_process_set_socket( nt_cycle_t *cycle )
{
    nt_uint_t           m, i;
    nt_event_t         *rev, *wev;
    #if (NT_SSL && NT_SSL_ASYNC)
    nt_event_t         *aev;
    #endif
    nt_listening_t     *ls;
    nt_connection_t    *c, *next, *old;

    /* for each listening socket */
    //为每个监听套接字从connection数组中分配一个连接，即一个slot
    ls = cycle->listening.elts;
    for( i = 0; i < cycle->listening.nelts; i++ ) {

        #if (NT_HAVE_REUSEPORT)
        if( ls[i].reuseport && ls[i].worker != nt_worker ) {
            continue;
        }
        #endif

        c = nt_get_connection( ls[i].fd, cycle->log );

        if( c == NULL ) {
            return NT_ERROR;
        }

        c->type = ls[i].type;
        c->log = &ls[i].log;

        c->listening = &ls[i];
        ls[i].connection = c;

        rev = c->read;

        rev->log = c->log;
        rev->accept = 1; //读时间发生，调用accept

        #if (NT_HAVE_DEFERRED_ACCEPT)
        rev->deferred_accept = ls[i].deferred_accept;
        #endif

        if( !( nt_event_flags & NT_USE_IOCP_EVENT ) ) {
            if( ls[i].previous ) {

                /*
                 * delete the old accept events that were bound to
                 * the old cycle read events array
                 */

                old = ls[i].previous->connection;

                if( nt_del_event( old->read, NT_READ_EVENT, NT_CLOSE_EVENT )
                    == NT_ERROR ) {
                    return NT_ERROR;
                }

                old->fd = ( nt_socket_t ) - 1;
            }
        }

        #if (NT_WIN32)

        if( nt_event_flags & NT_USE_IOCP_EVENT ) {
            nt_iocp_conf_t  *iocpcf;

            rev->handler = nt_event_acceptex;

            if( nt_use_accept_mutex ) {
                continue;
            }

            if( nt_add_event( rev, 0, NT_IOCP_ACCEPT ) == NT_ERROR ) {
                return NT_ERROR;
            }

            ls[i].log.handler = nt_acceptex_log_error;

            iocpcf = nt_event_get_conf( cycle->conf_ctx, nt_iocp_module );
            if( nt_event_post_acceptex( &ls[i], iocpcf->post_acceptex )
                == NT_ERROR ) {
                return NT_ERROR;
            }

        } else {
            //注册监听套接口毒事件的回调函数 nt_event_accept
            rev->handler = nt_event_accept;

            //使用了accept_mutex，暂时不将监听套接字放入epoll中，而是
            //等到worker抢到accept互斥体后，再放入epoll，避免惊群的发生
            if( nt_use_accept_mutex ) {
                continue;
            }

            if( nt_add_event( rev, NT_READ_EVENT, 0 ) == NT_ERROR ) {
                return NT_ERROR;
            }
        }

        #else


//        rev->handler = ( c->type == SOCK_STREAM ) ? nt_event_accept
//                       : nt_event_recvmsg;

        rev->handler = ( c->type == SOCK_STREAM ) ? nt_event_accept
                       : nt_event_accept;


        #if (NT_HAVE_REUSEPORT)

        //没有使用accept互斥体，那么就将此监听套接字放入epoll中。
        /*
            如果运行端口复用，就添加read事件到epoll
        */
        if( ls[i].reuseport ) {
            if( nt_add_event( rev, NT_READ_EVENT, 0 ) == NT_ERROR ) {
                return NT_ERROR;
            }

            continue;
        }

        #endif

        if( nt_use_accept_mutex ) {
            continue;
        }

        #if (NT_HAVE_EPOLLEXCLUSIVE)

        if( ( nt_event_flags & NT_USE_EPOLL_EVENT )
            && ccf->worker_processes > 1 ) {
            if( nt_add_event( rev, NT_READ_EVENT, NT_EXCLUSIVE_EVENT )
                == NT_ERROR ) {
                return NT_ERROR;
            }

            continue;
        }
        #endif
        if( nt_add_event( rev, NT_READ_EVENT, 0 ) == NT_ERROR ) {
            return NT_ERROR;
        }

        #endif

    }

    return NT_OK;

}


nt_int_t nt_event_process( nt_cycle_t *cycle, nt_msec_t  timer, nt_uint_t  flags )
{
//    nt_uint_t flags ;
//    flags  |= NT_POST_EVENTS;
    ( void ) nt_process_events( cycle, timer, flags );

}

nt_int_t nt_handle_read_event( nt_event_t *rev, nt_uint_t flags )
{
    nt_log_debug2( NT_LOG_DEBUG_EVENT, rev->log, 0,
                   "rev->active : %d, rev->ready=%d", rev->active, rev->ready );

    if( !rev->active && !rev->ready ) {
        printf( "nt_add_event\n" );
        if( nt_add_event( rev, NT_READ_EVENT, NT_LEVEL_EVENT )
            == NT_ERROR ) {
            return NT_ERROR;
        }

        return NT_OK;
    }

    //当 active 为1 ,并且 ready为1 ，或 flags为NT_CLOSE_EVENT 或 NT_WRITE_EVENT 时，
    //会删除read事件
    if( rev->active && ( rev->ready || ( flags & NT_CLOSE_EVENT ) ) ) {
        printf( "nt_del_event\n" );
        if( nt_del_event( rev, NT_READ_EVENT, NT_LEVEL_EVENT | flags )
            == NT_ERROR ) {
            return NT_ERROR;
        }

        return NT_OK;
    }

//    return NT_ERROR;
}



nt_int_t nt_handle_write_event( nt_event_t *wev, size_t lowat )
{
    nt_log_debug2( NT_LOG_DEBUG_EVENT, wev->log, 0,
                   "wev->active : %d, wev->ready=%d", wev->active, wev->ready );

    if( !wev->active && !wev->ready ) {
        if( nt_add_event( wev, NT_WRITE_EVENT, NT_LEVEL_EVENT )
            == NT_ERROR ) {
            return NT_ERROR;
        }

        return NT_OK;
    }

    if( wev->active && wev->ready ) {
        if( nt_del_event( wev, NT_WRITE_EVENT, NT_LEVEL_EVENT )
            == NT_ERROR ) {
            return NT_ERROR;
        }

        return NT_OK;
    }

}
