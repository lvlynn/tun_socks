#include <nt_def.h>
#include <nt_core.h>
#include <nt_event.h>

//处理select、 poll、 epoll 网络模型
//定义在modules/nt_select.c中
extern nt_module_t nt_select_module;
nt_int_t nt_event_process_set_socket( nt_cycle_t *cycle );


#define DEFAULT_CONNECTIONS  512

extern nt_module_t nt_kqueue_module;
extern nt_module_t nt_eventport_module;
extern nt_module_t nt_devpoll_module;
extern nt_module_t nt_epoll_module;
/* extern nt_module_t nt_select_module; */


static char *nt_event_init_conf( nt_cycle_t *cycle, void *conf );
static nt_int_t nt_event_module_init( nt_cycle_t *cycle );
nt_int_t nt_event_process_init( nt_cycle_t *cycle );
static char *nt_events_block( nt_conf_t *cf, nt_command_t *cmd, void *conf );

static char *nt_event_connections( nt_conf_t *cf, nt_command_t *cmd,
                                   void *conf );
static char *nt_event_use( nt_conf_t *cf, nt_command_t *cmd, void *conf );
static char *nt_event_debug_connection( nt_conf_t *cf, nt_command_t *cmd,
                                        void *conf );

static void *nt_event_core_create_conf( nt_cycle_t *cycle );
static char *nt_event_core_init_conf( nt_cycle_t *cycle, void *conf );
static void nt_timer_signal_handler( int signo );


#if (T_NT_ACCEPT_FILTER)
    static nt_int_t nt_event_dummy_accept_filter( nt_connection_t *c );
    nt_int_t ( *nt_event_top_accept_filter )( nt_connection_t *c );
#endif


static nt_uint_t     nt_timer_resolution;
sig_atomic_t          nt_event_timer_alarm;

static nt_uint_t     nt_event_max_module;


//event的flags值
nt_uint_t            nt_event_flags;

//event的回调函数集合
nt_event_actions_t   nt_event_actions;


static nt_atomic_t   connection_counter = 1;
nt_atomic_t         *nt_connection_counter = &connection_counter;

nt_atomic_t         *nt_accept_mutex_ptr;
nt_shmtx_t           nt_accept_mutex;
nt_uint_t            nt_use_accept_mutex;
nt_uint_t            nt_accept_events;
nt_uint_t            nt_accept_mutex_held;
nt_msec_t            nt_accept_mutex_delay;
nt_int_t             nt_accept_disabled;

#if (NT_STAT_STUB)

    static nt_atomic_t   nt_stat_accepted0;
    nt_atomic_t         *nt_stat_accepted = &nt_stat_accepted0;
    static nt_atomic_t   nt_stat_handled0;
    nt_atomic_t         *nt_stat_handled = &nt_stat_handled0;
    static nt_atomic_t   nt_stat_requests0;
    nt_atomic_t         *nt_stat_requests = &nt_stat_requests0;
    static nt_atomic_t   nt_stat_active0;
    nt_atomic_t         *nt_stat_active = &nt_stat_active0;
    static nt_atomic_t   nt_stat_reading0;
    nt_atomic_t         *nt_stat_reading = &nt_stat_reading0;
    static nt_atomic_t   nt_stat_writing0;
    nt_atomic_t         *nt_stat_writing = &nt_stat_writing0;
    static nt_atomic_t   nt_stat_waiting0;
    nt_atomic_t         *nt_stat_waiting = &nt_stat_waiting0;

    #if (T_NT_HTTP_STUB_STATUS)
        static nt_atomic_t   nt_stat_request_time0;
        nt_atomic_t         *nt_stat_request_time = &nt_stat_request_time0;
    #endif

#endif

#if 1
static nt_command_t  nt_events_commands[] = {

    {
        nt_string( "events" ),
        NT_MAIN_CONF | NT_CONF_BLOCK | NT_CONF_NOARGS,
        nt_events_block,
        0,
        0,
        NULL
    },

    nt_null_command
};


static nt_core_module_t  nt_events_module_ctx = {
    nt_string( "events" ),
    NULL,
    nt_event_init_conf
};


nt_module_t  nt_events_module = {
    NT_MODULE_V1,
    &nt_events_module_ctx,                /* module context */
    nt_events_commands,                   /* module directives */
    NT_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NT_MODULE_V1_PADDING
};


static nt_str_t  event_core_name = nt_string( "event_core" );


