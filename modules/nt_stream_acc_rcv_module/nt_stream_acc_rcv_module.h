#ifndef _NT_STREAM_ACC_RCV_MODULE_H_
#define _NT_STREAM_ACC_RCV_MODULE_H_



#include "protocol/protocol.h"

enum{
    NT_TUN_ACC_DROP = 0,
    NT_TUN_ACC_LOCAL,
    NT_TUN_ACC_PROXY,
};

int nt_open_tun_socket();
void nt_event_accept_tun( nt_event_t *ev );
#endif

