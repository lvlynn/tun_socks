#include "nt_core.h"


/* volatile nt_cycle_t *nt_cycle;   */


typedef struct {
    nt_str_t servername;
    nt_int_t seq;
} TestWildcardHashNode;


int main()
{
    nt_log_t *log;
    nt_pool_t *p;
    nt_hash_init_t hash = {0};
    nt_hash_keys_arrays_t ha = {0};
    nt_hash_combined_t combinedHash = {0}; //支持通配符的散列表
    int i = 0;
    int ret = 0 ;

    nt_time_init();
    log = nt_log_init( NULL );
    log->log_level = NT_LOG_DEBUG_ALL;

    nt_log_debug1( NT_LOG_DEBUG_EVENT, log, 0,
                   "timer delta: %M", nt_current_msec );




    p = nt_create_pool( NT_DEFAULT_POOL_SIZE, log ); //16KB
    if( p == NULL ) {
        printf( "nt_create_pool error\r\n" );
        return NT_ERROR;
    }

    nt_memzero( &ha, sizeof( nt_hash_keys_arrays_t ) );

    ha.temp_pool = nt_create_pool( NT_DEFAULT_POOL_SIZE, log );
    if( ha.temp_pool == NULL ) {
        printf( "nt_create_pool error\r\n" );
        return NT_ERROR;
    }
    ha.pool = p;

    if( nt_hash_keys_array_init( &ha, NT_HASH_LARGE ) != NT_OK ) {
        return NT_ERROR;
    }

    TestWildcardHashNode testnode[4] = {0};
    //这里不能用常量区的字符串来初始化nt_str变量，应该申请内存来存放所需的字符串，因为nt_hash_add_key方法可能会将字符串的内容改变（大写变小写）
    char *d0 = "*.test.com";
    /* char *d0 = "www.test0.com"; */
    testnode[0].servername.data = nt_pcalloc( p, nt_strlen( d0 ) );
    testnode[0].servername.len = strlen( d0 );
    nt_memcpy( testnode[0].servername.data,  d0, testnode[0].servername.len );

    char *d1 = "*.www.test.com";
    /* char *d1 = "www.test1.com"; */
    testnode[1].servername.data = nt_pcalloc( p, nt_strlen( d1 ) );
    testnode[1].servername.len = strlen( d1 );
    nt_memcpy( testnode[1].servername.data,  d1, testnode[1].servername.len );


    char *d2 = "www.test.*";
    /* char *d1 = "www.test1.com"; */
    testnode[2].servername.data = nt_pcalloc( p, nt_strlen( d2 ) );
    testnode[2].servername.len = strlen( d2 );
    nt_memcpy( testnode[2].servername.data,  d2, testnode[2].servername.len );

    char *d3 = "www.test.com";
    /* char *d1 = "www.test1.com"; */
    testnode[3].servername.data = nt_pcalloc( p, nt_strlen( d3 ) );
    testnode[3].servername.len = strlen( d3 );
    nt_memcpy( testnode[3].servername.data,  d3, testnode[3].servername.len );




    //添加到ha中
    for( i = 0; i < 4; i++ ) {
        testnode[i].seq = i;
        ret = nt_hash_add_key( &ha, &testnode[i].servername, &testnode[i], NT_HASH_WILDCARD_KEY ); //NT_HASH_WILDCARD_KEY表明可以处理带通配符的关键字
        debug( "ret=%d", ret );
    }

    nt_cacheline_size = 32; //必须包含这句否则会段错误
    hash.key = nt_hash_key_lc;
    hash.max_size = 1000;
    hash.bucket_size = 48;
    hash.name = "my_hash_test";
    hash.pool = p;

    //初始化精确匹配散列表
    if( ha.keys.nelts ) {
        debug( "hash nelts=%d", ha.keys.nelts );
        hash.hash = &combinedHash.hash;
        hash.temp_pool = NULL;
        if( nt_hash_init( &hash, ha.keys.elts, ha.keys.nelts ) != NT_OK )
            return NT_ERROR;
    }

    #if 1
    //初始化前置通配符散列表
    if( ha.dns_wc_head.nelts ) {
        hash.hash = NULL;

        debug( "hash wc head nelts=%d", ha.dns_wc_head.nelts );
        //nt_hash_wildcard_init方法需要使用临时内存池
        hash.temp_pool = ha.temp_pool;
        if( nt_hash_wildcard_init( &hash, ha.dns_wc_head.elts, ha.dns_wc_head.nelts ) != NT_OK ) {
            return NT_ERROR;
        }

        combinedHash.wc_head = ( nt_hash_wildcard_t * )hash.hash;
    }

    //初始化后置通配符散列表
    if( ha.dns_wc_tail.nelts ) {
        hash.hash = NULL;

        debug( "hash wc tail nelts=%d", ha.dns_wc_tail.nelts );
        //nt_hash_wildcard_init方法需要使用临时内存池
        hash.temp_pool = ha.temp_pool;
        if( nt_hash_wildcard_init( &hash, ha.dns_wc_tail.elts, ha.dns_wc_tail.nelts ) != NT_OK ) {
            return NT_ERROR;
        }

        combinedHash.wc_tail = ( nt_hash_wildcard_t * )hash.hash;
    }

    #endif
    //此时临时内存池已经没有存在的意义了
    nt_destroy_pool( ha.temp_pool );
    #if 1

    char *test = "www.test.org";
    /* char *test = "www.test.com"; */
    nt_str_t findServer ={0};
    findServer.data = nt_pcalloc( p, nt_strlen( test ) );
    findServer.len = nt_strlen( test );
    nt_memcpy( findServer.data, test, nt_strlen( test ) );


    #endif
    #if 1
    #endif

    nt_uint_t hashkey = 0;
    hashkey = nt_hash_key_lc( findServer.data, findServer.len );
    printf( "hash_key %ld\r\n", hashkey );
    #if 1
    nt_strlow( findServer.data, findServer.data, findServer.len );

    TestWildcardHashNode *res = NULL;
    res = nt_hash_find_combined( &combinedHash,
                                 hashkey,
                                 findServer.data,
                                 findServer.len );
    if( NULL == res ) {
        printf( "not find\r\n" );
    } else {
        printf( "servername: %s ,seq: %d\r\n", res->servername.data, res->seq );
    }

    #endif
//    nt_destroy_pool(p);
    return 0;
}