static nt_command_t  nt_event_core_commands[] = {

    {
        nt_string( "worker_connections" ),
        NT_EVENT_CONF | NT_CONF_TAKE1,
        nt_event_connections,
        0,
        0,
        NULL
    },

    {
        nt_string( "use" ),
        NT_EVENT_CONF | NT_CONF_TAKE1,
        nt_event_use,
        0,
        0,
        NULL
    },

    {
        nt_string( "multi_accept" ),
        NT_EVENT_CONF | NT_CONF_FLAG,
        nt_conf_set_flag_slot,
        0,
        offsetof( nt_event_conf_t, multi_accept ),
        NULL
    },

    {
        nt_string( "accept_mutex" ),
        NT_EVENT_CONF | NT_CONF_FLAG,
        nt_conf_set_flag_slot,
        0,
        offsetof( nt_event_conf_t, accept_mutex ),
        NULL
    },

    {
        nt_string( "accept_mutex_delay" ),
        NT_EVENT_CONF | NT_CONF_TAKE1,
        nt_conf_set_msec_slot,
        0,
        offsetof( nt_event_conf_t, accept_mutex_delay ),
        NULL
    },

    {
        nt_string( "debug_connection" ),
        NT_EVENT_CONF | NT_CONF_TAKE1,
        nt_event_debug_connection,
        0,
        0,
        NULL
    },

    nt_null_command
};


static nt_event_module_t  nt_event_core_module_ctx = {
    &event_core_name,
    nt_event_core_create_conf,            /* create configuration */
    nt_event_core_init_conf,              /* init configuration */
    #if (NT_SSL && NT_SSL_ASYNC)
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
    #else
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
    #endif
};


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
    event_module->actions.init( cycle, 1 );

    //完成回调地址关联
    //nt_event_actions = event_module->actions;

//    测试
    //nt_done_events( cycle );
    //设置初始值
}


void
nt_process_events_and_timers( nt_cycle_t *cycle )
{
    nt_uint_t  flags;
    nt_msec_t  timer, delta;

    debug( "start" );

    if( nt_timer_resolution ) {
        /*  开启了时间精度  给epoll_wait的超时参数为-1  阻塞等待
         *  epoll_wait返回有两种情况：
         *  1、有网络事件发生
         *  2、有信号产生 提前加好的ALARM定时器在nt_timer_resolution时间耗尽后 触发ALARM信号
         *  */
        //无限大的时间传给epoll_wait
        timer = NT_TIMER_INFINITE;
        flags = 0;

    } else {
        timer = nt_event_find_timer();
        flags = NT_UPDATE_TIME;

        #if (NT_WIN32)

        /* handle signals from master in case of network inactivity */

        if( timer == NT_TIMER_INFINITE || timer > 500 ) {
            timer = 500;
        }

        #endif
    }


    /*
     *  nt_use_accept_mutex变量代表是否使用accept互斥体
     *  默认是使用，可以通过accept_mutex off;指令关闭；
     *  accept mutex 的作用就是避免惊群，同时实现负载均衡
     */
    if( nt_use_accept_mutex ) {

        // nt_event_process_init 中设置了 nt_event_accept执行入口
        /*
         *  nt_accept_disabled变量在nt_event_accept函数中计算。
         *  如果nt_accept_disabled大于0，就表示该进程接受的链接过多，
         *  因此放弃一次争抢accept mutex的机会，同时将自己减一。
         *  然后，继续处理已有连接上的事件。
         *  nginx就利用这一点实现了继承关于连接的基本负载均衡。
         */
        if( nt_accept_disabled > 0 ) {
            nt_accept_disabled--;

        } else {
            /*
             *  尝试锁accept mutex，只有成功获取锁的进程，才会将listen套接字放到epoll中。
             *  因此，这就保证了只有一个进程拥有监听套接口，故所有进程阻塞在epoll_wait时，
             *  才不会惊群现象。
             */
            if( nt_trylock_accept_mutex( cycle ) == NT_ERROR ) {
                return;
            }

            if( nt_accept_mutex_held ) {
                /* 如果进程获得了锁，将添加一个 NT_POST_EVENTS 标志。
                 * 这个标志的作用是将所有产生的事件放入一个队列中，等释放后，在慢慢来处理事件。
                 * 因为，处理时间可能会很耗时，如果不先施放锁再处理的话，该进程就长时间霸占了锁，
                 * 导致其他进程无法获取锁，这样accept的效率就低了。  */
                flags |= NT_POST_EVENTS;
            } else {
                /*
                 * 没有获得锁的进程，当然不需要NT_POST_EVENTS标志。
                 * 但需要设置延时多长时间，再去争抢锁。
                 */
                if( timer == NT_TIMER_INFINITE
                    || timer > nt_accept_mutex_delay ) {
                    timer = nt_accept_mutex_delay;
                }
            }
        }
    }
    delta = nt_current_msec;


    /* 接下来，epoll要开始wait事件，
     * 前提是配置使用use epoll，
     * nt_process_events的具体实现是对应到epoll模块中的nt_epoll_process_events函数
     */
    ( void ) nt_process_events( cycle, timer, flags );

    //统计本次wait事件的耗时
    delta = nt_current_msec - delta;

    nt_log_debug1( NT_LOG_DEBUG_EVENT, cycle->log, 0,
                   "timer delta: %M", delta );

    debug( "timer delta:%d", delta );
    /*
     *  nt_posted_accept_events是一个事件队列，暂存epoll从监听套接口wait到的accept事件。
     *  前文提到的NT_POST_EVENTS标志被使用后，会将所有的accept事件暂存到这个队列
     */
    nt_event_process_posted( cycle, &nt_posted_accept_events );

    //所有accept事件处理完之后，如果持有锁的话，就释放掉。
    if( nt_accept_mutex_held ) {
        nt_shmtx_unlock( &nt_accept_mutex );
    }

    /*
     *  delta是之前统计的耗时，存在毫秒级的耗时，就对所有时间的timer进行检查，
     *  果timeout 就从time rbtree中删除到期的timer，同时调用相应事件的handler函数处理
     */
    if( delta ) {
        nt_event_expire_timers();
    }

    /*
     *  处理普通事件(连接上获得的读写事件)，
     *  因为每个事件都有自己的handler方法，
     */
    nt_event_process_posted( cycle, &nt_posted_events );
}



