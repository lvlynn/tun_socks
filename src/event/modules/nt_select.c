
#include <nt_def.h>
#include <nt_core.h>
#include <nt_event.h>

static nt_int_t nt_select_init( nt_cycle_t *cycle ,nt_msec_t timer );
static void nt_select_done( nt_cycle_t *cycle );
static nt_int_t nt_select_add_event( nt_event_t *ev, nt_int_t event,
                                     nt_uint_t flags );
static nt_int_t nt_select_del_event( nt_event_t *ev, nt_int_t event,
                                     nt_uint_t flags );
static nt_int_t nt_select_process_events( nt_cycle_t *cycle, nt_msec_t  timer,
        nt_uint_t flags );

static char *nt_select_init_conf( nt_cycle_t *cycle, void *conf );


static fd_set         master_read_fd_set;
static fd_set         master_write_fd_set;
static fd_set         work_read_fd_set;
static fd_set         work_write_fd_set;

static nt_int_t     max_fd;     //连接fd总数
static nt_uint_t     nevents;   //记录事件总数
static nt_event_t  **event_index;   //存储事件

static nt_str_t     select_name = nt_string( "select" );

static nt_event_module_t  nt_select_module_ctx = {
    &select_name,
    NULL,                                  /* create configuration */
    nt_select_init_conf,                  /* init configuration */

    {
        nt_select_add_event,              /* add an event */
        nt_select_del_event,              /* delete an event */
        nt_select_add_event,              /* enable an event */
        nt_select_del_event,              /* disable an event */
        NULL,                              /* add an connection */
        NULL,                              /* delete an connection */
        NULL,                              /* trigger a notify */
        nt_select_process_events,         /* process the events */
        nt_select_init,                   /* init the events */
        nt_select_done,                   /* done the events */
        #if (NT_SSL && NT_SSL_ASYNC)
        NULL,                              /* add an async conn */
        NULL,                              /* del an async conn */
        #endif
    }

};

nt_module_t  nt_select_module = {
    NT_MODULE_V1,                         //配置索引
    &nt_select_module_ctx,                //模块索引
    NULL,                                 //模块名称
    NT_EVENT_MODULE,                      //模块类型
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NT_MODULE_V1_PADDING
};


static nt_int_t nt_select_init( nt_cycle_t *cycle ,nt_msec_t timer )
{
    nt_event_t  **index;

    if( event_index == NULL ) {
        FD_ZERO( &master_read_fd_set );
        FD_ZERO( &master_write_fd_set );
        nevents = 0;
    }

#if 0
    index = nt_alloc(  sizeof( nt_event_t * ) * 2 * cycle->connection_n ,
                       cycle->log);
#else
    cycle->connection_n = 256;
    index = nt_palloc(  cycle->pool, sizeof( nt_event_t * ) * 2 * cycle->connection_n );
#endif

    if( index == NULL ) {
        return NT_ERROR;

    }

    if( event_index ) {
        nt_memcpy( index, event_index, sizeof( nt_event_t * ) * nevents );
        nt_free( event_index );

    }

    event_index = index;


    //设置io函数为 os_io
    nt_io = nt_os_io;

    //设置event的事件函数为select函数
    nt_event_actions = nt_select_module_ctx.actions;

    nt_event_flags = NT_USE_LEVEL_EVENT;

    max_fd = -1;

    return NT_OK;
}

static void nt_select_done( nt_cycle_t *cycle )
{
    printf( "%s\n", __func__ );
    nt_free( event_index );
    event_index = NULL;
}



static nt_int_t nt_select_add_event( nt_event_t *ev, nt_int_t event, nt_uint_t flags )
{
    nt_connection_t  *c;

    c = ev->data;

    nt_log_debug1( NT_LOG_DEBUG_EVENT, ev->log, 0,
                   "c->fd =%d ", c->fd );



    nt_log_debug3( NT_LOG_DEBUG_EVENT, ev->log, 0,
                   "select add event fd:%d ev:%i, ev->write=%d", c->fd, event , ev->write);

    //判断事件的序号已经被设置，避免重复设置
    if( ev->index != NT_INVALID_INDEX ) {
        nt_log_error( NT_LOG_ALERT, ev->log, 0,
                      "select event fd:%d ev:%i is already set", c->fd, event );
        return NT_OK;
    }

    //如果ev处于write状态，不能推入READ事件，
    //如果ev不处于write状态，不能推入WRITE事件，
    if( ( event == NT_READ_EVENT && ev->write )
        || ( event == NT_WRITE_EVENT && !ev->write ) ) {
        nt_log_error( NT_LOG_ALERT, ev->log, 0,
                      "invalid select %s event fd:%d ev:%i",
                      ev->write ? "write" : "read", c->fd, event );
        return NT_ERROR;

    }

    //触发READ事件
    if( event == NT_READ_EVENT ) {
        FD_SET( c->fd, &master_read_fd_set );
        printf( "%s NT_READ_EVENT\n", __func__ );
        //触发WRITE事件
    } else if( event == NT_WRITE_EVENT ) {
        FD_SET( c->fd, &master_write_fd_set );

    }

    //设置max_fd等于当前连接fd
    if( max_fd != -1 && max_fd < c->fd ) {
        max_fd = c->fd;

    }

    //设置ev为活跃状态
    ev->active = 1;

    printf( "%d\n", nevents );
    //对ev赋值
    event_index[nevents] = ev;

    //设置ev序号
    ev->index = nevents;

    //序号加一
    nevents++;

    return NT_OK;
}

