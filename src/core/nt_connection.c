
#include <nt_def.h>
#include <nt_core.h>
#include <nt_event.h>


nt_os_io_t  nt_io;

static void nt_drain_connections( nt_cycle_t *cycle );

/*
 * 在执行 http 或stream block的时候，创建一个监听类型的结构体
 * */
nt_listening_t *
nt_create_listening( nt_conf_t *cf, struct sockaddr *sockaddr,
                     socklen_t socklen )
{
    debug( "nt_create_listening" );
    size_t            len;
    nt_listening_t  *ls;
    struct sockaddr  *sa;
    u_char            text[NT_SOCKADDR_STRLEN];

    ls = nt_array_push( &cf->cycle->listening );
    if( ls == NULL ) {
        return NULL;
    }

    nt_memzero( ls, sizeof( nt_listening_t ) );

    sa = nt_palloc( cf->pool, socklen );
    if( sa == NULL ) {
        return NULL;
    }

    nt_memcpy( sa, sockaddr, socklen );

    ls->sockaddr = sa;
    ls->socklen = socklen;

    len = nt_sock_ntop( sa, socklen, text, NT_SOCKADDR_STRLEN, 1 );
    ls->addr_text.len = len;

    switch( ls->sockaddr->sa_family ) {
        #if (NT_HAVE_INET6)
    case AF_INET6:
        ls->addr_text_max_len = NT_INET6_ADDRSTRLEN;
        break;
        #endif
        #if (NT_HAVE_UNIX_DOMAIN)
    case AF_UNIX:
        ls->addr_text_max_len = NT_UNIX_ADDRSTRLEN;
        len++;
        break;
        #endif
    case AF_INET:
        ls->addr_text_max_len = NT_INET_ADDRSTRLEN;
        break;
    default:
        ls->addr_text_max_len = NT_SOCKADDR_STRLEN;
        break;
    }

    ls->addr_text.data = nt_pnalloc( cf->pool, len );
    if( ls->addr_text.data == NULL ) {
        return NULL;
    }

    nt_memcpy( ls->addr_text.data, text, len );
    
    #if !(NT_WIN32)
        nt_rbtree_init(&ls->rbtree, &ls->sentinel, nt_udp_rbtree_insert_value);
    #endif

    ls->fd = ( nt_socket_t ) -1;
    ls->type = SOCK_STREAM;

    ls->backlog = NT_LISTEN_BACKLOG;
    ls->rcvbuf = -1;
    ls->sndbuf = -1;

    #if (NT_HAVE_SETFIB)
    ls->setfib = -1;
    #endif

    #if (NT_HAVE_TCP_FASTOPEN)
    ls->fastopen = -1;
    #endif

    return ls;
}


nt_int_t
nt_clone_listening( nt_cycle_t *cycle, nt_listening_t *ls )
{
    #if (NT_HAVE_REUSEPORT)

    nt_int_t         n;
    nt_core_conf_t  *ccf;
    nt_listening_t   ols;

    if( !ls->reuseport || ls->worker != 0 ) {
        return NT_OK;
    }

    ols = *ls;

    ccf = ( nt_core_conf_t * ) nt_get_conf( cycle->conf_ctx, nt_core_module );

    for( n = 1; n < ccf->worker_processes; n++ ) {

        /* create a socket for each worker process */

        ls = nt_array_push( &cycle->listening );
        if( ls == NULL ) {
            return NT_ERROR;
        }

        *ls = ols;
        ls->worker = n;
    }

    #endif

    return NT_OK;
}

/*
 * 设置socket 继承
 *
 * */