/*
   * worker进程创建时调用
   * */
nt_int_t nt_event_process_init( nt_cycle_t *cycle )
{
  //  cycle->connection_n = 1024;
    nt_uint_t           m, i;
    nt_event_t         *rev, *wev;
    #if (NT_SSL && NT_SSL_ASYNC)
    nt_event_t         *aev;
    #endif
	nt_listening_t     *ls;
	nt_connection_t    *c, *next, *old;
	nt_core_conf_t     *ccf;
	nt_event_conf_t    *ecf;
	nt_event_module_t  *module;




	ccf = ( nt_core_conf_t * ) nt_get_conf( cycle->conf_ctx, nt_core_module );
	ecf = nt_event_get_conf( cycle->conf_ctx, nt_event_core_module );

	if ( ccf->master && ccf->worker_processes > 1 && ecf->accept_mutex ) {
		nt_use_accept_mutex = 1; 
		nt_accept_mutex_held = 0; 
		nt_accept_mutex_delay = ecf->accept_mutex_delay;

	} else {
		nt_use_accept_mutex = 0; 
	}    

#if (NT_WIN32)

	/*   
	 * disable accept mutex on win32 as it may cause deadlock if
	 * grabbed by a process which can't accept connections
	 */

	nt_use_accept_mutex = 0; 

#endif


    nt_queue_init( &nt_posted_accept_events );
    nt_queue_init( &nt_posted_events );
    //初始化计数器，此处将会创建一颗红黑树，来维护计时器，之后会详细讲解
    if( nt_event_timer_init( cycle->log ) == NT_ERROR ) {
        return NT_ERROR;
    }

	//完成event add del 等函数回调的绑定
	for ( m = 0; cycle->modules[m]; m++ ) {
		if ( cycle->modules[m]->type != NT_EVENT_MODULE ) {
			continue;
		}

		if ( cycle->modules[m]->ctx_index != ecf->use ) {
			continue;
		}

		module = cycle->modules[m]->ctx;

		if ( module->actions.init( cycle, nt_timer_resolution ) != NT_OK ) {
			/* fatal */
			exit( 2 );
		}

		break;
	}


#if !(NT_WIN32)
      //如果时间不为0，则更新时间缓存改为 每nt_timer_resolution ms触发一次
      if ( nt_timer_resolution && !( nt_event_flags & NT_USE_TIMER_EVENT ) ) {
          struct sigaction  sa;
          struct itimerval  itv;
  
          nt_memzero( &sa, sizeof( struct sigaction ) );
          sa.sa_handler = nt_timer_signal_handler;
          sigemptyset( &sa.sa_mask );
  
          if ( sigaction( SIGALRM, &sa, NULL ) == -1 ) {
              nt_log_error( NT_LOG_ALERT, cycle->log, nt_errno,
                             "sigaction(SIGALRM) failed" );
              return NT_ERROR;
          }
  
          itv.it_interval.tv_sec = nt_timer_resolution / 1000;
          itv.it_interval.tv_usec = ( nt_timer_resolution % 1000 ) * 1000;
          itv.it_value.tv_sec = nt_timer_resolution / 1000;
          itv.it_value.tv_usec = ( nt_timer_resolution % 1000 ) * 1000;
  
          if ( setitimer( ITIMER_REAL, &itv, NULL ) == -1 ) {
              nt_log_error( NT_LOG_ALERT, cycle->log, nt_errno,
                             "setitimer() failed" );
          }
      }
  
      if ( nt_event_flags & NT_USE_FD_EVENT ) {
          struct rlimit  rlmt;
  
          if ( getrlimit( RLIMIT_NOFILE, &rlmt ) == -1 ) {
              nt_log_error( NT_LOG_ALERT, cycle->log, nt_errno,
                             "getrlimit(RLIMIT_NOFILE) failed" );
              return NT_ERROR;
          }
  
          cycle->files_n = ( nt_uint_t ) rlmt.rlim_cur;
  
          cycle->files = nt_calloc( sizeof( nt_connection_t * ) * cycle->files_n,
                                     cycle->log );
          if ( cycle->files == NULL ) {
              return NT_ERROR;
          }
      }
  
  #else
      if ( nt_timer_resolution && !( nt_event_flags & NT_USE_TIMER_EVENT ) ) {
          nt_log_error( NT_LOG_WARN, cycle->log, 0,
                         "the \"timer_resolution\" directive is not supported "
                         "with the configured event method, ignored" );
          nt_timer_resolution = 0;
      }
  
  #endif


    //创建全局的nt_connection_t数组，保存所有的connection
    //由于这个过程是在各个worker进程中执行的，所以每个worker都有自己的connection数组
    cycle->connections =
        nt_palloc( cycle->pool, sizeof( nt_connection_t ) * cycle->connection_n );
    if( cycle->connections == NULL ) {
        return NT_ERROR;
    }

    c = cycle->connections;


    //创建与连接同个数的一个读事件数组
#if 1
    cycle->read_events = nt_palloc( cycle->pool, sizeof( nt_event_t ) * cycle->connection_n );
#else
    cycle->read_events = nt_alloc(  sizeof( nt_event_t ) * cycle->connection_n , cycle->log);
#endif
    if( cycle->read_events == NULL ) {
        return NT_ERROR;
    }

    rev = cycle->read_events;
    for( i = 0; i < cycle->connection_n; i++ ) {
        rev[i].closed = 1;
        rev[i].instance = 1;
    }

    //创建创建与连接同个数的一个写事件数组
#if 1
    cycle->write_events = nt_palloc( cycle->pool, sizeof( nt_event_t ) * cycle->connection_n );
#else
    cycle->write_events = nt_alloc(  sizeof( nt_event_t ) * cycle->connection_n , cycle->log);
#endif
    if( cycle->write_events == NULL ) {
        return NT_ERROR;
    }

    wev = cycle->write_events;
    for( i = 0; i < cycle->connection_n; i++ ) {
        wev[i].closed = 1;
    }



    #if (NT_SSL && NT_SSL_ASYNC)
    //创建创建与连接同个数的一个异步事件数组
#if 1
    cycle->async_events = nt_palloc( cycle->pool, sizeof( nt_event_t ) * cycle->connection_n );
#else
    cycle->async_events = nt_alloc(  sizeof( nt_event_t ) * cycle->connection_n , cycle->log);
#endif
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


        debug( "c->type=%d", c->type );
#if (NT_HAVE_ACC_RCV)
        if( c->type == NT_AF_TUN )
            rev->handler = nt_event_accept_tun;
        else
#endif
            rev->handler = ( c->type == SOCK_STREAM ) ? nt_event_accept
                : nt_event_recvmsg;

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
        /* debug( "rev=%p", rev ); */
        debug( "nt_add_event=%p", nt_add_event );

        // nt_add_event 未赋值
        if( nt_add_event( rev, NT_READ_EVENT, 0 ) == NT_ERROR ) {
            return NT_ERROR;
        }

        #endif

    }

    return NT_OK;

}


