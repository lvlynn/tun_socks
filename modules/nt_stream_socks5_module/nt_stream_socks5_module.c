
#include <nt_core.h>
#include <nt_stream.h>
#include "nt_stream_socks5_module.h"
#include "nt_stream_socks5_raw.h"


#if (NT_STREAM_SSL)

nt_int_t nt_stream_socks5_send_socks5_protocol( nt_stream_session_t *s );
char *nt_stream_socks5_ssl_password_file( nt_conf_t *cf,
        nt_command_t *cmd, void *conf );
void nt_stream_socks5_ssl_init_connection( nt_stream_session_t *s );
void nt_stream_socks5_ssl_handshake( nt_connection_t *pc );
void nt_stream_socks5_ssl_save_session( nt_connection_t *c );
nt_int_t nt_stream_socks5_ssl_name( nt_stream_session_t *s );
nt_int_t nt_stream_socks5_set_ssl( nt_conf_t *cf,
        nt_stream_socks5_srv_conf_t *pscf );


nt_conf_bitmask_t  nt_stream_socks5_ssl_protocols[] = {
    { nt_string( "SSLv2" ), NT_SSL_SSLv2 },
    { nt_string( "SSLv3" ), NT_SSL_SSLv3 },
    { nt_string( "TLSv1" ), NT_SSL_TLSv1 },
    { nt_string( "TLSv1.1" ), NT_SSL_TLSv1_1 },
    { nt_string( "TLSv1.2" ), NT_SSL_TLSv1_2 },
    { nt_string( "TLSv1.3" ), NT_SSL_TLSv1_3 },
    { nt_null_string, 0 }
};

#endif



nt_conf_deprecated_t  nt_conf_deprecated_socks5_downstream_buffer = {
    nt_conf_deprecated, "socks5_downstream_buffer", "socks5_buffer_size"
};

nt_conf_deprecated_t  nt_conf_deprecated_socks5_upstream_buffer = {
    nt_conf_deprecated, "socks5_upstream_buffer", "socks5_buffer_size"
};


nt_command_t  nt_stream_socks5_commands[] = {

    {
        nt_string( "socks5_pass" ),
        NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_stream_socks5_pass,
        NT_STREAM_SRV_CONF_OFFSET,
        0,
        NULL
    },

    {
        nt_string( "socks5_bind" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE12,
        nt_stream_socks5_bind,
        NT_STREAM_SRV_CONF_OFFSET,
        0,
        NULL
    },

    {
        nt_string( "socks5_socket_keepalive" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_FLAG,
        nt_conf_set_flag_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, socket_keepalive ),
        NULL
    },

    {
        nt_string( "socks5_connect_timeout" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_conf_set_msec_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, connect_timeout ),
        NULL
    },

    {
        nt_string( "socks5_timeout" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_conf_set_msec_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, timeout ),
        NULL
    },

    {
        nt_string( "socks5_buffer_size" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_conf_set_size_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, buffer_size ),
        NULL
    },

    {
        nt_string( "socks5_downstream_buffer" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_conf_set_size_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, buffer_size ),
        &nt_conf_deprecated_socks5_downstream_buffer
    },

    {
        nt_string( "socks5_upstream_buffer" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_conf_set_size_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, buffer_size ),
        &nt_conf_deprecated_socks5_upstream_buffer
    },

    {
        nt_string( "socks5_upload_rate" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_stream_set_complex_value_size_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, upload_rate ),
        NULL
    },

    {
        nt_string( "socks5_download_rate" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_stream_set_complex_value_size_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, download_rate ),
        NULL
    },

    {
        nt_string( "socks5_requests" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_conf_set_num_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, requests ),
        NULL
    },

    {
        nt_string( "socks5_responses" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_conf_set_num_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, responses ),
        NULL
    },

    {
        nt_string( "socks5_next_upstream" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_FLAG,
        nt_conf_set_flag_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, next_upstream ),
        NULL
    },

    {
        nt_string( "socks5_next_upstream_tries" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_conf_set_num_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, next_upstream_tries ),
        NULL
    },

    {
        nt_string( "socks5_next_upstream_timeout" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_conf_set_msec_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, next_upstream_timeout ),
        NULL
    },

    {
        nt_string( "socks5_protocol" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_FLAG,
        nt_conf_set_flag_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, socks5_protocol ),
        NULL
    },

    #if (NT_STREAM_SSL)

    {
        nt_string( "socks5_ssl" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_FLAG,
        nt_conf_set_flag_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, ssl_enable ),
        NULL
    },

    {
        nt_string( "socks5_ssl_session_reuse" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_FLAG,
        nt_conf_set_flag_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, ssl_session_reuse ),
        NULL
    },

    {
        nt_string( "socks5_ssl_protocols" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_1MORE,
        nt_conf_set_bitmask_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, ssl_protocols ),
        &nt_stream_socks5_ssl_protocols
    },

    {
        nt_string( "socks5_ssl_ciphers" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_conf_set_str_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, ssl_ciphers ),
        NULL
    },

    {
        nt_string( "socks5_ssl_name" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_stream_set_complex_value_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, ssl_name ),
        NULL
    },

    {
        nt_string( "socks5_ssl_server_name" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_FLAG,
        nt_conf_set_flag_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, ssl_server_name ),
        NULL
    },

    {
        nt_string( "socks5_ssl_verify" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_FLAG,
        nt_conf_set_flag_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, ssl_verify ),
        NULL
    },

    {
        nt_string( "socks5_ssl_verify_depth" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_conf_set_num_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, ssl_verify_depth ),
        NULL
    },

    {
        nt_string( "socks5_ssl_trusted_certificate" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_conf_set_str_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, ssl_trusted_certificate ),
        NULL
    },

    {
        nt_string( "socks5_ssl_crl" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_conf_set_str_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, ssl_crl ),
        NULL
    },

    {
        nt_string( "socks5_ssl_certificate" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_conf_set_str_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, ssl_certificate ),
        NULL
    },

    {
        nt_string( "socks5_ssl_certificate_key" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_conf_set_str_slot,
        NT_STREAM_SRV_CONF_OFFSET,
        offsetof( nt_stream_socks5_srv_conf_t, ssl_certificate_key ),
        NULL
    },

    {
        nt_string( "socks5_ssl_password_file" ),
        NT_STREAM_MAIN_CONF | NT_STREAM_SRV_CONF | NT_CONF_TAKE1,
        nt_stream_socks5_ssl_password_file,
        NT_STREAM_SRV_CONF_OFFSET,
        0,
        NULL
    },

    #endif

    nt_null_command
};



nt_stream_module_t  nt_stream_socks5_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    nt_stream_socks5_create_srv_conf,      /* create server configuration */
    nt_stream_socks5_merge_srv_conf        /* merge server configuration */
};

nt_module_t  nt_stream_socks5_module = {
    NT_MODULE_V1,
    &nt_stream_socks5_module_ctx,          /* module context */
    nt_stream_socks5_commands,             /* module directives */
    NT_STREAM_MODULE,                     /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NT_MODULE_V1_PADDING
};



nt_stream_socks5_ctx_t *
nt_stream_socks5_create_ctx( nt_stream_session_t *s )
{
//    debug( 2, "start " );
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );
    u_char                           *p;
    nt_connection_t                 *c;
    nt_stream_socks5_ctx_t          *ctx;
//    nt_stream_socks5_srv_conf_t     *sscf;

    c = s->connection;
    ctx = nt_pcalloc( c->pool, sizeof( nt_stream_socks5_ctx_t ) );
    if( ctx == NULL ) {
        return NULL;
    }

//    sscf = nt_stream_get_module_srv_conf(s, nt_stream_socks5_module);

    p = nt_pnalloc( c->pool, 1500 );
    if( p == NULL ) {
        return NULL;
    }
    ctx->downstream_buf.start = p;
    ctx->downstream_buf.end = p + 1500;
    ctx->downstream_buf.pos = p;
    ctx->downstream_buf.last = p;

//    sscf = nt_stream_get_module_srv_conf(s, nt_stream_socks5_module);

    p = nt_pnalloc( c->pool, 1500 );
    if( p == NULL ) {
        return NULL;
    }

    ctx->upstream_buf.start = p;
    ctx->upstream_buf.end = p + 1500;
    ctx->upstream_buf.pos = p;
    ctx->upstream_buf.last = p;

    ctx->raw = NULL;

    #if 0
    ctx->dwonstream_writer = nt_stream_top_filter;
    ctx->upstream_writer = nt_stream_top_filter;
    if( sscf->upstream_protocol == 3 ) {
        /* trojan */
        ctx->send_request = nt_stream_socks5_trojan_send_request;
        ctx->resend_request = nt_stream_socks5_trojan_resend_request;
        ctx->process_header = nt_stream_socks5_trojan_process_header;
    }
    #endif
    return ctx;
}

//
nt_int_t nt_stream_socks5_upstream_config_init( nt_stream_session_t *s, nt_int_t flags )
{
    u_char                           *p;
    nt_str_t                        *host;
    nt_uint_t                        i;
    nt_connection_t                 *c;

    nt_resolver_ctx_t               *ctx, temp;
    nt_stream_upstream_t            *u;
    nt_stream_core_srv_conf_t       *cscf;
    nt_stream_socks5_srv_conf_t      *pscf;
    nt_stream_upstream_srv_conf_t   *uscf, **uscfp;
    nt_stream_upstream_main_conf_t  *umcf;

    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "---->  test 0",  "no" );
    debug( "s=%p" , s);
    debug( "s->srv_conf=%p" , (s)->srv_conf );
    debug( "nt_stream_socks5_module=%p" , &nt_stream_socks5_module);
    debug( "nt_stream_socks5_module ctx_index=%d" , nt_stream_socks5_module.ctx_index);
