#ifndef _UDP_H_
#define _UDP_H_

#include "protocol.h"

int udp_input( nt_connection_t *c );

int udp_output( nt_connection_t *c );

typedef struct nt_skb_udp_s {
    struct sockaddr *src;  //源信息
    struct sockaddr *dst;  //目的信息

    u_int8_t phase; //tcp处于哪个阶段
    uint16_t hdr_len; //数据包的头部长度

    uint16_t data_len; //数据包的载荷长度
    u_char *data ;
     
} nt_skb_udp_t;


#endif