nt_int_t nt_event_process( nt_cycle_t *cycle, nt_msec_t  timer, nt_uint_t  flags )
{
    /* nt_uint_t flags ; */
    /* flags  |= NT_POST_EVENTS; */
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


static char *
nt_event_init_conf( nt_cycle_t *cycle, void *conf )
{
    debug( "call ev init" );
    #if (NT_HAVE_REUSEPORT)
    nt_uint_t        i;
    nt_listening_t  *ls;
    #endif

    //判断nt_events_module是否调用过初始化conf操作
    if( nt_get_conf( cycle->conf_ctx, nt_events_module ) == NULL ) {
        nt_log_error( NT_LOG_EMERG, cycle->log, 0,
                       "no \"events\" section in configuration" );
        return NT_CONF_ERROR;
    }

    if( cycle->connection_n < cycle->listening.nelts + 1 ) {

        /*
         * there should be at least one connection for each listening
         * socket, plus an additional connection for channel
         */

        nt_log_error( NT_LOG_EMERG, cycle->log, 0,
                       "%ui worker_connections are not enough "
                       "for %ui listening sockets",
                       cycle->connection_n, cycle->listening.nelts );

        return NT_CONF_ERROR;
    }

    #if (NT_HAVE_REUSEPORT)

    ls = cycle->listening.elts;
    for( i = 0; i < cycle->listening.nelts; i++ ) {

        if( !ls[i].reuseport || ls[i].worker != 0 ) {
            continue;
        }

        if( nt_clone_listening( cycle, &ls[i] ) != NT_OK ) {
            return NT_CONF_ERROR;
        }

        /* cloning may change cycle->listening.elts */

        ls = cycle->listening.elts;
    }


    #endif

    return NT_CONF_OK;
}

/*
    模块初始化的回调函数
*/
static nt_int_t
nt_event_module_init( nt_cycle_t *cycle )
{
    void              ***cf;
    u_char              *shared;
    size_t               size, cl;
    nt_shm_t            shm;
    nt_time_t          *tp;
    nt_core_conf_t     *ccf;
    nt_event_conf_t    *ecf;

    //判断nt_events_module是否调用过初始化conf操作
    cf = nt_get_conf( cycle->conf_ctx, nt_events_module );

    //获取nt_event_core_module模块的配置结构
    ecf = ( *cf )[nt_event_core_module.ctx_index];

    if( !nt_test_config && nt_process <= NT_PROCESS_MASTER ) {
        nt_log_error( NT_LOG_NOTICE, cycle->log, 0,
                       "using the \"%s\" event method", ecf->name );
    }

    //获取nt_core_module模块的配置结构
    ccf = ( nt_core_conf_t * ) nt_get_conf( cycle->conf_ctx, nt_core_module );

    //从nt_core_module模块的配置结构中获取timer_resolution参数
    nt_timer_resolution = ccf->timer_resolution;

    #if !(NT_WIN32)
    {
        nt_int_t      limit;
        struct rlimit  rlmt;

        //获取当前进程能够打开的最大文件数     man getrlimit
        if( getrlimit( RLIMIT_NOFILE, &rlmt ) == -1 ) {
            nt_log_error( NT_LOG_ALERT, cycle->log, nt_errno,
                           "getrlimit(RLIMIT_NOFILE) failed, ignored" );
        } else {
            //如果nt_event_core_module模块连接数大于当前(软)限制
            //并且nt_core_module最大连接数无限制
            //或者nt_event_core_module连接数大于nt_core_module最大连接数
            if( ecf->connections > ( nt_uint_t ) rlmt.rlim_cur
                && ( ccf->rlimit_nofile == NT_CONF_UNSET
                     || ecf->connections > ( nt_uint_t ) ccf->rlimit_nofile ) ) {
                limit = ( ccf->rlimit_nofile == NT_CONF_UNSET ) ?
                        ( nt_int_t ) rlmt.rlim_cur : ccf->rlimit_nofile;

                nt_log_error( NT_LOG_WARN, cycle->log, 0,
                               "%ui worker_connections exceed "
                               "open file resource limit: %i",
                               ecf->connections, limit );
            }
        }
    }
    #endif /* !(NT_WIN32) */



    //如果关闭了master进程，就返回
    //因为关闭了master进程就是单进程工作方式，
    //之后的操作时创建共享内存实现锁等工作，单进程不需要。
    if( ccf->master == 0 ) {
        return NT_OK;
    }

    //如果已经存在accept互斥体了，不需要再重复创建了
    if( nt_accept_mutex_ptr ) {
        return NT_OK;
    }


    /* cl should be equal to or greater than cache line size */

    cl = 128;

    //这里创建size大小的共享内存，这块共享内存将被均分成三段
    size = cl            /* nt_accept_mutex */
           + cl          /* nt_connection_counter */
           + cl;         /* nt_temp_number */

    #if (NT_STAT_STUB)
    size += cl           /* nt_stat_accepted */
            + cl          /* nt_stat_handled */
            + cl          /* nt_stat_requests */
            + cl          /* nt_stat_active */
            + cl          /* nt_stat_reading */
            + cl          /* nt_stat_writing */
            + cl          /* nt_stat_waiting */
            #if (T_NT_HTTP_STUB_STATUS)
            + cl         /* nt_stat_request_time */
            #endif
            ;

    #endif

    //准备共享内存，大小为size，命名nginx_shared_zone，
    shm.size = size;
    nt_str_set( &shm.name, "nginx_shared_zone" );
    shm.log = cycle->log;

    //创建共享内存，起始地址保存在shm.addr
    if( nt_shm_alloc( &shm ) != NT_OK ) {
        return NT_ERROR;
    }

    //获取起始地址保存
    shared = shm.addr;

    //accept互斥体取得共享内存的第一段cl大小内存
    nt_accept_mutex_ptr = ( nt_atomic_t * ) shared;
    nt_accept_mutex.spin = ( nt_uint_t ) - 1;

    /*创建accept互斥体
      accept互斥体的实现依赖是否支持原子操作，如果有相应的原子操作；
      就是用取得的这段共享内存来实现accept互斥体;否则，将使用文件锁
      来实现accept互斥体。
      accept互斥体的作用是：避免惊群和实现worker进程的负载均衡。
      */
    if( nt_shmtx_create( &nt_accept_mutex, ( nt_shmtx_sh_t * ) shared,
                          cycle->lock_file.data )
        != NT_OK ) {
        return NT_ERROR;
    }
    //获取内存的第二段cl大小的地址
    nt_connection_counter = ( nt_atomic_t * )( shared + 1 * cl );

    ( void ) nt_atomic_cmp_set( nt_connection_counter, 0, 1 );

    nt_log_debug2( NT_LOG_DEBUG_EVENT, cycle->log, 0,
                    "counter: %p, %uA",
                    nt_connection_counter, *nt_connection_counter );

    //获取内存的第三段cl大小的地址
    nt_temp_number = ( nt_atomic_t * )( shared + 2 * cl );

    tp = nt_timeofday();

    nt_random_number = ( tp->msec << 16 ) + nt_pid;

    #if (NT_STAT_STUB)

    nt_stat_accepted = ( nt_atomic_t * )( shared + 3 * cl );
    nt_stat_handled = ( nt_atomic_t * )( shared + 4 * cl );
    nt_stat_requests = ( nt_atomic_t * )( shared + 5 * cl );
    nt_stat_active = ( nt_atomic_t * )( shared + 6 * cl );
    nt_stat_reading = ( nt_atomic_t * )( shared + 7 * cl );
    nt_stat_writing = ( nt_atomic_t * )( shared + 8 * cl );
    nt_stat_waiting = ( nt_atomic_t * )( shared + 9 * cl );

    #if (T_NT_HTTP_STUB_STATUS)
    nt_stat_request_time = ( nt_atomic_t * )( shared + 10 * cl );
    #endif

    #endif

    return NT_OK;
}


#if !(NT_WIN32)

static void
nt_timer_signal_handler( int signo )
{
    debug(  "nt_event_timer_alarm = 1" );
    nt_event_timer_alarm = 1;

    #if 1
    nt_log_debug0( NT_LOG_DEBUG_EVENT, nt_cycle->log, 0, "timer signal" );
    #endif
}

#endif


nt_int_t
nt_send_lowat( nt_connection_t *c, size_t lowat )
{
    int  sndlowat;

    #if (NT_HAVE_LOWAT_EVENT)

    if( nt_event_flags & NT_USE_KQUEUE_EVENT ) {
        c->write->available = lowat;
        return NT_OK;
    }

    #endif

    if( lowat == 0 || c->sndlowat ) {
        return NT_OK;
    }

    sndlowat = ( int ) lowat;

    if( setsockopt( c->fd, SOL_SOCKET, SO_SNDLOWAT,
                    ( const void * ) &sndlowat, sizeof( int ) )
        == -1 ) {
        nt_connection_error( c, nt_socket_errno,
                              "setsockopt(SO_SNDLOWAT) failed" );
        return NT_ERROR;
    }

    c->sndlowat = 1;

    return NT_OK;
}
/* events的回调函数
     最重要的一个过程，此处调用nt_conf_parse()的作用就是完成配置文件中events{}这个block的解析，从而调用其下所有的
 配置指令的回调函数，完成解析配置文件的初始化工作。
 */
static char *
nt_events_block( nt_conf_t *cf, nt_command_t *cmd, void *conf )
{
    debug( "call nt_events_block" );
    char                 *rv;
    void               ***ctx;
    nt_uint_t            i;
    nt_conf_t            pcf;
    nt_event_module_t   *m;

    if( *( void ** ) conf ) {
        return "is duplicate";
    }

    /* count the number of the event modules and set up their indices */

    //计算event模块数量，并且记录
    nt_event_max_module = nt_count_modules( cf->cycle, NT_EVENT_MODULE );

    ctx = nt_pcalloc( cf->pool, sizeof( void * ) );
    if( ctx == NULL ) {
        return NT_CONF_ERROR;
    }

    //为每一个event模块分配空间，用来保存响应配置结构的地址
    //共分配了nt_event_max_module个空间
    *ctx = nt_pcalloc( cf->pool, nt_event_max_module * sizeof( void * ) );
    if( *ctx == NULL ) {
        return NT_CONF_ERROR;
    }

    *( void ** ) conf = ctx;

    for( i = 0; cf->cycle->modules[i]; i++ ) {
        if( cf->cycle->modules[i]->type != NT_EVENT_MODULE ) {
            continue;
        }
        m = cf->cycle->modules[i]->ctx;

        //循环调用每个模块的creat_conf钩子函数，用于创建配置结构
        if( m->create_conf ) {
            ( *ctx )[cf->cycle->modules[i]->ctx_index] =
                m->create_conf( cf->cycle );
            if( ( *ctx )[cf->cycle->modules[i]->ctx_index] == NULL ) {
                return NT_CONF_ERROR;
            }
        }
    }

    #if (T_NT_ACCEPT_FILTER)
    nt_event_top_accept_filter = nt_event_dummy_accept_filter;
    #endif

    pcf = *cf;
    cf->ctx = ctx;
    cf->module_type = NT_EVENT_MODULE;
    cf->cmd_type = NT_EVENT_CONF;

    //由于events是一个block指令，events域下还可以配置很多其他指令，
    //比如之前提过的use等，现在开始解析events block中的指令，完成初始化工作。
    rv = nt_conf_parse( cf, NULL );

    debug( "nt_events_block rv =%s", rv );
    *cf = pcf;

    if( rv != NT_CONF_OK ) {
        return rv;
    }

    for( i = 0; cf->cycle->modules[i]; i++ ) {
        if( cf->cycle->modules[i]->type != NT_EVENT_MODULE ) {
            continue;
        }

        m = cf->cycle->modules[i]->ctx;

        debug( " m name =%s", cf->cycle->modules[i]->name  );
        //循环执行每个event模块的init_conf函数，初始化配置结构
        if( m->init_conf ) {
            rv = m->init_conf( cf->cycle,
                               ( *ctx )[cf->cycle->modules[i]->ctx_index] );


            debug( "nt_events_block init conf fail rv =%s", rv );
            if( rv != NT_CONF_OK ) {
                return rv;
            }
        }
    }

    return NT_CONF_OK;
}


static char *
nt_event_connections( nt_conf_t *cf, nt_command_t *cmd, void *conf )
{
    debug( " cmd handler nt_event_connections" );
    nt_event_conf_t  *ecf = conf;

    nt_str_t  *value;

    if( ecf->connections != NT_CONF_UNSET_UINT ) {
        return "is duplicate";
    }

    value = cf->args->elts;
    ecf->connections = nt_atoi( value[1].data, value[1].len );
    if( ecf->connections == ( nt_uint_t ) NT_ERROR ) {
        nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                            "invalid number \"%V\"", &value[1] );

        return NT_CONF_ERROR;
    }

    cf->cycle->connection_n = ecf->connections;

    return NT_CONF_OK;
}
static char *
nt_event_use( nt_conf_t *cf, nt_command_t *cmd, void *conf )
{
    debug( "nt_event_use" );
    nt_event_conf_t  *ecf = conf;

    nt_int_t             m;
    nt_str_t            *value;
    nt_event_conf_t     *old_ecf;
    nt_event_module_t   *module;

    if( ecf->use != NT_CONF_UNSET_UINT ) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if( cf->cycle->old_cycle->conf_ctx ) {
        old_ecf = nt_event_get_conf( cf->cycle->old_cycle->conf_ctx,
                                      nt_event_core_module );
    } else {
        old_ecf = NULL;
    }


    for( m = 0; cf->cycle->modules[m]; m++ ) {
        if( cf->cycle->modules[m]->type != NT_EVENT_MODULE ) {
            continue;
        }

        module = cf->cycle->modules[m]->ctx;
        debug(  "%s ,module=%s,len=%d, value=%s,len=%d", __func__, module->name->data, module->name->len,  value[1].data, value[1].len );

        #ifdef router_name_r6220
        //更改读入配置 epoll 为poll, 6220旧版固件不支持epoll
        //      nt_str_set(&value[1], "poll");
        #endif

        #ifdef make_cpu_type_mips
        //更改读入配置 epoll 为poll, ac66u不支持epoll
        //      nt_str_set( &value[1], "poll" );

        #endif
        if( module->name->len == value[1].len ) {
            if( nt_strcmp( module->name->data, value[1].data ) == 0 ) {

                ecf->use = cf->cycle->modules[m]->ctx_index;
                ecf->name = module->name->data;
                debug( "%s ,use %s", __func__, module->name->data );

                
                if( nt_process == NT_PROCESS_SINGLE
                    && old_ecf
                    && old_ecf->use != ecf->use ) {
                    nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                                        "when the server runs without a master process "
                                        "the \"%V\" event type must be the same as "
                                        "in previous configuration - \"%s\" "
                                        "and it cannot be changed on the fly, "
                                        "to change it you need to stop server "
                                        "and start it again",
                                        &value[1], old_ecf->name );

                     debug( "use conf error" );
                    return NT_CONF_ERROR;
                }

                debug( "use conf ok" );
                return NT_CONF_OK;
            }
        }
    }

    nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                        "invalid event type \"%V\"", &value[1] );

    debug( "use conf error" );
    return NT_CONF_ERROR;
}


