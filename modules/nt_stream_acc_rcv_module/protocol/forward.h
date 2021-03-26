#ifndef _FORWARD_H_
#define _FORWARD_H_

#include <nt_core.h>


nt_int_t nt_socks5_test_connection( nt_connection_t *c);
void nt_tun_upsteam_timeout_handler( nt_event_t *ev );


void nt_tun_data_forward( nt_connection_t *c,  char *buf, size_t n );


nt_int_t nt_upstream_socks_send( nt_connection_t *c , nt_buf_t *b);
nt_int_t nt_upstream_socks_read( nt_connection_t *c );
void nt_tun_tcp_data_upload_handler( nt_connection_t *c );
void nt_tun_tcp_download_handler( nt_event_t *ev );
nt_int_t nt_downstream_raw_read_send( nt_connection_t *c );




void nt_tun_upsteam_tcp_write_handler( nt_event_t *ev );



void nt_upstream_udp_read_handler( nt_event_t *ev );
#endif

