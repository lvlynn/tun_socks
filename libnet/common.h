#ifndef _COMMON_H_
#define _COMMON_H_

#define IP4_8(var,n) (var >> n) & 0x000000ff
#define IP4_STR(var) IP4_8(var,0),IP4_8(var,8),IP4_8(var,16),IP4_8(var,24)

typedef struct tcp_pcb_s nt_tpcb_t; 
typedef struct tcp_info_s nt_tcp_info_t; 
typedef struct nt_skb_s nt_skb_t;
typedef struct nt_last_s nt_last_t;
typedef struct connection_s nt_conn_t;

#endif
