#include "rbtree.h"

nt_rbtree_node_t sentinel;

int nt_rbtree_insert_conn_handle( nt_flag_t flag, nt_rbtree_key_t tree_key, nt_rbtree_key_t cur_key )
{

    if( flag == RBTREE_INSERT ) {
        //   printf( "RBTREE_INSERT \n" );
        nt_rev_connection_t *tcur = ( nt_rev_connection_t * )cur_key;
        nt_rev_connection_t *ttree = ( nt_rev_connection_t * )tree_key;

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
        nt_rev_connection_t *ttree = ( nt_rev_connection_t * )tree_key;

        if( cur_key < ttree->port )
            return 1;
        else if( cur_key > ttree->port )
            return -1;
        else if( cur_key == ttree->port ) {
            return 0;
        }
    }
}

//连接添加到红黑树
int rcv_conn_add( nt_rbtree_t *tree, nt_connection_t  *conn )
{

    int port;
    nt_rbtree_node_t *node;

    node = ( nt_rbtree_node_t * ) malloc( sizeof( nt_rbtree_node_t ) );

    node->key = port;
    node->parent =  &sentinel;
    node->left =  &sentinel;
    node->right =  &sentinel;
   
    nt_rbtree_insert( &tree, node );

    return 0;
}


//连接删除
int rcv_conn_del( nt_rbtree_t *tree, nt_connection_t  *conn )
{
    int port ;
    nt_rbtree_delete_key( tree, port  );
}

