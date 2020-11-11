#ifndef __MYTCPIP_H__
#define __MYTCPIP_H__

#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include "connection.h"


int ip_input( const char*data );
int ipv4_input( nt_conn_t *conn );



#define IP_CHK 1
#define TCP_CHK 2


enum{
    IP_PKG = 1,
    TCP_PKG,
    UDP_PKG,
    ICMP_PKG,
}PKG_TYPE;

#define TCP_PKG_LEN (sizeof(struct iphdr) + sizeof(struct tcphdr))
#define UDP_PKG_LEN (sizeof(struct iphdr) + sizeof(struct udphdr))



typedef struct{
    unsigned int saddr;
    unsigned int daddr;
    unsigned char mbz; //mbz = 0;
    unsigned char ptcl;//proto num; tcp = 6;
    unsigned short tcpl; //tcp头部及头层皮载荷长度; 为网络字节; htons(int);
}pshead;



unsigned long chk( const unsigned  short * data, int size);
unsigned short chksum(const char *data, int size, int type);

int create_pkg( nt_conn_t *conn);
int ip_create( nt_conn_t *conn );


void print_pkg(char *data);

#endif
