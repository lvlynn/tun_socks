#include "nt_core.h"

nt_log_t *log;
int HASH_TOP_DOMAIN_GENERY = 0;
int HASH_TOP_DOMAIN_COUNTRY = 1;
hash_t hash_top_domain;



/* volatile nt_cycle_t *nt_cycle;   */

#define random(x) (rand()%x)


int DOMAIN_PROXY = 100;

/*
 *  精确表格式
 *  baidu(key) -> elt(value) ->  1|news.com 0|www.com
 *
 *  通配表格式
 *  baidu(key) -> elt(value) ->  1|*.com 0|www.*
 * */

//为了让精确域名先查询， 精确域名用头插， 通配域名用尾插
typedef struct {
    struct nt_acc_rule_s *next;

    signed  rule: 4; // 0/1  不加速/加速
    unsigned  len: 4; // 0/1  不加速/加速

    u_char name[1]; //存域名, //去除主域名 news.com   www.com

} nt_acc_elt_t;


//为了一个value申请的空间少且查询只需要遍历一个链
typedef struct {
    // exact ， wildcard
    //精确匹配表
    nt_acc_elt_t *elt;  //第一个为后缀， 后面为精确域名的前缀

    //wildcard
    //正则匹配
//    nt_acc_rule_t *wildcard;
} hash_value_t;


typedef struct {

    nt_str_t *data;
    size_t position;

} hash_sub_host_t;

typedef struct {
    nt_array_t *array;

    nt_str_t *head;
    nt_str_t *center;
    nt_str_t *tail;

    int8_t type ;
} hash_host_t;



void rand_str( char *out, int n )
{
    //srand(utime(NULL));
    int i = 0;
    for( i = 0; i < n; i++ ) {

        *( out + i ) = 97 + random( 26 );
    }

    *( out + n - 1 ) = '\0';
}


/* nt_int_t set_str( nt_str_t *str,  size_t len ){

    str->len = i - d1;
    str->data = malloc( sizeof( str ) );
    nt_memcpy( center->data, key->data + ( key->len - i ),  center->len );
}
 */
/*
 *.test.com -> test
 .test.com  -> test
 test.com   -> test
 *.com      -> com
 test.*     -> test
 www.test.* -> test
 */
void get_cut_host( hash_t *hash, nt_str_t *key, hash_host_t *host )
{

    size_t i = 0, n = 0;
    size_t size;


    if( *( key->data ) == '*' && *( key->data + 1 ) == '.' )
        host->type = 1;
    else if( *( key->data ) == '.' )
        host->type = -1;
    else if( *( key->data + key->len - 1 ) == '*' && *( key->data + key->len - 2 ) == '.' )
        host->type = 2;
    else
        host->type = 0;

    debug( "type = %d", host->type );


    hash_sub_host_t *name = NULL;
    nt_array_t *elements;
    unsigned char *q, *tmp;

    elements = nt_array_create( hash->pool, 2, sizeof( hash_sub_host_t ) );

    debug( "data=%s", key->data );
    char org[256] = {0};

    memcpy( org, key->data, key->len + 1 );
    q = key->data;

    //先按 . 分割域名保存到数组中
    while( tmp = strsep( &q, "." ) ) {

        debug( "position=%d", tmp -  key->data );
        name = ( hash_sub_host_t* ) nt_array_push( elements );
        name->data = nt_palloc( hash->temp_pool, sizeof( nt_str_t ) );
        nt_str_set( name->data, tmp );
        name->position = tmp -  key->data;
    }

    //查找主域名
    /*  先找最后面两个是否在顶级哈希表中，
     *  如果没有，就取倒数第二个
     * */
    hash_sub_host_t *main;
    hash_sub_host_t *top1;
    hash_sub_host_t *top2;

    top1 = ( hash_sub_host_t* )elements->elts + elements->nelts - 1;
    top2 = ( hash_sub_host_t* )elements->elts + elements->nelts - 2;

    char top[ 128 ]  = { 0 };

    snprintf( top, 128, "%s.%s", top2->data->data, top1->data->data );

    debug( "top=%s", top );

    if( hash_query( &hash_top_domain, top ) == NULL ) {
        //未找到
        main = top2;
        debug( "未找到" );

        host->tail = top1->data;
        host->center = top2->data;

   
    } else {
        //找到
        debug( "找到了" );
//        main = nt_palloc( hash->pool, sizeof( nt_str_t) );
        main = ( hash_sub_host_t* )elements->elts + elements->nelts - 3;

        host->center = main->data;
        host->tail = (nt_str_t * ) nt_palloc( hash->temp_pool, sizeof( nt_str_t  )  );
        nt_str_set( host->tail, top );

    }


    host->head = (nt_str_t * ) nt_palloc( hash->temp_pool, sizeof( nt_str_t ) );
    host->array = elements;

    if(  main->position )
        host->head->len = main->position - 1;
    else 
        host->head->len = 0;

    debug( "h len=%d",  host->head->len );
    if( host->head->len ){
        host->head->data =  nt_palloc( hash->temp_pool, host->head->len);
         

        memcpy( host->head->data, org, host->head->len );
        host->head->data[ host->head->len  ] = '\0';
    }

    debug( "center=%s" , host->center->data );
    debug( "head=%s" , host->head->data );
    debug( "tail=%s" , host->tail->data );
}

