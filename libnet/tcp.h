#ifndef _NT_TCP_H_
#define _NT_TCP_H_

#include <netinet/ip.h>
#include <netinet/tcp.h>
#include "common.h"

/*
 *  TCP option
 */

#define TCPOPT_NOP      1   /* Padding */
#define TCPOPT_EOL      0   /* End of options */
#define TCPOPT_MSS      2   /* Segment size negotiating */
#define TCPOPT_WINDOW       3   /* Window scaling */
#define TCPOPT_SACK_PERM        4       /* SACK Permitted */
#define TCPOPT_SACK             5       /* SACK Block */
#define TCPOPT_TIMESTAMP    8   /* Better RTT estimations/PAWS */
#define TCPOPT_MD5SIG       19  /* MD5 Signature (RFC2385) */
#define TCPOPT_FASTOPEN     34  /* Fast open (RFC7413) */
#define TCPOPT_EXP      254 /* Experimental */
/* Magic number to be after the option value for sharing TCP
 * experimental options. See draft-ietf-tcpm-experimental-options-00.txt
 */
#define TCPOPT_FASTOPEN_MAGIC   0xF989

/*
 *     TCP option lengths
 */

#define TCPOLEN_MSS            4
#define TCPOLEN_WINDOW         3
#define TCPOLEN_SACK_PERM      2
#define TCPOLEN_TIMESTAMP      10
#define TCPOLEN_MD5SIG         18
#define TCPOLEN_FASTOPEN_BASE  2
#define TCPOLEN_EXP_FASTOPEN_BASE  4

/* But this is what stacks really send out. */
#define TCPOLEN_TSTAMP_ALIGNED      12
#define TCPOLEN_WSCALE_ALIGNED      4
#define TCPOLEN_SACKPERM_ALIGNED    4
#define TCPOLEN_SACK_BASE       2
#define TCPOLEN_SACK_BASE_ALIGNED   4
#define TCPOLEN_SACK_PERBLOCK       8
#define TCPOLEN_MD5SIG_ALIGNED      20
#define TCPOLEN_MSS_ALIGNED     4



#define TCP_SYN_ACK 0
#define TCP_PAYLOAD_ACK 1
#define TCP_SEND_PAYLOAD 3
#define TCP_SEND_PAYLOAD_END 4
#define TCP_FIN_ACK 5
#define TCP_SEND_FIN_ACK 6

struct tcp_pcb_s {
    union
    {   
        struct
        {   
            uint32_t ip;
            uint16_t port;

        }  ipv4_src;
#ifdef NT_IPV6
        struct
        {   
            uint8_t ip[16];
            uint16_t port;
        } ipv6_src;
#endif
    };  

    union
    {   
        struct
        {   
            uint32_t ip;
            uint16_t port;

        } ipv4_dst;
#ifdef NT_IPV6
        struct
        {   
            uint8_t ip[16];
            uint16_t port;
        } ipv6_dst;
#endif
    };  


};



struct tcp_info_s{
//    struct tcphdr *th;    
    int16_t hdr_len ; //tcp 头部长度，包含option

    char *payload; //载荷
    int16_t payload_len; //载荷长度

    uint32_t seq ;
    uint32_t ack_seq ;

};



void tcp_parse_option( char *data );
void tcp_create( nt_conn_t *conn );

extern int tun;

int tcp_input(nt_conn_t *conn);

#endif