static char *
nt_event_debug_connection( nt_conf_t *cf, nt_command_t *cmd, void *conf )
{
    #if (NT_DEBUG)
    nt_event_conf_t  *ecf = conf;

    nt_int_t             rc;
    nt_str_t            *value;
    nt_url_t             u;
    nt_cidr_t            c, *cidr;
    nt_uint_t            i;
    struct sockaddr_in   *sin;
    #if (NT_HAVE_INET6)
    struct sockaddr_in6  *sin6;
    #endif

    value = cf->args->elts;

    #if (NT_HAVE_UNIX_DOMAIN)

    if( nt_strcmp( value[1].data, "unix:" ) == 0 ) {
        cidr = nt_array_push( &ecf->debug_connection );
        if( cidr == NULL ) {
            return NT_CONF_ERROR;
        }

        cidr->family = AF_UNIX;
        return NT_CONF_OK;
    }

    #endif

    rc = nt_ptocidr( &value[1], &c );

    if( rc != NT_ERROR ) {
        if( rc == NT_DONE ) {
            nt_conf_log_error( NT_LOG_WARN, cf, 0,
                                "low address bits of %V are meaningless",
                                &value[1] );
        }


        cidr = nt_array_push( &ecf->debug_connection );
        if( cidr == NULL ) {
            return NT_CONF_ERROR;
        }

        *cidr = c;

        return NT_CONF_OK;
    }

    nt_memzero( &u, sizeof( nt_url_t ) );
    u.host = value[1];

    if( nt_inet_resolve_host( cf->pool, &u ) != NT_OK ) {
        if( u.err ) {
            nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                                "%s in debug_connection \"%V\"",
                                u.err, &u.host );
        }

        return NT_CONF_ERROR;
    }

    cidr = nt_array_push_n( &ecf->debug_connection, u.naddrs );
    if( cidr == NULL ) {
        return NT_CONF_ERROR;
    }

    nt_memzero( cidr, u.naddrs * sizeof( nt_cidr_t ) );

    for( i = 0; i < u.naddrs; i++ ) {
        cidr[i].family = u.addrs[i].sockaddr->sa_family;

        switch( cidr[i].family ) {

            #if (NT_HAVE_INET6)
        case AF_INET6:
            sin6 = ( struct sockaddr_in6 * ) u.addrs[i].sockaddr;
            cidr[i].u.in6.addr = sin6->sin6_addr;
            nt_memset( cidr[i].u.in6.mask.s6_addr, 0xff, 16 );
            break;
            #endif

        default: /* AF_INET */
            sin = ( struct sockaddr_in * ) u.addrs[i].sockaddr;
            cidr[i].u.in.addr = sin->sin_addr.s_addr;
            cidr[i].u.in.mask = 0xffffffff;
            break;
        }
    }

    #else

    nt_conf_log_error( NT_LOG_WARN, cf, 0,
                        "\"debug_connection\" is ignored, you need to rebuild "
                        "nginx using --with-debug option to enable it" );

    #endif

    return NT_CONF_OK;
}


