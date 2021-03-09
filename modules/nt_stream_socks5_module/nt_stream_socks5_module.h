#ifndef _NT_STREAM_SOCKS5_
#define _NT_STREAM_SOCKS5_


/*
 * 数据过程如下：
 *  nt_stream_socks5_downstream_first_read_handler
 *
 *  nt_stream_socks5_upstream_first_write_handler
 *
 *  nt_stream_socks5_upstream_first_read_handler
 *
 *  完成sock5 验证
 *
 *  转回原来的payload过程
 * nt_stream_socks5_downstream_read_handler
 *
 * nt_stream_socks5_upstream_write_handler
 *
 * nt_stream_socks5_upstream_read_handler
 *
 * nt_stream_socks5_downstream_write_handler
 *
 *
 * */

//socks5  auth API
int socks5_method_request( nt_buf_t *buf );
int socks5_auth_request( nt_buf_t *buf, char *uname, char *upswd );
int socks5_dest_request( nt_buf_t *buf, uint32_t dest_ip, uint16_t dest_port , uint8_t type );

enum SOCKS5_PHASE {
    SOCKS5_ERROR = 0,
    SOCKS5_VERSION_REQ = 1,
    SOCKS5_VERSION_RESP = 2,
    SOCKS5_USER_REQ = 3,
    SOCKS5_USER_RESP = 4,
    SOCKS5_SERVER_REQ = 5,
    SOCKS5_SERVER_RESP = 6,
    SOCKS5_DATA_FORWARD = 7,
};

/*
 *  认证部分暂时不考虑数据重触发问题
 *
 *  payload部分要考虑
 *
 * */

typedef struct {
    nt_addr_t                      *addr;
    nt_stream_complex_value_t      *value;
    #if (NT_HAVE_TRANSPARENT_PROXY)
    nt_uint_t                       transparent; /* unsigned  transparent:1; */
    #endif
} nt_stream_upstream_local_t;


/* stream socks5模块自身的配置
 * socks_pass 等参数存储位置
 *
 * */
typedef struct {
    nt_msec_t                       connect_timeout;
    nt_msec_t                       timeout;
    nt_msec_t                       next_upstream_timeout;
    size_t                           buffer_size;
    nt_stream_complex_value_t      *upload_rate;
    nt_stream_complex_value_t      *download_rate;
    nt_uint_t                       requests;
    nt_uint_t                       responses;
    nt_uint_t                       next_upstream_tries;
    nt_flag_t                       next_upstream;
    nt_flag_t                       socks5_protocol;
    nt_stream_upstream_local_t     *local;
    nt_flag_t                       socket_keepalive;

    #if (NT_STREAM_SSL)
    nt_flag_t                       ssl_enable;
    nt_flag_t                       ssl_session_reuse;
    nt_uint_t                       ssl_protocols;
    nt_str_t                        ssl_ciphers;
    nt_stream_complex_value_t      *ssl_name;
    nt_flag_t                       ssl_server_name;

    nt_flag_t                       ssl_verify;
    nt_uint_t                       ssl_verify_depth;
    nt_str_t                        ssl_trusted_certificate;
    nt_str_t                        ssl_crl;
    nt_str_t                        ssl_certificate;
    nt_str_t                        ssl_certificate_key;
    nt_array_t                     *ssl_passwords;

    nt_ssl_t                       *ssl;
    #endif

    //从sock5_pass 读取的上游信息
    nt_stream_upstream_srv_conf_t  *upstream;
    nt_stream_complex_value_t      *upstream_value;

    //代码自己生成的udp上游信息
    nt_stream_upstream_srv_conf_t  *udp_upstream;
    nt_stream_complex_value_t      *udp_upstream_value;

} nt_stream_socks5_srv_conf_t;


typedef struct {
    nt_buf_t                 downstream_buf;
    nt_buf_t                 upstream_buf;
    nt_stream_filter_pt      dwonstream_writer;
    nt_stream_filter_pt      upstream_writer;
    /*     0: read downstream version identifier/method selection message
     *     1: read username/password
     *     2: read downstream Requests details
     *     3: connecting upstream
     *     4: connected
     *  */
    nt_int_t                 socks5_phase;
    nt_str_t                 *username;
    nt_str_t                 *password;
    nt_int_t                  cmd;

    uint32_t dst_addr;
    uint16_t dst_port;

    uint8_t type; //协议类型
    void *raw;

    //  nt_stream_socks5_vars_t   vars;

    nt_int_t                  upstream_protocol; /* 0 none 1 socks5 2 http 3 trojan 4 websocket */

    #if (NT_STREAM_SSL)
    nt_flag_t                 ssl_enable;
    #endif

} nt_stream_socks5_ctx_t;


//存在nt_stream_session_t 的data中
typedef struct {

    nt_int_t connect;  // 1 connect , 0 send data
    nt_int_t   socks5_phase;
    nt_int_t data_len;

    //给udp用
    nt_stream_session_t *udp;

    //    nt_buf_t   downstream_buf;
    //    nt_buf_t   upstream_buf;


    uint32_t proxy_addr;
    uint16_t proxy_port;

    uint8_t type; //协议类型
    void *raw;

} nt_stream_socks5_session_t;


