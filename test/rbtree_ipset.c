#include <nt_core.h>

#define random(x) (rand()%x)


typedef struct {

    uint32_t net;
    uint64_t mask;
    int8_t bits;

} nt_test_t;

#define NIP( addr )\
    ((unsigned char *)&addr)[3],\
    ((unsigned char *)&addr)[2],\
    ((unsigned char *)&addr)[1],\
    ((unsigned char *)&addr)[0]

int nt_rbtree_insert_handle( nt_flag_t flag, nt_rbtree_key_t tree_key, nt_rbtree_key_t cur_key )
{

//    printf( "%s\n", __FUNCTION__ );

    if( flag == RBTREE_INSERT ) {
        //   printf( "RBTREE_INSERT \n" );
        nt_test_t *tcur = ( nt_test_t * )cur_key;
        nt_test_t *ttree = ( nt_test_t * )tree_key;

        //   printf( "tcur->port=%d\n", tcur->port );

        /* printf( "root net=%u.%u.%u.%u, insert net = %u.%u.%u.%u,\n", NIP( ttree->net ), NIP( tcur->net ) ); */

        if( tcur->net < ttree->net )
            return 1;
        else if( tcur->net > ttree->net )
            return -1;
        else if( tcur->net == ttree->net ) {
            if(  tcur->mask < ttree->mask  ){
                ttree->mask = tcur->mask;
                return 0;
            } else 
                return 0;
            //       printf( "相同ttree->port=%d\n", ttree->port );
        }
    }

    if( flag == RBTREE_SEARCH ) {
        //    printf( "RBTREE_SEARCH \n" );

        //    printf( "tcur->port=%d\n", cur_key );



        nt_test_t *ttree = ( nt_test_t * )tree_key;
        
        

        //    printf( "ttree->port=%d\n", ttree->port );

         printf( "root net=%u.%u.%u.%u/%u.%u.%u.%u, insert net = %u.%u.%u.%u,\n", NIP( ttree->net ),NIP(ttree->mask), NIP( cur_key ) ); 

 //       printf( "tree_key=%#x\n", tree_key );
        //printf( "mask=%#x\n", ttree->mask );
        if( (cur_key & ttree->mask) < ttree->net )
            return 1;
        else if( (cur_key & ttree->mask)  > ttree->net )
            return -1;
        else if( (cur_key & ttree->mask)  == ttree->net ) {
            //        printf( "相同ttree->port=%d\n", ttree->port );
            return 0;
        }
    }



}

void nt_rbtree_dump_handle( nt_rbtree_key_t key )
{

    nt_test_t *ttree = ( nt_test_t * )key;
    printf( "port=%#x\n",  ttree->net );


}


