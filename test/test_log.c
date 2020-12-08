#include <nt_core.h>

 u_char      *nt_prefix = NULL;


 nt_cycle_t *init_conf( nt_conf_t *conf , nt_log_t  *log){

	 void                *rv;
	 char               **senv;
	 nt_uint_t           i, n;
//	 nt_log_t           *log;
	 nt_time_t          *tp;
//	 nt_conf_t           conf;
	 nt_pool_t          *pool;
	 nt_cycle_t         *cycle, **old;
	 nt_list_part_t     *part, *opart;
	 nt_open_file_t     *file;
	 nt_listening_t     *ls, *nls;
//	 nt_core_conf_t     *ccf, *old_ccf;
	 nt_core_module_t   *module;
	 char                 hostname[NT_MAXHOSTNAMELEN];


	 //初始化一个内存池
	 pool = nt_create_pool(NT_CYCLE_POOL_SIZE, log);                                         
     if (pool == NULL) {                                                                       
         return NULL;                                                                          
      }                                                                                         
      pool->log = log;                                                                          
                      
	//从内存池中申请cycle                                                                          
      cycle = nt_pcalloc(pool, sizeof(nt_cycle_t));                                           
     if (cycle == NULL) {                                                                      
          nt_destroy_pool(pool);                                                                                              
         return NULL;                                                                          
      }                  

	pool->log = log;

	cycle->pool = pool;                                                                       
	cycle->log = log;                                                                         
	cycle->old_cycle = NULL;


	//初始化路径存储变量paths
	n =  10;
	if (nt_array_init(&cycle->paths, pool, n, sizeof(nt_path_t *))
		!= NT_OK)
	{
		nt_destroy_pool(pool);
		return NULL;
	}

	nt_memzero(cycle->paths.elts, n * sizeof(nt_path_t *));


	//初始化config_dump
	if (nt_array_init(&cycle->config_dump, pool, 1, sizeof(nt_conf_dump_t))
		!= NT_OK)
	{
		nt_destroy_pool(pool);
		return NULL;
	}

	nt_rbtree_init(&cycle->config_dump_rbtree, &cycle->config_dump_sentinel,
					nt_str_rbtree_insert_value);


	//初始化文件开启变量open_files
	n = 20;
	if (nt_list_init(&cycle->open_files, pool, n, sizeof(nt_open_file_t))                                                  
		!= NT_OK)
	{
		nt_destroy_pool(pool);
		return NULL;
	}

	/* cycle->conf_ctx = nt_pcalloc(pool, nt_max_module * sizeof(void *));
	if (cycle->conf_ctx == NULL) {
		nt_destroy_pool(pool);
		return NULL;
	} */


	nt_memzero(conf, sizeof(nt_conf_t));
	//初始化 conf args
	/* STUB: init array ? */
	conf->args = nt_array_create(pool, 10, sizeof(nt_str_t));
	if (conf->args == NULL) {
		nt_destroy_pool(pool);
		return NULL;
	}

	conf->temp_pool = nt_create_pool(NT_CYCLE_POOL_SIZE, log);
	if (conf->temp_pool == NULL) {
		nt_destroy_pool(pool);
		return NULL;
	}


	conf->ctx = cycle->conf_ctx;
	conf->cycle = cycle;
	conf->pool = pool;
	conf->log = log;
	conf->module_type = NT_CORE_MODULE;
	conf->cmd_type = NT_MAIN_CONF;

	conf->args->nelts = 0;
	
	nt_str_t *word = nt_array_push( conf->args ); 
	word->data = nt_pnalloc( conf->pool, 9 );
	word->len = nt_copy( word->data, "error_log" ,   9 );
	
    word = nt_array_push( conf->args  );
    word->data = nt_pnalloc( conf->pool, 16  );
    word->len = nt_copy( word->data, "/tmp/dl/test.log" ,   16  );

    printf( "word=%s\n",  word->data );
	return cycle;
 }

#define nt_log_debug1(level, log, err, fmt, arg1)                            \
    if ((log)->log_level & level)                                             \
    nt_log_debug_core(log, err, fmt, arg1)



int main()
{
    nt_log_t        *log;

    nt_time_init();
    log = nt_log_init( nt_prefix  );

    if ( log == NULL  ) {
        printf( "log init fail\n" );
        return 1;
    }  

    //打印到stderr
     nt_log_stderr( 0, "[nt_%s]  test nt_log_stderr ",
                                                   "log" );

    //测试错误日志打印，
    nt_log_error( NT_LOG_NOTICE, log, 0,
                                        "[nt_log]  test nt_log_error %s", "." );

    //设置log配置
    nt_conf_t *cf = ( nt_conf_t * ) malloc( sizeof( nt_conf_t ) );


    init_conf( cf , log);
    nt_str_t *value = cf->args->elts ;

    printf( "value=%s\n", value[1].data );
    printf( "log fd =%d\n", log->file->fd );

    nt_log_t    *error_log = log; 

    //  error_log->log_level = NT_LOG_DEBUG_ALL;
    //  error_log = nt_log_init( "/tmp/dl/test.log"  );
    
//    char * ret = nt_log_set_log(cf, &error_log);
//    printf( "ret=%s\n", ret );
    printf( "log name =%s\n", error_log->file->name );
    printf( "log fd =%d\n", error_log->file->fd );

    error_log->log_level = NT_LOG_DEBUG_ALL;
    printf( "log_level=%d\n", error_log->log_level );
    printf( "log_level=%d\n", error_log->log_level & NT_LOG_DEBUG_EVENT );

    nt_log_debug1( NT_LOG_DEBUG_EVENT, error_log, 0,
                                          "timer delta: %M", nt_current_msec );

    return 0;
}

