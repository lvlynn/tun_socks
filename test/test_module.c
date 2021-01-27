#include <nt_core.h>
#include <nt_stream.h>

// https://www.cnblogs.com/codestack/archive/2004/01/13/12154050.html
/*
 * 本测试文件用于测试stream 底下的 test配置模块。
 * 配置信息分成了几个作用域(scope,有时也称作上下文)，这就是main, server, 以及location
 * */

// 每个模块都有一个属于自己的配置
static void * nt_stream_test_create_srv_conf( nt_conf_t *cf );
static char * nt_stream_test_merge_srv_conf( nt_conf_t *cf, void *parent, void *child );
static nt_int_t nt_stream_test_init( nt_conf_t *cf );

static void *nt_stream_test_create_loc_conf( nt_conf_t *cf );

static char *nt_stream_test_string( nt_conf_t *cf, nt_command_t *cmd,
                                    void *conf );
static char *nt_stream_test_counter( nt_conf_t *cf, nt_command_t *cmd,
                                     void *conf );



typedef struct {
    nt_str_t hello_string;
    nt_int_t hello_counter;

} nt_stream_test_loc_conf_t;


typedef struct {
    nt_uint_t test;
    nt_str_t hello_string;
    nt_int_t hello_counter;
} nt_stream_test_srv_conf_t;


typedef struct {

} nt2_stream_test_srv_conf_t;


//stream 没有location

static nt_command_t nt_stream_test_commands[] = {

    {
        nt_string( "socks5_pass" ),
        NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_stream_test_string,
        NT_STREAM_SRV_CONF_OFFSET,
        0,
        NULL

    },
    {
        nt_string( "hello_string" ),
        NT_STREAM_SRV_CONF | NT_CONF_NOARGS | NT_CONF_TAKE1,
        nt_stream_test_string,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_test_srv_conf_t, hello_string ),
        NULL
    },

    {
        nt_string( "hello_counter" ),
        NT_STREAM_SRV_CONF | NT_CONF_FLAG,
        nt_stream_test_counter,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_test_srv_conf_t, hello_counter ),
        NULL
    },

    nt_null_command
};



typedef struct {
    nt_int_t ( *preconfiguration )( nt_conf_t *cf );
    nt_int_t ( *postconfiguration )( nt_conf_t *cf );

    void                        *( *create_main_conf )( nt_conf_t *cf );
    char                        *( *init_main_conf )( nt_conf_t *cf, void *conf );

    void                        *( *create_srv_conf )( nt_conf_t *cf );
    char                        *( *merge_srv_conf )( nt_conf_t *cf, void *prev,
            void *conf );
    void       *( *create_loc_conf )( nt_conf_t *cf );
    char       *( *merge_loc_conf )( nt_conf_t *cf, void *prev, void *conf );
} nt_stream_test_module_t;



/*
preconfiguration:
    在创建和读取该模块的配置信息之前被调用。
postconfiguration:
    在创建和读取该模块的配置信息之后被调用。
create_main_conf:
    调用该函数创建本模块位于http block的配置信息存储结构。该函数成功的时候，返回创建的配置对象。失败的话，返回NULL。
init_main_conf:
    调用该函数初始化本模块位于http block的配置信息存储结构。该函数成功的时候，返回NT_CONF_OK。失败的话，返回NT_CONF_ERROR或错误字符串。
create_srv_conf:
    调用该函数创建本模块位于http server block的配置信息存储结构，每个server block会创建一个。该函数成功的时候，返回创建的配置对象。失败的话，返回NULL。
merge_srv_conf:
    因为有些配置指令既可以出现在http block，也可以出现在http server block中。那么遇到这种情况，每个server都会有自己存储结构来存储该server的配置，但是在这种情况下http block中的配置与server block中的配置信息发生冲突的时候，就需要调用此函数进行合并，该函数并非必须提供，当预计到绝对不会发生需要合并的情况的时候，就无需提供。当然为了安全起见还是建议提供。该函数执行成功的时候，返回NT_CONF_OK。失败的话，返回NT_CONF_ERROR或错误字符串。
create_loc_conf:
    调用该函数创建本模块位于location block的配置信息存储结构。每个在配置中指明的location创建一个。该函数执行成功，返回创建的配置对象。失败的话，返回NULL。
merge_loc_conf:
    与merge_srv_conf类似，这个也是进行配置值合并的地方。该函数成功的时候，返回NT_CONF_OK。失败的话，返回NT_CONF_ERROR或错误字符串。

*/


static nt_stream_test_module_t nt_stream_test_module_ctx = {
    NULL,                                  /* preconfiguration */
    nt_stream_test_init,                    /* postconfiguration */
    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    nt_stream_test_create_srv_conf,      /* create server configuration */
    nt_stream_test_merge_srv_conf,        /* merge server configuration */
    NULL,
    NULL
};

nt_module_t nt_stream_test_module = {
    NT_MODULE_V1,
    &nt_stream_test_module_ctx,    /* module context */
    nt_stream_test_commands,       /* module directives */
    NT_STREAM_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    NULL,                          /* exit master */
    NT_MODULE_V1_PADDING

};

