#include <nt_core.h>
#include "rbtree.h"
/*本文件用来写所有的session回调*/


//添加一个tcp连接到红黑树中
int nt_connection_rbtree_add( nt_connection_t *c ){

    nt_rbtree_node_t *node = ( nt_rbtree_node_t *  ) nt_palloc( c->pool, sizeof( nt_rbtree_node_t  )  );

    if( c->type  == IPPROTO_TCP ){
        rcv_conn_add( &acc_tcp_tree, c, node  );

    } else {
        rcv_conn_add( &acc_tcp_tree, c, node  );
    }

    return NT_OK;
}


//tun 数据流入回调
int nt_tun_accept_new( nt_acc_session_t *s ){

    //设置 s 参数

    //如果是tcp ,先进行三次握手。
    //再进行数据中转，
    
    acc_tcp_handshake( s );

    //如果是udp，直接进行中转

}


int nt_tun_data_forward( nt_acc_session_t *s ){
    acc_tcp_handshake( s );
}


//tun 数据流出到local回调
int nt_tun_local_handler(){


}

//tun 数据流出到3proxy回调
int nt_tun_server_handler(){


}


