#include "nt_core.h"


/* volatile nt_cycle_t *nt_cycle;   */
/*
 *  本程序用于测试域名识别，判断加速域名和非加速域名
 *
 * */

typedef struct {
    nt_str_t servername;
    nt_int_t seq;
}  ;

//char DOMAIN_DIRECT[2]="0";
//char DOMAIN_PROXY[2]="1";
int DOMAIN_DIRECT=101;
int DOMAIN_PROXY=100;

nt_int_t hast_add_domain( char *path, nt_hash_keys_arrays_t *ha )
{

    FILE *f;
    char line[128] = {0};
    nt_array_t* elements = NULL;
    nt_hash_key_t* arr_node = NULL;
    nt_str_t *key = NULL;
    int type = 0;
    int ret = 0;

    f = fopen( path, "r" );
    if( f == NULL ) {
        printf( "open file error\n" );
        return -1;
    }

    

    while( fgets( line, sizeof( line ), f) != NULL ) {
        //NT_HASH_WILDCARD_KEY表明可以处理带通配符的关键字
        
        key = nt_pcalloc( ha->pool, sizeof( nt_str_t ) );
        
        key->len = nt_strlen( line ) - 1 ;

        if( key->len < 1 )
            continue;

        if( *line == '#' )
            continue;

        if( *line == '[' && *(line + 1) == 'e'){
            type = 0;
            break;
        } 

        if( *line == '[' && *(line + 1) == 'd'){
            type = 0;
            continue;
        } else if( *line == '[' && *(line + 1) == 'p' ){
            type = 1;
            continue;
        }

        key->data = nt_pcalloc( ha->pool, key->len);
        nt_memcpy( key->data, line, key->len);

//        debug( "line=%s, len=%d", line , key->len);
        if( type )
            ret =  nt_hash_add_key( ha, key, &DOMAIN_PROXY, NT_HASH_WILDCARD_KEY );
        else
            ret =  nt_hash_add_key( ha, key, &DOMAIN_DIRECT, NT_HASH_WILDCARD_KEY );

            /* ret =  nt_hash_add_key( ha, key, key->data, NT_HASH_WILDCARD_KEY ); */

    //   debug(  "ret=%d", ret);
    }

    fclose( f );
    return NT_OK;
}

int main( int argc, char **argv )
{
    nt_log_t *log;
    nt_pool_t *p;
    nt_hash_init_t hinit = {0};
    nt_hash_keys_arrays_t ha = {0};
    nt_hash_combined_t combinedHash = {0}; //支持通配符的散列表
    int i = 0;

    nt_time_init();
    log = nt_log_init( NULL );
    log->log_level = NT_LOG_DEBUG_ALL;

    nt_log_debug1( NT_LOG_DEBUG_EVENT, log, 0,
                   "timer delta: %M", nt_current_msec );

    nt_memzero( &ha, sizeof( nt_hash_keys_arrays_t ) );
    //申请hash内存
    p = nt_create_pool( NT_DEFAULT_POOL_SIZE, log ); //16KB
    if( p == NULL ) {
        printf( "nt_create_pool error\r\n" );
        return NT_ERROR;
    }
    ha.pool = p;


    //申请临时内存
    ha.temp_pool = nt_create_pool( NT_DEFAULT_POOL_SIZE, log );
//    nt_memzero( ha.temp_pool, NT_DEFAULT_POOL_SIZE);
    if( ha.temp_pool == NULL ) {
        printf( "nt_create_pool error\r\n" );
        return NT_ERROR;
    }


    //初始化key用的数组， NT_HASH_LARGE 代表size为10000
    if( nt_hash_keys_array_init( &ha, NT_HASH_SMALL ) != NT_OK ) {
        return NT_ERROR;
    }

    //这里不能用常量区的字符串来初始化nt_str变量，应该申请内存来存放所需的字符串，因为nt_hash_add_key方法可能会将字符串的内容改变（大写变小写）
    /* char *d0 = "*.test.com"; */

    hast_add_domain( argv[1], &ha );

    nt_cacheline_size = 4; //必须包含这句否则会段错误
    hinit.key = nt_hash_key;
    hinit.max_size = 8000;
    hinit.bucket_size = 80;
    hinit.name = "my_hash_test";
    hinit.pool = p;

    //初始化精确匹配散列表
    if( ha.keys.nelts ) {
        hinit.hash = &combinedHash.hash;
        hinit.temp_pool = NULL;
        debug( "hash nelts=%d", ha.keys.nelts );
        if( nt_hash_init( &hinit, ha.keys.elts, ha.keys.nelts ) != NT_OK )
            return NT_ERROR;
    }

    debug( "hash_init.hash.size=%d", hinit.hash->size );


    
    //初始化前置通配符散列表
    if( ha.dns_wc_head.nelts ) {
        hinit.hash = NULL;

        debug( "hash wc head nelts=%d", ha.dns_wc_head.nelts );
        //nt_hash_wildcard_init方法需要使用临时内存池
        hinit.temp_pool = ha.temp_pool;
        if( nt_hash_wildcard_init( &hinit, ha.dns_wc_head.elts, ha.dns_wc_head.nelts ) != NT_OK ) {
            return NT_ERROR;
        }

        combinedHash.wc_head = ( nt_hash_wildcard_t * )hinit.hash;
    }

    //初始化后置通配符散列表
    if( ha.dns_wc_tail.nelts ) {
        hinit.hash = NULL;

        debug( "hash wc tail nelts=%d", ha.dns_wc_tail.nelts );
        //nt_hash_wildcard_init方法需要使用临时内存池
        hinit.temp_pool = ha.temp_pool;
        if( nt_hash_wildcard_init( &hinit, ha.dns_wc_tail.elts, ha.dns_wc_tail.nelts ) != NT_OK ) {
            return NT_ERROR;
        }

        combinedHash.wc_tail = ( nt_hash_wildcard_t * )hinit.hash;
    }

    //此时临时内存池已经没有存在的意义了
    nt_destroy_pool( ha.temp_pool );



    #if 1
//   char *test = "WWW.test.org";
    char *test = "baidu.com";
    nt_str_t findServer = {0};
    findServer.data = nt_pcalloc( p, nt_strlen( test ) );
    findServer.len = nt_strlen( test );
    nt_memcpy( findServer.data, test, nt_strlen( test ) );
    #endif

    nt_uint_t hashkey = 0;
    hashkey = nt_hash_key_lc( findServer.data, findServer.len );
    printf( "hash_key %ld\r\n", hashkey );
    #if 1
    nt_strlow( findServer.data, findServer.data, findServer.len );

    char *res = NULL; 
    res = nt_hash_find_combined( &combinedHash,
                               hashkey,
                               findServer.data,
                               findServer.len );
    if( NULL == res ) {
        printf( "not find\r\n" );
    } else {
        /* printf( "servername: %d \r\n", *res ); */
        printf( "servername: %s \r\n", res );
    }

    #endif
    pause();
    nt_destroy_pool( p );
    return 0;
}
