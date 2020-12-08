#ifndef _RBTREE_H_
#define _RBTREE_H_

#include <nt_core.h>

typedef struct {
    int port;
    nt_connection_t *conn;

} nt_rev_connection_t;

int nt_rbtree_insert_handle( nt_flag_t flag, nt_rbtree_key_t tree_key, nt_rbtree_key_t cur_key );

extern nt_rbtree_node_t *sentinel;
#endif