nt_int_t
nt_set_inherited_sockets( nt_cycle_t *cycle )
{
    size_t                     len;
    nt_uint_t                 i;
    nt_listening_t           *ls;
    socklen_t                  olen;
    #if (NT_HAVE_DEFERRED_ACCEPT || NT_HAVE_TCP_FASTOPEN)
    nt_err_t                  err;
    #endif
    #if (NT_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    struct accept_filter_arg   af;
    #endif
    #if (NT_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)
    int                        timeout;
    #endif
    #if (NT_HAVE_REUSEPORT)
    int                        reuseport;
    #endif

    ls = cycle->listening.elts;
    for( i = 0; i < cycle->listening.nelts; i++ ) {

        ls[i].sockaddr = nt_palloc( cycle->pool, sizeof( nt_sockaddr_t ) );
        if( ls[i].sockaddr == NULL ) {
            return NT_ERROR;
        }

        ls[i].socklen = sizeof( nt_sockaddr_t );
        if( getsockname( ls[i].fd, ls[i].sockaddr, &ls[i].socklen ) == -1 ) {
            nt_log_error( NT_LOG_CRIT, cycle->log, nt_socket_errno,
                          "getsockname() of the inherited "
                          "socket #%d failed", ls[i].fd );
            ls[i].ignore = 1;
            continue;
        }

        if( ls[i].socklen > ( socklen_t ) sizeof( nt_sockaddr_t ) ) {
            ls[i].socklen = sizeof( nt_sockaddr_t );
        }

        switch( ls[i].sockaddr->sa_family ) {

            #if (NT_HAVE_INET6)
        case AF_INET6:
            ls[i].addr_text_max_len = NT_INET6_ADDRSTRLEN;
            len = NT_INET6_ADDRSTRLEN + sizeof( "[]:65535" ) - 1;
            break;
            #endif

            #if (NT_HAVE_UNIX_DOMAIN)
        case AF_UNIX:
            ls[i].addr_text_max_len = NT_UNIX_ADDRSTRLEN;
            len = NT_UNIX_ADDRSTRLEN;
            break;
            #endif

        case AF_INET:
            ls[i].addr_text_max_len = NT_INET_ADDRSTRLEN;
            len = NT_INET_ADDRSTRLEN + sizeof( ":65535" ) - 1;
            break;

        default:
            nt_log_error( NT_LOG_CRIT, cycle->log, nt_socket_errno,
                          "the inherited socket #%d has "
                          "an unsupported protocol family", ls[i].fd );
            ls[i].ignore = 1;
            continue;
        }

        ls[i].addr_text.data = nt_pnalloc( cycle->pool, len );
        if( ls[i].addr_text.data == NULL ) {
            return NT_ERROR;
        }

        len = nt_sock_ntop( ls[i].sockaddr, ls[i].socklen,
                            ls[i].addr_text.data, len, 1 );
        if( len == 0 ) {
            return NT_ERROR;
        }

        ls[i].addr_text.len = len;

        ls[i].backlog = NT_LISTEN_BACKLOG;

        olen = sizeof( int );

        if( getsockopt( ls[i].fd, SOL_SOCKET, SO_TYPE, ( void * ) &ls[i].type,
                        &olen )
            == -1 ) {
            nt_log_error( NT_LOG_CRIT, cycle->log, nt_socket_errno,
                          "getsockopt(SO_TYPE) %V failed", &ls[i].addr_text );
            ls[i].ignore = 1;
            continue;
        }

        olen = sizeof( int );

        if( getsockopt( ls[i].fd, SOL_SOCKET, SO_RCVBUF, ( void * ) &ls[i].rcvbuf,
                        &olen )
            == -1 ) {
            nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                          "getsockopt(SO_RCVBUF) %V failed, ignored",
                          &ls[i].addr_text );

            ls[i].rcvbuf = -1;
        }

        olen = sizeof( int );

        if( getsockopt( ls[i].fd, SOL_SOCKET, SO_SNDBUF, ( void * ) &ls[i].sndbuf,
                        &olen )
            == -1 ) {
            nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                          "getsockopt(SO_SNDBUF) %V failed, ignored",
                          &ls[i].addr_text );

            ls[i].sndbuf = -1;
        }

        #if 0
        /* SO_SETFIB is currently a set only option */

        #if (NT_HAVE_SETFIB)

        olen = sizeof( int );

        if( getsockopt( ls[i].fd, SOL_SOCKET, SO_SETFIB,
                        ( void * ) &ls[i].setfib, &olen )
            == -1 ) {
            nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                          "getsockopt(SO_SETFIB) %V failed, ignored",
                          &ls[i].addr_text );

            ls[i].setfib = -1;
        }

        #endif
        #endif

        #if (NT_HAVE_REUSEPORT)

        reuseport = 0;
        olen = sizeof( int );

        #ifdef SO_REUSEPORT_LB

        if( getsockopt( ls[i].fd, SOL_SOCKET, SO_REUSEPORT_LB,
                        ( void * ) &reuseport, &olen )
            == -1 ) {
            nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                          "getsockopt(SO_REUSEPORT_LB) %V failed, ignored",
                          &ls[i].addr_text );

        } else {
            ls[i].reuseport = reuseport ? 1 : 0;
        }

        #else

        if( getsockopt( ls[i].fd, SOL_SOCKET, SO_REUSEPORT,
                        ( void * ) &reuseport, &olen )
            == -1 ) {
            nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                          "getsockopt(SO_REUSEPORT) %V failed, ignored",
                          &ls[i].addr_text );

        } else {
            ls[i].reuseport = reuseport ? 1 : 0;
        }
        #endif

        #endif

        if( ls[i].type != SOCK_STREAM ) {
            continue;
        }

        #if (NT_HAVE_TCP_FASTOPEN)

        olen = sizeof( int );

        if( getsockopt( ls[i].fd, IPPROTO_TCP, TCP_FASTOPEN,
                        ( void * ) &ls[i].fastopen, &olen )
            == -1 ) {
            err = nt_socket_errno;

            if( err != NT_EOPNOTSUPP && err != NT_ENOPROTOOPT
                && err != NT_EINVAL ) {
                nt_log_error( NT_LOG_NOTICE, cycle->log, err,
                              "getsockopt(TCP_FASTOPEN) %V failed, ignored",
                              &ls[i].addr_text );
            }

            ls[i].fastopen = -1;
        }

        #endif

        #if (NT_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)

        nt_memzero( &af, sizeof( struct accept_filter_arg ) );
        olen = sizeof( struct accept_filter_arg );

        if( getsockopt( ls[i].fd, SOL_SOCKET, SO_ACCEPTFILTER, &af, &olen )
            == -1 ) {
            err = nt_socket_errno;

            if( err == NT_EINVAL ) {
                continue;
            }

            nt_log_error( NT_LOG_NOTICE, cycle->log, err,
                          "getsockopt(SO_ACCEPTFILTER) for %V failed, ignored",
                          &ls[i].addr_text );
            continue;
        }

        if( olen < sizeof( struct accept_filter_arg ) || af.af_name[0] == '\0' ) {
            continue;
        }

        ls[i].accept_filter = nt_palloc( cycle->pool, 16 );
        if( ls[i].accept_filter == NULL ) {
            return NT_ERROR;
        }

        ( void ) nt_cpystrn( ( u_char * ) ls[i].accept_filter,
                             ( u_char * ) af.af_name, 16 );
        #endif

        #if (NT_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)

        timeout = 0;
        olen = sizeof( int );

        if( getsockopt( ls[i].fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &timeout, &olen )
            == -1 ) {
            err = nt_socket_errno;

            if( err == NT_EOPNOTSUPP ) {
                continue;
            }

            nt_log_error( NT_LOG_NOTICE, cycle->log, err,
                          "getsockopt(TCP_DEFER_ACCEPT) for %V failed, ignored",
                          &ls[i].addr_text );
            continue;
        }

        if( olen < sizeof( int ) || timeout == 0 ) {
            continue;
        }

        ls[i].deferred_accept = 1;
        #endif
    }

    return NT_OK;
}


