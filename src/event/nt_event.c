#include <nt_def.h>
#include <nt_core.h>
#include <nt_event.h>

//处理select、 poll、 epoll 网络模型
//定义在modules/nt_select.c中
extern nt_module_t nt_select_module;

//event的flags值
nt_uint_t            nt_event_flags;

//event的回调函数集合
nt_event_actions_t   nt_event_actions;

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