int main( int argc, char **argv )
{

    FILE * ipset;
    char *net_str;
    char *net_len;
    char file_str[20];


    nt_log_t *log ;
    nt_pool_t *pool;
    nt_cycle_t *cycle;


    nt_time_init();
    log = nt_log_init( NULL );
    log->log_level = NT_LOG_DEBUG_ALL;

    nt_log_debug1( NT_LOG_DEBUG_EVENT, log, 0,
                   "timer delta: %M", nt_current_msec );



    //初始化一个内存池
    pool = nt_create_pool( NT_CYCLE_POOL_SIZE, log );
    if( pool == NULL ) {
        return NULL;
    }
    pool->log = log;


    nt_rbtree_t tree;
    nt_rbtree_node_t sentinel;
    srand( time( NULL ) );
    nt_rbtree_init( &tree, &sentinel, nt_rbtree_insert_handle );

    nt_test_t test[10];

    nt_rbtree_node_t *insert;
    nt_rbtree_node_t *search;



    //ipset = fopen( "/usr/zyl/mylib/myrbtree/chnroute.txt", "r" );
    ipset = fopen( argv[1], "r" );
    if( ipset == NULL ) {
        printf( "open file error\n" );
        return -1;
    }
    /*
    uint32_t ip = inet_addr( argv[2] );
    ip = ntohl( ip );
    printf( "ip = %#x\n", ip );
    */


    uint32_t ip;
    //插入文件内的所有条目
    while( fgets( file_str, sizeof( file_str ), ipset ) != NULL ) {
        //   printf("%s\n", file_str);
        net_str = file_str;
        while( *net_str != 0 ) {
            if( *net_str == '/' ) {
                *net_str = 0;
                break;
            }
            net_str++;
        }
        net_str++;
        net_len = net_str;
        net_str = file_str;
        //    printf("%s\n", net_str);
        //    printf("%s\n", net_len);
        nt_test_t *t = ( nt_test_t * )malloc( sizeof( nt_test_t ) );
        t->net = ntohl( inet_addr( net_str ) );
        t->bits = atoi( net_len  );
        t->mask = ( 0xffffffff << ( 32 - atoi( net_len ) ) );
        //    printf("t->n_net = %#x\n", t->n_net);
        //    printf("t->mak = %#x\n", t->mask);
        //    printf("set key\n");

        //printf( "insert net = %u.%u.%u.%u,\n", NIP( t->net ) );
        insert = nt_palloc( pool, sizeof( nt_rbtree_node_t ) );

        insert->key = t;
        insert->parent =  &sentinel;
        insert->left =  &sentinel;
        insert->right =  &sentinel;

        nt_rbtree_insert( &tree, insert );
    }

    printf( "count=%d\n",  tree.count );

    /* struct timeval start, end;
    rewind( ipset );
    int cunt = 0;
    while( fgets( file_str, sizeof( file_str ), ipset ) != NULL ) {
        net_str = file_str;
        while( *net_str != 0 ) {
            if( *net_str == '/' ) {
                *net_str = 0;
                *( net_str - 1 ) += 1;
                break;
            }
            net_str++;
        }
        net_str++;
        net_len = net_str;
        net_str = file_str;
        gettimeofday( &start, 0 );
        ip = ntohl( inet_addr( net_str ) ) ;

        nt_test_t net ;
        net.net = ip;

        if(nt_rbtree_search( &tree, ip ) != &sentinel ) {
            cunt++;
        } else {
            printf( "find none\n" );
            printf( " no find %s\n", net_str );
        }
        gettimeofday( &end, 0 );
        uint32_t time = ( end.tv_sec - start.tv_sec ) * 1000000 + ( end.tv_usec - start.tv_usec );
        //printf( "search use time = %d\n", time );

    }
    printf( "tree->count = %d\n", tree.count );
    printf( " find count = %d\n", cunt ); */

    uint32_t new = ntohl( inet_addr(argv[2])  );
    printf("find ip=%u.%u.%u.%u\n", NIP( new ));
        
    nt_rbtree_node_t *p = nt_rbtree_min( tree.root, &sentinel );
    nt_test_t *ttree = ( nt_test_t * )p->key;
    printf("find ip=%u.%u.%u.%u\n", NIP( ttree->net ));


    if(nt_rbtree_search( &tree, new)  != &sentinel){
        printf("find\n");
    }else{
        printf("find none\n");
    }
    /*
        printf( "================search==================\n" );
        search =  (nt_rbtree_node_t *)nt_rbtree_search( &tree, ( nt_rbtree_key_t )test[1].port );

        if( search != &sentinel  ){

            nt_test_t *ttree = ( nt_test_t * )search->key;
            printf( "port=%d\n",  ttree->port );

            nt_rbtree_delete_key( &tree, ttree->port );

        } else
            printf( "search null\n"  );

        printf( "================search===end===============\n" );


        //  tree.root = &sentinel;
        //  nt_rbtree_node_t *p = nt_rbtree_min( tree.root, &sentinel );
        //  if( p == &sentinel )
        //      printf("p end\n");
        //   return ;


        printf( "================dump==================\n" );
        nt_rbtree_dump( &tree , nt_rbtree_dump_handle );
        printf( "================dump===end===============\n" );



        printf( "================print min==================\n" );
         for(  i = 0; i < 10; i++ ) {

            if( tree.root == &sentinel )
                continue;

            nt_rbtree_node_t *p = nt_rbtree_min( tree.root, &sentinel );

            if( p == &sentinel || p == NULL )
                continue;

            nt_test_t *ttree = ( nt_test_t * )p->key;

            //    if( ttree->port == 0 )
            //        continue;

            printf( "port=%d\n",  ttree->port );
            //  printf("test=%d\n",  ttree->test);

            nt_rbtree_delete( &tree, p );

            printf( "count=%d\n",  tree.count  );
        }

    */
    return 0;
}