nt_int_t
nt_open_listening_sockets( nt_cycle_t *cycle )
{
    int               reuseaddr;
    nt_uint_t        i, tries, failed;
    nt_err_t         err;
    nt_log_t        *log;
    nt_socket_t      s;
    nt_listening_t  *ls;

    reuseaddr = 1;
    #if (NT_SUPPRESS_WARN)
    failed = 0;
    #endif

    log = cycle->log;

    /* TODO: configurable try number */
    //建立socket监听会有5次重试
    for( tries = 5; tries; tries-- ) {
        failed = 0;

        /* for each listening socket */

        ls = cycle->listening.elts;
        for( i = 0; i < cycle->listening.nelts; i++ ) {

            if( ls[i].ignore ) {
                continue;
            }


            #if (NT_HAVE_REUSEPORT)

            if( ls[i].add_reuseport ) {

                /*
                 * to allow transition from a socket without SO_REUSEPORT
                 * to multiple sockets with SO_REUSEPORT, we have to set
                 * SO_REUSEPORT on the old socket before opening new ones
                 */

                int  reuseport = 1;

                #ifdef SO_REUSEPORT_LB

                if( setsockopt( ls[i].fd, SOL_SOCKET, SO_REUSEPORT_LB,
                                ( const void * ) &reuseport, sizeof( int ) )
                    == -1 ) {
                    nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                                  "setsockopt(SO_REUSEPORT_LB) %V failed, "
                                  "ignored",
                                  &ls[i].addr_text );
                }

                #else

                if( setsockopt( ls[i].fd, SOL_SOCKET, SO_REUSEPORT,
                                ( const void * ) &reuseport, sizeof( int ) )
                    == -1 ) {
                    nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                                  "setsockopt(SO_REUSEPORT) %V failed, ignored",
                                  &ls[i].addr_text );
                }
                #endif

                ls[i].add_reuseport = 0;
            }
            #endif

            //判断fd 是否已经启动了。
            if( ls[i].fd != ( nt_socket_t ) -1 ) {
                continue;
            }

            if( ls[i].inherited ) {

                /* TODO: close on exit */
                /* TODO: nonblocking */
                /* TODO: deferred accept */

                continue;
            }

            //启动为一个tun类型的服务
#if (NT_HAVE_ACC_RCV)
            if( ls[i].type == NT_AF_TUN ){
                nt_open_tun_socket( ls ); 
                continue;
            }
#endif

            debug( "ls[i].type=%d", ls[i].type );
            s = nt_socket( ls[i].sockaddr->sa_family, ls[i].type, 0 );

            if( s == ( nt_socket_t ) -1 ) {
                nt_log_error( NT_LOG_EMERG, log, nt_socket_errno,
                              nt_socket_n " %V failed", &ls[i].addr_text );
                return NT_ERROR;
            }

            if( setsockopt( s, SOL_SOCKET, SO_REUSEADDR,
                            ( const void * ) &reuseaddr, sizeof( int ) )
                == -1 ) {
                nt_log_error( NT_LOG_EMERG, log, nt_socket_errno,
                              "setsockopt(SO_REUSEADDR) %V failed",
                              &ls[i].addr_text );

                if( nt_close_socket( s ) == -1 ) {
                    nt_log_error( NT_LOG_EMERG, log, nt_socket_errno,
                                  nt_close_socket_n " %V failed",
                                  &ls[i].addr_text );
                }

                return NT_ERROR;
            }

            #if (NT_HAVE_REUSEPORT)

            if( ls[i].reuseport && !nt_test_config ) {
                int  reuseport;

                reuseport = 1;

                #ifdef SO_REUSEPORT_LB

                if( setsockopt( s, SOL_SOCKET, SO_REUSEPORT_LB,
                                ( const void * ) &reuseport, sizeof( int ) )
                    == -1 ) {
                    nt_log_error( NT_LOG_EMERG, log, nt_socket_errno,
                                  "setsockopt(SO_REUSEPORT_LB) %V failed",
                                  &ls[i].addr_text );

                    if( nt_close_socket( s ) == -1 ) {
                        nt_log_error( NT_LOG_EMERG, log, nt_socket_errno,
                                      nt_close_socket_n " %V failed",
                                      &ls[i].addr_text );
                    }

                    return NT_ERROR;
                }

                #else

                if( setsockopt( s, SOL_SOCKET, SO_REUSEPORT,
                                ( const void * ) &reuseport, sizeof( int ) )
                    == -1 ) {
                    nt_log_error( NT_LOG_EMERG, log, nt_socket_errno,
                                  "setsockopt(SO_REUSEPORT) %V failed",
                                  &ls[i].addr_text );

                    if( nt_close_socket( s ) == -1 ) {
                        nt_log_error( NT_LOG_EMERG, log, nt_socket_errno,
                                      nt_close_socket_n " %V failed",
                                      &ls[i].addr_text );
                    }

                    return NT_ERROR;
                }
                #endif
            }
            #endif

            #if (NT_HAVE_INET6 && defined IPV6_V6ONLY)

            if( ls[i].sockaddr->sa_family == AF_INET6 ) {
                int  ipv6only;

                ipv6only = ls[i].ipv6only;

                if( setsockopt( s, IPPROTO_IPV6, IPV6_V6ONLY,
                                ( const void * ) &ipv6only, sizeof( int ) )
                    == -1 ) {
                    nt_log_error( NT_LOG_EMERG, log, nt_socket_errno,
                                  "setsockopt(IPV6_V6ONLY) %V failed, ignored",
                                  &ls[i].addr_text );
                }
            }
            #endif
            /* TODO: close on exit */

            if( !( nt_event_flags & NT_USE_IOCP_EVENT ) ) {
                if( nt_nonblocking( s ) == -1 ) {
                    nt_log_error( NT_LOG_EMERG, log, nt_socket_errno,
                                  nt_nonblocking_n " %V failed",
                                  &ls[i].addr_text );

                    if( nt_close_socket( s ) == -1 ) {
                        nt_log_error( NT_LOG_EMERG, log, nt_socket_errno,
                                      nt_close_socket_n " %V failed",
                                      &ls[i].addr_text );
                    }

                    return NT_ERROR;
                }
            }

            nt_log_debug2( NT_LOG_DEBUG_CORE, log, 0,
                           "bind() %V #%d ", &ls[i].addr_text, s );



            //判断当前的文件是否为本地套接字，如果是，就删除
            struct sockaddr a;
            if( ls[i].sockaddr->sa_family == AF_UNIX ) {

                struct sockaddr_un *su;

                su = ( struct sockaddr_un * ) ls[i].sockaddr;

                if( access( su->sun_path, F_OK ) == 0 ) {

                    debug( "文件存在sockaddr = %s", su->sun_path );
                    unlink( su->sun_path );
                }  else
                    debug( "文件不存在sockaddr = %s", su->sun_path );
            }



            if( bind( s, ls[i].sockaddr, ls[i].socklen ) == -1 ) {
                err = nt_socket_errno;

                if( err != NT_EADDRINUSE || !nt_test_config ) {
                    nt_log_error( NT_LOG_EMERG, log, err,
                                  "bind() to %V failed", &ls[i].addr_text );
                }

                if( nt_close_socket( s ) == -1 ) {
                    nt_log_error( NT_LOG_EMERG, log, nt_socket_errno,
                                  nt_close_socket_n " %V failed",
                                  &ls[i].addr_text );
                }

                if( err != NT_EADDRINUSE ) {
                    return NT_ERROR;
                }

                if( !nt_test_config ) {
                    failed = 1;
                }

                continue;
            }

            #if (NT_HAVE_UNIX_DOMAIN)

            if( ls[i].sockaddr->sa_family == AF_UNIX ) {
                mode_t   mode;
                u_char  *name;

                name = ls[i].addr_text.data + sizeof( "unix:" ) - 1;
                mode = ( S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

                if( chmod( ( char * ) name, mode ) == -1 ) {
                    nt_log_error( NT_LOG_EMERG, cycle->log, nt_errno,
                                  "chmod() \"%s\" failed", name );
                }

                if( nt_test_config ) {
                    if( nt_delete_file( name ) == NT_FILE_ERROR ) {
                        nt_log_error( NT_LOG_EMERG, cycle->log, nt_errno,
                                      nt_delete_file_n " %s failed", name );
                    }
                }
            }
            #endif

            if( ls[i].type != SOCK_STREAM ) {
                ls[i].fd = s;
                continue;
            }

            if( listen( s, ls[i].backlog ) == -1 ) {
                err = nt_socket_errno;

                /*
                 * on OpenVZ after suspend/resume EADDRINUSE
                 * may be returned by listen() instead of bind(), see
                 * https://bugzilla.openvz.org/show_bug.cgi?id=2470
                 */

                if( err != NT_EADDRINUSE || !nt_test_config ) {
                    nt_log_error( NT_LOG_EMERG, log, err,
                                  "listen() to %V, backlog %d failed",
                                  &ls[i].addr_text, ls[i].backlog );
                }

                if( nt_close_socket( s ) == -1 ) {
                    nt_log_error( NT_LOG_EMERG, log, nt_socket_errno,
                                  nt_close_socket_n " %V failed",
                                  &ls[i].addr_text );
                }

                if( err != NT_EADDRINUSE ) {
                    return NT_ERROR;
                }

                if( !nt_test_config ) {
                    failed = 1;
                }

                continue;
            }

            ls[i].listen = 1;

            ls[i].fd = s;
        }

        if( !failed ) {
            break;
        }

        /* TODO: delay configurable */

        nt_log_error( NT_LOG_NOTICE, log, 0,
                      "try again to bind() after 500ms" );

        nt_msleep( 500 );
    }

    if( failed ) {
        nt_log_error( NT_LOG_EMERG, log, 0, "still could not bind()" );
        return NT_ERROR;
    }

    return NT_OK;
}


