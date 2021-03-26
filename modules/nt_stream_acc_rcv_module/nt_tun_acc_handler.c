#include <nt_core.h>
#include "rbtree.h"
#include "nt_tun_acc_handler.h"
#include <nt_stream.h>
/* #include "protocol/protocol.h" */


/*本文件用来写所有的session回调*/


//添加一个tcp连接到红黑树中
int nt_connection_rbtree_add( nt_connection_t *c ){

    nt_rbtree_node_t *node = ( nt_rbtree_node_t *  ) nt_palloc( c->pool, sizeof( nt_rbtree_node_t  )  );

    if( c->type  == IPPROTO_TCP ){
        rcv_conn_add( &acc_tcp_tree, c, node  );

    } else {
        rcv_conn_add( &acc_udp_tree, c, node  );
    }

    return NT_OK;
}


int nt_connection_rbtree_del( nt_connection_t *c ){
    nt_acc_session_t *s;
    s = c->data;
    if( c->type  == IPPROTO_TCP ){
        rcv_conn_del( &acc_tcp_tree, s->port  );

    } else {
        /* rcv_conn_add( &acc_tcp_tree, c, node  ); */
    }

}


void nt_tun_connection_close_handler( nt_event_t *ev ){
    nt_connection_t     *c;
    debug( "close tun input conn" );
    c = ev->data ;
    if( c == NULL )
        return;

    nt_connection_rbtree_del( c );

    nt_close_connection( c  );

    /* return ; */
    nt_destroy_pool( c->pool );
}

/*
 * 关闭有tun 进来的连接数据
 *
 * */
int nt_tun_close_session( nt_connection_t *c ){
    nt_acc_session_t *s;
    nt_upstream_socks_t *ss;

    debug( "start close" );
    if( c == NULL )
        return NT_OK;

    s = c->data;

    //先关闭上游
    if( c->type  == IPPROTO_TCP  ){
        debug( "tc free" );
        nt_connection_t *tc ;

        tc = s->ss->tcp_conn ;
        if( tc )
            nt_close_connection( tc  );

    } else{

    }

    //最好是延后关闭
    c->read->handler = nt_tun_connection_close_handler;

    nt_add_timer( c->read, 1 );
    //再关闭下游
    /* nt_close_connection( c ); */
    /* exit(0); */
}


//tun 数据流入回调
/* int nt_tun_accept_new( nt_acc_session_t *s ){ */
void nt_tun_accept_new_handler( nt_event_t *rev )
{
    nt_connection_t      *c;
    nt_acc_session_t  *s;

    c = rev->data;
    s = c->data;

    debug( "start"  );

    if( c->type == IPPROTO_TCP )
        acc_tcp_handshake( s );
    else
        nt_tun_udp_accept_new( c );

    //设置 s 参数

    //如果是tcp ,先进行三次握手。
    //再进行数据中转，
    

    //如果是udp，直接进行中转

}


int nt_tun_data_forward_handler( nt_event_t *ev ){
    nt_connection_t     *c;
    nt_acc_session_t *s;

    c = ev->data ;
    s = c->data;

    acc_tcp_handshake( s );
}

/*
 * 进行发起3proxy的连接过程
 *
 * */
void nt_tun_upstream_connect_handler( nt_event_t *ev ){
    debug( "start" );
    nt_acc_session_t *s ;
    nt_connection_t *c;

    c = ev->data;
    s = c->data;

    if( s->ss == NULL  ){
        //初始化一个ss
        nt_stream_init_socks5_upstream( c );

        //当前应该为 认证第一阶段
        s->ss->phase = SOCKS5_VERSION_REQ ;
    }

    //进入阶段的认证
    nt_tun_socks5_handle_phase( s  );
}

//tun 数据流出到local回调
int nt_tun_local_handler(){


}

//tun 数据流出到3proxy回调
int nt_tun_server_handler(){


}


