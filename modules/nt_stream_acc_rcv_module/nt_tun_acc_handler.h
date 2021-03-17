#ifndef _NT_TUN_ACC_HANDLER_H_
#define _NT_TUN_ACC_HANDLER_H_

int nt_connection_rbtree_add( nt_connection_t *c );
int nt_tun_accept_new( nt_acc_session_t *s );
int nt_tun_data_forward( nt_acc_session_t *s );
#endif

