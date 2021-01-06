
#include "nt_core.h"

volatile nt_cycle_t *nt_cycle;

static nt_str_t names[] = {nt_string( "rainx" ), nt_string( "xiaozhe" ), nt_string( "zhoujian" )};
static char* descs[] = {"rainx's id is 1", "xiaozhe's id is 2", "zhoujian's id is 3"};

// hash table的一些基本操作
int main()
{
    nt_uint_t k; //, p, h;
    nt_pool_t* pool;
    nt_hash_init_t hash_init;
    nt_hash_t* hash;
    nt_array_t* elements;
    nt_hash_key_t* arr_node;
    char* find;
    int i;

    nt_log_t *log;
    nt_pool_t *p;

    nt_time_init();
    log = nt_log_init( NULL );
    log->log_level = NT_LOG_DEBUG_ALL;

    nt_cacheline_size = 8;
// hash key cal start
//    nt_str_t str       = nt_string( "hello, world" );
//    k                   = nt_hash_key_lc( str.data, str.len );
    pool                = nt_create_pool( 1024 * 1, log );
    printf( "caculated key is %u \n", k );
// hask key cal end
//
    hash = ( nt_hash_t* ) nt_pcalloc( pool, sizeof( hash ) );
    hash_init.hash      = hash;                      // hash结构
    hash_init.key       = &nt_hash_key_lc;          // hash算法函数
    hash_init.max_size  = 128 * 1;                 // max_size
    hash_init.bucket_size = 24; // nt_align(64, nt_cacheline_size);
    hash_init.name      = "yahoo_guy_hash";          // 在log里会用到
    hash_init.pool           = pool;                 // 内存池
    hash_init.temp_pool      = NULL;

// 创建数组

    elements = nt_array_create( pool, 3, sizeof( nt_hash_key_t ) );
    for( i = 0; i < 3; i++ ) {
        arr_node            = ( nt_hash_key_t* ) nt_array_push( elements );
        arr_node->key       = ( names[i] );
        arr_node->key_hash  = nt_hash_key_lc( arr_node->key.data, arr_node->key.len );
        arr_node->value     = ( void* ) descs[i];
        //
        printf( "key: %s , key_hash: %u\n", arr_node->key.data, arr_node->key_hash );
    }

    if( nt_hash_init( &hash_init, ( nt_hash_key_t* ) elements->elts, elements->nelts ) != NT_OK ) {
        return 1;
    }

    debug( "hash_init.hash.size=%d", hash_init.hash->size );
// 查找
    k    = nt_hash_key_lc( names[0].data, names[0].len );
    printf( "%s key is %d\n", names[0].data, k );
    find = ( char* )
           nt_hash_find( hash, k, ( u_char* ) names[0].data, names[0].len );

    if( find ) {
        printf( "get desc of rainx: %s\n", ( char* ) find );
    }

    nt_array_destroy( elements );
    nt_destroy_pool( pool );

    return 0;
}
