
#include <nt_core.h>
#include <nt_stream.h>
#include "nt_stream_socks5_module.h"

//========================================

/* Socks5 authentication packet */
typedef struct {
    char ver;
    char ulen;
    char *uname;
    char plen;
    char *passwd;

} Socks5Auth;

typedef struct _METHOD_REQ_ {
    uint8_t ver;
    uint8_t methods;
    uint8_t method[255];
} socks5_method_req_t;

typedef struct _METHOD_REP_ {
    uint8_t ver;
    uint8_t method;   // 0: 无用户名密码 2: 有用户名密码
} socks5_method_rep_t;


typedef struct _AUTH_REQ_ {
    char version;       // 版本，此处恒定为0x01
    char name_len;      // 第三个字段用户名的长度，一个字节，最长为0xff
    char name[255];     // 用户名
    char pwd_len;       // 第四个字段密码的长度，一个字节，最长为0xff
    char pwd[255];      // 密码

} socks5_auth_req_t;

typedef struct _AUTH_REP_ {
    char version;       // 版本，此处恒定为0x01
    char result;        // 服务端认证结果，0x00为成功，其他均为失败
} socks5_auth_rep_t;

typedef struct _COMMEND_ {
    char version; // 客户端支持的Socks版本，0x04或者0x05
    char cmd; // 客户端命令，CONNECT为0x01，BIND为0x02，UDP为0x03，一般为0x01
    char reserved; // 保留位，恒定位0x00
    char address_type; // 客户端请求的真实主机的地址类型，IP V4为0x01
} socks5_commend_t;


typedef struct _REP_ {
    char version; // 服务器支持的Socks版本，0x04或者0x05
    char reply; // 代理服务器连接真实主机的结果，0x00成功
    char reserved; // 保留位，恒定位0x00
    char address_type; // Socks代理服务器绑定的地址类型，IP V4为0x01
} socks5_rep_t;


// 代理服务器信息
typedef struct _PROXY_SERVER_ {
    uint32_t ip;
    uint16_t port;
    char uname[32];
    char upswd[32];
} proxy_info_t;


typedef struct _HANDLE_ {
    int32_t      tcp_fd;
    int32_t      udp_fd;
    uint16_t     udp_port;
    uint32_t     dest_ip;   // 目标服务器ip
    uint16_t     dest_port; // 目标服务器端口
    proxy_info_t proxy_info;
    uint8_t      protocol;
} socks5_handle_t;


#include "nt_stream.h"
 int socks5_method_request( nt_buf_t *buf )
//int socks5_method_request( int fd )
{
    *buf->last++ = 0x05;   //ver
    *buf->last++ = 0x02;   //methods
    *buf->last++ = 0;      //method[0]
    *buf->last++ = 2;      //method[1]

    return 0;
}

/* Build authentication packet in buf,
    * It set subnegotiation version to 0x01
    * It get uname/password from c->config.cli->username/password
    * It copy Socks5Auth struct in buf->data.
    * And set buf->a to zero and buf->b to 3 + req.ulen + req.plen
    * Return:
    *  -1, error no username or password
    *   0, success
    *
    * From RFC1929:
    * Once the SOCKS V5 server has started, and the client has selected the
    * Username/Password Authentication protocol, the Username/Password
    * subnegotiation begins.  This begins with the client producing a
    * Username/Password request:
    *
    * +----+------+----------+------+----------+
    * |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
    * +----+------+----------+------+----------+
    * | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
    * +----+------+----------+------+----------+
    *
    * The VER field contains the current version of the subnegotiation,
    * which is X'01'
    */
int socks5_auth_request( nt_buf_t *buf, char *uname, char *upswd )
{
    size_t c = 0;
    uint32_t ulen = strlen( uname );
    uint32_t plen = strlen( upswd );

    *buf->last++ = 0x01; //ver;

    c = ulen & 0xff;
    *buf->last++ = c;    //存入ulen

    memcpy( buf->last, uname, c ); //存入uname
    buf->last += c;

    c = plen & 0xff;
    *buf->last++ = c;     //存入plen

    memcpy( buf->last, upswd, c ); //存入upswd
    buf->last += c;

    return 0;
}


int socks5_auth_response( int32_t fd )
{
    char in[4] = {0};
    if( 2 != read( fd, in, 2 ) ) {
        printf( "recv auth response failed! [%s]\n", strerror( errno ) );
        return -1;
    }

    //STATUS是0x00，代表验证成功，非0x00,代表鉴权失败，必须关闭连接
    if( in[0] == 0x01 && in[1] == 0x00 ) {
        return 0;
    }

    printf( "recv auth response failed ver: %d status: %d\n", in[0], in[1] );
    return -1;
}

int socks5_method_response( int32_t fd,  char *user, char *passwd )
{
    socks5_method_rep_t socks5_method_rep;
    memset( &socks5_method_rep, 0, sizeof( socks5_method_rep ) );
    if( 2 != read( fd, &socks5_method_rep, 2 ) ) {
        printf( "recv method response failed![%s]\n", strerror( errno ) );
        return -1;
    }

    printf( "socks5_method_rep.method=%x\n", socks5_method_rep.method );

    if( socks5_method_rep.ver == 0x05 && socks5_method_rep.method == 0x00 ) {
        return 0;
    } else if( socks5_method_rep.ver == 0x05 && socks5_method_rep.method == 0x02 ) {
        // 需要用户名密码验证
        /* if( socks5_auth_request( fd, user, passwd ) < 0 )
         *     return -2; */
        if( socks5_auth_response( fd ) < 0 )
            return -3;
    } else {
        printf( "客户端验证方式不正确！[%s] [%d]\n ", __FILE__, __LINE__ );
        return -1;
    }
    return 1;
}