void
nt_configure_listening_sockets( nt_cycle_t *cycle )
{
    int                        value;
    nt_uint_t                 i;
    nt_listening_t           *ls;

    #if (NT_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    struct accept_filter_arg   af;
    #endif

    ls = cycle->listening.elts;
    for( i = 0; i < cycle->listening.nelts; i++ ) {

        ls[i].log = *ls[i].logp;

        if( ls[i].rcvbuf != -1 ) {
            if( setsockopt( ls[i].fd, SOL_SOCKET, SO_RCVBUF,
                            ( const void * ) &ls[i].rcvbuf, sizeof( int ) )
                == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "setsockopt(SO_RCVBUF, %d) %V failed, ignored",
                              ls[i].rcvbuf, &ls[i].addr_text );
            }
        }

        if( ls[i].sndbuf != -1 ) {
            if( setsockopt( ls[i].fd, SOL_SOCKET, SO_SNDBUF,
                            ( const void * ) &ls[i].sndbuf, sizeof( int ) )
                == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "setsockopt(SO_SNDBUF, %d) %V failed, ignored",
                              ls[i].sndbuf, &ls[i].addr_text );
            }
        }

        if( ls[i].keepalive ) {
            value = ( ls[i].keepalive == 1 ) ? 1 : 0;

            if( setsockopt( ls[i].fd, SOL_SOCKET, SO_KEEPALIVE,
                            ( const void * ) &value, sizeof( int ) )
                == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "setsockopt(SO_KEEPALIVE, %d) %V failed, ignored",
                              value, &ls[i].addr_text );
            }
        }

        #if (NT_HAVE_KEEPALIVE_TUNABLE)

        if( ls[i].keepidle ) {
            value = ls[i].keepidle;

            #if (NT_KEEPALIVE_FACTOR)
            value *= NT_KEEPALIVE_FACTOR;
            #endif

            if( setsockopt( ls[i].fd, IPPROTO_TCP, TCP_KEEPIDLE,
                            ( const void * ) &value, sizeof( int ) )
                == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "setsockopt(TCP_KEEPIDLE, %d) %V failed, ignored",
                              value, &ls[i].addr_text );
            }
        }

        if( ls[i].keepintvl ) {
            value = ls[i].keepintvl;

            #if (NT_KEEPALIVE_FACTOR)
            value *= NT_KEEPALIVE_FACTOR;
            #endif

            if( setsockopt( ls[i].fd, IPPROTO_TCP, TCP_KEEPINTVL,
                            ( const void * ) &value, sizeof( int ) )
                == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "setsockopt(TCP_KEEPINTVL, %d) %V failed, ignored",
                              value, &ls[i].addr_text );
            }
        }

        if( ls[i].keepcnt ) {
            if( setsockopt( ls[i].fd, IPPROTO_TCP, TCP_KEEPCNT,
                            ( const void * ) &ls[i].keepcnt, sizeof( int ) )
                == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "setsockopt(TCP_KEEPCNT, %d) %V failed, ignored",
                              ls[i].keepcnt, &ls[i].addr_text );
            }
        }

        #endif

        #if (NT_HAVE_SETFIB)
        if( ls[i].setfib != -1 ) {
            if( setsockopt( ls[i].fd, SOL_SOCKET, SO_SETFIB,
                            ( const void * ) &ls[i].setfib, sizeof( int ) )
                == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "setsockopt(SO_SETFIB, %d) %V failed, ignored",
                              ls[i].setfib, &ls[i].addr_text );
            }
        }
        #endif

        #if (NT_HAVE_TCP_FASTOPEN)
        if( ls[i].fastopen != -1 ) {
            if( setsockopt( ls[i].fd, IPPROTO_TCP, TCP_FASTOPEN,
                            ( const void * ) &ls[i].fastopen, sizeof( int ) )
                == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "setsockopt(TCP_FASTOPEN, %d) %V failed, ignored",
                              ls[i].fastopen, &ls[i].addr_text );
            }
        }
        #endif

        #if 0
        if( 1 ) {
            int tcp_nodelay = 1;

            if( setsockopt( ls[i].fd, IPPROTO_TCP, TCP_NODELAY,
                            ( const void * ) &tcp_nodelay, sizeof( int ) )
                == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "setsockopt(TCP_NODELAY) %V failed, ignored",
                              &ls[i].addr_text );
            }
        }
        #endif

        if( ls[i].listen ) {

            /* change backlog via listen() */

            if( listen( ls[i].fd, ls[i].backlog ) == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "listen() to %V, backlog %d failed, ignored",
                              &ls[i].addr_text, ls[i].backlog );
            }
        }

        /*
         * setting deferred mode should be last operation on socket,
         * because code may prematurely continue cycle on failure
         */

        #if (NT_HAVE_DEFERRED_ACCEPT)

        #ifdef SO_ACCEPTFILTER

        if( ls[i].delete_deferred ) {
            if( setsockopt( ls[i].fd, SOL_SOCKET, SO_ACCEPTFILTER, NULL, 0 )
                == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "setsockopt(SO_ACCEPTFILTER, NULL) "
                              "for %V failed, ignored",
                              &ls[i].addr_text );

                if( ls[i].accept_filter ) {
                    nt_log_error( NT_LOG_ALERT, cycle->log, 0,
                                  "could not change the accept filter "
                                  "to \"%s\" for %V, ignored",
                                  ls[i].accept_filter, &ls[i].addr_text );
                }

                continue;
            }

            ls[i].deferred_accept = 0;
        }

        if( ls[i].add_deferred ) {
            nt_memzero( &af, sizeof( struct accept_filter_arg ) );
            ( void ) nt_cpystrn( ( u_char * ) af.af_name,
                                 ( u_char * ) ls[i].accept_filter, 16 );

            if( setsockopt( ls[i].fd, SOL_SOCKET, SO_ACCEPTFILTER,
                            &af, sizeof( struct accept_filter_arg ) )
                == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "setsockopt(SO_ACCEPTFILTER, \"%s\") "
                              "for %V failed, ignored",
                              ls[i].accept_filter, &ls[i].addr_text );
                continue;
            }

            ls[i].deferred_accept = 1;
        }

        #endif

        #ifdef TCP_DEFER_ACCEPT

        if( ls[i].add_deferred || ls[i].delete_deferred ) {

            if( ls[i].add_deferred ) {
                /*
                 * There is no way to find out how long a connection was
                 * in queue (and a connection may bypass deferred queue at all
                 * if syncookies were used), hence we use 1 second timeout
                 * here.
                 */
                value = 1;

            } else {
                value = 0;
            }

            if( setsockopt( ls[i].fd, IPPROTO_TCP, TCP_DEFER_ACCEPT,
                            &value, sizeof( int ) )
                == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "setsockopt(TCP_DEFER_ACCEPT, %d) for %V failed, "
                              "ignored",
                              value, &ls[i].addr_text );

                continue;
            }
        }

        if( ls[i].add_deferred ) {
            ls[i].deferred_accept = 1;
        }

        #endif

        #endif /* NT_HAVE_DEFERRED_ACCEPT */

        #if (NT_HAVE_IP_RECVDSTADDR)

        if( ls[i].wildcard
            && ls[i].type == SOCK_DGRAM
            && ls[i].sockaddr->sa_family == AF_INET ) {
            value = 1;

            if( setsockopt( ls[i].fd, IPPROTO_IP, IP_RECVDSTADDR,
                            ( const void * ) &value, sizeof( int ) )
                == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "setsockopt(IP_RECVDSTADDR) "
                              "for %V failed, ignored",
                              &ls[i].addr_text );
            }
        }

        #elif (NT_HAVE_IP_PKTINFO)

        if( ls[i].wildcard
            && ls[i].type == SOCK_DGRAM
            && ls[i].sockaddr->sa_family == AF_INET ) {
            value = 1;

            if( setsockopt( ls[i].fd, IPPROTO_IP, IP_PKTINFO,
                            ( const void * ) &value, sizeof( int ) )
                == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "setsockopt(IP_PKTINFO) "
                              "for %V failed, ignored",
                              &ls[i].addr_text );
            }
        }

        #endif

        #if (NT_HAVE_INET6 && NT_HAVE_IPV6_RECVPKTINFO)

        if( ls[i].wildcard
            && ls[i].type == SOCK_DGRAM
            && ls[i].sockaddr->sa_family == AF_INET6 ) {
            value = 1;

            if( setsockopt( ls[i].fd, IPPROTO_IPV6, IPV6_RECVPKTINFO,
                            ( const void * ) &value, sizeof( int ) )
                == -1 ) {
                nt_log_error( NT_LOG_ALERT, cycle->log, nt_socket_errno,
                              "setsockopt(IPV6_RECVPKTINFO) "
                              "for %V failed, ignored",
                              &ls[i].addr_text );
            }
        }

        #endif
    }

    return;
}


