#include "rbtree.h"
#include "debug.h"


nt_rbtree_t acc_tcp_tree;
nt_rbtree_t acc_udp_tree;
nt_rbtree_t acc_udp_id_tree;  //该红黑树用来存储udp 分片数据

nt_rbtree_node_t g_sentinel;

int nt_rbtree_insert_conn_handle( nt_flag_t flag, nt_rbtree_key_t tree_key, nt_rbtree_key_t cur_key )
{
    if( flag == RBTREE_INSERT ) {
        /* debug( "RBTREE_INSERT" ); */
        if( tree_key == 0  )
            return 1;

        nt_acc_session_t *tcur = ( nt_acc_session_t * )cur_key;
        /* debug( "tcur->port=%d", tcur->port ); */

        nt_acc_session_t *ttree = ( nt_acc_session_t * )tree_key;
        /* debug( "ttree->port=%d", ttree->port ); */
        if( tcur->port < ttree->port )
            return 1;
        else if( tcur->port > ttree->port )
            return -1;
        else if( tcur->port == ttree->port ) {
            return 0;
        }
    }

    if( flag == RBTREE_SEARCH ) {
        /* debug( "RBTREE_SEARCH" ); */
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


int nt_rbtree_insert_udp_id_handle( nt_flag_t flag, nt_rbtree_key_t tree_key, nt_rbtree_key_t cur_key )
{
    if( flag == RBTREE_INSERT ) {
        /* debug( "RBTREE_INSERT" ); */
        if( tree_key == 0  )
            return 1;

        nt_acc_session_t *tcur = ( nt_acc_session_t * )cur_key;
        nt_skb_udp_t *tcur_udp = tcur->skb->data;
        /* debug( "tcur_udp->ip_id=%d", tcur_udp->ip_id ); */

        nt_acc_session_t *ttree = ( nt_acc_session_t * )tree_key;
        nt_skb_udp_t *ttree_udp = ttree->skb->data;
        /* debug( "ttree_udp->ip_id=%d", ttree_udp->ip_id ); */

        uint16_t cur_id = tcur_udp->ip_id;
        uint16_t tree_id = ttree_udp->ip_id;


        if( cur_id < tree_id )
            return 1;
        else if( cur_id > tree_id )
            return -1;
        else if( cur_id == tree_id ) {
            return 0;
        }
    }

    if( flag == RBTREE_SEARCH ) {
        /* debug( "RBTREE_SEARCH" ); */
        nt_acc_session_t *ttree = ( nt_acc_session_t * )tree_key;
        nt_skb_udp_t *ttree_udp = ttree->skb->data;

        uint16_t tree_id = ttree_udp->ip_id;
        /* debug( "tcur_udp->ip_id=%d", cur_key ); */
        /* debug( "ttree_udp->ip_id=%d", ttree_udp->ip_id ); */

        if( cur_key < tree_id )
            return 1;
        else if( cur_key > tree_id )
            return -1;
        else if( cur_key == tree_id ) {
            return 0;
        }
    }
}




//连接查询
nt_rbtree_node_t *rcv_conn_search( nt_rbtree_t *tree, u_int16_t key )
{
    nt_rbtree_node_t *node;

    node = nt_rbtree_search( tree, key );

    if( node  != &g_sentinel )
        return node;
    else    
        return NULL;
}


//连接添加到红黑树
int rcv_conn_add( nt_rbtree_t *tree,  void *key , nt_rbtree_node_t *node )
{
#if 0 
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
        nt_skb_udp_t *skb_data;
        skb_data = skb->data;
        port = s->port;
    } 

//    debug( "port=%d", port );
    
#endif

//    debug( "node=%p", node );
    node->key = key;

    node->parent =  &g_sentinel;
    node->left =  &g_sentinel;
    node->right =  &g_sentinel;
   
    nt_rbtree_insert( tree, node );
    debug( "tree->root addr=%p", tree->root );
    debug( "tree->sen addr=%p", tree->sentinel );
    debug( "tree->count=%d", tree->count );

    return 0;
}


//连接删除
int rcv_conn_del( nt_rbtree_t *tree, nt_connection_t  *conn )
{
    int port ;
    nt_rbtree_delete_key( tree, port  );
}
