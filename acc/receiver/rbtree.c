#include "rbtree.h"
#include "debug.h"


nt_rbtree_t acc_tcp_tree;
nt_rbtree_t acc_udp_tree;

nt_rbtree_node_t g_sentinel;

int nt_rbtree_insert_conn_handle( nt_flag_t flag, nt_rbtree_key_t tree_key, nt_rbtree_key_t cur_key )
{

    if( flag == RBTREE_INSERT ) {
        debug( "RBTREE_INSERT" );
        //   printf( "RBTREE_INSERT \n" );
        if( tree_key == 0  )
            return 1;

        nt_acc_session_t *tcur = ( nt_acc_session_t * )cur_key;
        debug( "tcur->port=%d", tcur->port );

        nt_acc_session_t *ttree = ( nt_acc_session_t * )tree_key;
        debug( "ttree->port=%d", ttree->port );
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
        nt_acc_session_t *ttree = ( nt_acc_session_t * )tree_key;

        /* debug( "cur_key=%d", cur_key );
        debug( "ttree->port=%d", ttree->port ); */

        if( cur_key < ttree->port )
            return 1;
        else if( cur_key > ttree->port )
            return -1;
        else if( cur_key == ttree->port ) {
            return 0;
        }
    }
}

//连接查询
nt_rbtree_node_t *rcv_conn_search( nt_rbtree_t *tree, u_int16_t port )
{
    nt_rbtree_node_t *node;

    node = nt_rbtree_search( tree, port );

    if( node  != &g_sentinel )
        return node;
    else    
        return NULL;
}


//连接添加到红黑树
int rcv_conn_add( nt_rbtree_t *tree, nt_connection_t  *conn )
{
    u_int16_t port;
    nt_rbtree_node_t *node;
    nt_acc_session_t *s ;
    nt_skb_t *skb;

    s = conn->data;


    skb = s->skb;

    if( skb->protocol == IPPROTO_TCP ){
        nt_skb_tcp_t *skb_data;

        skb_data = skb->data;
        port = s->port;

    } else if ( skb->protocol == IPPROTO_UDP  ){

    } 

//    debug( "port=%d", port );
    
    node = ( nt_rbtree_node_t * ) nt_palloc( conn->pool, sizeof( nt_rbtree_node_t ) );


//    debug( "node=%p", node );
    node->key = s;

    node->parent =  &g_sentinel;
    node->left =  &g_sentinel;
    node->right =  &g_sentinel;
   
    nt_rbtree_insert( tree, node );
//    debug( "tree->root addr=%p", tree->root );
//    debug( "tree->sen addr=%p", tree->sentinel );
//    debug( "tree->count=%d", tree->count );

    return 0;
}


//连接删除
int rcv_conn_del( nt_rbtree_t *tree, nt_connection_t  *conn )
{
    int port ;
    nt_rbtree_delete_key( tree, port  );
}