void
nt_close_listening_sockets( nt_cycle_t *cycle )
{
    nt_uint_t         i;
    nt_listening_t   *ls;
    nt_connection_t  *c;

    if( nt_event_flags & NT_USE_IOCP_EVENT ) {
        return;
    }

    nt_accept_mutex_held = 0;
    nt_use_accept_mutex = 0;

    ls = cycle->listening.elts;
    for( i = 0; i < cycle->listening.nelts; i++ ) {

        c = ls[i].connection;

        if( c ) {
            if( c->read->active ) {
                if( nt_event_flags & NT_USE_EPOLL_EVENT ) {

                    /*
                     * it seems that Linux-2.6.x OpenVZ sends events
                     * for closed shared listening sockets unless
                     * the events was explicitly deleted
                     */
                    #if (NT_SSL && NT_SSL_ASYNC)
                    if( c->async_enable && nt_del_async_conn ) {
                        if( c->num_async_fds ) {
                            nt_del_async_conn( c, NT_DISABLE_EVENT );
                            c->num_async_fds--;
                        }
                    }
                    #endif

                    nt_del_event( c->read, NT_READ_EVENT, 0 );

                } else {
                    nt_del_event( c->read, NT_READ_EVENT, NT_CLOSE_EVENT );
                }
            }

            nt_free_connection( c );

            c->fd = ( nt_socket_t ) -1;
        }

        nt_log_debug2( NT_LOG_DEBUG_CORE, cycle->log, 0,
                       "close listening %V #%d ", &ls[i].addr_text, ls[i].fd );

        if( nt_close_socket( ls[i].fd ) == -1 ) {
            nt_log_error( NT_LOG_EMERG, cycle->log, nt_socket_errno,
                          nt_close_socket_n " %V failed", &ls[i].addr_text );
        }

        #if (NT_HAVE_UNIX_DOMAIN)

        if( ls[i].sockaddr->sa_family == AF_UNIX
            && nt_process <= NT_PROCESS_MASTER
            && nt_new_binary == 0 ) {
            u_char *name = ls[i].addr_text.data + sizeof( "unix:" ) - 1;

            if( nt_delete_file( name ) == NT_FILE_ERROR ) {
                nt_log_error( NT_LOG_EMERG, cycle->log, nt_socket_errno,
                              nt_delete_file_n " %s failed", name );
            }
        }

        #endif

        ls[i].fd = ( nt_socket_t ) -1;
    }

    cycle->listening.nelts = 0;
}


