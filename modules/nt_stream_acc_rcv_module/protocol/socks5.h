#ifndef _SOCKS_H_
#define _SOCKS_H_

#include <nt_core.h>
#include "protocol.h"

enum SOCKS5_PHASE {
    SOCKS5_ERROR = 0,
    SOCKS5_VERSION_REQ = 1,
    SOCKS5_VERSION_RESP = 2,
    SOCKS5_USER_REQ = 3,
    SOCKS5_USER_RESP = 4,
    SOCKS5_SERVER_REQ = 5,
    SOCKS5_SERVER_RESP = 6,
    SOCKS5_DATA_FORWARD = 7,
};



typedef struct {
   
   //socks 阶段
   int8_t phase;
   int8_t type;

   nt_connection_t *tcp_conn;    

   nt_connection_t *udp_conn;

} nt_upstream_socks_t;


nt_int_t nt_stream_init_socks5_upstream( nt_connection_t *c );
 // nt_int_t nt_tun_socks5_handle_phase( nt_acc_session_t *s );
#endif

