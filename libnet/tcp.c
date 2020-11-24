#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "./log.h"
#include "./ip.h"
#include "tcp.h"




struct{
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
}




/*
 *   Kind | Length | Name           | RFC     | 描述
 *   0    | 1      | EOL            | RFC0793 | 结束TCP选项列表
 *   1    | 1      | NOP            | RFC0793 | 空操作填充TCP选项
 *   2    | 4      | MSS            | RFC0793 | 最大Segment长度
 *   3    | 3      | WSOPT          | RFC1323 | 窗口扩大系数
 *   4    | 2      | SACK-Premitted | RFC2018 | 表明支持SACK
 *   5    | 可变   | SACK           | RFC2018 | SACK Block（收到乱序数据
 *   8    | 10     | TSOPT          | RFC1323 | Timestamps
 *   19   | 18     | TCP-MD5        | RFC2385 | MD5认证
 *   28   | 4      | UTO            | RFC5482 | User Timeout（超过一定闲置时间后拆除连接）
 *   29   | 可变   | TCP-AO         | RFC5925 | 认证
 * */

void tcp_parse_option( char *data )
{
    const unsigned char *ptr;
    struct iphdr *ih = ( struct iphdr * )data;
    const struct tcphdr *th = ( struct tcphdr * )( ih + 1 );    //取得tcp的头部
    int length = ( th->doff * 4 ) - sizeof( struct tcphdr ); //取得除tcp头部的的长度

    ptr = ( const unsigned char * )( th + 1 ); //跳过tcp的头部

    while( length > 0 ) {
        int opcode = *ptr++;  //选项kind
        int opsize;  //选项长度，包括kind和length

        switch( opcode ) {
        case TCPOPT_EOL: //End of options ,options的结束标志
            return;
        case TCPOPT_NOP:    /* Ref: RFC 793 section 3.1 */ //填充标志，为了字节对齐
            length--;
            continue;
        default:
            opsize = *ptr++;
            if( opsize < 2 ) /* "silly options" */ //选项大小小于2个字节，直接退出
                return;
            if( opsize > length )
                return;    /* don't parse partial options */
            switch( opcode ) {
            case TCPOPT_MSS: { //对端发送的长度的限制，整个tcp传输segment的过程中不能大于这个值
                if( opsize == TCPOLEN_MSS && th->syn ) {
                    uint16_t maxseg = ntohs( *( uint16_t * )ptr );
                    debug( "maxseg=%u ", maxseg );
                }
                break;
            }
            case TCPOPT_WINDOW:  //窗口扩大选项
                if( opsize == TCPOLEN_WINDOW && th->syn ) {
                    debug( "wscale=%u ", *( uint8_t * )ptr );
                }
                break;
            case TCPOPT_SACK_PERM:
                if( opsize == TCPOLEN_SACK_PERM && th->syn ) {
                    debug( "TCPOPT_SACK_PERM" );
                }
                break;
            case TCPOPT_SACK:
                debug( "TCPOPT_SACK opsize=%d", opsize );
                break;
            case TCPOPT_TIMESTAMP:
                debug( "TCPOPT_TIMESTAMP" );
                break;
            case TCPOPT_MD5SIG:
                debug( "TCPOPT_MD5SIG" );
                break;
            default:
                break;
            }
        }

        ptr += opsize - 2;
        length -= opsize;   //减去一个option的大小，接着再进入while循环，进行下一个选项的解析。
    }

}


