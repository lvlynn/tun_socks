#ifndef _NT_RAW_SOCKET_H_
#define _NT_RAW_SOCKET_H_

#include <nt_core.h>

#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#define IP4_8(var,n) (var >> n) & 0x000000ff
#define IP4_STR(var) IP4_8(var,0),IP4_8(var,8),IP4_8(var,16),IP4_8(var,24)


#define TCP_PHASE_SEND_PSH 15
#define TCP_PHASE_SEND_PSH_END 16

typedef struct{
    nt_int_t   phase;
	nt_int_t	tot_len;
	

    uint32_t   seq;
    uint32_t   ack_seq;


    //上一次发送的载荷长度
    nt_int_t   payload_len;

    //当前要发送的载荷长度，
    nt_int_t   data_len;
    //当前要发送的载荷
    void *data;


    uint32_t sip;   //源端口
    uint16_t sport; //源ip
    uint32_t dip;   //目的端口
    uint16_t dport; //目的ip

} nt_tcp_raw_socket_t;


typedef struct{
    nt_int_t   phase;
	nt_int_t	tot_len;
	
    //当前要发送的载荷长度，
    nt_int_t   data_len;
    //当前要发送的载荷
    void *data;

    uint32_t sip;   //源端口
    uint16_t sport; //源ip
    uint32_t dip;   //目的端口
    uint16_t dport; //目的ip

} nt_udp_raw_socket_t;



#define IP_CHK 1
#define TCP_CHK 2
#define UDP_CHK 3



unsigned long nt_chk( const unsigned  short * data, int size );
unsigned short nt_tcp_chk( const unsigned short *data, int size );
unsigned short nt_ip_chk( const unsigned short *data, int size  );

void nt_print_pkg( char *data );
void nt_tcp_create( nt_buf_t *b , nt_tcp_raw_socket_t *rs );
void nt_udp_create( nt_buf_t *b , nt_udp_raw_socket_t *rs );

#if 0
typedef struct{
	unsigned int saddr;
	unsigned int daddr;
	unsigned char mbz; //mbz = 0;                                                                                                        
	unsigned char ptcl;//proto num; tcp = 6;
	unsigned short tcpl; //tcp头部及头层皮载荷长度; 为网络字节; htons(int);

}pshead;



typedef struct{
	uint32_t seq;
	uint32_t ack;

	// 100 bit
	uint32_t data_len;

	char data[1500];
} nt_acc_tcp_t;

typedef struct{
	// 100 bit
	uint32_t data_len;

	char data[1500];
} nt_acc_udp_t;



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
	int protocol; //tcp udp icmp                                                                               
	int type;  //数据包类型 ， 1，发起连接  2. 转发payload
	int domain; //ipv4 /ipv6

} nt_socks_protocol_t;


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
#endif

#endif