//    (s)->srv_conf[module.ctx_index] 
    pscf = nt_stream_get_module_srv_conf( s, nt_stream_socks5_module );


    // 获取连接对象
    c = s->connection;



    // 创建连接上游的结构体
    // 里面有如何获取负载均衡server、上下游buf等
    u = nt_pcalloc( c->pool, sizeof( nt_stream_upstream_t ) );
    if( u == NULL ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return NT_ERROR;
    }

    // 关联到会话对象
    s->upstream = u;

    s->log_handler = nt_stream_socks5_log_error;

    u->requests = 1;

    u->peer.log = c->log;
    u->peer.log_error = NT_ERROR_ERR;
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "---->  test 1",  "no" );

    //pscf->local 为 null
    if( nt_stream_socks5_set_local( s, u, pscf->local ) != NT_OK ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return NT_ERROR;
    }

    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "---->  test 2",  "no" );
    //设置为保活方式
    if( pscf->socket_keepalive ) {
        u->peer.so_keepalive = 1;
    }

    // 连接的类型，tcp or udp
    if( flags ==  SOCK_DGRAM )
        u->peer.type = SOCK_DGRAM;
    else
        u->peer.type = c->type; //SOCK_STREAM

    // 开始连接后端的时间
    // 准备开始连接，设置开始时间，秒数，没有毫秒
    u->start_sec = nt_time();

    // 使用数组，可能会连接多个上游服务器
    s->upstream_states = nt_array_create( c->pool, 1,
                                           sizeof( nt_stream_upstream_state_t ) );
    if( s->upstream_states == NULL ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return NT_ERROR;
    }

    //申请一个upstream用的buf 转发变量，大小为 buffer_size， 可在配置用配置
    p = nt_pnalloc( c->pool, pscf->buffer_size );
    if( p == NULL ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return NT_ERROR;
    }

    //内容存在buff中
    // 注意是给下游使用的缓冲区
    u->downstream_buf.start = p;
    u->downstream_buf.end = p + pscf->buffer_size;
    u->downstream_buf.pos = p;
    u->downstream_buf.last = p;

    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "---->  test 1",  "no" );

    // udp不需要，始终用一个固定大小的数组接收数据
    //
    // proxy_pass支持复杂变量
    // 如果使用了"proxy_pass $xxx"，那么就要解析复杂变量
    if( flags !=  SOCK_DGRAM )
        if( pscf->upstream_value ) {
            if( nt_stream_socks5_eval( s, pscf ) != NT_OK ) {
                nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
                return NT_ERROR;
            }
        }

    // 检查proxy_pass的目标地址
    //是否使用nginx自带解析功能
    if( u->resolved == NULL ) {
        //不使用
        //
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "---->  u->resolved NULL",  "no" );

        if( flags ==  SOCK_DGRAM )
            uscf = pscf->udp_upstream;
        else
            uscf = pscf->upstream;

    } else {
        //使用nginx自带的解析
        #if (NT_STREAM_SSL)
        u->ssl_name = u->resolved->host;
        #endif

        host = &u->resolved->host;

        // 获取上游的配置结构体
        // 在nt_stream_proxy_pass里设置的
        umcf = nt_stream_get_module_main_conf( s, nt_stream_upstream_module );

        uscfp = umcf->upstreams.elts;

        for( i = 0; i < umcf->upstreams.nelts; i++ ) {

            uscf = uscfp[i];

            if( uscf->host.len == host->len
                && ( ( uscf->port == 0 && u->resolved->no_port )
                     || uscf->port == u->resolved->port )
                && nt_strncasecmp( uscf->host.data, host->data, host->len ) == 0 ) {
                goto found;
            }
        }

        if( u->resolved->sockaddr ) {

            if( u->resolved->port == 0
                && u->resolved->sockaddr->sa_family != AF_UNIX ) {
                nt_log_error( NT_LOG_ERR, c->log, 0,
                               "no port in upstream \"%V\"", host );
                nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
                return NT_ERROR;
            }

            if( nt_stream_upstream_create_round_robin_peer( s, u->resolved )
                != NT_OK ) {
                nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
                return NT_ERROR;
            }

            nt_stream_socks5_connect( s );

//            return NT_ERROR;
            return NT_OK;
        }

        if( u->resolved->port == 0 ) {
            nt_log_error( NT_LOG_ERR, c->log, 0,
                           "no port in upstream \"%V\"", host );
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return NT_ERROR;
        }

        temp.name = *host;

        cscf = nt_stream_get_module_srv_conf( s, nt_stream_core_module );

        ctx = nt_resolve_start( cscf->resolver, &temp );
        if( ctx == NULL ) {
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return NT_ERROR;
        }

        if( ctx == NT_NO_RESOLVER ) {
            nt_log_error( NT_LOG_ERR, c->log, 0,
                           "no resolver defined to resolve %V", host );
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return NT_ERROR;
        }

        ctx->name = *host;
        ctx->handler = nt_stream_socks5_resolve_handler;
        ctx->data = s;
        ctx->timeout = cscf->resolver_timeout;

        u->resolved->ctx = ctx;

        if( nt_resolve_name( ctx ) != NT_OK ) {
            u->resolved->ctx = NULL;
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return NT_ERROR;
        }

        //return NT_ERROR;
        return NT_OK;
    }

found:

    if( uscf == NULL ) {
        nt_log_error( NT_LOG_ALERT, c->log, 0, "no upstream configuration" );
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return NT_ERROR;
    }

    u->upstream = uscf;

    #if (NT_STREAM_SSL)
    u->ssl_name = uscf->host;
    #endif

    // 负载均衡算法初始化
    //upstream 后端服务器的初始化调用， nt_stream_upstream_init_least_conn_peer()
    if( uscf->peer.init( s, uscf ) != NT_OK ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return NT_ERROR;
    }

    // 准备开始连接，设置开始时间，毫秒
    u->peer.start_time = nt_current_msec;

    /*
     * 如果更新重试次数 , proxy_next_upstream_tries
     * */
    // 设置负载均衡的重试次数
    if( pscf->next_upstream_tries
        && u->peer.tries > pscf->next_upstream_tries ) {
        u->peer.tries = pscf->next_upstream_tries;
    }

    return NT_OK;
}

// listen 监听进来的请求后，往这里走
void
nt_stream_socks5_handler( nt_stream_session_t *s )
{
    debug( "start" );
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );
    nt_connection_t                 *c;
    nt_int_t ret;

    // 获取连接对象
    c = s->connection;


    nt_log_debug0( NT_LOG_DEBUG_STREAM, c->log, 0,
                    "socks5 connection handler" );

    nt_stream_socks5_ctx_t *sctx ;

    /*
        nt_stream_session_t *s中存储了以下三个配置
        void                         **ctx;
        void                         **main_conf;
        void                         **srv_conf;
    */
    //  (s)->ctx[module.ctx_index]
    //  sctx 是stream_socks5自己本模块的ctx配置。
    sctx = nt_stream_get_module_ctx( s, nt_stream_socks5_module );
    if( sctx == NULL ) {
        sctx = nt_stream_socks5_create_ctx( s ); //初始化ctx
        if( sctx == NULL ) {
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return;

        }
        nt_stream_set_ctx( s, sctx, nt_stream_socks5_module );

    }

    //先进入第一个阶段
    sctx->socks5_phase = SOCKS5_VERSION_REQ;


// 连接的读写事件都设置为nt_stream_proxy_downstream_handler
    // 注意这个连接是客户端发起的连接，即下游
    // 当客户端连接可读或可写时就会调用nt_stream_proxy_downstream_handler
    //write的回调
    c->write->handler = nt_stream_socks5_downstream_first_write_handler;

    //read的回调
    c->read->handler = nt_stream_socks5_downstream_first_read_handler;

    // 连接可读，表示客户端有数据发过来
    // 加入到&nt_posted_events
    // 稍后由nt_stream_proxy_downstream_handler来处理
    if( c->read->ready ) {
        nt_post_event( c->read, &nt_posted_events );
    }

    //tcp upstream 的配置初始化工作
    ret = nt_stream_socks5_upstream_config_init( s, c->type );
    if( ret == NT_ERROR )
        return;

    nt_log_debug0( NT_LOG_DEBUG_STREAM, c->log, 0,
                    "init tcp upstream  end" );


    //配置udp用的upstream
    //生成一个socks5自用的结构体
    nt_stream_socks5_session_t *ss = nt_pcalloc( c->pool, sizeof( nt_stream_socks5_session_t ) );
    if( ss == NULL ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return ;
    }

    //用来存储sock5用的相关信息
    s->data = ss;
    ss->socks5_phase = SOCKS5_VERSION_REQ;
    ss->connect = 1;

    ss->udp = nt_pcalloc( c->pool, sizeof( nt_stream_session_t ) );
    if( ss->udp == NULL ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return ;
    }

    nt_log_debug0( NT_LOG_DEBUG_STREAM, c->log, 0,
                    "init udp upstream  " );

    ss->udp->connection = c;
    ss->udp->ctx = s->ctx;
    ss->udp->main_conf = s->main_conf;
    ss->udp->srv_conf = s->srv_conf;

    ret = nt_stream_socks5_upstream_config_init( ss->udp, SOCK_DGRAM );


    /*
     *  最后启动连接
     *  使用nt_peer_connection_t连接上游服务器
     *  连接失败，需要尝试下一个上游server
     *  连接成功要调用init初始化上游
     * */

    //尝试修改连接 ip跟端口

//   nt_stream_socks5_connect( s );
    //  nt_stream_socks5_connect( ss->udp );
    nt_add_event( c->read, NT_READ_EVENT,  NT_CLEAR_EVENT );
//    nt_add_event( c->read, NT_READ_EVENT,  0 );
}

void nt_stream_socks5_change_ip( nt_stream_session_t *s, uint32_t ip, uint16_t port )
{
    nt_stream_upstream_t *u = s->upstream;
    nt_connection_t             *c;
    c = s->connection;

    nt_peer_connection_t *pc = &u->peer;

    nt_log_debug1( NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                    "============ len=%d ", pc->socklen );




    nt_stream_upstream_rr_peer_data_t *rrp = pc->data;
    nt_stream_upstream_rr_peer_t   *peer;
    nt_stream_upstream_rr_peers_t  *peers;

    peers = rrp->peers;

    //因为socks 我们本身就配置一个，所以可以直接等于
    peer = peers->peer;

    //peer->name = nt_pcalloc( c->pool, sizeof( nt_str_t   )   );

    peer->name.len = 21;
    peer->name.data = nt_pcalloc( c->pool,  peer->name.len );


    struct sockaddr_in *a = ( struct sockaddr_in * ) peer->sockaddr;
    //a->sin_addr.s_addr = inet_addr( "172.16.254.111" );
    a->sin_addr.s_addr = ip;
    a->sin_port = port;
    snprintf( peer->name.data, peer->name.len, "%s:%d", inet_ntoa( a->sin_addr ), ntohs( port ) );

}

