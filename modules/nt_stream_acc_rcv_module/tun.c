#include <nt_core.h>
#include "tun.h"
#include "debug.h"

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


int tun_create( char *dev, int flags )
{
    struct ifreq ifr;
    int fd, err;

    //发现ac66u-b1 里面的 tun path 为 /dev/tun ，但得用 /dev/net/tun 才行
    if( ( fd = open( "/dev/net/tun", O_RDWR ) ) < 0 ) {
//    if( ( fd = open( "/dev/tun", O_RDWR ) ) < 0 ) {
        debug( "open Error : %d\n", errno );
        return -1;

    }

    memset( &ifr, 0, sizeof( ifr ) );
    ifr.ifr_flags |= flags;

    if( *dev != '\0' ) {
        strncpy( ifr.ifr_name, dev, IFNAMSIZ );

    }

    if( ( err = ioctl( fd, TUNSETIFF, ( void * )&ifr ) ) < 0 ) {
        debug( "TUNSETIFF Error :%d\n", errno );
        close( fd );
        return -1;

    }

//    strcpy( dev, ifr.ifr_name );
    return fd;

}

int tun_init( char *interface_name )
{
    int ret ;
    int fd ;
    /* char *tun_name = "nt"; */
    fd = tun_create( interface_name, IFF_TUN | IFF_NO_PI );
    if( fd < 0 )
        return -1;

    ret = interface_up( interface_name );
    if( ret < 0 )
        return -1;

    return fd;
}
