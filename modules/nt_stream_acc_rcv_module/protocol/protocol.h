#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <nt_core.h>
#include "debug.h"


typedef struct {
   
   //socks 阶段
   int8_t phase;
   int8_t type;

   nt_connection_t *tcp_conn;    

   nt_connection_t *udp_conn;

} nt_upstream_socks_t;



//一个连接的数据包内容
typedef struct nt_skb_s {
    uint16_t skb_len;  //进来的数据包总长
    uint8_t iphdr_len; //ip层头长

    int protocol;      //数据包协议
    nt_buf_t *buffer;  //用来存储当前回复内容
    uint16_t buf_len;  //要回复的内容长度
    void *data;        //存储 tcp/udp 内容( nt_skb_tcp_t, nt_skb_udp_t )
} nt_skb_t;


//存储在红黑树中的连接
typedef struct {

    nt_connection_t     *connection;
    off_t               received;
    time_t              start_sec;
    nt_msec_t           start_msec;
    nt_log_handler_pt   log_handler;

    nt_upstream_socks_t *ss;

    u_int16_t           port;
    nt_connection_t     *conn;
    nt_skb_t            *skb;
    int fd ; 
} nt_acc_session_t;




#include "forward.h"
#include "tcp.h"
#include "udp.h"
#include "socks5.h"


#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>


#define IP_CHK 1
#define TCP_CHK 2
#define UDP_CHK 3

#define SEND_USE_TUN_FD 1

typedef struct {
    unsigned int saddr;
    unsigned int daddr;
    unsigned char mbz; //mbz = 0;
    unsigned char ptcl;//proto num; tcp = 6;
    unsigned short tcpl; //tcp头部及头层皮载荷长度; 为网络字节; htons(int);

} pshead;


unsigned long chk( const unsigned  short * data, int size );
unsigned short ip_chk( const unsigned short *data, int size );
unsigned short tcp_chk( const unsigned short *data, int size );
unsigned short udp_chk( const unsigned short *data, int size );
void print_pkg( char *data );


static unsigned short chksum( const char *data, int size, int type )
{
    unsigned short sum = 0;
    unsigned short *udata = ( unsigned short * )data;
    switch( type ) {
    case IP_CHK:
        sum = ip_chk( udata, size );
        break;
    case TCP_CHK:
        sum = tcp_chk( udata, size );
        break;
    case UDP_CHK:
        sum = udp_chk( udata, size );
        break;
    default:
        return ( unsigned short )( ~sum );
    }
    return sum;
}



typedef struct {
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


} nt_acc_socks5_auth_t;




int acc_tcp_ip_create(   nt_buf_t *b, nt_skb_tcp_t *tcp );
void acc_tcp_psh_create( nt_buf_t *b, nt_skb_tcp_t *tcp );


int ip_create( nt_skb_t *skb, struct iphdr *ih );
nt_connection_t*  ip_input( char *data );
 nt_int_t nt_tun_socks5_handle_phase( nt_acc_session_t *s );
void nt_tun_socks5_read_handler( nt_event_t *ev );
#endif