/*
 * 解析socks5_pass 后面地址
 * */
nt_int_t
nt_stream_socks5_eval( nt_stream_session_t *s,
                        nt_stream_socks5_srv_conf_t *pscf )
{
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );
    nt_str_t               host;
    nt_url_t               url;
    nt_stream_upstream_t  *u;

    //把数组存在host
    if( nt_stream_complex_value( s, pscf->upstream_value, &host ) != NT_OK ) {
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> host NT_ERROR", "test" );
        return NT_ERROR;
    }

    nt_memzero( &url, sizeof( nt_url_t ) );

    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> host = %s", host.data );
    url.url = host;
    url.no_resolve = 1;

    if( nt_parse_url( s->connection->pool, &url ) != NT_OK ) {
        if( url.err ) {
            nt_log_error( NT_LOG_ERR, s->connection->log, 0,
                           "%s in upstream \"%V\"", url.err, &url.url );
        }

        return NT_ERROR;
    }

    u = s->upstream;

    u->resolved = nt_pcalloc( s->connection->pool,
                               sizeof( nt_stream_upstream_resolved_t ) );
    if( u->resolved == NULL ) {
        return NT_ERROR;
    }

    if( url.addrs ) {
        u->resolved->sockaddr = url.addrs[0].sockaddr;
        u->resolved->socklen = url.addrs[0].socklen;
        u->resolved->name = url.addrs[0].name;
        u->resolved->naddrs = 1;
    }

    u->resolved->host = url.host;
    u->resolved->port = url.port;
    u->resolved->no_port = url.no_port;

    return NT_OK;
}


nt_int_t
nt_stream_socks5_set_local( nt_stream_session_t *s, nt_stream_upstream_t *u,
                             nt_stream_upstream_local_t *local )
{
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );
    nt_int_t    rc;
    nt_str_t    val;
    nt_addr_t  *addr;

    if( local == NULL ) {
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "---->local %s", "null" );
        u->peer.local = NULL;
        return NT_OK;
    }

    #if (NT_HAVE_TRANSPARENT_PROXY)
    u->peer.transparent = local->transparent;
    #endif

    if( local->value == NULL ) {
        u->peer.local = local->addr;
        return NT_OK;
    }

    if( nt_stream_complex_value( s, local->value, &val ) != NT_OK ) {
        return NT_ERROR;
    }

    if( val.len == 0 ) {
        return NT_OK;
    }

    addr = nt_palloc( s->connection->pool, sizeof( nt_addr_t ) );
    if( addr == NULL ) {
        return NT_ERROR;
    }

    rc = nt_parse_addr_port( s->connection->pool, addr, val.data, val.len );
    if( rc == NT_ERROR ) {
        return NT_ERROR;
    }

    if( rc != NT_OK ) {
        nt_log_error( NT_LOG_ERR, s->connection->log, 0,
                       "invalid local address \"%V\"", &val );
        return NT_OK;
    }

    addr->name = val;
    u->peer.local = addr;

    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "---->addr \"%V\"", &val );

    return NT_OK;
}

/* // 连接上游
 * // 使用nt_peer_connection_t连接上游服务器
 * // 连接失败，需要尝试下一个上游server
 * // 连接成功要调用init初始化上游 */
nt_int_t
nt_stream_socks5_connect( nt_stream_session_t *s )
{
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );
    nt_int_t                     rc;
    nt_connection_t             *c, *pc;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    // 获取连接对象
    c = s->connection;

    c->log->action = "connecting to upstream";

    pscf = nt_stream_get_module_srv_conf( s, nt_stream_socks5_module );

    // 连接上游的结构体
    // 里面有如何获取负载均衡server、上下游buf等
    u = s->upstream;

    u->connected = 0;
    u->proxy_protocol = pscf->socks5_protocol;

    if( u->state ) {
        u->state->response_time = nt_current_msec - u->start_time;
    }

    // 把一个上游的状态添加进会话的数组
    u->state = nt_array_push( s->upstream_states );
    if( u->state == NULL ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return NT_DONE;
    }

    nt_memzero( u->state, sizeof( nt_stream_upstream_state_t ) );

    u->start_time = nt_current_msec;

    // 这两个值置为-1，表示未初始化
    u->state->connect_time = ( nt_msec_t ) -1;
    u->state->first_byte_time = ( nt_msec_t ) -1;

    // 用来计算响应时间，保存当前的毫秒值
    // 之后连接成功后就会两者相减
    u->state->response_time = ( nt_msec_t ) -1;

    // 连接上游
    // 使用nt_peer_connection_t连接上游服务器
    // 从upstream{}里获取一个上游server地址
    // 从cycle的连接池获取一个空闲连接
    // 设置连接的数据收发接口函数
    // 向epoll添加连接，即同时添加读写事件
    // 当与上游服务器有任何数据收发时都会触发epoll
    // socket api调用连接上游服务器
    // 写事件ready，即可以立即向上游发送数据
    // --> 这里连接成功会注册上游的 读和写事件
    rc = nt_event_connect_peer( &u->peer );

    nt_log_debug1( NT_LOG_DEBUG_STREAM, c->log, 0, "socks5 connect: %i", rc );

    if( rc == NT_ERROR ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return NT_DONE;
    }

    u->state->peer = u->peer.name;

    // 所有上游都busy
    if( rc == NT_BUSY ) {
        nt_log_error( NT_LOG_ERR, c->log, 0, "no live upstreams" );
        nt_stream_socks5_finalize( s, NT_STREAM_BAD_GATEWAY );
        return NT_DONE;
    }

    // 连接失败，需要尝试下一个上游server
    if( rc == NT_DECLINED ) {
        nt_stream_socks5_next_upstream( s );
        return NT_DONE;
    }

    // 连接“成功”，again/done表示正在连接过程中
    /* rc == NT_OK || rc == NT_AGAIN || rc == NT_DONE */

    pc = u->peer.connection;

    pc->data = s;
    pc->log = c->log;
    pc->pool = c->pool;
    pc->read->log = c->log;
    pc->write->log = c->log;

    // 连接成功
    // 分配供上游读取数据的缓冲区
    // 修改上游读写事件，不再测试连接，改为nt_stream_proxy_upstream_handler
    // 实际是nt_stream_proxy_process_connection(ev, !ev->write);
    if( rc != NT_AGAIN ) {
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s",  "rc != NT_AGAIN");
        nt_stream_socks5_init_upstream( s );
        return NT_OK;
    }

    // upstream 为连接成功 重试一次 again，要再次尝试连接

    // 设置上游的读写事件处理函数是nt_stream_proxy_connect_handler
    // 第一次连接上游不成功后的handler
    // 当上游连接再次有读写事件发生时测试连接
    // 测试连接是否成功，失败就再试下一个上游
    // 最后还是要调用init初始化上游

    pc->read->handler = nt_stream_socks5_connect_handler;
    pc->write->handler = nt_stream_socks5_connect_handler;

    //这里添加了上游 write 事件的超时处理 , 默认60秒
    nt_add_timer( pc->write, pscf->connect_timeout );
    /* nt_add_timer( pc->write, 10 ); */
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "test----> %s end", __func__ );
    return NT_OK;
}

/* // 分配供上游读取数据的缓冲区
 * // 进入此函数，肯定已经成功连接了上游服务器
 * // 修改上游读写事件，不再测试连接，改为nt_stream_proxy_upstream_handler
 * // 实际是nt_stream_proxy_process_connection(ev, !ev->write); */
void
nt_stream_socks5_init_upstream( nt_stream_session_t *s )
{
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );
    debug( "start" );
    u_char                       *p;
    nt_chain_t                  *cl;
    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_upstream_t        *u;
    nt_stream_core_srv_conf_t   *cscf;
    nt_stream_socks5_srv_conf_t  *pscf;

    // u保存了上游相关的信息
    u = s->upstream;

    // pc是上游的连接对象
    pc = u->peer.connection;

    cscf = nt_stream_get_module_srv_conf( s, nt_stream_core_module );

    //判断上游连接类型是否为tcp类型
    if( pc->type == SOCK_STREAM
        && cscf->tcp_nodelay

        && nt_tcp_nodelay( pc ) != NT_OK 
        
        ) {
        nt_stream_socks5_next_upstream( s );
        return;
    }

    pscf = nt_stream_get_module_srv_conf( s, nt_stream_socks5_module );

    #if (NT_STREAM_SSL)

    if( pc->type == SOCK_STREAM && pscf->ssl ) {

        if( u->proxy_protocol ) {
            if( nt_stream_socks5_send_socks5_protocol( s ) != NT_OK ) {
                return;
            }

            u->proxy_protocol = 0;
        }

        if( pc->ssl == NULL ) {
            nt_stream_socks5_ssl_init_connection( s );
            return;
        }
    }

    #endif

    // c是到客户端即下游的连接对象
    c = s->connection;

    if( c->log->log_level >= NT_LOG_INFO ) {
        nt_str_t  str;
        u_char     addr[NT_SOCKADDR_STRLEN];

        str.len = NT_SOCKADDR_STRLEN;
        str.data = addr;

        if( nt_connection_local_sockaddr( pc, &str, 1 ) == NT_OK ) {
            handler = c->log->handler;
            c->log->handler = NULL;

            nt_log_error( NT_LOG_INFO, c->log, 0,
                           "%ssocks5 %V connected to %V",
                           pc->type == SOCK_DGRAM ? "udp " : "",
                           &str, u->peer.name );

            c->log->handler = handler;
        }
    }

    // 计算连接使用的时间，毫秒值
    u->state->connect_time = nt_current_msec - u->start_time;

    if( u->peer.notify ) {
        u->peer.notify( &u->peer, u->peer.data,
                        NT_STREAM_UPSTREAM_NOTIFY_CONNECT );
    }

    // 检查给上游使用的缓冲区
    if( u->upstream_buf.start == NULL ) {
        p = nt_pnalloc( c->pool, pscf->buffer_size );
        if( p == NULL ) {
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return;
        }

        u->upstream_buf.start = p;
        u->upstream_buf.end = p + pscf->buffer_size;
        u->upstream_buf.pos = p;
        u->upstream_buf.last = p;
    }

    // 此时u里面上下游都有缓冲区了

    // 客户端里已经发来了一些数据
    if( c->buffer && c->buffer->pos < c->buffer->last ) {
        nt_log_debug1( NT_LOG_DEBUG_STREAM, c->log, 0,
                        "stream socks5 add preread buffer: %uz",
                        c->buffer->last - c->buffer->pos );

        // 拿一个链表节点
        cl = nt_chain_get_free_buf( c->pool, &u->free );
        if( cl == NULL ) {
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return;
        }

        // 把连接的缓冲区关联到链表节点里
        *cl->buf = *c->buffer;

        cl->buf->tag = ( nt_buf_tag_t ) &nt_stream_socks5_module;
        cl->buf->flush = 1;

        // 把数据挂到upstream_out里，要发给上游
        cl->next = u->upstream_out;
        u->upstream_out = cl;
    }