nt_connection_t *
nt_get_connection( nt_socket_t s, nt_log_t *log )
{
    nt_uint_t         instance;
    nt_event_t       *rev, *wev;
    #if (NT_SSL && NT_SSL_ASYNC)
    nt_event_t       *aev;
    #endif
    nt_connection_t  *c;

    /* disable warning: Win32 SOCKET is u_int while UNIX socket is int */

    if( nt_cycle->files && ( nt_uint_t ) s >= nt_cycle->files_n ) {
        nt_log_error( NT_LOG_ALERT, log, 0,
                      "the new socket has number %d, "
                      "but only %ui files are available",
                      s, nt_cycle->files_n );
        return NULL;
    }

    c = nt_cycle->free_connections;

    if( c == NULL ) {
        nt_drain_connections( ( nt_cycle_t * ) nt_cycle );
        c = nt_cycle->free_connections;
    }

    if( c == NULL ) {
        nt_log_error( NT_LOG_ALERT, log, 0,
                      "%ui worker_connections are not enough",
                      nt_cycle->connection_n );

        return NULL;
    }

    nt_cycle->free_connections = c->data;
    nt_cycle->free_connection_n--;

    if( nt_cycle->files && nt_cycle->files[s] == NULL ) {
        nt_cycle->files[s] = c;
    }

    rev = c->read;
    wev = c->write;
    #if (NT_SSL && NT_SSL_ASYNC)
    aev = c->async;
    #endif

    nt_memzero( c, sizeof( nt_connection_t ) );

    c->read = rev;
    c->write = wev;
    #if (NT_SSL && NT_SSL_ASYNC)
    c->async = aev;
    #endif

    c->fd = s;
    c->log = log;

    instance = rev->instance;

    nt_memzero( rev, sizeof( nt_event_t ) );
    nt_memzero( wev, sizeof( nt_event_t ) );
    #if (NT_SSL && NT_SSL_ASYNC)
    nt_memzero( aev, sizeof( nt_event_t ) );
    #endif

    rev->instance = !instance;
    wev->instance = !instance;
    #if (NT_SSL && NT_SSL_ASYNC)
    aev->instance = !instance;
    #endif

    rev->index = NT_INVALID_INDEX;
    wev->index = NT_INVALID_INDEX;
    #if (NT_SSL && NT_SSL_ASYNC)
    aev->index = NT_INVALID_INDEX;
    #endif

    rev->data = c;
    wev->data = c;
    #if (NT_SSL && NT_SSL_ASYNC)
    aev->data = c;
    #endif

    wev->write = 1;
    #if (NT_SSL && NT_SSL_ASYNC)
    aev->async = 1;
    #endif

    return c;
}


