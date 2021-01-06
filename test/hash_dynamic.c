#include "nt_core.h"


/* volatile nt_cycle_t *nt_cycle;   */

#define random(x) (rand()%x)


void rand_str( char *out, int n )
{
    //srand(utime(NULL));
    int i = 0;
    for( i = 0; i < n; i++ ) {

        *( out + i ) = 97 + random( 26 );
    }

    *( out + n - 1 ) = '\0';
}



int main( int argc, char **argv )
{
    nt_log_t *log;
    nt_pool_t *p;
    hash_t *hash;
    u_short ret;
    int i = 0;

    nt_time_init();
    log = nt_log_init( NULL );
    log->log_level = NT_LOG_DEBUG_ALL;

    nt_log_debug1( NT_LOG_DEBUG_EVENT, log, 0,
                   "timer delta: %M", nt_current_msec );

    p = nt_create_pool( 1024 * 4, log ); //16KB
    /* p = nt_create_pool(100, log); //16KB */
    if( p == NULL ) {
        printf( "nt_create_pool error\r\n" );
        return NT_ERROR;
    }

    hash = ( nt_hash_init_t * )nt_palloc( p, sizeof( hash_t ) );

    hash->pool = p;

    hash->temp_pool = nt_create_pool( 1024 * 1000, log ); //16KB

    debug( "size=%d", sizeof( hash_elt_t ) );

    ret = hash_init( hash, 10000 );
    if( ret != NT_OK ) {
        return NT_ERROR;
    }

    char *d0 = "baidu";
    char key[ 12 ] = { 0 };
    /* nt_acc_elt_t *query; */
    char *query;

    //添加
    for( i = 0; i < 8000; i++ ) {
        rand_str( key, 12 );
        if( hash_add( hash,  key, key ) != NT_OK ) {
            debug( "add fail" );
            continue ;
        }
    }

    debug( "pool-max=%d", p->max );

    //查询
    query = hash_query( hash,  "baidu" );
    if( query ){
        debug( "query=%s", query );
        debug( "找到了" );
    }
    //删除

    if( hash_del( hash,  key ) != NT_OK ) {
        debug( "del fail" );
        return ;
    }

//查询
    query = hash_query( hash,  key );
    debug( "query=%s", query );

    //全打印
//    nt_hash_dump( hinit.hash  );
    hash_free( hash );
    nt_destroy_pool( p );
    return 0;
}