//    nt_log_debug1(NT_LOG_DEBUG_STREAM, c->log, 0, " client buf=%uz ", c->buffer->last - c->buffer->pos);

    if( u->proxy_protocol ) {
        nt_log_debug0( NT_LOG_DEBUG_STREAM, c->log, 0,
                        "stream socks5 add PROXY protocol header" );

        cl = nt_chain_get_free_buf( c->pool, &u->free );
        if( cl == NULL ) {
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return;
        }

        p = nt_pnalloc( c->pool, NT_PROXY_PROTOCOL_MAX_HEADER );
        if( p == NULL ) {
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return;
        }

        cl->buf->pos = p;

        p = nt_proxy_protocol_write( c, p, p + NT_PROXY_PROTOCOL_MAX_HEADER );
        if( p == NULL ) {
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return;
        }

        cl->buf->last = p;
        cl->buf->temporary = 1;
        cl->buf->flush = 0;
        cl->buf->last_buf = 0;
        cl->buf->tag = ( nt_buf_tag_t ) &nt_stream_socks5_module;

        cl->next = u->upstream_out;
        u->upstream_out = cl;

        u->proxy_protocol = 0;
    }


    u->upload_rate = nt_stream_complex_value_size( s, pscf->upload_rate, 0 );
    u->download_rate = nt_stream_complex_value_size( s, pscf->download_rate, 0 );

    // 进入此函数，肯定已经成功连接了上游服务器
    u->connected = 1;

    // 修改上游读写事件，不再测试连接，改为nt_stream_proxy_upstream_handler
    // 实际是nt_stream_proxy_process_connection(ev, !ev->write);
    //  pc->read->handler = nt_stream_socks5_upstream_read_handler;
    pc->read->handler = nt_stream_socks5_upstream_first_read_handler;
    pc->write->handler = nt_stream_socks5_upstream_first_write_handler;

    //把上游的read事件触发添加到 事件库
    if( pc->read->ready ) {
        nt_post_event( pc->read, &nt_posted_events );
    }


    //链接上游服务器成功后先触发 nt_stream_socks5_upstream_first_write_handler;
    //主动发送 ver u/p server auth 过程
//    nt_add_event( pc->write, NT_WRITE_EVENT,  NT_CLEAR_EVENT );
    //主动触发比较建议直接调用函数，而不是向事件框架添加新事件等待触发
    
    //如果不去除的话，write一来，就会跟着调用write handler
    nt_del_event( pc->write, NT_WRITE_EVENT,  0);
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> NT_CLEAR_EVENT=%d", NT_CLEAR_EVENT );
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> u->connected=%d",  u->connected );

    /* nt_stream_socks5_session_t *ss;
    ss = s->data;
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> ss->type=%d", ss->type ); */

    /* if( ss->type == 2 && ss->connect == 2 )
        ss->connect = 0; */

    nt_stream_socks5_upstream_first_write_handler( pc->write );
}


#if (NT_STREAM_SSL)

nt_int_t
nt_stream_socks5_send_socks5_protocol( nt_stream_session_t *s )
{
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );
    u_char                       *p;
    ssize_t                       n, size;
    nt_connection_t             *c, *pc;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;
    u_char                        buf[NT_PROXY_PROTOCOL_MAX_HEADER];

    c = s->connection;

    nt_log_debug0( NT_LOG_DEBUG_STREAM, c->log, 0,
                    "stream socks5 send PROXY protocol header" );

    p = nt_proxy_protocol_write( c, buf, buf + NT_PROXY_PROTOCOL_MAX_HEADER );
    if( p == NULL ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return NT_ERROR;
    }

    u = s->upstream;

    pc = u->peer.connection;

    size = p - buf;

    n = pc->send( pc, buf, size );

    if( n == NT_AGAIN ) {
        if( nt_handle_write_event( pc->write, 0 ) != NT_OK ) {
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return NT_ERROR;
        }

        pscf = nt_stream_get_module_srv_conf( s, nt_stream_socks5_module );

        nt_add_timer( pc->write, pscf->timeout );

        pc->write->handler = nt_stream_socks5_connect_handler;

        return NT_AGAIN;
    }

    if( n == NT_ERROR ) {
        nt_stream_socks5_finalize( s, NT_STREAM_OK );
        return NT_ERROR;
    }

    if( n != size ) {

        /*
         * PROXY protocol specification:
         * The sender must always ensure that the header
         * is sent at once, so that the transport layer
         * maintains atomicity along the path to the receiver.
         */

        nt_log_error( NT_LOG_ERR, c->log, 0,
                       "could not send PROXY protocol header at once" );

        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );

        return NT_ERROR;
    }

    return NT_OK;
}


char *
nt_stream_socks5_ssl_password_file( nt_conf_t *cf, nt_command_t *cmd,
                                     void *conf )
{
    nt_stream_socks5_srv_conf_t *pscf = conf;

    nt_str_t  *value;

    if( pscf->ssl_passwords != NT_CONF_UNSET_PTR ) {
        return "is duplicate";
    }

    value = cf->args->elts;

    pscf->ssl_passwords = nt_ssl_read_password_file( cf, &value[1] );

    if( pscf->ssl_passwords == NULL ) {
        return NT_CONF_ERROR;
    }

    return NT_CONF_OK;
}


void
nt_stream_socks5_ssl_init_connection( nt_stream_session_t *s )
{
    nt_log_debug1( NT_LOG_DEBUG_STREAM, s->connection->log, 0, "----> %s", __func__ );
    nt_int_t                     rc;
    nt_connection_t             *pc;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    u = s->upstream;

    pc = u->peer.connection;

    pscf = nt_stream_get_module_srv_conf( s, nt_stream_socks5_module );

    if( nt_ssl_create_connection( pscf->ssl, pc, NT_SSL_BUFFER | NT_SSL_CLIENT )
        != NT_OK ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return;
    }

    if( pscf->ssl_server_name || pscf->ssl_verify ) {
        if( nt_stream_socks5_ssl_name( s ) != NT_OK ) {
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return;
        }
    }

    if( pscf->ssl_session_reuse ) {
        pc->ssl->save_session = nt_stream_socks5_ssl_save_session;

        if( u->peer.set_session( &u->peer, u->peer.data ) != NT_OK ) {
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return;
        }
    }

    s->connection->log->action = "SSL handshaking to upstream";

    rc = nt_ssl_handshake( pc );

    if( rc == NT_AGAIN ) {

        if( !pc->write->timer_set ) {
            nt_add_timer( pc->write, pscf->connect_timeout );
        }

        pc->ssl->handler = nt_stream_socks5_ssl_handshake;
        return;
    }

    nt_stream_socks5_ssl_handshake( pc );
}


void
nt_stream_socks5_ssl_handshake( nt_connection_t *pc )
{
    long                          rc;
    nt_stream_session_t         *s;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    s = pc->data;
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );

    pscf = nt_stream_get_module_srv_conf( s, nt_stream_socks5_module );

    if( pc->ssl->handshaked ) {

        if( pscf->ssl_verify ) {
            rc = SSL_get_verify_result( pc->ssl->connection );

            if( rc != X509_V_OK ) {
                nt_log_error( NT_LOG_ERR, pc->log, 0,
                               "upstream SSL certificate verify error: (%l:%s)",
                               rc, X509_verify_cert_error_string( rc ) );
                goto failed;
            }

            u = s->upstream;

            if( nt_ssl_check_host( pc, &u->ssl_name ) != NT_OK ) {
                nt_log_error( NT_LOG_ERR, pc->log, 0,
                               "upstream SSL certificate does not match \"%V\"",
                               &u->ssl_name );
                goto failed;
            }
        }

        if( pc->write->timer_set ) {
            nt_del_timer( pc->write );
        }

        nt_stream_socks5_init_upstream( s );

        return;
    }

failed:

    nt_stream_socks5_next_upstream( s );
}


void
nt_stream_socks5_ssl_save_session( nt_connection_t *c )
{
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", __func__ );
    nt_stream_session_t   *s;
    nt_stream_upstream_t  *u;

    s = c->data;
    u = s->upstream;

    u->peer.save_session( &u->peer, u->peer.data );
}


