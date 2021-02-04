#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <nt_core.h>
#include "debug.h"
#include "tcp.h"
#include "udp.h"


#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>


#define IP_CHK 1
#define TCP_CHK 2
#define UDP_CHK 3


typedef struct{
    unsigned int saddr;
    unsigned int daddr;
    unsigned char mbz; //mbz = 0;                                                                                                      
    unsigned char ptcl;//proto num; tcp = 6;
    unsigned short tcpl; //tcp头部及头层皮载荷长度; 为网络字节; htons(int);

}pshead;


unsigned long chk( const unsigned  short * data, int size );
unsigned short ip_chk( const unsigned short *data, int size  );
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

//一个连接的数据包内容
typedef struct nt_skb_s {
    uint16_t skb_len;
    uint8_t iphdr_len;

    int protocol;
    nt_buf_t *buffer;       //用来存储当前回复内容
    uint16_t buf_len;
    void *data;

} nt_skb_t;

//存储在红黑树中的连接
typedef struct {
    u_int16_t port;
    nt_connection_t *conn;
    nt_skb_t *skb;
    int fd ; //连接中server的fd
} nt_acc_session_t;


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




int ip_create( nt_skb_t *skb , struct iphdr *ih);
int ip_input(nt_connection_t *c);
#endif

