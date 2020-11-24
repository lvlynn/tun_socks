
#include <nt_def.h>
#include <nt_core.h>
#include <nt_event.h>

static nt_int_t nt_select_init( nt_cycle_t *cycle );
static void nt_select_done( nt_cycle_t *cycle );
static nt_int_t nt_select_add_event( nt_event_t *ev, nt_int_t event,
                                            nt_uint_t flags );
static nt_int_t nt_select_del_event( nt_event_t *ev, nt_int_t event,
                                            nt_uint_t flags );
static nt_int_t nt_select_process_events( nt_cycle_t *cycle,
             nt_uint_t flags );

static char *nt_select_init_conf(nt_cycle_t *cycle, void *conf);


static fd_set         master_read_fd_set;
static fd_set         master_write_fd_set;
static fd_set         work_read_fd_set;
static fd_set         work_write_fd_set;

static nt_int_t      max_fd;
static nt_str_t           select_name = nt_string("select");

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


static nt_int_t nt_select_init( nt_cycle_t *cycle )
{

    FD_ZERO( &master_read_fd_set );
    FD_ZERO( &master_write_fd_set );

    //设置io函数为 os_io
    nt_io = nt_os_io;

    //设置event的事件函数为select函数
    nt_event_actions = nt_select_module_ctx.actions;
}

static void nt_select_done( nt_cycle_t *cycle )
{

}



static nt_int_t nt_select_add_event( nt_event_t *ev, nt_int_t event, nt_uint_t flags )
{



}

static nt_int_t nt_select_del_event( nt_event_t *ev, nt_int_t event, nt_uint_t flags )
{



}

static nt_int_t nt_select_process_events( nt_cycle_t *cycle, nt_uint_t flags )
{

    int                ready, nready;

    struct timeval     tv, *tp;

    int timer = 1;
    tv.tv_sec = ( long )( timer / 1000 );
    tv.tv_usec = ( long )( ( timer % 1000 ) * 1000 );
    tp = &tv;
    ready = select( max_fd + 1, &work_read_fd_set, &work_write_fd_set, NULL, tp );

}



static char *nt_select_init_conf(nt_cycle_t *cycle, void *conf){


}