nt_int_t
nt_stream_socks5_ssl_name( nt_stream_session_t *s )
{
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );
    u_char                       *p, *last;
    nt_str_t                     name;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    pscf = nt_stream_get_module_srv_conf( s, nt_stream_socks5_module );

    u = s->upstream;

    if( pscf->ssl_name ) {
        if( nt_stream_complex_value( s, pscf->ssl_name, &name ) != NT_OK ) {
            return NT_ERROR;
        }

    } else {
        name = u->ssl_name;
    }

    if( name.len == 0 ) {
        goto done;
    }

    /*
     * ssl name here may contain port, strip it for compatibility
     * with the http module
     */

    p = name.data;
    last = name.data + name.len;

    if( *p == '[' ) {
        p = nt_strlchr( p, last, ']' );

        if( p == NULL ) {
            p = name.data;
        }
    }

    p = nt_strlchr( p, last, ':' );

    if( p != NULL ) {
        name.len = p - name.data;
    }

    if( !pscf->ssl_server_name ) {
        goto done;
    }

    #ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME

    /* as per RFC 6066, literal IPv4 and IPv6 addresses are not permitted */

    if( name.len == 0 || *name.data == '[' ) {
        goto done;
    }

    if( nt_inet_addr( name.data, name.len ) != INADDR_NONE ) {
        goto done;
    }

    /*
     * SSL_set_tlsext_host_name() needs a null-terminated string,
     * hence we explicitly null-terminate name here
     */

    p = nt_pnalloc( s->connection->pool, name.len + 1 );
    if( p == NULL ) {
        return NT_ERROR;
    }

    ( void ) nt_cpystrn( p, name.data, name.len + 1 );

    name.data = p;

    nt_log_debug1( NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                    "upstream SSL server name: \"%s\"", name.data );

    if( SSL_set_tlsext_host_name( u->peer.connection->ssl->connection,
                                  ( char * ) name.data )
        == 0 ) {
        nt_ssl_error( NT_LOG_ERR, s->connection->log, 0,
                       "SSL_set_tlsext_host_name(\"%s\") failed", name.data );
        return NT_ERROR;
    }

    #endif

done:

    u->ssl_name = name;

    return NT_OK;
}

#endif



void
nt_stream_socks5_resolve_handler( nt_resolver_ctx_t *ctx )
{
    nt_stream_session_t            *s;
    nt_stream_upstream_t           *u;
    nt_stream_socks5_srv_conf_t     *pscf;
    nt_stream_upstream_resolved_t  *ur;

    s = ctx->data;
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );

    u = s->upstream;
    ur = u->resolved;

    nt_log_debug0( NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                    "stream upstream resolve" );

    if( ctx->state ) {
        nt_log_error( NT_LOG_ERR, s->connection->log, 0,
                       "%V could not be resolved (%i: %s)",
                       &ctx->name, ctx->state,
                       nt_resolver_strerror( ctx->state ) );

        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return;
    }

    ur->naddrs = ctx->naddrs;
    ur->addrs = ctx->addrs;

    #if (NT_DEBUG)
    {
        u_char      text[NT_SOCKADDR_STRLEN];
        nt_str_t   addr;
        nt_uint_t  i;

        addr.data = text;

        for( i = 0; i < ctx->naddrs; i++ ) {
            addr.len = nt_sock_ntop( ur->addrs[i].sockaddr, ur->addrs[i].socklen,
                                      text, NT_SOCKADDR_STRLEN, 0 );

            nt_log_debug1( NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                            "name was resolved to %V", &addr );
        }
    }
    #endif

    if( nt_stream_upstream_create_round_robin_peer( s, ur ) != NT_OK ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return;
    }

    nt_resolve_name_done( ctx );
    ur->ctx = NULL;

    u->peer.start_time = nt_current_msec;

    pscf = nt_stream_get_module_srv_conf( s, nt_stream_socks5_module );

    if( pscf->next_upstream_tries
        && u->peer.tries > pscf->next_upstream_tries ) {
        u->peer.tries = pscf->next_upstream_tries;
    }

    nt_stream_socks5_connect( s );
}



/* // 处理上下游的数据收发
 * // from_upstream参数标记是否是上游，使用的是ev->write
 * // 上游下游的可读可写回调函数都调用了该函数
 *
 * ev 代表下游的 c->read c->write
 * 或
 * ev 代表上游的 pc->read pc->write
 *
 * 如果ev->write 1 我们可以认定为当前事件为write事件
 *
 * 那么传入 nt_stream_socks5_process的 from_ups为 0， do_write为1，
 *
 *  所以下游读，即 from_ups = 1， ev->write = 0
 *
 * from_upstream =1 , do_write = 0 , 上游的read
 * from_upstream =0 , do_write = 1 , 上游的write
 *
 * from_upstream =0 , do_write = 0 , 下游的read
 * from_upstream =1 , do_write = 1 , 下游的write
 *
 * 下游的read -->  调用recv --> 触发 dst为 pc 的write  --> 就是上游的write
 * 上游游的read -->  调用recv --> 触发 dst为 c 的write  --> 就是下游的write

 * 当前函数是为了做连接的检测，是否需要延时处理，是否断开了
 */
void
nt_stream_socks5_process_connection( nt_event_t *ev, nt_uint_t from_upstream )
{
    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_session_t         *s;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    // 连接、会话、上游
    c = ev->data;
    s = c->data;
    u = s->upstream;
    nt_log_debug2( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s, from_upstream=%d", __func__, from_upstream );

    if( c->close ) {
        nt_log_error( NT_LOG_INFO, c->log, 0, "shutdown timeout" );
        nt_stream_socks5_finalize( s, NT_STREAM_OK );
        return;
    }

    // c是下游连接，pc是上游连接
    c = s->connection;
    pc = u->peer.connection;

    pscf = nt_stream_get_module_srv_conf( s, nt_stream_socks5_module );

    // 超时处理，没有delay则失败
//    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "ev->timedout=%d", ev->timedout );
//    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "ev->delayed=%d", ev->delayed );
    if( ev->timedout ) {
        ev->timedout = 0;

        if( ev->delayed ) {
            ev->delayed = 0;

//           nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "ev->ready=%d", ev->ready );
            if( !ev->ready ) {
                if( nt_handle_read_event( ev, 0 ) != NT_OK ) {
                    nt_stream_socks5_finalize( s,
                                                NT_STREAM_INTERNAL_SERVER_ERROR );
                    return;
                }

                if( u->connected && !c->read->delayed && !pc->read->delayed ) {
                    nt_add_timer( c->write, pscf->timeout );
                }

                return;
            }

        } else {
            if( s->connection->type == SOCK_DGRAM ) {

                if( pscf->responses == NT_MAX_INT32_VALUE
                    || ( u->responses >= pscf->responses * u->requests ) ) {

                    /*
                     * successfully terminate timed out UDP session
                     * if expected number of responses was received
                     */

                    handler = c->log->handler;
                    c->log->handler = NULL;

                    nt_log_error( NT_LOG_INFO, c->log, 0,
                                   "udp timed out"
                                   ", packets from/to client:%ui/%ui"
                                   ", bytes from/to client:%O/%O"
                                   ", bytes from/to upstream:%O/%O",
                                   u->requests, u->responses,
                                   s->received, c->sent, u->received,
                                   pc ? pc->sent : 0 );

                    c->log->handler = handler;

                    nt_stream_socks5_finalize( s, NT_STREAM_OK );
                    return;
                }

                nt_connection_error( pc, NT_ETIMEDOUT, "upstream timed out" );

                pc->read->error = 1;

                nt_stream_socks5_finalize( s, NT_STREAM_BAD_GATEWAY );

                return;
            }

            nt_connection_error( c, NT_ETIMEDOUT, "connection timed out" );

            nt_stream_socks5_finalize( s, NT_STREAM_OK );

            return;
        }

    } else if( ev->delayed ) {

        nt_log_debug0( NT_LOG_DEBUG_STREAM, c->log, 0,
                        "stream connection delayed" );

        if( nt_handle_read_event( ev, 0 ) != NT_OK ) {
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        }

        return;
    }

    //如果 参数为处理上游，from_upstream =1
    //并且连接上游已经断开，则直接返回
    if( from_upstream && !u->connected ) {
        return;
    }

    // 核心处理函数，处理两个连接的数据收发
    nt_stream_socks5_process( s, from_upstream, ev->write );
}


void
nt_stream_socks5_connect_handler( nt_event_t *ev )
{
    nt_connection_t      *c;
    nt_stream_session_t  *s;

    c = ev->data;
    s = c->data;
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );

    if( ev->timedout ) {
        nt_log_error( NT_LOG_ERR, c->log, NT_ETIMEDOUT, "upstream timed out" );
        nt_stream_socks5_next_upstream( s );
        return;
    }

    nt_del_timer( c->write );

    nt_log_debug0( NT_LOG_DEBUG_STREAM, c->log, 0,
                    "stream socks5 connect upstream" );

    if( nt_stream_socks5_test_connect( c ) != NT_OK ) {
        //连接上游服务器失败，断开连接
        nt_stream_socks5_next_upstream( s );
        return;
    }

    nt_stream_socks5_init_upstream( s );
}


nt_int_t
nt_stream_socks5_test_connect( nt_connection_t *c )
{
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0, "----> %s", __func__ );
    int        err;
    socklen_t  len;

    #if (NT_HAVE_KQUEUE)

    if( nt_event_flags & NT_USE_KQUEUE_EVENT )  {
        err = c->write->kq_errno ? c->write->kq_errno : c->read->kq_errno;

        if( err ) {
            ( void ) nt_connection_error( c, err,
                                           "kevent() reported that connect() failed" );
            return NT_ERROR;
        }

    } else
    #endif
    {
        err = 0;
        len = sizeof( int );

        /*
         * BSDs and Linux return 0 and set a pending error in err
         * Solaris returns -1 and sets errno
         */

        if( getsockopt( c->fd, SOL_SOCKET, SO_ERROR, ( void * ) &err, &len )
            == -1 ) {
            err = nt_socket_errno;
        }

        if( err ) {
            ( void ) nt_connection_error( c, err, "connect() failed" );
            return NT_ERROR;
        }
    }

    return NT_OK;
}



/* // 核心处理函数，处理两个连接的数据收发
 * // 可以处理上下游的数据收发
 * // 参数标记是否是上游、是否写数据
 * // 最终都会调用到这个处理函数
 * // 无论是上游可读可写事件还是下游可读可写事件都会调用该函数
 * // from_ups == 1 可能是上游的可读事件（从上游读内容），也可能是下游的可写事件（写的内容来自上游） */
