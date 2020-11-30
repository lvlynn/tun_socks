#include <nt_core.h>

#define random(x) (rand()%x)


typedef struct {

    int port;
    int test;
    char ip[18];

} nt_test_t;


int nt_rbtree_insert_handle( nt_flag_t flag, nt_rbtree_key_t tree_key, nt_rbtree_key_t cur_key )
{

//    printf( "%s\n", __FUNCTION__ );

    if( flag == RBTREE_INSERT ) {
     //   printf( "RBTREE_INSERT \n" );
        nt_test_t *tcur = ( nt_test_t * )cur_key;
        nt_test_t *ttree = ( nt_test_t * )tree_key;

     //   printf( "tcur->port=%d\n", tcur->port );
       // printf( "ttree->port=%d\n", ttree->port );

        if( tcur->port < ttree->port )
            return 1;
        else if( tcur->port > ttree->port )
            return -1;
        else if( tcur->port == ttree->port ) {
     //       printf( "相同ttree->port=%d\n", ttree->port );
            return 0;
        }
    }

    if( flag == RBTREE_SEARCH ) {
    //    printf( "RBTREE_SEARCH \n" );

    //    printf( "tcur->port=%d\n", cur_key );

        nt_test_t *ttree = ( nt_test_t * )tree_key;

    //    printf( "ttree->port=%d\n", ttree->port );



        if( cur_key < ttree->port )
            return 1;
        else if( cur_key > ttree->port )
            return -1;
        else if( cur_key == ttree->port ) {
    //        printf( "相同ttree->port=%d\n", ttree->port );
            return 0;
        }
    }



}

void nt_rbtree_dump_handle( nt_rbtree_key_t key ){

    nt_test_t *ttree = ( nt_test_t *   )key;
    printf( "port=%d\n",  ttree->port   );

    
}

int main()
{
    nt_rbtree_t tree;
    nt_rbtree_node_t sentinel;
    srand( time( NULL ) );
    nt_rbtree_init( &tree, &sentinel, nt_rbtree_insert_handle );

    nt_rbtree_node_t rbnode[10] ;
    nt_test_t test[10];

    nt_rbtree_node_t *search;

    for( int i = 9; i >= 0 ; i-- ) {
        //  rbnode[i].key = i;
        test[i].port = random( 10 );
        test[i].test = random( 1000 );
        rbnode[i].key = &test[i];
        rbnode[i].parent =  &sentinel;
        rbnode[i].left =  &sentinel;
        rbnode[i].right =  &sentinel;

        printf( "port=%d\n", test[i].port );
        nt_rbtree_insert( &tree, &rbnode[i] );
        printf( "count=%d\n",  tree.count  );
    }


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
     for( int i = 0; i < 10; i++ ) {

        if( tree.root == &sentinel )
            continue;

        /*tree.root 在最后一个被设置成NULL了；因此会出错*/
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

    return 0;
}
