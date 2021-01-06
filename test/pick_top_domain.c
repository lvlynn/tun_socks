#include "nt_core.h"


/* volatile nt_cycle_t *nt_cycle;   */

#define random(x) (rand()%x)

nt_log_t *log;

void rand_str( char *out, int n )
{
    //srand(utime(NULL));
    int i = 0;
    for( i = 0; i < n; i++ ) {

        *( out + i ) = 97 + random( 26 );
    }

    *( out + n - 1 ) = '\0';
}


int DOMAIN_DERECT=0;
int DOMAIN_PROXY=1;

nt_int_t hash_top_domain( hash_t *hash )
{


    FILE *f;
    char line[128] = {0};
    int len = 0;


    f = fopen( "./top_domain_list.txt", "r" );
    if( f == NULL ) {
        printf( "open file error\n" );
        return -1;

    }

    char country[8] , code[4];
    int *value;
    while( fgets( line, sizeof( line ), f ) != NULL ) {

        len = nt_strlen( line  ) - 1 ;
        line[len] = '\0';

        if( len < 1  )
            continue;

        //q =line;
        /* while( (ret = strsep( &q, " \n" ) )   ){

        } */

        sscanf( line, "%s %s", country, code);

        /* debug( "country = %s" , country );
        debug( "code = %s" ,  code ); */

        if( atoi( code ) == 1 )
            value = &DOMAIN_PROXY ;
        else
            value = &DOMAIN_DERECT ;

        /* debug( "line=%s, len=%d", line, len ); */
        if( hash_add( hash,  country, value ) != NT_OK ) {
            debug( "add fail" );
            continue ;
        }

    }

    fclose( f );
}


nt_int_t save_to_file( hash_t *hash ){

    FILE *f;
    f = fopen( "./result.txt", "w+" );
    if( f == NULL ) {
        printf( "open file error\n" );
        return -1;
	}


	debug( "=========dump============"  );
	hash_elt_t  *elt;
	hash_elt_t  *next;
	nt_uint_t pos; 
	char name[64] = {0}; 
	while( pos < hash->size  ){
		elt = hash->buckets[ pos  ];
		while( elt ){
			snprintf( name,  elt->len + 1, "%s", elt->name  );
			debug( "%s", name  );
			fprintf( f, "%s\n",  name );
			next = elt->next;
			nt_free( elt );
			elt = next;
		}
		pos ++;
	}    

	fclose(f);

	return 1;
}

nt_int_t hash_conf_domain( hash_t *hash, char *path )
{

    size_t i = 0, n = 0;
    size_t d1;
    nt_str_t *name = NULL;
    nt_str_t *name2 = NULL;

    char *ret;
    int len = 0;

    FILE *f;
    char line[128] = {0};
    f = fopen( path, "r" );
    if( f == NULL ) {
        printf( "open file error\n" );
        return -1;
    }

	nt_array_t *elements;
    int *query;
    char *q;

    FILE *f2 = fopen( "list.txt", "w+" );
    if( f2 == NULL ) {
        printf( "open file error\n" );
        return -1;
    }


    char org[128] = {0};
    hash_t *result;
    nt_pool_t *p;
    p = nt_create_pool( 1024 * 10, log ); //16KB
     result = ( nt_hash_init_t * )nt_palloc( p, sizeof( hash_t ) );

    result->pool = p;

    hash_init( result, 100 );
    char top_domain[64] = {0};

    while( fgets( org, sizeof( org ), f ) != NULL ) {
     //   debug( "line=%s", line );
        len = nt_strlen( org  ) - 1 ;
        *( org + len ) = '\0';
        strcpy( line, org );

        if( len <1 )
            continue;

        if( *line == '[' )
            continue;

        q = line ;
	    elements = nt_array_create( hash->pool, 2, sizeof( nt_str_t ) );
        while( ret = strsep( &q, "." ) ) {
            n++;

            name = ( nt_str_t* ) nt_array_push( elements );
            nt_str_set( name, ret );
        //    debug( "name=%s", name->data );
        }

     //   debug( "elements->nelts = %d", elements->nelts );


        int found1 = 0 ; 
        int found2 = 0 ; 
        name = ( nt_str_t* )elements->elts + elements->nelts - 2;
        /* debug( "查询2 name=%s", name->data ); */
        query = hash_query( hash, name->data  );
        if( query ){
            /* debug( "位置2 find %d", *query  ); */
            found1 = 1;
        }

        name2 = ( nt_str_t* )elements->elts + elements->nelts - 1;
        /* debug( "查询2 name2=%s", name2->data ); */
        query = hash_query( hash, name2->data  );
        if( query ){
            /* debug( "位置2 find %d", *query  ); */
            if( *query == 1 )
                found2 = 2;
            else
                found2 = 1;
        }

        if( (found1 == 0) && (found2 == 0 )){
            debug( "没找到 %s", org );
            continue;
        }

        if( found1 && found2 == 2 ){
            fprintf( f2, "host= %s,", org );
            sprintf( top_domain, "%s.%s",  name->data, name2->data );
            fprintf( f2, "top=%s\n", top_domain );

            if( hash_query( result, top_domain   ) == NULL ){
                /* debug( "top_domain=%s",  top_domain ); */
                hash_add( result, top_domain, top_domain );
            }

        } else if( (found1 &&  found2 == 1 ) || ( found1 == 0 && found2 ) ){
            fprintf( f2, "host=%*s,top=%s\n", len, org, name2->data );

            if( query = hash_query( result, name2->data   ) == NULL ){
                /* debug( "添加top_domain=%s",   name->data   ); */
                hash_add( result, name2->data, name2->data );
            } else {

                /* debug( "%s,不添加top_domain=%s", query,  name->data   ); */
            }

        }



        memset( line, 0, sizeof( line  )  );
    }

    fclose( f );
    fclose( f2 );

    /* hash_dump( result ); */
    save_to_file( result );
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
    char *query;

    //添加
    hash_top_domain( hash );

	hash_conf_domain( hash , argv[1]);

    debug( "pool-max=%d", p->max );

    //查询
    query = hash_query( hash,  "baidu" );
    if( query ) {
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