//删除事件的绑定， 即不会触发READ或WRITE
static nt_int_t nt_select_del_event( nt_event_t *ev, nt_int_t event, nt_uint_t flags )
{

    nt_event_t       *e;
    nt_connection_t  *c;

    c = ev->data;

    //设置ev 不活跃
    ev->active = 0;

    //该ev事件已经不是事件，不用重新设置
    if( ev->index == NT_INVALID_INDEX ) {
        return NT_OK;
    }

    nt_log_debug2( NT_LOG_DEBUG_EVENT, ev->log, 0,
                   "select del event fd:%d ev:%i", c->fd, event );

    //去除 READ 事件
    if( event == NT_READ_EVENT ) {
        FD_CLR( c->fd, &master_read_fd_set );

        //去除WRITE 事件
    } else if( event == NT_WRITE_EVENT ) {
        FD_CLR( c->fd, &master_write_fd_set );

    }

    //设置max_fd为不可用
    if( max_fd == c->fd ) {
        max_fd = -1;

    }

    //nevents减一，
    //ev->index序号
    /* 事件的序号 < (总事件数 - 1)
     * */
    if( ev->index < --nevents ) {
        //取出事件库中，序号为总事件数减一后的事件
        e = event_index[nevents];

        //上面得到的事件，赋值给当前序号，避免当前序号是个空事件
        event_index[ev->index] = e;

        //赋值序号
        e->index = ev->index;

    }

    //ev 序号设置为不可用
    ev->index = NT_INVALID_INDEX;

    return NT_OK;



}

static nt_int_t nt_select_process_events( nt_cycle_t *cycle, nt_msec_t  timer,nt_uint_t flags )
{

    int                ready, nready;
    nt_err_t          err;
    nt_uint_t         i, found;
    nt_event_t       *ev;
    nt_queue_t       *queue;
    struct timeval     tv, *tp;
    nt_connection_t  *c;


    //如果 max_fd 为-1， 从事件库从取得当前最新进来的fd
    if( max_fd == -1 ) {
        for( i = 0; i < nevents; i++ ) {
            c = event_index[i]->data;
            if( max_fd < c->fd ) {
                max_fd = c->fd;

            }

        }

        nt_log_debug1( NT_LOG_DEBUG_EVENT, cycle->log, 0,
                       "change max_fd: %i", max_fd );
    }


    if (timer == NT_TIMER_INFINITE) {
        tp = NULL;
    } else {
        tv.tv_sec = (long) (timer / 1000);
        tv.tv_usec = (long) ((timer % 1000) * 1000);
        tp = &tv;                                                                                                             }  


    nt_log_debug1( NT_LOG_DEBUG_EVENT, cycle->log, 0,
                   "select timer: %M", timer );

    work_read_fd_set = master_read_fd_set;
    work_write_fd_set = master_write_fd_set;

    ready = select( max_fd + 1, &work_read_fd_set, &work_write_fd_set, NULL, tp );

    err = ( ready == -1 ) ? nt_errno : 0;

  //  if (flags & NT_UPDATE_TIME || nt_event_timer_alarm) {
    if (flags & NT_UPDATE_TIME ) {
                  nt_time_update();                                                                    
    }   


    if( err ) {
        if( err == NT_EINTR ) {

        }

        if( err == NT_EBADF ) {
            //nt_select_repair_fd_sets(cycle);
        }
        return NT_ERROR;
    }

    if( ready == 0 ) {

        return NT_ERROR;
    }

    nready = 0;

    //遍历总事件
    for( i = 0; i < nevents; i++ ) {
        ev = event_index[i];
        c = ev->data;
        found = 0;

        //事件可写
        if( ev->write ) {
            if( FD_ISSET( c->fd, &work_write_fd_set ) ) {
                found = 1;
                nt_log_debug1( NT_LOG_DEBUG_EVENT, c->log, 0,
                               "select write %d", c->fd );
            }
            //事件可读
        } else {
            if( FD_ISSET( c->fd, &work_read_fd_set ) ) {
                found = 1;
                nt_log_debug1( NT_LOG_DEBUG_EVENT, c->log, 0,
                               "select read %d", c->fd );
            }
        }

        //如果触发事件
        if( found ) {
            //设置事件 ready为1
            ev->ready = 1;

            nt_log_debug1( NT_LOG_DEBUG_EVENT, c->log, 0,
                           "ev-accept %d", ev->accept );

            //判断当前事件的accept是否为1， 如果不是1，推入post事件
            queue = ev->accept ? &nt_posted_accept_events
                    : &nt_posted_events;

            //推入queue等候触发
            nt_post_event( ev, queue );

            nready++;
        }
    }

    if( ready != nready ) {
        nt_log_error( NT_LOG_ALERT, cycle->log, 0,
                      "select ready != events: %d:%d", ready, nready );

        //nt_select_repair_fd_sets(cycle);
    }

    return NT_OK;
}


//select模块的配置初始化
static char *nt_select_init_conf( nt_cycle_t *cycle, void *conf )
{
    nt_event_conf_t  *ecf;

    ecf = nt_event_get_conf(cycle->conf_ctx, nt_event_core_module);

    if (ecf->use != nt_select_module.ctx_index) {
        return NT_CONF_OK;

    }   

    /* disable warning: the default FD_SETSIZE is 1024U in FreeBSD 5.x */

    if (cycle->connection_n > FD_SETSIZE) {
        nt_log_error(NT_LOG_EMERG, cycle->log, 0,
                      "the maximum number of files "
                      "supported by select() is %ud", FD_SETSIZE);
        return NT_CONF_ERROR;

    }   

    return NT_CONF_OK;
}
