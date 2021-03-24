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




nt_int_t nt_acc_session_finalize( nt_acc_session_t *s );
nt_int_t nt_sock5_tcp_upstream_free( nt_connection_t *c );
nt_int_t nt_stream_init_socks5_upstream( nt_connection_t *c );
 // nt_int_t nt_tun_socks5_handle_phase( nt_acc_session_t *s );
#endif

