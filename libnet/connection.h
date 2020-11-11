#ifndef _CONNECTION_H_
#define _CONNECTION_H_

//#include "./tcp.h"
/** Main packet buffer struct */
#include "common.h"

struct nt_skb_s {
    char *skb ;  //接收的数据包
    uint8_t skb_len;

    struct iphdr *ih;
    uint8_t ip_version;  //ipv4 or ipv6
    uint8_t iphdr_len;  //ipv4 or ipv6

    uint8_t protocol; //协议类型

    union {
        nt_tcp_info_t  *tcp_info;
    };


    char *pkg ; //用来存放回应包
    uint16_t pkg_len ;
    uint8_t type;
};

//保存上一个包的seq ，ack序号
struct nt_last_s{
    uint16_t payload_len; //上一个数据包的长度
    uint32_t seq ;   //存主机字节序
    uint32_t ack_seq ;  //存主机字节序
};

struct connection_s {

    //一个连接的源和目的信息

    union{
        nt_tpcb_t *pcb;
    };
    //   nt_tcp_info_t *test;
    //        udp_pcb tpcb;

    //该连接新接收到的数据包信息
    nt_skb_t *ndt;

    //上一个数据包的信息
    nt_last_t *odt;   // 存seq跟ack信息

};


#endif
