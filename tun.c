#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <errno.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <time.h>
#include <stdlib.h>
#define ICMP 1
#define IGMP 2
#define TCP 6
#define UDP 17
#define IGRP 88
#define OSPF 89
#include "./libnet/ip.h"
#include "./libnet/tcp.h"
#include "./libnet/log.h"




typedef struct tun_data {
    unsigned char data[4096];
    struct ip *ip;
    void * tl_proto;
} t_data;



int interface_up( char *interface_name )
{
    int s;

    if( ( s = socket( PF_INET, SOCK_STREAM, 0 ) ) < 0 ) {
        debug( "Error create socket: %d\n", errno );
        return -1;
    }

    struct ifreq ifr;
    strcpy( ifr.ifr_name, interface_name );

    short flag;
    flag = IFF_UP;

    if( ioctl( s, SIOCGIFFLAGS, &ifr ) < 0 ) {
        debug( "Error up %s : %d\n", interface_name, errno );
        return -1;
    }

    ifr.ifr_ifru.ifru_flags |= flag;
    if( ioctl( s, SIOCSIFFLAGS, &ifr ) < 0 ) {
        debug( "Error up %s :%d\n", interface_name, errno );
        return -1;
    }

    return 0;
}

int set_ipaddr( char * interface_name, char *ip )
{
    int s;

    if( ( s = socket( PF_INET, SOCK_STREAM, 0 ) ) < 0 ) {
        debug( "Error up %s : %d\n", interface_name, errno );
        return -1;
    }

    struct ifreq ifr;
    strcpy( ifr.ifr_name, interface_name );

    struct sockaddr_in addr;
    bzero( &addr, sizeof( struct sockaddr_in ) );
    addr.sin_family = PF_INET;
    inet_aton( ip, &addr.sin_addr );

    memcpy( &ifr.ifr_ifru.ifru_addr, &addr, sizeof( struct sockaddr_in ) );

    if( ioctl( s, SIOCSIFADDR, &ifr ) < 0 ) {
        debug( "Error set %s ip %d\n", interface_name, errno );
        return -1;
    }

    return 0;
}

int tun_create( char *dev, int flags )
{
    struct ifreq ifr;
    int fd, err;

    if( ( fd = open( "/dev/net/tun", O_RDWR ) ) < 0 ) {
        debug( "Error : %d\n", errno );
        return -1;
    }

    memset( &ifr, 0, sizeof( ifr ) );
    ifr.ifr_flags |= flags;

    if( *dev != '\0' ) {
        strncpy( ifr.ifr_name, dev, IFNAMSIZ );
    }

    if( ( err = ioctl( fd, TUNSETIFF, ( void * )&ifr ) ) < 0 ) {
        debug( "Error :%d\n", errno );
        close( fd );
        return -1;
    }

    strcpy( dev, ifr.ifr_name );
    return fd;
}

int route_add( char *interface_name )
{
    int skfd;
    struct rtentry rt;
    struct sockaddr_in dst;
    struct sockaddr_in genmask;

    memset( &rt, 0, sizeof( rt ) );

    genmask.sin_addr.s_addr = inet_addr( "255.255.255.255" );

    bzero( &dst, sizeof( dst ) );
    dst.sin_family = PF_INET;
    dst.sin_addr.s_addr = inet_addr( "10.0.0.1" );

    rt.rt_metric = 0;
    rt.rt_dst = *( struct sockaddr * )&dst;
    rt.rt_genmask = *( struct sockaddr * )&genmask;

    rt.rt_dev = interface_name;
    rt.rt_flags = RTF_UP | RTF_HOST;

    skfd = socket( AF_INET, SOCK_DGRAM, 0 );
    if( ioctl( skfd, SIOCADDRT, &rt ) < 0 ) {
        debug( "Error route add : %d\n", errno );
        return -1;
    }

    return 0;

}

short ipcheck( unsigned short * buf, int size )
{
    unsigned long cksum = 0;
    debug( "sizeof iphr = %d\n", size );
    while( size > 1 ) {
        cksum += *buf++;
        size -= sizeof( unsigned short );
    }
    if( size ) {
        cksum += *( unsigned char * )buf;
    }
    cksum = ( cksum >> 16 ) + ( cksum & 0xffff );
    cksum += ( cksum >> 16 );
    return ( unsigned short )( ~cksum );
}


unsigned short cal_chksum( unsigned short * addr, int len )
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;

    while( nleft > 1 ) {
        sum += *w++;
        nleft -= 2;
    }
    if( nleft == 1 ) {
        * ( unsigned char * )( &answer ) = * ( unsigned char * )w;
        sum += answer;
    }

    sum = ( sum >> 16 ) + ( sum & 0xffff );
    sum += ( sum >> 16 );
    answer = ~sum;

    return answer;

}

#include "signal.h"
int tun;
int main()
{
    /* struct sigaction sa;
     * sa.sa_handler = SIG_IGN;
     * sigaction( SIGPIPE, &sa, 0  );  */
    signal( SIGPIPE, SIG_IGN );

    int  ret;
    char tun_name[IFNAMSIZ];
    struct in_addr ipo;
    char src[16] = {0}, dst[16] = {0};
    struct icmp * icmp = NULL;
    struct tcphdr *tcp = NULL;
    struct udphdr *udp = NULL;
    t_data data = {
        .data = {0},
        .ip = ( struct ip * )data.data,
        .tl_proto = ( void * )( data.data + ( data.ip->ip_hl << 2 ) ),
    };
    tun_name[0] = '\0';
    tun = tun_create( tun_name, IFF_TUN | IFF_NO_PI );
    if( tun < 0 ) {
        return 1;
    }
    debug( "TUN name is %s\n", tun_name );

    struct in_addr ngx_ip;
    ngx_ip.s_addr = inet_addr( "127.0.0.1" );

    interface_up( tun_name );
    route_add( tun_name );
    ret = read( tun, data.data, sizeof( data.data ) );
    //int fd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    unsigned char ip[4];
    int pkg_len = 0;

    while( 1 ) {
        pkg_len = read( tun, data.data, sizeof( data.data ) );

        debug( "------------------>new pkgret=%d", ret );
        if( pkg_len < 0 )
            continue;

        ip_input( data.data  );

    }

    return 0;
}