//添加前先查询是否存在
nt_int_t add_to_hash_table( hash_t *hash, nt_str_t *key, u_int8_t rule )
{
    size_t i, n;

    hash_value_t *add;
    hash_value_t *query;
    hash_host_t host = {0};

    //1. 获取二级域名
    //2. 获得一级域名


    get_cut_host( hash,  key, &host );


    /* return NT_OK; */

    #if 1

    debug( " host.head.data=%s",  host.head->data );
    debug( " host.center.data=%s",  host.center->data );
    debug( " host.tail.data=%s",  host.tail->data );

    char name[64] = {0};

    query = hash_query( hash, host.center->data );

    if( query == NULL ) {

        add = ( hash_value_t * ) malloc( sizeof( hash_value_t ) );

        nt_acc_elt_t *elt;
        elt = ( nt_acc_elt_t * ) nt_pcalloc( hash->pool, sizeof( nt_acc_elt_t ) + host.head->len + host.tail->len );
        elt->rule = rule;
        elt->next = NULL;
        elt->len = host.head->len + host.tail->len;

        nt_memcpy( elt->name, host.head->data, host.head->len );
        nt_memcpy( elt->name + host.head->len, host.tail->data, host.tail->len );

        nt_memcpy( name, host.center->data, host.center->len );
//        *( name + host.main.len + 1 ) = '\0';
        add->elt = elt;

        hash_add( hash, name, add );
//        nt_free( exact );
    } else {
        //先判断有没有存在

        nt_acc_elt_t *elt;
        nt_acc_elt_t *pre;

        elt = query->elt ;

        while( elt ) {
            pre = elt;

            //正序比较
            debug( "name=%s , host.head.data=%s", elt->name, host.head->data );
            if( nt_strncmp( elt->name, host.head->data, host.head->len ) != 0 ) {
                elt = elt->next;
                continue;
            }

            //改倒序比较
            debug( "name=%s , host.tail.data=%s", elt->name, host.tail->data );
            if( nt_rstrncmp( elt->name + host.head->len, host.tail->data, host.tail->len ) != 0 ) {
                elt = elt->next;
                continue;
            }

            debug( "l l %d, %d", elt->len, host.head->len + host.tail->len );
            if( elt->len != ( host.head->len + host.tail->len ) ) {
                elt = elt->next;
                continue;
            }

            break;
        }

        if( elt ) {
            debug( "存在相同的域名，%s", elt->name );
            return NT_BUSY;
        }

        nt_acc_elt_t *new;
        new = ( nt_acc_elt_t * ) nt_pcalloc( hash->pool, sizeof( nt_acc_elt_t ) + host.head->len + host.tail->len );
        new->rule = rule;
        nt_memcpy( new->name, host.head->data, host.head->len );
        nt_memcpy( new->name + host.head->len, host.tail->data, host.tail->len );
        new->next = NULL;
        new->len = host.head->len + host.tail->len;

//        query->rule;
        //有正则，存后面
        if( host.type ) {
            pre->next = new;
        } else { //无正则，存前面
            elt = query->elt;

            query->elt = new;
            new->next = elt;
        }
    }

    #endif
    return 1;
}



