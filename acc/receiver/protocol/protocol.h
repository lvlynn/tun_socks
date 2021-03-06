#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <nt_core.h>
#include "debug.h"
#include "tcp.h"


#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>


#define IP_CHK 1
#define TCP_CHK 2


typedef struct{
    unsigned int saddr;
    unsigned int daddr;
    unsigned char mbz; //mbz = 0;                                                                                                      
    unsigned char ptcl;//proto num; tcp = 6;
    unsigned short tcpl; //tcp头部及头层皮载荷长度; 为网络字节; htons(int);

}pshead;


unsigned long chk( const unsigned  short * data, int size );
unsigned short tcp_chk( const unsigned short *data, int size );
unsigned short ip_chk( const unsigned short *data, int size  );
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
	default:
		return ( unsigned short )( ~sum );
	}   
	return sum;
}

typedef struct nt_skb_s {
    uint8_t skb_len;
    uint8_t iphdr_len;

    int protocol;
    nt_buf_t *buffer;       //用来存储当前回复内容
    uint16_t buf_len;
    void *data;

} nt_skb_t;


typedef struct {
    u_int16_t port;
    nt_connection_t *conn;
    nt_skb_t *skb;
} nt_rev_connection_t;


int ip_create( nt_skb_t *skb , struct iphdr *ih);
int ip_input(nt_connection_t *c);
#endif