/*
 *
 *
 SOCKS请求如下表所示:
  +----+-----+-------+------+----------+----------+ 
  | VER| CMD | RSV   | ATYP |  DST.ADDR|  DST.PORT|
  +----+-----+-------+------+----------+----------+ 
  | 1  | 1   | X'00' | 1    | variable |      2   |
  +----+-----+-------+------+----------+----------+ 
``
  各个字段含义如下:
  VER  版本号X'05'
  CMD：  
       1. CONNECT X'01'
       2. BIND    X'02'
       3. UDP ASSOCIATE X'03'
  RSV  保留字段
  ATYP IP类型 
       1.IPV4 X'01'
       2.DOMAINNAME X'03'
       3.IPV6 X'04'
  DST.ADDR 目标地址 
       1.如果是IPv4地址，这里是big-endian序的4字节数据
       2.如果是FQDN，比如"www.nsfocus.net"，这里将是:
         0F 77 77 77 2E 6E 73 66 6F 63 75 73 2E 6E 65 74
         注意，没有结尾的NUL字符，非ASCIZ串，第一字节是长度域
       3.如果是IPv6地址，这里是16字节数据。
  DST.PORT 目标端口（按网络次序排列） 


**sock5响应如下:**
 OCKS Server评估来自SOCKS Client的转发请求并发送响应报文:
 +----+-----+-------+------+----------+----------+
 |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
 +----+-----+-------+------+----------+----------+
 | 1  |  1  | X'00' |  1   | Variable |    2     |
 +----+-----+-------+------+----------+----------+
 VER  版本号X'05'
 REP  
      1. 0x00        成功
      2. 0x01        一般性失败
      3. 0x02        规则不允许转发
      4. 0x03        网络不可达
      5. 0x04        主机不可达
      6. 0x05        连接拒绝
      7. 0x06        TTL超时
      8. 0x07        不支持请求包中的CMD
      9. 0x08        不支持请求包中的ATYP
      10. 0x09-0xFF   unassigned
 RSV         保留字段，必须为0x00
 ATYP        用于指明BND.ADDR域的类型
 BND.ADDR    CMD相关的地址信息，不要为BND所迷惑
 BND.PORT    CMD相关的端口信息，big-endian序的2字节数据

 *
 * */
int socks5_dest_request( nt_buf_t *buf, uint32_t dest_ip, uint16_t dest_port , uint8_t type )
{

    *buf->last++ =  0x05;  //ver
 
    if( type == 2 )
        *buf->last++ =  0x03;  // cmd  // 1 tcp  ,3 udp
    else
        *buf->last++ =  0x01;  // cmd  // 1 tcp  ,3 udp

    *buf->last++ =  0;     //reserved 

    *buf->last++ =  1;     //ipv4 

    memcpy( buf->last, &dest_ip, 4 );
    buf->last += 4;

    memcpy( buf->last, &dest_port, 2 );
    buf->last += 2;

    return 0;
}



static int sockt5_send( nt_connection_t *conn, char *buff, int size )
{

    nt_buf_t buf;
    buf.start = buff;
    buf.pos = buf.last = buf.start;
    buf.end = buf.last + size;

    int i = 0;
    for( i; i < 3; i++ ) {
        int rc ;
        rc = conn->send( conn, buf.pos, buf.last - buf.pos );

        if( rc < ( buf.last - buf.pos ) ) {
            buf.pos += rc;
            continue;
        }

        if( rc == NT_AGAIN )
            continue;

        if( rc == NT_ERROR )
            return NT_ERROR;

        if( rc == buf.last - buf.pos )
            return NT_OK;
    }

    return NT_ERROR;
}



static int read_n_bytes( int32_t fd, char *buf, int len )
{
    int rlen = 0;
    size_t r_tol_len = 0;
    while( 1 ) {
        rlen = read( fd, buf, len - r_tol_len );
        if( rlen <= 0 )
            return rlen;
        r_tol_len += rlen;
        if( r_tol_len == len )
            return r_tol_len;
    }
    return 0;
}



static int socks5_dest_response( int32_t fd )
{
    printf( "%s start\n", __func__ );
    char buff[512] = {0};
    int len = 0;
    uint16_t udp_port = 0;
    char ip_str[16] = {0};
    uint32_t ip;

    if( 4 != read_n_bytes( fd, buff, 4 ) ) {
        if( buff[1] == 0x07 ) {
            printf( "proxy server not support udp!\n" );
            return -1;
        }
        printf( "read dest response failed![%d] [%s]\n", errno, strerror( errno ) );
        return -1;
    }

    if( buff[1] == 0x05 ) {
        printf( "proxy refuse!!!\n" );
        return -1;
    }

    if( 6 != read_n_bytes( fd, buff, 6 ) ) {
        printf( "read dest response port failed! [%s]\n", strerror( errno ) );
        return -1;
    }
    ip = *( ( uint32_t* )( buff ) );
    //  inet_ntop( AF_INET, &ip, ip_str );
    //printf("ip:%s\n", ip_str);

    return 0;
//    if( socks5_handle.protocol == 6 )
//        return 0;

    // udp 会重新创建udp_fd
    /* udp_port = *( ( uint16_t* )( buff + 4 ) );

    socks5_handle.udp_port = udp_port;
    socks5_handle.udp_fd = create_socks5_udp_fd();
    if( socks5_handle.udp_fd < 0 ) {
        return -1;
    } */
    printf( "%s end\n", __func__ );
}