/*
 *  原先逻辑， 先检查是否可以发送数据，如果可以调用nint_stream_top_filter进行发包，
 *  如果可以接收数据，就调用recv
 *
 *  修改后逻辑，数据应当满足
 *  下游收 -> 上游发
 *  上游收 -> 下游发 的过程
 *
 * */
void
nt_stream_socks5_process( nt_stream_session_t *s, nt_uint_t from_upstream,
                           nt_uint_t do_write )
{
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );
    nt_log_debug2( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> from_upstream=%d, do_write=%d", from_upstream, do_write );
    char                         *recv_action, *send_action;
    off_t                        *received, limit;
    size_t                        size, limit_rate;
    ssize_t                       n;
    nt_buf_t                    *b;
    nt_int_t                     rc;
    nt_uint_t                    flags, *packets;
    nt_msec_t                    delay;
    nt_chain_t                  *cl, **ll, **out, **busy;
    nt_connection_t             *c, *pc, *src, *dst;
    nt_log_handler_pt            handler;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    // u是上游结构体
    u = s->upstream;

    // c是下游的连接
    c = s->connection;

    // pc是上游的连接，如果连接失败就是nullptr
    pc = u->connected ? u->peer.connection : NULL;

    // nginx处于即将停止状态，连接是udp
    // 使用连接的log记录日志
    if( c->type == SOCK_DGRAM && ( nt_terminate || nt_exiting ) ) {

        /* socket is already closed on worker shutdown */

        handler = c->log->handler;
        c->log->handler = NULL;

        nt_log_error( NT_LOG_INFO, c->log, 0, "disconnected on shutdown" );

        c->log->handler = handler;

        nt_stream_socks5_finalize( s, NT_STREAM_OK );
        return;
    }

    // 取proxy模块的配置
    pscf = nt_stream_get_module_srv_conf( s, nt_stream_socks5_module );


    // 根据上下游状态决定来源和目标
    // 以及缓冲区、限速等
    // 注意使用的缓冲区指针
    if( from_upstream ) {
        //触发上游事件

        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s 接收到上游", __func__ );
        // 数据下行
        src = pc;
        dst = c;
        // 缓冲区是upstream_buf，即上游来的数据
        b = &u->upstream_buf;
        limit_rate = u->download_rate;
        received = &u->received;
        packets = &u->responses;
        out = &u->downstream_out;
        busy = &u->downstream_busy;
        recv_action = "socks5ing and reading from upstream";
        send_action = "socks5ing and sending to client";

    } else {
        //触发下游事件
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s 接收到下游", __func__ );
        // 数据上行
        src = c;
        dst = pc;

        // 缓冲区是downstream_buf，即下游来的数据
        // 早期downstream_buf直接就是客户端连接的buffer
        // 现在是一个正常分配的buffer
        b = &u->downstream_buf;
        limit_rate = u->upload_rate;
        received = &s->received;
        packets = &u->requests;
        out = &u->upstream_out;
        busy = &u->upstream_busy;
        recv_action = "socks5ing and reading from client";
        send_action = "socks5ing and sending to upstream";
    }

    // b指向当前需要操作的缓冲区
    // 死循环操作，直到出错或者again
    for( ;; ) {

        // 如果要求写，那么把缓冲区里的数据发到dst
        if( do_write && dst ) {

            /* break; */
            // 条件是有数据，且dst连接是可写的
            if( *out || *busy || dst->buffered ) {
                c->log->action = send_action;

//                nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", "call nt_stream_top_filter" );
                // 调用filter过滤链表，过滤数据最后发出去
                // 在filter init中赋值
                // nt_stream_top_filter = nt_stream_write_filter
                // 即调用  nt_stream_write_filter()
                rc = nt_stream_top_filter( s, *out, from_upstream );

                if( rc == NT_ERROR ) {
                    nt_stream_socks5_finalize( s, NT_STREAM_OK );
                    return;
                }

                // 调整缓冲区链表，节约内存使用
                nt_chain_update_chains( c->pool, &u->free, busy, out,
                                         ( nt_buf_tag_t ) &nt_stream_socks5_module );

                if( *busy == NULL ) {
                    b->pos = b->start;
                    b->last = b->start;
                }
            }
        }

        // size是缓冲区的剩余可用空间
        size = b->end - b->last;
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> size=%d", size );
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> read->ready=%d", src->read->ready );
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> read->error=%d", src->read->error );
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> read->delayed=%d", src->read->delayed );

        // 如果缓冲区有剩余，且src还可以读数据
        if( size && src->read->ready && !src->read->delayed
            && !src->read->error ) {
            // 限速处理
            if( limit_rate ) {
                limit = ( off_t ) limit_rate * ( nt_time() - u->start_sec + 1 )
                        - *received;

                if( limit <= 0 ) {
                    src->read->delayed = 1;
                    delay = ( nt_msec_t )( - limit * 1000 / limit_rate + 1 );
                    nt_add_timer( src->read, delay );
                    break;
                }

                if( c->type == SOCK_STREAM && ( off_t ) size > limit ) {
                    size = ( size_t ) limit;
                }
            }

            c->log->action = recv_action;

            // 尽量读满缓冲区
            n = src->recv( src, b->last, size );

            // 如果不可读，或者已经读完
            // break结束for循环
            if( n == NT_AGAIN ) {
                break;
            }

            // 出错，标记为eof
            if( n == NT_ERROR ) {
                src->read->eof = 1;
                n = 0;
            }

            // 读取了n字节的数据
            if( n >= 0 ) {

                // 限速
                if( limit_rate ) {
                    delay = ( nt_msec_t )( n * 1000 / limit_rate );

                    if( delay > 0 ) {
                        src->read->delayed = 1;
                        nt_add_timer( src->read, delay );
                    }
                }

                //上游获取数据时间记录
                if( from_upstream ) {
                    if( u->state->first_byte_time == ( nt_msec_t ) -1 ) {
                        u->state->first_byte_time = nt_current_msec
                                                    - u->start_time;
                    }
                }

                // 找到链表末尾
                for( ll = out; *ll; ll = &( *ll )->next ) { /* void */ }

                // 把读到的数据挂到链表末尾
                cl = nt_chain_get_free_buf( c->pool, &u->free );
                if( cl == NULL ) {
                    nt_stream_socks5_finalize( s,
                                                NT_STREAM_INTERNAL_SERVER_ERROR );
                    return;
                }

                *ll = cl;

                cl->buf->pos = b->last;
                cl->buf->last = b->last + n;
                cl->buf->tag = ( nt_buf_tag_t ) &nt_stream_socks5_module;

                cl->buf->temporary = ( n ? 1 : 0 );
                cl->buf->last_buf = src->read->eof;
                cl->buf->flush = 1;

                ( *packets )++;
                // 增加接收的数据字节数
                *received += n;
                // 缓冲区的末尾指针移动，表示收到了n字节新数据
                b->last += n;

                // 有数据，那么就可以继续向dst发送
                // my_test 可注释掉。不影响结果
                do_write = 1;

                // 读数据部分结束, 重新回到 send 部分
                // my_test 可改成break。不影响结果
                continue;
                /* break; */

            }
        }

        break;
    }

    c->log->action = "socks5ing connection";
    // 这时应该是src已经读完，数据也发送完
    // 读取出错也会有eof标志
    if( nt_stream_socks5_test_finalize( s, from_upstream ) == NT_OK ) {
        nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s ", "finalize OK" );
        return;
    }

    // 如果eof就要关闭读事件
    flags = src->read->eof ? NT_CLOSE_EVENT : 0;


    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> flags=%d ", flags );
    if( nt_handle_read_event( src->read, flags ) != NT_OK ) {
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return;
    }
    return;




    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s ", "before dst" );

    if( dst ) {

        nt_log_debug3( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s 触发 dst write->active=%d, ready=%d ", __func__, dst->write->active,  dst->write->ready );
        return;

        /**
         * 调用此函数的nt_handle_write_event的意思是
         * dst->write ， 可写、已就绪，就删除写事件
         * 不可写、未就绪，就添加写事件
         */
        if( nt_handle_write_event( dst->write, 0 ) != NT_OK ) {
            nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
            return;
        }

        //下游未延时， 上游未延时
        //添加下游的write 定时任务
        nt_log_debug2( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> c->read->delayed=%d, pc->read->delayed=%d ", c->read->delayed,  pc->read->delayed );
        if( !c->read->delayed && !pc->read->delayed ) {
            nt_add_timer( c->write, pscf->timeout );

        } else if( c->write->timer_set ) {
            nt_del_timer( c->write );
        }
    }
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s ", "forward end" );
}


/*检测上下游的连接是否断开了
 *
 * 返回NT_OK表示连接已经断开并且释放
 * */
nt_int_t
nt_stream_socks5_test_finalize( nt_stream_session_t *s,
                                 nt_uint_t from_upstream )
{
    nt_connection_t             *c, *pc;
    nt_log_handler_pt            handler;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );
    pscf = nt_stream_get_module_srv_conf( s, nt_stream_socks5_module );

    c = s->connection;
    u = s->upstream;
    pc = u->connected ? u->peer.connection : NULL;

    if( c->type == SOCK_DGRAM ) {

        if( pscf->requests && u->requests < pscf->requests ) {
            return NT_DECLINED;
        }

        if( pscf->requests ) {
            nt_delete_udp_connection( c );
        }

        if( pscf->responses == NT_MAX_INT32_VALUE
            || u->responses < pscf->responses * u->requests ) {
            return NT_DECLINED;
        }

        if( pc == NULL || c->buffered || pc->buffered ) {
            return NT_DECLINED;
        }

        handler = c->log->handler;
        c->log->handler = NULL;

        nt_log_error( NT_LOG_INFO, c->log, 0,
                       "udp done"
                       ", packets from/to client:%ui/%ui"
                       ", bytes from/to client:%O/%O"
                       ", bytes from/to upstream:%O/%O",
                       u->requests, u->responses,
                       s->received, c->sent, u->received, pc ? pc->sent : 0 );

        c->log->handler = handler;

        nt_stream_socks5_finalize( s, NT_STREAM_OK );

        return NT_OK;
    }

    /* c->type == SOCK_STREAM */

    if( pc == NULL
        || ( !c->read->eof && !pc->read->eof )
        || ( !c->read->eof && c->buffered )
        || ( !pc->read->eof && pc->buffered ) ) {
        return NT_DECLINED;
    }

    handler = c->log->handler;
    c->log->handler = NULL;

    nt_log_error( NT_LOG_INFO, c->log, 0,
                   "%s disconnected"
                   ", bytes from/to client:%O/%O"
                   ", bytes from/to upstream:%O/%O",
                   from_upstream ? "upstream" : "client",
                   s->received, c->sent, u->received, pc ? pc->sent : 0 );

    c->log->handler = handler;

    nt_stream_socks5_finalize( s, NT_STREAM_OK );

    return NT_OK;
}


void
nt_stream_socks5_next_upstream( nt_stream_session_t *s )
{
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );
    nt_msec_t                    timeout;
    nt_connection_t             *pc;
    nt_stream_upstream_t        *u;
    nt_stream_socks5_srv_conf_t  *pscf;

    nt_log_debug0( NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                    "stream socks5 next upstream" );

    u = s->upstream;
    pc = u->peer.connection;

    if( pc && pc->buffered ) {
        nt_log_error( NT_LOG_ERR, s->connection->log, 0,
                       "buffered data on next upstream" );
        nt_stream_socks5_finalize( s, NT_STREAM_INTERNAL_SERVER_ERROR );
        return;
    }

    if( s->connection->type == SOCK_DGRAM ) {
        u->upstream_out = NULL;
    }

    if( u->peer.sockaddr ) {
        u->peer.free( &u->peer, u->peer.data, NT_PEER_FAILED );
        u->peer.sockaddr = NULL;
    }

    pscf = nt_stream_get_module_srv_conf( s, nt_stream_socks5_module );

    timeout = pscf->next_upstream_timeout;

    if( u->peer.tries == 0
        || !pscf->next_upstream
        || ( timeout && nt_current_msec - u->peer.start_time >= timeout ) ) {
        nt_stream_socks5_finalize( s, NT_STREAM_BAD_GATEWAY );
        return;
    }

    if( pc ) {
        nt_log_debug1( NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                        "close socks5 upstream connection: %d", pc->fd );

        #if (NT_STREAM_SSL)
        if( pc->ssl ) {
            pc->ssl->no_wait_shutdown = 1;
            pc->ssl->no_send_shutdown = 1;

            ( void ) nt_ssl_shutdown( pc );
        }
        #endif

        u->state->bytes_received = u->received;
        u->state->bytes_sent = pc->sent;

        nt_close_connection( pc );
        u->peer.connection = NULL;
    }

    nt_stream_socks5_connect( s );
}