void
nt_free_connection( nt_connection_t *c )
{
    c->data = nt_cycle->free_connections;
    nt_cycle->free_connections = c;
    nt_cycle->free_connection_n++;

    if( nt_cycle->files && nt_cycle->files[c->fd] == c ) {
        nt_cycle->files[c->fd] = NULL;
    }
}


void
nt_close_connection( nt_connection_t *c )
{
    nt_err_t     err;
    nt_uint_t    log_error, level;
    nt_socket_t  fd;

    if( c->fd == ( nt_socket_t ) -1 ) {
        nt_log_error( NT_LOG_ALERT, c->log, 0, "connection already closed" );
        return;
    }

    if( c->read->timer_set ) {
        nt_del_timer( c->read );
    }

    if( c->write->timer_set ) {
        nt_del_timer( c->write );
    }

    #if (NT_SSL && NT_SSL_ASYNC)
    if( c->async->timer_set ) {
        nt_del_timer( c->async );
    }

    if( c->async_enable && nt_del_async_conn ) {
        if( c->num_async_fds ) {
            nt_del_async_conn( c, NT_DISABLE_EVENT );
            c->num_async_fds--;
        }
    }
    #endif

    if( !c->shared ) {
        if( nt_del_conn ) {
            nt_del_conn( c, NT_CLOSE_EVENT );

        } else {
            #if (NT_SSL && NT_SSL_ASYNC)
            if( c->async_enable && nt_del_async_conn ) {
                if( c->num_async_fds ) {
                    nt_del_async_conn( c, NT_DISABLE_EVENT );
                    c->num_async_fds--;
                }
            }
            #endif
            if( c->read->active || c->read->disabled ) {
                nt_del_event( c->read, NT_READ_EVENT, NT_CLOSE_EVENT );
            }

            if( c->write->active || c->write->disabled ) {
                nt_del_event( c->write, NT_WRITE_EVENT, NT_CLOSE_EVENT );
            }
        }
    }

    if( c->read->posted ) {
        nt_delete_posted_event( c->read );
    }

    if( c->write->posted ) {
        nt_delete_posted_event( c->write );
    }

    #if (NT_SSL && NT_SSL_ASYNC)
    if( c->async->posted ) {
        nt_delete_posted_event( c->async );
    }
    #endif

    c->read->closed = 1;
    c->write->closed = 1;
    #if (NT_SSL && NT_SSL_ASYNC)
    c->async->closed = 1;
    #endif

    nt_reusable_connection( c, 0 );

    log_error = c->log_error;

    nt_free_connection( c );

    fd = c->fd;
    c->fd = ( nt_socket_t ) -1;
    #if (NT_SSL && NT_SSL_ASYNC)
    c->async_fd = ( nt_socket_t ) -1;
    #endif

    if( c->shared ) {
        return;
    }

    if( nt_close_socket( fd ) == -1 ) {

        err = nt_socket_errno;

        if( err == NT_ECONNRESET || err == NT_ENOTCONN ) {

            switch( log_error ) {

            case NT_ERROR_INFO:
                level = NT_LOG_INFO;
                break;

            case NT_ERROR_ERR:
                level = NT_LOG_ERR;
                break;

            default:
                level = NT_LOG_CRIT;
            }

        } else {
            level = NT_LOG_CRIT;
        }

        nt_log_error( level, c->log, err, nt_close_socket_n " %d failed", fd );
    }
}


void
nt_reusable_connection( nt_connection_t *c, nt_uint_t reusable )
{
    nt_log_debug1( NT_LOG_DEBUG_CORE, c->log, 0,
                   "reusable connection: %ui", reusable );

    if( c->reusable ) {
        nt_queue_remove( &c->queue );
        nt_cycle->reusable_connections_n--;

        #if (NT_STAT_STUB)
        ( void ) nt_atomic_fetch_add( nt_stat_waiting, -1 );
        #endif
    }

    c->reusable = reusable;

    if( reusable ) {
        /* need cast as nt_cycle is volatile */

        nt_queue_insert_head(
            ( nt_queue_t * ) &nt_cycle->reusable_connections_queue, &c->queue );
        nt_cycle->reusable_connections_n++;

        #if (NT_STAT_STUB)
        ( void ) nt_atomic_fetch_add( nt_stat_waiting, 1 );
        #endif
    }
}


