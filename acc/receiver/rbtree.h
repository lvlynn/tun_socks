#ifndef _RBTREE_H_
#define _RBTREE_H_

#include <nt_core.h>
#include <protocol.h>


int nt_rbtree_insert_conn_handle( nt_flag_t flag, nt_rbtree_key_t tree_key, nt_rbtree_key_t cur_key );


extern nt_rbtree_node_t g_sentinel;
extern nt_rbtree_t tcp_udp_tree;

nt_rbtree_node_t *rcv_conn_search( nt_rbtree_t *tree, u_int16_t port );
#endif