static void *
nt_stream_test_create_srv_conf( nt_conf_t *cf )
{
    // nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0 ,"%s start", __func__ );
    nt_stream_test_srv_conf_t  *conf;

    conf = nt_pcalloc( cf->pool, sizeof( nt_stream_test_srv_conf_t ) );
    if( conf == NULL ) {
        return NULL;
    }

    conf->test = NT_CONF_UNSET;
}

static char *
nt_stream_test_merge_srv_conf( nt_conf_t *cf, void *parent, void *child )
{
    nt_stream_test_srv_conf_t *prev = parent;
    nt_stream_test_srv_conf_t *conf = child;

    nt_conf_merge_uint_value( conf->test,
                              prev->test, 1 );

}

static char *
nt_stream_test_string( nt_conf_t *cf, nt_command_t *cmd, void *conf )
{

    nt_stream_test_loc_conf_t* local_conf;


    local_conf = conf;
    char* rv = nt_conf_set_str_slot( cf, cmd, conf );

    nt_conf_log_error( NT_LOG_EMERG, cf, 0, "hello_string:%s", local_conf->hello_string.data );

    return rv;
}


static char *nt_stream_test_counter( nt_conf_t *cf, nt_command_t *cmd,
                                     void *conf )
{
    nt_stream_test_loc_conf_t* local_conf;

    local_conf = conf;

    char* rv = NULL;

    rv = nt_conf_set_flag_slot( cf, cmd, conf );


    nt_conf_log_error( NT_LOG_EMERG, cf, 0, "hello_counter:%d", local_conf->hello_counter );
    return rv;
}


static nt_int_t
nt_stream_test_handler( nt_stream_session_t *s )
{
    debug( "test" );
}

static nt_int_t
nt_stream_test_init( nt_conf_t *cf )
{
    //  nt_http_handler_pt        *h;
    //  nt_http_core_main_conf_t  *cmcf;

    nt_stream_core_srv_conf_t          *cscf;
    cscf = nt_stream_conf_get_module_srv_conf( cf, nt_stream_core_module );
    cscf->handler = nt_stream_test_handler; //回调处理函数

    /* cmcf = nt_http_conf_get_module_main_conf(cf, nt_http_core_module);

    h = nt_array_push(&cmcf->phases[NT_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NT_ERROR;
    }

    *h = nt_stream_test_handler; */

    return NT_OK;
}

//core create_conf
void create_conf(){
#if 0
	for (i = 0; cycle->modules[i]; i++) {
		if (cycle->modules[i]->type != NT_CORE_MODULE) {
			continue;
		}

		module = cycle->modules[i]->ctx;

		if (module->create_conf) {
			rv = module->create_conf(cycle);
			if (rv == NULL) {
				nt_destroy_pool(pool);
				return NULL;
			}                                                                                                                 
			cycle->conf_ctx[cycle->modules[i]->index] = rv;
		}
	}
#endif
}

//core init_conf
void init_conf(){
#if 0
	for( i = 0; cycle->modules[i]; i++ ) {
		if( cycle->modules[i]->type != NT_CORE_MODULE ) {
			continue;
		}

		module = cycle->modules[i]->ctx;

		if( module->init_conf ) {
			if( module->init_conf( cycle,
								   cycle->conf_ctx[cycle->modules[i]->index] )
				== NT_CONF_ERROR ) {
				environ = senv;
				nt_destroy_cycle_pools( &conf );
				return NULL;
			}
		}
	}
#endif
}



void init_process(){

#if 0
    if (cycle->modules[i]->init_process) {
        if (cycle->modules[i]->init_process(cycle) == NT_ERROR) {
            /* fatal */
            exit(2);

        }

    }
#endif

}

void exit_process(){
#if 0
    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->exit_process) {
            cycle->modules[i]->exit_process(cycle);

        }

    }
#endif
}

//ngx_http_block
//ngx_stream_block
void block_init(){
#if 0
    preconfiguration();

    create_main_conf();
    create_srv_conf();
    create_loc_conf();

    init_main_conf();
    merge_srv_conf();

    init_loc_conf();
    merge_loc_conf();

    postconfiguration();
#endif

}

void main()
{
#if 0
    //模块启动流程如下：
	//对模块数据初始化
	nt_preinit_modules();

    //nt_cycle.c
	//把所有的静态模块添加到cycle的modules中来，为了后面调用做准备
	if (nt_cycle_modules(cycle) != NT_OK) {
		nt_destroy_pool(pool);
		return NULL;
	}

    //NGX_CORE_MODULE 调用CORE类型的模块。 如http stream等 
    //调用模块中的 create_conf
    create_conf();

    /* 在 nginx 初始化的过程中，执行了 ngx_init_cycle 函数，其中进行了配置文件解析，调用了 ngx_conf_parse 函数
    函数 ngx_conf_handler 根据配置项的 command 调用了对应的 set 回调函数
    ngx_conf_parse -> ngx_conf_handler 
    //通过调用ngx_http_block方法，解析{}中的HTTP模块配置 */
    block_init();

    //调用模块中的 init_conf
    init_conf();



    init_process();
    exit_process();
#endif
}
