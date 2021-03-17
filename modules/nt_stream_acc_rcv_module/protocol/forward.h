#ifndef _FORWARD_H_
#define _FORWARD_H_

#include <nt_core.h>

nt_int_t nt_upstream_socks_send( nt_connection_t *c );
nt_int_t nt_upstream_socks_read( nt_connection_t *c );
void nt_tun_tcp_data_upload_handler( nt_connection_t *c );
void nt_tun_tcp_download_handler( nt_event_t *ev );
nt_int_t nt_downstream_raw_read_send( nt_connection_t *c );

#endif