void
nt_stream_socks5_finalize_org( nt_stream_session_t *s, nt_uint_t rc )
{
    nt_log_debug1( NT_LOG_DEBUG_STREAM,  s->connection->log, 0, "----> %s", __func__ );
    nt_uint_t              state;
    nt_connection_t       *pc;
    nt_stream_upstream_t  *u;

    nt_log_debug1( NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                    "finalize stream socks5: %i", rc );

    u = s->upstream;


    if( u == NULL ) {
        goto noupstream;
    }

    if( u->resolved && u->resolved->ctx ) {
        nt_resolve_name_done( u->resolved->ctx );
        u->resolved->ctx = NULL;
    }

    pc = u->peer.connection;

    if( u->state ) {
        if( u->state->response_time == ( nt_msec_t ) -1 ) {
            u->state->response_time = nt_current_msec - u->start_time;
        }

        if( pc ) {
            u->state->bytes_received = u->received;
            u->state->bytes_sent = pc->sent;
        }
    }

    if( u->peer.free && u->peer.sockaddr ) {
        state = 0;

        if( pc && pc->type == SOCK_DGRAM
            && ( pc->read->error || pc->write->error ) ) {
            state = NT_PEER_FAILED;
        }

        u->peer.free( &u->peer, u->peer.data, state );
        u->peer.sockaddr = NULL;
    }

    if( pc ) {
        nt_log_debug1( NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                        "close stream socks5 upstream connection: %d", pc->fd );

        #if (NT_STREAM_SSL)
        if( pc->ssl ) {
            pc->ssl->no_wait_shutdown = 1;
            ( void ) nt_ssl_shutdown( pc );
        }
        #endif

        nt_close_connection( pc );
        u->peer.connection = NULL;
    }

noupstream:

    nt_stream_finalize_session( s, rc );
}

void
nt_stream_socks5_finalize( nt_stream_session_t *s, nt_uint_t rc )
{

    nt_stream_socks5_session_t *ss = s->data;
    /* nt_stream_socks5_finalize_org( ss->udp, rc ); */

    nt_stream_socks5_finalize_org( s, rc );
}



u_char *
nt_stream_socks5_log_error( nt_log_t *log, u_char *buf, size_t len )
{
//    nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0 ,"%s start", __func__ );
    u_char                 *p;
    nt_connection_t       *pc;
    nt_stream_session_t   *s;
    nt_stream_upstream_t  *u;

    s = log->data;

    u = s->upstream;

    p = buf;

    if( u->peer.name ) {
        p = nt_snprintf( p, len, ", upstream: \"%V\"", u->peer.name );
        len -= p - buf;
    }

    pc = u->peer.connection;

    p = nt_snprintf( p, len,
                      ", bytes from/to client:%O/%O"
                      ", bytes from/to upstream:%O/%O",
                      s->received, s->connection->sent,
                      u->received, pc ? pc->sent : 0 );

    return p;
}


void *
nt_stream_socks5_create_srv_conf( nt_conf_t *cf )
{
    // nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0 ,"%s start", __func__ );
    debug( "start" );
    nt_stream_socks5_srv_conf_t  *conf;

    conf = nt_pcalloc( cf->pool, sizeof( nt_stream_socks5_srv_conf_t ) );
    if( conf == NULL ) {
        return NULL;
    }

    /*
     * set by nt_pcalloc():
     *
     *     conf->ssl_protocols = 0;
     *     conf->ssl_ciphers = { 0, NULL };
     *     conf->ssl_name = NULL;
     *     conf->ssl_trusted_certificate = { 0, NULL };
     *     conf->ssl_crl = { 0, NULL };
     *     conf->ssl_certificate = { 0, NULL };
     *     conf->ssl_certificate_key = { 0, NULL };
     *
     *     conf->upload_rate = NULL;
     *     conf->download_rate = NULL;
     *     conf->ssl = NULL;
     *     conf->upstream = NULL;
     *     conf->upstream_value = NULL;
     */

    conf->connect_timeout = NT_CONF_UNSET_MSEC;
    conf->timeout = NT_CONF_UNSET_MSEC;
    conf->next_upstream_timeout = NT_CONF_UNSET_MSEC;
    conf->buffer_size = NT_CONF_UNSET_SIZE;
    conf->requests = NT_CONF_UNSET_UINT;
    conf->responses = NT_CONF_UNSET_UINT;
    conf->next_upstream_tries = NT_CONF_UNSET_UINT;
    conf->next_upstream = NT_CONF_UNSET;
    conf->socks5_protocol = NT_CONF_UNSET;
    conf->local = NT_CONF_UNSET_PTR;
    conf->socket_keepalive = NT_CONF_UNSET;

    #if (NT_STREAM_SSL)
    conf->ssl_enable = NT_CONF_UNSET;
    conf->ssl_session_reuse = NT_CONF_UNSET;
    conf->ssl_server_name = NT_CONF_UNSET;
    conf->ssl_verify = NT_CONF_UNSET;
    conf->ssl_verify_depth = NT_CONF_UNSET_UINT;
    conf->ssl_passwords = NT_CONF_UNSET_PTR;
    #endif

    return conf;
}


char *
nt_stream_socks5_merge_srv_conf( nt_conf_t *cf, void *parent, void *child )
{
    debug( "start" );
//   nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0 ,"%s start", __func__ );
    nt_stream_socks5_srv_conf_t *prev = parent;
    nt_stream_socks5_srv_conf_t *conf = child;

    nt_conf_merge_msec_value( conf->connect_timeout,
                               prev->connect_timeout, 60000 );

    nt_conf_merge_msec_value( conf->timeout,
                               prev->timeout, 10 * 60000 );

    nt_conf_merge_msec_value( conf->next_upstream_timeout,
                               prev->next_upstream_timeout, 0 );

    nt_conf_merge_size_value( conf->buffer_size,
                               prev->buffer_size, 16384 );

    if( conf->upload_rate == NULL ) {
        conf->upload_rate = prev->upload_rate;
    }

    if( conf->download_rate == NULL ) {
        conf->download_rate = prev->download_rate;
    }

    nt_conf_merge_uint_value( conf->requests,
                               prev->requests, 0 );

    nt_conf_merge_uint_value( conf->responses,
                               prev->responses, NT_MAX_INT32_VALUE );

    nt_conf_merge_uint_value( conf->next_upstream_tries,
                               prev->next_upstream_tries, 0 );

    nt_conf_merge_value( conf->next_upstream, prev->next_upstream, 1 );

    nt_conf_merge_value( conf->socks5_protocol, prev->socks5_protocol, 0 );

    nt_conf_merge_ptr_value( conf->local, prev->local, NULL );

    nt_conf_merge_value( conf->socket_keepalive,
                          prev->socket_keepalive, 0 );

    #if (NT_STREAM_SSL)

    nt_conf_merge_value( conf->ssl_enable, prev->ssl_enable, 0 );

    nt_conf_merge_value( conf->ssl_session_reuse,
                          prev->ssl_session_reuse, 1 );

    nt_conf_merge_bitmask_value( conf->ssl_protocols, prev->ssl_protocols,
                                  ( NT_CONF_BITMASK_SET | NT_SSL_TLSv1
                                    | NT_SSL_TLSv1_1 | NT_SSL_TLSv1_2 ) );

    nt_conf_merge_str_value( conf->ssl_ciphers, prev->ssl_ciphers, "DEFAULT" );

    if( conf->ssl_name == NULL ) {
        conf->ssl_name = prev->ssl_name;
    }

    nt_conf_merge_value( conf->ssl_server_name, prev->ssl_server_name, 0 );

    nt_conf_merge_value( conf->ssl_verify, prev->ssl_verify, 0 );

    nt_conf_merge_uint_value( conf->ssl_verify_depth,
                               prev->ssl_verify_depth, 1 );

    nt_conf_merge_str_value( conf->ssl_trusted_certificate,
                              prev->ssl_trusted_certificate, "" );

    nt_conf_merge_str_value( conf->ssl_crl, prev->ssl_crl, "" );

    nt_conf_merge_str_value( conf->ssl_certificate,
                              prev->ssl_certificate, "" );

    nt_conf_merge_str_value( conf->ssl_certificate_key,
                              prev->ssl_certificate_key, "" );

    nt_conf_merge_ptr_value( conf->ssl_passwords, prev->ssl_passwords, NULL );

    if( conf->ssl_enable && nt_stream_socks5_set_ssl( cf, conf ) != NT_OK ) {
        return NT_CONF_ERROR;
    }

    #endif

    return NT_CONF_OK;
}


