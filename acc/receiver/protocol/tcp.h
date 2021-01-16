#ifndef _ACC_TCP_H_
#define _ACC_TCP_H_

#include "protocol.h"

#define TCP_SRC 1
#define TCP_DST 2


/*
 *  *  TCP option
 *   */

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
 *  * experimental options. See draft-ietf-tcpm-experimental-options-00.txt
 *   */
#define TCPOPT_FASTOPEN_MAGIC   0xF989

/*
 *  *     TCP option lengths
 *   */

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



#define TCP_PHASE_NULL 0


//所有进来的ACK，都标记为这个

//client 发起
#define TCP_PHASE_SYN 1

//client 发起
#define TCP_PHASE_SYN_ACK 2

#define TCP_PHASE_ACK 3

//server 返回
#define TCP_PHASE_SEND_SYN 4

//server 返回
#define TCP_PHASE_SEND_SYN_ACK 5


//client 发payload; payload长度不为0 ,并且 psh标志位为0
#define TCP_PHASE_PSH 11

//client 发payload 结束，psh标志位为1 
#define TCP_PHASE_PSH_END 12

//回应psh
#define TCP_PHASE_PSH_ACK 13


#define TCP_PHASE_SEND_PSH_ACK 14

#define TCP_PHASE_SEND_PSH 15

#define TCP_PHASE_SEND_PSH_END 16



//fin跟ack标志位都为1
#define TCP_PHASE_FIN 21

#define TCP_PHASE_FIN_ACK 22

#define TCP_PHASE_SEND_FIN 23

//回应fin   ack标志位都为1
#define TCP_PHASE_SEND_FIN_ACK 24


#define TCP_PHASE_PROXY_DIRECT 31

#define TCP_PHASE_PROXY_SOCKS 32


typedef struct nt_skb_tcp_s {
    struct sockaddr *src;  //源信息
    struct sockaddr *dst;  //目的信息

    u_int8_t phase; //tcp处于哪个阶段
    uint16_t hdr_len; //数据包的头部长度

    uint16_t data_len; //数据包的载荷长度
    char *data ;


    uint16_t payload_len; //上一个数据包的载荷长度
    uint32_t seq ;   //存主机字节序
    uint32_t ack_seq ;  //存主机字节序

   
} nt_skb_tcp_t;

typedef struct{
    int type;  //数据包类型 ， 1，发起连接  2. 转发payload
    int domain; //ipv4 /ipv6
    int protocol; //tcp udp icmp
    int seq;
    int ack;

    char user[32] ; //用户账号
    char password[32]; //用户密码
    uint32_t server_ip;  //代理ip
    uint16_t server_port; //代理端口

    uint32_t sip;   //源端口
    uint16_t sport; //源ip
    uint32_t dip;   //目的端口
    uint16_t dport; //目的ip
    // 100 bit
    uint16_t data_len;

    char data[1500];
} nt_tcp_socks_t;


u_int16_t tcp_get_port( char *pkg , u_int8_t direction );
void tcp_rbtree( nt_connection_t *c );

int tcp_input( nt_connection_t *c );
int  tcp_output( nt_connection_t *c );
int tcp_phase_handle( nt_connection_t *c  );


#endif