static void *
nt_event_core_create_conf( nt_cycle_t *cycle )
{
    nt_event_conf_t  *ecf;

    ecf = nt_palloc( cycle->pool, sizeof( nt_event_conf_t ) );
    if( ecf == NULL ) {
        return NULL;
    }

    ecf->connections = NT_CONF_UNSET_UINT;
    ecf->use = NT_CONF_UNSET_UINT;
    ecf->multi_accept = NT_CONF_UNSET;
    ecf->accept_mutex = NT_CONF_UNSET;
    ecf->accept_mutex_delay = NT_CONF_UNSET_MSEC;
    ecf->name = ( void * ) NT_CONF_UNSET;

    #if (NT_DEBUG)

    if( nt_array_init( &ecf->debug_connection, cycle->pool, 4,
                        sizeof( nt_cidr_t ) ) == NT_ERROR ) {
        return NULL;
    }

    #endif

    return ecf;
}


static char *
nt_event_core_init_conf( nt_cycle_t *cycle, void *conf )
{
    nt_event_conf_t  *ecf = conf;

    #if (NT_HAVE_EPOLL) && !(NT_TEST_BUILD_EPOLL)
    int                  fd;
    #endif
    nt_int_t            i;
    nt_module_t        *module;
    nt_event_module_t  *event_module;

    module = NULL;

    #if (NT_HAVE_EPOLL) && !(NT_TEST_BUILD_EPOLL)

    fd = epoll_create( 100 );

    if( fd != -1 ) {
        ( void ) close( fd );
        module = &nt_epoll_module;

    } else if( nt_errno != NT_ENOSYS ) {
        module = &nt_epoll_module;
    }

    #endif

    #if (NT_HAVE_DEVPOLL) && !(NT_TEST_BUILD_DEVPOLL)

    module = &nt_devpoll_module;

    #endif

    #if (NT_HAVE_KQUEUE)

    module = &nt_kqueue_module;

    #endif

    #if (NT_HAVE_SELECT)
    if( module == NULL ) {
        module = &nt_select_module;
    }

    #endif

    if( module == NULL ) {
        for( i = 0; cycle->modules[i]; i++ ) {

            if( cycle->modules[i]->type != NT_EVENT_MODULE ) {
                continue;
            }

            event_module = cycle->modules[i]->ctx;

            if( nt_strcmp( event_module->name->data, event_core_name.data ) == 0 ) {
                continue;
            }

            module = cycle->modules[i];
            break;
        }
    }

    if( module == NULL ) {
        nt_log_error( NT_LOG_EMERG, cycle->log, 0, "no events module found" );
        return NT_CONF_ERROR;
    }

    nt_conf_init_uint_value( ecf->connections, DEFAULT_CONNECTIONS );
    cycle->connection_n = ecf->connections;

    nt_conf_init_uint_value( ecf->use, module->ctx_index );

    event_module = module->ctx;
    nt_conf_init_ptr_value( ecf->name, event_module->name->data );

    nt_conf_init_value( ecf->multi_accept, 0 );
    nt_conf_init_value( ecf->accept_mutex, 0 );
    nt_conf_init_msec_value( ecf->accept_mutex_delay,
                              #if (T_NT_MODIFY_DEFAULT_VALUE)
                              100 );
                              #else
                              500 );
                              #endif

    return NT_CONF_OK;
}



#if (T_NT_ACCEPT_FILTER)
static nt_int_t
nt_event_dummy_accept_filter( nt_connection_t *c )
{
    nt_log_debug0( NT_LOG_DEBUG_EVENT, c->log, 0, "event dummy accept filter" );
    return NT_OK;
}
#endif
