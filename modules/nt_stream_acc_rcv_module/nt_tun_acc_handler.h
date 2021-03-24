#ifndef _NT_TUN_ACC_HANDLER_H_
#define _NT_TUN_ACC_HANDLER_H_

int nt_tun_data_forward_handler( nt_event_t *ev );
int nt_tun_close_session( nt_connection_t *c );
int nt_connection_rbtree_add( nt_connection_t *c );
int nt_tun_accept_new( nt_acc_session_t *s );
// int nt_tun_data_forward( nt_acc_session_t *s );
#endif

