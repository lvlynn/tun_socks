#ifndef _UDP_H_
#define _UDP_H_

#include "protocol.h"



#define UDP_PHASE_SOCKS_CONNECT 1

#define UDP_PHASE_DATA_FORWARD 2

#define UDP_PHASE_SOCKS_DOWN 3




int udp_input( nt_connection_t *c );

int udp_output( nt_connection_t *c );


typedef struct nt_skb_udp_s {
    struct sockaddr *src;  //源信息
    struct sockaddr *dst;  //目的信息
    uint32_t sip;   //源端口
    uint16_t sport; //源ip
    uint32_t dip;   //目的端口
    uint16_t dport; //目的ip


    u_int8_t phase; //tcp处于哪个阶段
    uint16_t hdr_len; //数据包的头部长度
    uint16_t ip_id; //udp id 标识，用于分片数据的重组

    uint16_t data_len; //数据包的载荷长度
    u_char *data ;

    uint16_t    tot_len;
     
} nt_skb_udp_t;


typedef struct{
    int protocol; //tcp udp icmp
    int type;  //数据包类型 ， 1，发起连接  2. 转发payload
    int domain; //ipv4 /ipv6

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
} nt_udp_socks_t;


typedef struct{
    // 100 bit
    uint32_t data_len;

    char data[1500];
} nt_acc_udp_t;

nt_connection_t* acc_udp_input( char *data );
void acc_udp_create( nt_buf_t *b, nt_skb_udp_t *udp );

#endif