#if (NT_STREAM_SSL)

nt_int_t
nt_stream_socks5_set_ssl( nt_conf_t *cf, nt_stream_socks5_srv_conf_t *pscf )
{
    // nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0 ,"%s start", __func__ );
    nt_pool_cleanup_t  *cln;

    pscf->ssl = nt_pcalloc( cf->pool, sizeof( nt_ssl_t ) );
    if( pscf->ssl == NULL ) {
        return NT_ERROR;
    }

    pscf->ssl->log = cf->log;

    if( nt_ssl_create( pscf->ssl, pscf->ssl_protocols, NULL ) != NT_OK ) {
        return NT_ERROR;
    }

    cln = nt_pool_cleanup_add( cf->pool, 0 );
    if( cln == NULL ) {
        nt_ssl_cleanup_ctx( pscf->ssl );
        return NT_ERROR;
    }

    cln->handler = nt_ssl_cleanup_ctx;
    cln->data = pscf->ssl;

    if( pscf->ssl_certificate.len ) {

        if( pscf->ssl_certificate_key.len == 0 ) {
            nt_log_error( NT_LOG_EMERG, cf->log, 0,
                           "no \"socks5_ssl_certificate_key\" is defined "
                           "for certificate \"%V\"", &pscf->ssl_certificate );
            return NT_ERROR;
        }

        if( nt_ssl_certificate( cf, pscf->ssl, &pscf->ssl_certificate,
                                 &pscf->ssl_certificate_key, pscf->ssl_passwords )
            != NT_OK ) {
            return NT_ERROR;
        }
    }

    if( nt_ssl_ciphers( cf, pscf->ssl, &pscf->ssl_ciphers, 0 ) != NT_OK ) {
        return NT_ERROR;
    }

    if( pscf->ssl_verify ) {
        if( pscf->ssl_trusted_certificate.len == 0 ) {
            nt_log_error( NT_LOG_EMERG, cf->log, 0,
                           "no socks5_ssl_trusted_certificate for socks5_ssl_verify" );
            return NT_ERROR;
        }

        if( nt_ssl_trusted_certificate( cf, pscf->ssl,
                                         &pscf->ssl_trusted_certificate,
                                         pscf->ssl_verify_depth )
            != NT_OK ) {
            return NT_ERROR;
        }

        if( nt_ssl_crl( cf, pscf->ssl, &pscf->ssl_crl ) != NT_OK ) {
            return NT_ERROR;
        }
    }

    if( nt_ssl_client_session_cache( cf, pscf->ssl, pscf->ssl_session_reuse )
        != NT_OK ) {
        return NT_ERROR;
    }

    return NT_OK;
}

#endif


char *nt_stream_parse_socks5_pass( nt_stream_socks5_srv_conf_t *pscf, nt_conf_t *cf, nt_uint_t flags )
{

    nt_stream_complex_value_t           cv;
    nt_stream_compile_complex_value_t   ccv;
    nt_url_t                            u;
    nt_str_t                           *value, *url;

    /* nt_log_debug1( NT_LOG_DEBUG_STREAM,  cf->log, 0 ," socks5_pass %s", url->data ); */

    //获取配置内容
    value = cf->args->elts;

    //socks_pass 后面的内容
    url = &value[1];
    debug(  " socks5_pass %s", url->data );


    nt_memzero( &ccv, sizeof( nt_stream_compile_complex_value_t ) );

    ccv.cf = cf;
    ccv.value = url;
    ccv.complex_value = &cv;

    if( nt_stream_compile_complex_value( &ccv ) != NT_OK ) {
        return NT_CONF_ERROR;
    }

    if( cv.lengths ) {

        if( flags ) {
            pscf->upstream_value = nt_palloc( cf->pool,
                                               sizeof( nt_stream_complex_value_t ) );
            if( pscf->upstream_value == NULL ) {
                return NT_CONF_ERROR;
            }

            *pscf->upstream_value = cv;
        } else {
            pscf->udp_upstream_value = nt_palloc( cf->pool,
                                                   sizeof( nt_stream_complex_value_t ) );
            if( pscf->udp_upstream_value == NULL ) {
                return NT_CONF_ERROR;
            }

            *pscf->udp_upstream_value = cv;
        }

        return NT_CONF_OK;
    }

    nt_memzero( &u, sizeof( nt_url_t ) );

    u.url = *url;
    u.no_resolve = 1;

    //把流加入到upsteam 中
    if( flags ) {
        pscf->upstream = nt_stream_upstream_add( cf, &u, 0 );
        if( pscf->upstream == NULL ) {
            return NT_CONF_ERROR;
        }
    } else {
        pscf->udp_upstream = nt_stream_upstream_add( cf, &u, 0 );
        if( pscf->udp_upstream == NULL ) {
            return NT_CONF_ERROR;
        }
    }

    return NT_CONF_OK;
}


// socks5_pass 对应的回调函数。加载配置会进入此函数
// 读取socks5_pass 后的内容。
// 由于tun转发过来的代理需要支持，tcp， udp， 这里自动生成一个新的upstrea 给udp用
char *
nt_stream_socks5_pass( nt_conf_t *cf, nt_command_t *cmd, void *conf )
{
    debug( "start" );
    ///  nt_log_debug1( NT_LOG_DEBUG_STREAM,  c->log, 0 ,"%s start", __func__ );
    nt_stream_socks5_srv_conf_t *pscf = conf;

    nt_stream_core_srv_conf_t          *cscf;
    nt_int_t ret;

    //判断值是否已经读取过，如果读取过了，就不再读取
    if( pscf->upstream || pscf->upstream_value ) {
        return "is duplicate";
    }

    //获取stream core 配置
    cscf = nt_stream_conf_get_module_srv_conf( cf, nt_stream_core_module );

    //设定stream 的执行回调
    cscf->handler = nt_stream_socks5_handler; //回调处理函数


    //upstream
    ret = nt_stream_parse_socks5_pass( pscf, cf, 1 );

    if( ret ==  NT_CONF_ERROR )
        return NT_CONF_ERROR;

    //udp_upstream
    return nt_stream_parse_socks5_pass( pscf, cf, 0 );
}


char *
nt_stream_socks5_bind( nt_conf_t *cf, nt_command_t *cmd, void *conf )
{
    // nt_log_debug1( NT_LOG_DEBUG_STREAM,  cf, 0 ,"%s start", __func__ );
    nt_stream_socks5_srv_conf_t *pscf = conf;

    nt_int_t                            rc;
    nt_str_t                           *value;
    nt_stream_complex_value_t           cv;
    nt_stream_upstream_local_t         *local;
    nt_stream_compile_complex_value_t   ccv;

    if( pscf->local != NT_CONF_UNSET_PTR ) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if( cf->args->nelts == 2 && nt_strcmp( value[1].data, "off" ) == 0 ) {
        pscf->local = NULL;
        return NT_CONF_OK;
    }

    nt_memzero( &ccv, sizeof( nt_stream_compile_complex_value_t ) );

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &cv;

    if( nt_stream_compile_complex_value( &ccv ) != NT_OK ) {
        return NT_CONF_ERROR;
    }

    local = nt_pcalloc( cf->pool, sizeof( nt_stream_upstream_local_t ) );
    if( local == NULL ) {
        return NT_CONF_ERROR;
    }

    pscf->local = local;

    if( cv.lengths ) {
        local->value = nt_palloc( cf->pool, sizeof( nt_stream_complex_value_t ) );
        if( local->value == NULL ) {
            return NT_CONF_ERROR;
        }

        *local->value = cv;

    } else {
        local->addr = nt_palloc( cf->pool, sizeof( nt_addr_t ) );
        if( local->addr == NULL ) {
            return NT_CONF_ERROR;
        }

        rc = nt_parse_addr_port( cf->pool, local->addr, value[1].data,
                                  value[1].len );

        switch( rc ) {
        case NT_OK:
            local->addr->name = value[1];
            break;

        case NT_DECLINED:
            nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                                "invalid address \"%V\"", &value[1] );
        /* fall through */

        default:
            return NT_CONF_ERROR;
        }
    }

    if( cf->args->nelts > 2 ) {
        if( nt_strcmp( value[2].data, "transparent" ) == 0 ) {
            #if (NT_HAVE_TRANSPARENT_PROXY)
            nt_core_conf_t  *ccf;

            ccf = ( nt_core_conf_t * ) nt_get_conf( cf->cycle->conf_ctx,
                    nt_core_module );

            ccf->transparent = 1;
            local->transparent = 1;
            #else
            nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                                "transparent socks5ing is not supported "
                                "on this platform, ignored" );
            #endif
        } else {
            nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                                "invalid parameter \"%V\"", &value[2] );
            return NT_CONF_ERROR;
        }
    }

    return NT_CONF_OK;
}