static void
nt_drain_connections( nt_cycle_t *cycle )
{
    nt_uint_t         i, n;
    nt_queue_t       *q;
    nt_connection_t  *c;

    n = nt_max( nt_min( 32, cycle->reusable_connections_n / 8 ), 1 );

    for( i = 0; i < n; i++ ) {
        if( nt_queue_empty( &cycle->reusable_connections_queue ) ) {
            break;
        }

        q = nt_queue_last( &cycle->reusable_connections_queue );
        c = nt_queue_data( q, nt_connection_t, queue );

        nt_log_debug0( NT_LOG_DEBUG_CORE, c->log, 0,
                       "reusing connection" );

        c->close = 1;
        c->read->handler( c->read );
    }
}


void
nt_close_idle_connections( nt_cycle_t *cycle )
{
    nt_uint_t         i;
    nt_connection_t  *c;

    c = cycle->connections;

    for( i = 0; i < cycle->connection_n; i++ ) {

        /* THREAD: lock */

        if( c[i].fd != ( nt_socket_t ) -1 && c[i].idle ) {
            c[i].close = 1;
            c[i].read->handler( c[i].read );
        }
    }
}


nt_int_t
nt_connection_local_sockaddr( nt_connection_t *c, nt_str_t *s,
                              nt_uint_t port )
{
    socklen_t             len;
    nt_uint_t            addr;
    nt_sockaddr_t        sa;
    struct sockaddr_in   *sin;
    #if (NT_HAVE_INET6)
    nt_uint_t            i;
    struct sockaddr_in6  *sin6;
    #endif

    addr = 0;

    if( c->local_socklen ) {
        switch( c->local_sockaddr->sa_family ) {

            #if (NT_HAVE_INET6)
        case AF_INET6:
            sin6 = ( struct sockaddr_in6 * ) c->local_sockaddr;

            for( i = 0; addr == 0 && i < 16; i++ ) {
                addr |= sin6->sin6_addr.s6_addr[i];
            }

            break;
            #endif

            #if (NT_HAVE_UNIX_DOMAIN)
        case AF_UNIX:
            addr = 1;
            break;
            #endif

        default: /* AF_INET */
            sin = ( struct sockaddr_in * ) c->local_sockaddr;
            addr = sin->sin_addr.s_addr;
            break;
        }
    }

    if( addr == 0 ) {

        len = sizeof( nt_sockaddr_t );

        if( getsockname( c->fd, &sa.sockaddr, &len ) == -1 ) {
            nt_connection_error( c, nt_socket_errno, "getsockname() failed" );
            return NT_ERROR;
        }

        c->local_sockaddr = nt_palloc( c->pool, len );
        if( c->local_sockaddr == NULL ) {
            return NT_ERROR;
        }

        nt_memcpy( c->local_sockaddr, &sa, len );

        c->local_socklen = len;
    }

    if( s == NULL ) {
        return NT_OK;
    }

    s->len = nt_sock_ntop( c->local_sockaddr, c->local_socklen,
                           s->data, s->len, port );

    return NT_OK;
}


nt_int_t
nt_tcp_nodelay( nt_connection_t *c )
{
    int  tcp_nodelay;

    if( c->tcp_nodelay != NT_TCP_NODELAY_UNSET ) {
        return NT_OK;
    }

    nt_log_debug0( NT_LOG_DEBUG_CORE, c->log, 0, "tcp_nodelay" );

    tcp_nodelay = 1;

    if( setsockopt( c->fd, IPPROTO_TCP, TCP_NODELAY,
                    ( const void * ) &tcp_nodelay, sizeof( int ) )
        == -1 ) {
        #if (NT_SOLARIS)
        if( c->log_error == NT_ERROR_INFO ) {

            /* Solaris returns EINVAL if a socket has been shut down */
            c->log_error = NT_ERROR_IGNORE_EINVAL;

            nt_connection_error( c, nt_socket_errno,
                                 "setsockopt(TCP_NODELAY) failed" );

            c->log_error = NT_ERROR_INFO;

            return NT_ERROR;
        }
        #endif

        nt_connection_error( c, nt_socket_errno,
                             "setsockopt(TCP_NODELAY) failed" );
        return NT_ERROR;
    }

    c->tcp_nodelay = NT_TCP_NODELAY_SET;

    return NT_OK;
}


nt_int_t
nt_connection_error( nt_connection_t *c, nt_err_t err, char *text )
{
    nt_uint_t  level;

    /* Winsock may return NT_ECONNABORTED instead of NT_ECONNRESET */

    if( ( err == NT_ECONNRESET
      #if (NT_WIN32)
          || err == NT_ECONNABORTED
      #endif
        ) && c->log_error == NT_ERROR_IGNORE_ECONNRESET ) {
        return 0;

    }

    if( err == 0
        || err == NT_ECONNRESET
    #if (NT_WIN32)
        || err == NT_ECONNABORTED
    #else
        || err == NT_EPIPE
    #endif
        || err == NT_ENOTCONN
        || err == NT_ETIMEDOUT
        || err == NT_ECONNREFUSED
        || err == NT_ENETDOWN
        || err == NT_ENETUNREACH
        || err == NT_EHOSTDOWN
        || err == NT_EHOSTUNREACH ) {
        switch( c->log_error ) {

        case NT_ERROR_IGNORE_EINVAL:
        case NT_ERROR_IGNORE_ECONNRESET:
        case NT_ERROR_INFO:
            level = NT_LOG_INFO;
            break;

        default:
            level = NT_LOG_ERR;

        }


    } else {
        level = NT_LOG_ALERT;

    }

    nt_log_error( level, c->log, err, text );

    return NT_ERROR;
}