void tcp_create( nt_conn_t *conn )
{

    nt_skb_t *ndt = conn->ndt;
    struct iphdr *ih = ( struct iphdr * )ndt->skb;
    struct tcphdr *th      = ( struct tcphdr * )( ndt->skb + ndt->iphdr_len);
//    struct iphdr * pkg_ih  = ip_create( conn );
    struct iphdr * pkg_ih  = ( struct iphdr *)ndt->pkg ;

    struct tcphdr * pkg_th = ( struct tcphdr * )( pkg_ih + 1 );
    pkg_th->source         = th->dest;                            // tcp source port
    pkg_th->dest           = th->source;                          // tcp dest port

    
    debug( "------before-----" );
    int playload_len = ndt->skb_len - ndt->iphdr_len - ndt->tcp_info->hdr_len ;
    debug( "before-->playload_len=%d", playload_len );
    debug( "before-->th->seq=%u", ntohl (th->seq  ));
    debug( "before-->th->ack_seq=%u", ntohl (th->ack_seq  ));

    
    /* if( playload_len )
        pkg_th->ack_seq        = htonl( ntohl( th->seq ) + playload_len );       // tcp ack number
    else
        pkg_th->ack_seq        = htonl( ntohl( th->seq ) + 1 );       // tcp ack number */

    //先置零    
    pkg_th->urg =0;
    pkg_th->ack =0;
    pkg_th->psh =0;
    pkg_th->rst =0;
    pkg_th->syn =0;
    pkg_th->fin =0;

    if( ndt->type == TCP_SYN_ACK  ){
        debug( " TCP_SYN_ACK " );
        pkg_th->doff = ( ndt->pkg_len - ndt->iphdr_len ) / 4;                                 // tcp head length
        pkg_th->ack  = 1;                                   // ack标识位
        pkg_th->syn  = 1;                                 // syn标记位1
        pkg_th->seq = th->ack_seq;                             // tcp seq number
        pkg_th->ack_seq = htonl( ntohl( th->seq ) + 1 );       // tcp ack number
        memcpy( pkg_th + 1, th + 1, ndt->tcp_info->hdr_len - 20  );  //syn 回应需要带上option
    } else if( ndt->type == TCP_PAYLOAD_ACK ){
        debug( " TCP_PAYLOAD_ACK " );
        pkg_th->doff = 0x5;
        pkg_th->ack  = 1;                                   // ack标识位
        pkg_th->seq = th->ack_seq;                             // tcp seq number

        debug( "before-->pkg_th->seq=%u", ntohl (pkg_th->seq  ));
        debug( "before-->th->seq=%u", ntohl (th->seq  ));
        pkg_th->ack_seq = htonl( ntohl( th->seq ) + playload_len );       // tcp ack number
    } else if(  ndt->type == TCP_FIN_ACK ||  ndt->type == TCP_SEND_FIN_ACK  ){
        debug( " TCP_FIN_ACK " );
        pkg_th->doff = 0x5;
        pkg_th->ack  = 1;                                   // ack标识位
        if( ndt->type == TCP_SEND_FIN_ACK )
            pkg_th->fin  = 1;
        pkg_th->seq = th->ack_seq;                             // tcp seq number

        debug( "before-->pkg_th->seq=%u", ntohl (pkg_th->seq  ));
        debug( "before-->th->seq=%u", ntohl (th->seq  ));
        pkg_th->ack_seq = htonl( ntohl( th->seq ) + 1 );       // tcp ack number

    } else if( ndt->type == TCP_SEND_PAYLOAD ){
        pkg_th->doff = 0x5;
        pkg_th->ack  = 1;
        debug( "TCP_SEND_PAYLOAD seq=%u", ntohl( conn->odt->seq ));
        pkg_th->seq = htonl( ntohl(conn->odt->seq) + conn->odt->payload_len)  ;                             // tcp seq number
        debug( "TCP_SEND_PAYLOAD seq=%u", ntohl( conn->odt->seq ));
        debug( "TCP_SEND_PAYLOAD conn->odt->payload_len=%d", conn->odt->payload_len);
        pkg_th->ack_seq = htonl( ntohl( th->seq ) + playload_len );       // tcp ack number
        memcpy( pkg_th + 1, ndt->tcp_info->payload, ndt->tcp_info->payload_len  ); //发送payload
    } else if( ndt->type == TCP_SEND_PAYLOAD_END ){
        pkg_th->doff = 0x5;
        pkg_th->ack  = 1;
        pkg_th->psh  = 1;
        pkg_th->seq = htonl( ntohl(conn->odt->seq) + conn->odt->payload_len)  ;                             // tcp seq number
        debug( "TCP_SEND_PAYLOAD_END seq=%u", ntohl( conn->odt->seq ));
        debug( "TCP_SEND_PAYLOAD_END conn->odt->payload_len=%d", conn->odt->payload_len);
        pkg_th->ack_seq = htonl( ntohl( th->seq ) + playload_len );       // tcp ack number
        memcpy( pkg_th + 1, ndt->tcp_info->payload, ndt->tcp_info->payload_len  ); //发送payload
    }
    debug( "------before-----" );

    debug( "------after-----" );
    debug( "after-->pkg_th->doff=%d",  pkg_th->doff);
    debug( "after-->pkg_th->seq=%u",  htonl( pkg_th->seq )); 
    debug( "after-->pkg_th->seq=%u",  ntohl( pkg_th->seq ));  //这个是对的
  //  debug( "after-->pkg_th->seq=%u",   pkg_th->seq );  //不是真实值
    debug( "after-->pkg_th->ack_seq=%u",  htonl( pkg_th->ack_seq ));
    debug( "after-->pkg_th->ack_seq=%u",  ntohl( pkg_th->ack_seq ));
    debug( "------after-----" );
    pkg_th->window         = htons( -1 );
    pkg_th->check          = 0;
    pkg_th->urg_ptr        = 0;                                   // 紧急指针为NULL;


    conn->odt->payload_len = ndt->tcp_info->payload_len;
    conn->odt->seq =  pkg_th->seq ;
    conn->odt->ack_seq =  pkg_th->ack_seq ;
    debug( "------final-----" );
    debug( "conn->odt->seq=%u", ntohl( conn->odt->seq ));
    debug( "conn->odt->ack_seq=%u", ntohl( conn->odt->ack_seq ));
    debug( "------final-----" );

   // if( th->fin ) pkg_th->fin = 1;
    pkg_th->check          = chksum( ndt->pkg, ndt->pkg_len , TCP_CHK );
}