//添加顶级域名表
nt_int_t add_hash_top_domain( char *line )
{

    char country[64], code[4];
    int *value;
    sscanf( line, "%s %s", country, code );

    if( atoi( code ) == 1 )
        value = &HASH_TOP_DOMAIN_COUNTRY;
    else
        value = &HASH_TOP_DOMAIN_GENERY;

    /* debug( "country=%s, value=%d", country, *value ); */
    if( hash_add( &hash_top_domain,  country, value ) != NT_OK ) {
        debug( "add fail" );
        return NT_ERROR;
    }

    return NT_OK;
}

//解析配置
nt_int_t parse_conf_option( hash_t *hash, char *line )
{
    size_t i = 0 ;
    nt_int_t ret = 0 ;

    while( line[i] ) {
        /* debug( "line=%s", line + i ); */

        if( line[i] == '[' ) {
            debug( "line=%s", line + i );

            if( strncmp( line + i, "[end]", 4 ) == 0 )
                ret = -1;

            if( strncmp( line + i, "[proxy]", 6 ) == 0 )
                ret = -10;

            if( strncmp( line + i, "[direct]", 7 ) == 0 )
                ret = -11;

            if( strncmp( line + i, "[tld]", 4 ) == 0 )
                ret = -2;

            break;
        }

        if( line[i] == '#' ) {
            debug( "line=%s", line + i );
            ret = 0;
            break;
        }

        ret = i;
        i++;
    }

    /* debug( "ret=%d", ret ); */
    return ret;
}


nt_int_t hast_add_domain( hash_t *hash, char *path )
{
    FILE *f;
    char line[128] = {0};
    nt_array_t* elements = NULL;
    nt_hash_key_t* arr_node = NULL;
    nt_str_t key ;
    int type = 0;
    int ret = 0;

    f = fopen( path, "r" );
    if( f == NULL ) {
        printf( "open file error\n" );
        return -1;
    }


    while( fgets( line, sizeof( line ), f ) != NULL ) {
        //NT_HASH_WILDCARD_KEY表明可以处理带通配符的关键字
        ret = parse_conf_option( hash, line );
        key.len = ret;

        if( ret == 0 )
            continue;

        if( ret == -1 )
            break;

        if( ret == -10 ) {
            type = 1;
            continue;
        }

        if( ret == -11 ) {
            type = 2;
            continue;
        }

        if( ret == -2 ) {
            //
            hash_top_domain.pool = nt_create_pool( 1024 * 10, log );
            hash_init( &hash_top_domain, 300 );
            type = 0;
            continue;
        }

        //最后一个字符为换行，需要剔除
        line[ ret ] = '\0';
        if( type ) {

            //        debug( "line=%s, len=%d", line, key.len );

            key.data = line;

            add_to_hash_table( hash, &key, type );

        } else {
            //添加顶级域名到hash表。
            add_hash_top_domain( line );
        }

    }

    fclose( f );

     hash_dump( hash  ); 
    nt_destroy_pool( hash->temp_pool );
    return NT_OK;
}




int main( int argc, char **argv )
{
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
    hash_value_t *query;
    /* hash_init( hinit, NULL, 2 );
    hinit->key = nt_hash_key;
    */

    //添加
    /* for( i = 0; i < 8000; i++ ) {
        rand_str( key, 12 );
        if( hash_add( hash,  key, key ) != NT_OK ) {
            debug( "add fail" );
            continue ;
        }
    } */
    hast_add_domain( hash, argv[1] );

    debug( "pool-max=%d", p->max );
//    return ;

    //查询
    query = hash_query( hash,  "baidu" );
    nt_acc_elt_t *elt;
    if( query ) {
        elt = query->elt;
        debug( "找到了" );
        while( elt ) {
            debug( "elt=%s", elt->name );
            elt = elt->next;
        }
    }
    //删除

    /* if( hash_del( hash,  key ) != NT_OK ) {
        debug( "del fail" );
        return ;
    }
    */
    //pause();
    return ;
//查询
    query = hash_query( hash,  key );
    debug( "query=%s", query );

    //全打印
//    nt_hash_dump( hinit.hash  );
    hash_free( hash );
    nt_destroy_pool( p );
    return 0;
}
