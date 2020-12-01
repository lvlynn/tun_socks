#include <nt_def.h>
#include <nt_core.h>
#include <nt_event.h>

//处理select、 poll、 epoll 网络模型
//定义在modules/nt_select.c中
extern nt_module_t nt_select_module;

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

        c[i].data = next;
        c[i].read = &cycle->read_events[i];
        c[i].write = &cycle->write_events[i];
        c[i].fd = ( nt_socket_t ) - 1;
        #if (NT_SSL && NT_SSL_ASYNC)
        c[i].async = &cycle->async_events[i];
        c[i].async_fd = ( nt_socket_t ) - 1;
        #endif

        next = &c[i];
    } while( i );

    cycle->free_connections = next;
    cycle->free_connection_n = cycle->connection_n;


}

nt_event_process( nt_cycle_t *cycle )
{
    nt_uint_t flags ;
    flags  |= NT_POST_EVENTS;
    ( void ) nt_process_events( cycle,  flags );

}

nt_handle_read_event( nt_event_t *rev, nt_uint_t flags )
{
    if( !rev->active && !rev->ready ) {
        if( nt_add_event( rev, NT_READ_EVENT, NT_LEVEL_EVENT )
            == NT_ERROR ) {
            return NT_ERROR;
        }

        return NT_OK;
    }

    if( rev->active && ( rev->ready || ( flags & NT_CLOSE_EVENT ) ) ) {
        if( nt_del_event( rev, NT_READ_EVENT, NT_LEVEL_EVENT | flags )
            == NT_ERROR ) {
            return NT_ERROR;
        }

        return NT_OK;
    }
}



nt_handle_write_event( nt_event_t *wev, nt_uint_t flags )
{
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