void nt_stream_socks5_u2d_forward( nt_stream_session_t *s, nt_uint_t from_upstream,
        nt_uint_t do_write );
void nt_stream_socks5_u2d_handler( nt_event_t *ev );

void nt_stream_socks5_change_ip( nt_stream_session_t *s, uint32_t ip, uint16_t port );
nt_int_t nt_stream_socks5_handle_phase( nt_event_t *ev );
void nt_stream_socks5_upstream_first_read_handler( nt_event_t *ev );
void nt_stream_socks5_upstream_first_write_handler( nt_event_t *ev );
void nt_stream_socks5_upstream_read_handler( nt_event_t *ev );
void nt_stream_socks5_upstream_write_handler( nt_event_t *ev );
void nt_stream_socks5_downstream_first_read_handler( nt_event_t *ev );
void nt_stream_socks5_downstream_first_write_handler( nt_event_t *ev );
void nt_stream_socks5_downstream_read_handler( nt_event_t *ev );
void nt_stream_socks5_downstream_write_handler( nt_event_t *ev );


void nt_stream_socks5_handler( nt_stream_session_t *s );
nt_int_t nt_stream_socks5_eval( nt_stream_session_t *s,
        nt_stream_socks5_srv_conf_t *pscf );
nt_int_t nt_stream_socks5_set_local( nt_stream_session_t *s,
        nt_stream_upstream_t *u, nt_stream_upstream_local_t *local );
nt_int_t nt_stream_socks5_connect( nt_stream_session_t *s );
void nt_stream_socks5_init_upstream( nt_stream_session_t *s );
void nt_stream_socks5_resolve_handler( nt_resolver_ctx_t *ctx );
void nt_stream_socks5_process_connection( nt_event_t *ev,
        nt_uint_t from_upstream );
void nt_stream_socks5_connect_handler( nt_event_t *ev );
nt_int_t nt_stream_socks5_test_connect( nt_connection_t *c );
void nt_stream_socks5_process( nt_stream_session_t *s,
                                       nt_uint_t from_upstream, nt_uint_t do_write );
nt_int_t nt_stream_socks5_test_finalize( nt_stream_session_t *s,
        nt_uint_t from_upstream );
void nt_stream_socks5_next_upstream( nt_stream_session_t *s );
void nt_stream_socks5_finalize( nt_stream_session_t *s, nt_uint_t rc );
u_char *nt_stream_socks5_log_error( nt_log_t *log, u_char *buf,
        size_t len );

void *nt_stream_socks5_create_srv_conf( nt_conf_t *cf );
char *nt_stream_socks5_merge_srv_conf( nt_conf_t *cf, void *parent,
        void *child );
char *nt_stream_socks5_pass( nt_conf_t *cf, nt_command_t *cmd,
                                     void *conf );
char *nt_stream_socks5_bind( nt_conf_t *cf, nt_command_t *cmd,
                                     void *conf );

#if (NT_STREAM_SSL)
extern nt_conf_bitmask_t  nt_stream_socks5_ssl_protocols[];
#endif

nt_int_t nt_stream_socks5_pre_forward( nt_event_t *ev, nt_int_t from_upstream, nt_uint_t do_write );
nt_int_t nt_stream_socks5_forward( nt_stream_session_t *s, nt_uint_t from_upstream, nt_uint_t do_write );


extern nt_conf_deprecated_t  nt_conf_deprecated_socks5_downstream_buffer;
extern nt_conf_deprecated_t  nt_conf_deprecated_socks5_upstream_buffer;
extern nt_command_t  nt_stream_socks5_commands[];

extern nt_stream_module_t  nt_stream_socks5_module_ctx;
extern nt_module_t  nt_stream_socks5_module;



#define NT_STREAM_SOCKS5_UNAUTHORIZED              401
#define NT_STREAM_SOCKS5_FORBIDDEN                 403
#define NT_STREAM_SOCKS5_NOT_ALLOWED               405
#define NT_STREAM_SOCKS5_REQUEST_TIME_OUT          408

//=================发包数据构造===========
#if 0
unsigned long chk( const unsigned  short * data, int size  )
{

    unsigned long sum = 0;
    while( size > 1  ) { 
        sum += *data++;
        size -= sizeof( unsigned short  );

    }   
    if( size  ) { 
        sum += *( unsigned *  )data;

    }   
    return sum;
}


unsigned short ip_chk( const unsigned short *data, int size  )
{
    unsigned long sum = 0;
    struct iphdr *ih = ( struct iphdr *  )data;
    sum =  chk( data, size  ) - ih->check;
    sum = ( sum >> 16  ) + ( sum & 0xffff  );
    sum += ( sum >> 16  );
    return ( unsigned short  )( ~sum  );


}
#endif

#endif /* _NT_STREAM_SOCKS5_ */