int handle_tcp( nt_conn_t *conn, int tunfd )
{
    char pkg[TCP_PKG_LEN] = {0};
    create_pkg( conn );
    print_pkg( pkg ) ;
    write( tunfd, conn->ndt->pkg, conn->ndt->pkg_len );
    return 1;
}

int direct_to_server( nt_conn_t *conn ){

    int rfd = socket( AF_INET, SOCK_STREAM, 0 );
    struct sockaddr_in remote ;
    remote.sin_family = AF_INET;
    
    
    remote.sin_addr.s_addr =  conn->pcb->ipv4_dst.ip;

    debug( "sip=%d.%d.%d.%d",  IP4_STR( conn->pcb->ipv4_src.ip ) );
    debug( "dip=%d.%d.%d.%d",  IP4_STR( conn->pcb->ipv4_dst.ip ) );
    debug( "port=%d",  ntohs( conn->pcb->ipv4_dst.port ));
    debug( "port=%d",  ntohs( conn->pcb->ipv4_src.port ));
    //remote.sin_port = htons( conn->pcb->ipv4_dst.port);
    struct tcphdr *th = ( struct tcphdr * )(conn->ndt->skb + conn->ndt->iphdr_len);
    remote.sin_port = th->dest;
    int ret = connect( rfd, ( struct sockaddr * )&remote, sizeof( remote ) );

    if( ret < 0 ) {
        debug( "connect error\n" );
        return 0;
    }

    char *payload = conn->ndt->tcp_info->payload ;
    int payload_len = conn->ndt->tcp_info->payload_len ;

    ret = send( rfd, payload, payload_len, 0 );

    if( ret < 0 ) {
        debug( "ret=%d", ret );
        return -1;
    }

    //char buf[1452] = {0};
    char buf[568] = {0};
    struct sockaddr_in in;
    int len = 0;

    int have_send_ack = 0;
    in.sin_family = AF_INET;

    conn->odt->payload_len = 0;

    do {
        ret = recv( rfd, buf, sizeof( buf ), 0 );
        debug( "ret=%d, buf=%s", ret, buf );

        //send playload
        if( 0 >= ret ){
            if ((0 > ret) && (EAGAIN == errno))
                return 0;
            else
                return -1;
        } else {
            conn->ndt->tcp_info->payload = buf ;
            conn->ndt->tcp_info->payload_len = ret;

            if( ret != sizeof( buf ) ){
                conn->ndt->type = TCP_SEND_PAYLOAD_END;
            }
            else
                conn->ndt->type = TCP_SEND_PAYLOAD;

            create_pkg( conn );
            write( tun, conn->ndt->pkg, conn->ndt->pkg_len );

            if( ret != sizeof( buf  ) )
                break ;
        }


    } while( ret > 0 );

    close( rfd );    
    debug("+++++++++++++++end+++++++++++++");
}


int tcp_input(nt_conn_t *conn){
    debug( "packet is tcp" );

    nt_skb_t *ndt = conn->ndt;
    struct iphdr *ih = ( struct iphdr * )ndt->skb;

    int tcp_len = ndt->skb_len - ndt->iphdr_len;

    //int rfd = socket(AF_INET, SOCK_STREAM,0);
    //int rfd = socket(AF_INET, SOCK_RAW,IPPROTO_TCP);

    int ret = 0;
    struct tcphdr *th = ( struct tcphdr * )( ih + 1 );
   // ndt->tcp_info->th = th;

    if( ndt->protocol == 4 ){
        conn->pcb->ipv4_src.port = th->source ; 
        conn->pcb->ipv4_dst.port = th->dest ; 
    } else {
        return 0;
    }

	/* [2020-11-10 14:30:25]  tcp_input  port=1919
	   [2020-11-10 14:30:25]  tcp_input  port=1919
	   [2020-11-10 14:30:25]  tcp_input  port=47392
	   [2020-11-10 14:30:25]  tcp_input  port=80 */
    debug( "port=%d",  ntohs( conn->pcb->ipv4_dst.port ));
    debug( "port=%d",  ntohs( conn->pcb->ipv4_src.port ));
    debug( "port=%d",  ntohs( th->source ));
    debug( "port=%d",  ntohs( th->dest ));

    ndt->tcp_info->hdr_len = th->doff << 2;
    ndt->tcp_info->payload_len = tcp_len - ndt->tcp_info->hdr_len;

    debug( "packet len=%d, playload_len=%d", tcp_len, ndt->tcp_info->payload_len );

    //        debug( "th->doff=%d", th->doff );
    // debug( "packet is size=%d", sizeof( struct iphdr  ) + sizeof( struct tcphdr  ));

    debug( "flag syn=%d", th->syn );
    debug( "flag ack=%d", th->ack );
    debug( "flag psh=%d", th->psh );
    debug( "flag fin=%d", th->fin );
    //debug( "flag size=%d", th->doff);

    if( ndt->tcp_info->payload_len == 0 ) {
        //syn包
        //   if(th->th_flags == TH_SYN)
        if( th->syn == 1 && th->ack == 0 ) {
            debug( " --> syn pkg" );
            ndt->type = TCP_SYN_ACK;
            tcp_parse_option( ndt->skb );
            handle_tcp( conn, tun );

            //发送认证包
        }

        //syn ack
        if( th->ack == 1 && th->syn == 1 ) {}

        if( th->seq == htonl( 1 ) && th->ack == 1 ) {
            debug( " --> third pkg, connect" );
        }



        //ack 第二包的ack或 FIN的ack
        if( th->ack == 1 && th->syn == 0 && th->fin == 0 ) {
            //drop
            debug( " --> ack pkg" );
        }

        //收到FIN包，先发ack。再发FIN
        if( th->fin == 1 ) {
            //close
            debug( " --> fin pkg" );
            ndt->type = TCP_FIN_ACK;
            create_pkg( conn );
            write( tun, conn->ndt->pkg, conn->ndt->pkg_len );

            ndt->type = TCP_SEND_FIN_ACK;
            create_pkg( conn );
            write( tun, conn->ndt->pkg, conn->ndt->pkg_len );

        }

    } else {
        debug( " --> psh pkg" );

        if( th->psh == 1 ) {
            //回应客户端 的payload ack
            ndt->type = TCP_PAYLOAD_ACK;
            create_pkg( conn );
            write( tun, conn->ndt->pkg, conn->ndt->pkg_len );
        }

        debug( "skb_len = %d", ndt->skb_len );
        debug( "iphdr_len = %d", ndt->iphdr_len );
        debug( "iphdr_len = %d", ndt->iphdr_len );
        debug( "payload_len = %d", ndt->tcp_info->payload_len );
        unsigned char *playload = ndt->skb + ndt->iphdr_len + ndt->tcp_info->hdr_len;
        debug( "buf = %s", playload );
        ndt->tcp_info->payload = playload;

        direct_to_server( conn );
        //发送payload
    }

}


