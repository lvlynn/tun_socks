
#include <nt_core.h>
#include <nt_event.h>


static void nt_close_posted_connection(nt_connection_t *c);

static nt_int_t nt_disable_accept_events(nt_cycle_t *cycle, nt_uint_t all);
#if (!T_NT_ACCEPT_FILTER)
static void nt_close_accepted_connection(nt_connection_t *c);
#endif


void nt_event_no_accept(nt_event_t *ev){

    nt_event_t       *rev, *wev;
    nt_connection_t *c, *lc;
    nt_listening_t   *ls;
    lc = ev->data;
    ls = lc->listening;
    nt_log_debug2(NT_LOG_DEBUG_EVENT, ev->log, 0,
                  "accept fd %d, ready: %d", lc->fd,  lc->read->ready);



    c->pool = nt_create_pool(ls->pool_size, ev->log);

	ev->ready = 0;

	//对新的连接进行初始化， 设置回调
	c->recv = nt_recv;
	c->send = nt_send;
	/* c->recv_chain = nt_recv_chain; */
	/* c->send_chain = nt_send_chain; */

	c->log = ev->log;
	c->pool->log = ev->log;

//	c->socklen = socklen;
	c->listening = ls;
	/* c->local_sockaddr = ls->sockaddr; */
	/* c->local_socklen = ls->socklen; */

//	ls->handler(c);

}



void nt_event_accept(nt_event_t *ev){



}


#if 0
void
nt_event_accept(nt_event_t *ev)
{
    socklen_t          socklen;
    nt_err_t          err;
    nt_log_t         *log;
    nt_uint_t         level;
    nt_socket_t       s;
    nt_event_t       *rev, *wev;
    nt_sockaddr_t     sa;
    nt_listening_t   *ls;
    nt_connection_t  *c, *lc;
    nt_event_conf_t  *ecf;
#if (NT_HAVE_ACCEPT4)
    static nt_uint_t  use_accept4 = 1;
#endif

    if (ev->timedout) {
        if (nt_enable_accept_events((nt_cycle_t *) nt_cycle) != NT_OK) {
            return;
        }

        ev->timedout = 0;
    }

    ecf = nt_event_get_conf(nt_cycle->conf_ctx, nt_event_core_module);

    if (!(nt_event_flags & NT_USE_KQUEUE_EVENT)) {
        ev->available = ecf->multi_accept;
    }

    lc = ev->data;
    ls = lc->listening;
    ev->ready = 0;

    nt_log_debug2(NT_LOG_DEBUG_EVENT, ev->log, 0,
                   "accept on %V, ready: %d", &ls->addr_text, ev->available);

    do {
        socklen = sizeof(nt_sockaddr_t);

        // accept一个新的连接
#if (NT_HAVE_ACCEPT4)
        if (use_accept4) {
            s = accept4(lc->fd, &sa.sockaddr, &socklen, SOCK_NONBLOCK);
        } else {
            s = accept(lc->fd, &sa.sockaddr, &socklen);
        }
#else
        s = accept(lc->fd, &sa.sockaddr, &socklen);
#endif

        if (s == (nt_socket_t) -1) {
            err = nt_socket_errno;

            if (err == NT_EAGAIN) {
                nt_log_debug0(NT_LOG_DEBUG_EVENT, ev->log, err,
                               "accept() not ready");
                return;
            }

            level = NT_LOG_ALERT;

            if (err == NT_ECONNABORTED) {
                level = NT_LOG_ERR;

            } else if (err == NT_EMFILE || err == NT_ENFILE) {
                level = NT_LOG_CRIT;
            }

#if (NT_HAVE_ACCEPT4)
            nt_log_error(level, ev->log, err,
                          use_accept4 ? "accept4() failed" : "accept() failed");

            if (use_accept4 && err == NT_ENOSYS) {
                use_accept4 = 0;
                nt_inherited_nonblocking = 0;
                continue;
            }
#else
            nt_log_error(level, ev->log, err, "accept() failed");
#endif

            if (err == NT_ECONNABORTED) {
                if (nt_event_flags & NT_USE_KQUEUE_EVENT) {
                    ev->available--;
                }

                if (ev->available) {
                    continue;
                }
            }

            if (err == NT_EMFILE || err == NT_ENFILE) {
                if (nt_disable_accept_events((nt_cycle_t *) nt_cycle, 1)
                    != NT_OK)
                {
                    return;
                }

                if (nt_use_accept_mutex) {
                    if (nt_accept_mutex_held) {
                        nt_shmtx_unlock(&nt_accept_mutex);
                        nt_accept_mutex_held = 0;
                    }

                    nt_accept_disabled = 1;

                } else {
                    nt_add_timer(ev, ecf->accept_mutex_delay);
                }
            }

            return;
        }

#if (NT_STAT_STUB)
        (void) nt_atomic_fetch_add(nt_stat_accepted, 1);
#endif


        /*  accept到一个新的连接后，就重新计算nt_accept_disabled的值， 
         *  它主要是用来做负载均衡，之前有提过。 
         * 
         *  这里，我们可以看到他的计算方式 
         *  “总连接数的八分之一   减去   剩余的连接数“ 
         *  总连接指每个进程设定的最大连接数，这个数字可以再配置文件中指定。 
         *  所以每个进程到总连接数的7/8后，nt_accept_disabled就大于零，连接超载了  
         */
        nt_accept_disabled = nt_cycle->connection_n / 8
                              - nt_cycle->free_connection_n;

         //获取一个connection
        c = nt_get_connection(s, ev->log);

        if (c == NULL) {
            if (nt_close_socket(s) == -1) {
                nt_log_error(NT_LOG_ALERT, ev->log, nt_socket_errno,
                              nt_close_socket_n " failed");
            }

            return;
        }

        c->type = SOCK_STREAM;

#if (NT_STAT_STUB)
        (void) nt_atomic_fetch_add(nt_stat_active, 1);
#endif

        //为新的链接创建起一个memory pool  
        //连接关闭的时候，才释放pool  
        c->pool = nt_create_pool(ls->pool_size, ev->log);
        if (c->pool == NULL) {
            nt_close_accepted_connection(c);
            return;
        }

        if (socklen > (socklen_t) sizeof(nt_sockaddr_t)) {
            socklen = sizeof(nt_sockaddr_t);
        }

        c->sockaddr = nt_palloc(c->pool, socklen);
        if (c->sockaddr == NULL) {
            nt_close_accepted_connection(c);
            return;
        }

        nt_memcpy(c->sockaddr, &sa, socklen);

        log = nt_palloc(c->pool, sizeof(nt_log_t));
        if (log == NULL) {
            nt_close_accepted_connection(c);
            return;
        }

        /* set a blocking mode for iocp and non-blocking mode for others */

        if (nt_inherited_nonblocking) {
            if (nt_event_flags & NT_USE_IOCP_EVENT) {
                if (nt_blocking(s) == -1) {
                    nt_log_error(NT_LOG_ALERT, ev->log, nt_socket_errno,
                                  nt_blocking_n " failed");
                    nt_close_accepted_connection(c);
                    return;
                }
            }

        } else {
             //我们使用epoll模型，这里我们设置连接为nonblocking  
            if (!(nt_event_flags & NT_USE_IOCP_EVENT)) {
                if (nt_nonblocking(s) == -1) {
                    nt_log_error(NT_LOG_ALERT, ev->log, nt_socket_errno,
                                  nt_nonblocking_n " failed");
                    nt_close_accepted_connection(c);
                    return;
                }
            }
        }

        *log = ls->log;

         //对新的连接进行初始化， 设置回调
        c->recv = nt_recv;
        c->send = nt_send;
//        c->recv_chain = nt_recv_chain;
//        c->send_chain = nt_send_chain;

        c->log = log;
        c->pool->log = log;

        c->socklen = socklen;
        c->listening = ls;
        c->local_sockaddr = ls->sockaddr;
        c->local_socklen = ls->socklen;

#if (NT_HAVE_UNIX_DOMAIN)
        if (c->sockaddr->sa_family == AF_UNIX) {
            c->tcp_nopush = NT_TCP_NOPUSH_DISABLED;
            c->tcp_nodelay = NT_TCP_NODELAY_DISABLED;
#if (NT_SOLARIS)
            /* Solaris's sendfilev() supports AF_NCA, AF_INET, and AF_INET6 */
            c->sendfile = 0;
#endif
        }
#endif

        rev = c->read;
        wev = c->write;

        wev->ready = 1;

        if (nt_event_flags & NT_USE_IOCP_EVENT) {
            rev->ready = 1;
        }

        if (ev->deferred_accept) {
            rev->ready = 1;
#if (NT_HAVE_KQUEUE || NT_HAVE_EPOLLRDHUP)
            rev->available = 1;
#endif
        }

        rev->log = log;
        wev->log = log;
#if (NT_SSL && NT_SSL_ASYNC)
        c->async->log = log;
#endif

        /*
         * TODO: MT: - nt_atomic_fetch_add()
         *             or protection by critical section or light mutex
         *
         * TODO: MP: - allocated in a shared memory
         *           - nt_atomic_fetch_add()
         *             or protection by critical section or light mutex
         */

        c->number = nt_atomic_fetch_add(nt_connection_counter, 1);

#if (NT_STAT_STUB)
        (void) nt_atomic_fetch_add(nt_stat_handled, 1);
#endif

        if (ls->addr_ntop) {
            c->addr_text.data = nt_pnalloc(c->pool, ls->addr_text_max_len);
            if (c->addr_text.data == NULL) {
                nt_close_accepted_connection(c);
                return;
            }

            c->addr_text.len = nt_sock_ntop(c->sockaddr, c->socklen,
                                             c->addr_text.data,
                                             ls->addr_text_max_len, 0);
            if (c->addr_text.len == 0) {
                nt_close_accepted_connection(c);
                return;
            }
        }

#if (NT_DEBUG)
        {
        nt_str_t  addr;
        u_char     text[NT_SOCKADDR_STRLEN];

        nt_debug_accepted_connection(ecf, c);

        if (log->log_level & NT_LOG_DEBUG_EVENT) {
            addr.data = text;
            addr.len = nt_sock_ntop(c->sockaddr, c->socklen, text,
                                     NT_SOCKADDR_STRLEN, 1);

            nt_log_debug3(NT_LOG_DEBUG_EVENT, log, 0,
                           "*%uA accept: %V fd:%d", c->number, &addr, s);
        }

        }
#endif

        if (nt_add_conn && (nt_event_flags & NT_USE_EPOLL_EVENT) == 0) {
            if (nt_add_conn(c) == NT_ERROR) {
                nt_close_accepted_connection(c);
                return;
            }
        }

        log->data = NULL;
        log->handler = NULL;

#if (T_NT_ACCEPT_FILTER)
        /* accept filter */
        nt_int_t          rc;
        rc = nt_event_top_accept_filter(c);
        if (rc == NT_ERROR) {
            nt_close_accepted_connection(c);
            return;
        }
        if (rc == NT_DECLINED) {
            if (nt_event_flags & NT_USE_KQUEUE_EVENT) {
                ev->available--;
            }
            continue;
        }
#endif

        /* 
         *  这里listen handler很重要，它将完成新连接的最后初始化工作， 
         *  同时将accept到的新的连接放入epoll中；挂在这个handler上的函数， 
         *  如果是http socket 调用nt_http_init_connection 
         *  如果是stream socket 调用nt_stream_init_connection
         *  ls->handler在 nt_http(steam)_block 调用
         *  nt_http(stream)_optimize_servers 设置回调
         */  
        ls->handler(c);

        if (nt_event_flags & NT_USE_KQUEUE_EVENT) {
            ev->available--;
        }

    } while (ev->available);
}


nt_int_t
nt_trylock_accept_mutex(nt_cycle_t *cycle)
{
    if (nt_shmtx_trylock(&nt_accept_mutex)) {

        nt_log_debug0(NT_LOG_DEBUG_EVENT, cycle->log, 0,
                       "accept mutex locked");

        if (nt_accept_mutex_held && nt_accept_events == 0) {
            return NT_OK;
        }

        if (nt_enable_accept_events(cycle) == NT_ERROR) {
            nt_shmtx_unlock(&nt_accept_mutex);
            return NT_ERROR;
        }

        nt_accept_events = 0;
        nt_accept_mutex_held = 1;

        return NT_OK;
    }

    nt_log_debug1(NT_LOG_DEBUG_EVENT, cycle->log, 0,
                   "accept mutex lock failed: %ui", nt_accept_mutex_held);

    if (nt_accept_mutex_held) {
        if (nt_disable_accept_events(cycle, 0) == NT_ERROR) {
            return NT_ERROR;
        }

        nt_accept_mutex_held = 0;
    }

    return NT_OK;
}


nt_int_t
nt_enable_accept_events(nt_cycle_t *cycle)
{
    nt_uint_t         i;
    nt_listening_t   *ls;
    nt_connection_t  *c;

    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {

        c = ls[i].connection;

        if (c == NULL || c->read->active) {
            continue;
        }

        if (nt_add_event(c->read, NT_READ_EVENT, 0) == NT_ERROR) {
            return NT_ERROR;
        }
    }

    return NT_OK;
}


static nt_int_t
nt_disable_accept_events(nt_cycle_t *cycle, nt_uint_t all)
{
    nt_uint_t         i;
    nt_listening_t   *ls;
    nt_connection_t  *c;

    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {

        c = ls[i].connection;

        if (c == NULL || !c->read->active) {
            continue;
        }

#if (NT_HAVE_REUSEPORT)

        /*
         * do not disable accept on worker's own sockets
         * when disabling accept events due to accept mutex
         */

        if (ls[i].reuseport && !all) {
            continue;
        }

#endif

#if (NT_SSL && NT_SSL_ASYNC)
        if (c->async_enable && nt_del_async_conn) {
            if (c->num_async_fds) {
                nt_del_async_conn(c, NT_DISABLE_EVENT);
                c->num_async_fds--;
            }
        }
#endif
        if (nt_del_event(c->read, NT_READ_EVENT, NT_DISABLE_EVENT)
            == NT_ERROR)
        {
            return NT_ERROR;
        }
    }

    return NT_OK;
}


#if (!T_NT_ACCEPT_FILTER)
static
#endif
void
nt_close_accepted_connection(nt_connection_t *c)
{
    nt_socket_t  fd;

    nt_free_connection(c);

    fd = c->fd;
    c->fd = (nt_socket_t) -1;

    if (nt_close_socket(fd) == -1) {
        nt_log_error(NT_LOG_ALERT, c->log, nt_socket_errno,
                      nt_close_socket_n " failed");
    }

    if (c->pool) {
        nt_destroy_pool(c->pool);
    }

#if (NT_STAT_STUB)
    (void) nt_atomic_fetch_add(nt_stat_active, -1);
#endif
}


u_char *
nt_accept_log_error(nt_log_t *log, u_char *buf, size_t len)
{
    return nt_snprintf(buf, len, " while accepting new connection on %V",
                        log->data);
}


#if (NT_DEBUG)

void
nt_debug_accepted_connection(nt_event_conf_t *ecf, nt_connection_t *c)
{
    struct sockaddr_in   *sin;
    nt_cidr_t           *cidr;
    nt_uint_t            i;
#if (NT_HAVE_INET6)
    struct sockaddr_in6  *sin6;
    nt_uint_t            n;
#endif

    cidr = ecf->debug_connection.elts;
    for (i = 0; i < ecf->debug_connection.nelts; i++) {
        if (cidr[i].family != (nt_uint_t) c->sockaddr->sa_family) {
            goto next;
        }

        switch (cidr[i].family) {

#if (NT_HAVE_INET6)
        case AF_INET6:
            sin6 = (struct sockaddr_in6 *) c->sockaddr;
            for (n = 0; n < 16; n++) {
                if ((sin6->sin6_addr.s6_addr[n]
                    & cidr[i].u.in6.mask.s6_addr[n])
                    != cidr[i].u.in6.addr.s6_addr[n])
                {
                    goto next;
                }
            }
            break;
#endif

#if (NT_HAVE_UNIX_DOMAIN)
        case AF_UNIX:
            break;
#endif

        default: /* AF_INET */
            sin = (struct sockaddr_in *) c->sockaddr;
            if ((sin->sin_addr.s_addr & cidr[i].u.in.mask)
                != cidr[i].u.in.addr)
            {
                goto next;
            }
            break;
        }

        c->log->log_level = NT_LOG_DEBUG_CONNECTION|NT_LOG_DEBUG_ALL;
        break;

    next:
        continue;
    }
}

#endif


void
nt_event_acceptex(nt_event_t *rev)
{
    nt_listening_t   *ls;
    nt_connection_t  *c;

    c = rev->data;
    ls = c->listening;

    c->log->handler = nt_accept_log_error;

    nt_log_debug1(NT_LOG_DEBUG_EVENT, c->log, 0, "AcceptEx: %d", c->fd);

    if (rev->ovlp.error) {
        nt_log_error(NT_LOG_CRIT, c->log, rev->ovlp.error,
                      "AcceptEx() %V failed", &ls->addr_text);
        return;
    }

    /* SO_UPDATE_ACCEPT_CONTEXT is required for shutdown() to work */

    if (setsockopt(c->fd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                   (char *) &ls->fd, sizeof(nt_socket_t))
        == -1)
    {
        nt_log_error(NT_LOG_CRIT, c->log, nt_socket_errno,
                      "setsockopt(SO_UPDATE_ACCEPT_CONTEXT) failed for %V",
                      &c->addr_text);
        /* TODO: close socket */
        return;
    }

    nt_getacceptexsockaddrs(c->buffer->pos,
                             ls->post_accept_buffer_size,
                             ls->socklen + 16,
                             ls->socklen + 16,
                             &c->local_sockaddr, &c->local_socklen,
                             &c->sockaddr, &c->socklen);

    if (ls->post_accept_buffer_size) {
        c->buffer->last += rev->available;
        c->buffer->end = c->buffer->start + ls->post_accept_buffer_size;

    } else {
        c->buffer = NULL;
    }

    if (ls->addr_ntop) {
        c->addr_text.data = nt_pnalloc(c->pool, ls->addr_text_max_len);
        if (c->addr_text.data == NULL) {
            /* TODO: close socket */
            return;
        }

        c->addr_text.len = nt_sock_ntop(c->sockaddr, c->socklen,
                                         c->addr_text.data,
                                         ls->addr_text_max_len, 0);
        if (c->addr_text.len == 0) {
            /* TODO: close socket */
            return;
        }
    }

    nt_event_post_acceptex(ls, 1);

    c->number = nt_atomic_fetch_add(nt_connection_counter, 1);

    ls->handler(c);

    return;

}


nt_int_t
nt_event_post_acceptex(nt_listening_t *ls, nt_uint_t n)
{
    u_long             rcvd;
    nt_err_t          err;
    nt_log_t         *log;
    nt_uint_t         i;
    nt_event_t       *rev, *wev;
    nt_socket_t       s;
    nt_connection_t  *c;

    for (i = 0; i < n; i++) {

        /* TODO: look up reused sockets */

        s = nt_socket(ls->sockaddr->sa_family, ls->type, 0);

        nt_log_debug1(NT_LOG_DEBUG_EVENT, &ls->log, 0,
                       nt_socket_n " s:%d", s);

        if (s == (nt_socket_t) -1) {
            nt_log_error(NT_LOG_ALERT, &ls->log, nt_socket_errno,
                          nt_socket_n " failed");

            return NT_ERROR;
        }

        c = nt_get_connection(s, &ls->log);

        if (c == NULL) {
            return NT_ERROR;
        }

        c->pool = nt_create_pool(ls->pool_size, &ls->log);
        if (c->pool == NULL) {
            nt_close_posted_connection(c);
            return NT_ERROR;
        }

        log = nt_palloc(c->pool, sizeof(nt_log_t));
        if (log == NULL) {
            nt_close_posted_connection(c);
            return NT_ERROR;
        }

        c->buffer = nt_create_temp_buf(c->pool, ls->post_accept_buffer_size
                                                 + 2 * (ls->socklen + 16));
        if (c->buffer == NULL) {
            nt_close_posted_connection(c);
            return NT_ERROR;
        }

        c->local_sockaddr = nt_palloc(c->pool, ls->socklen);
        if (c->local_sockaddr == NULL) {
            nt_close_posted_connection(c);
            return NT_ERROR;
        }

        c->sockaddr = nt_palloc(c->pool, ls->socklen);
        if (c->sockaddr == NULL) {
            nt_close_posted_connection(c);
            return NT_ERROR;
        }

        *log = ls->log;
        c->log = log;

        c->recv = nt_recv;
        c->send = nt_send;
//        c->recv_chain = nt_recv_chain;
 //       c->send_chain = nt_send_chain;

        c->listening = ls;

        rev = c->read;
        wev = c->write;

        rev->ovlp.event = rev;
        wev->ovlp.event = wev;
        rev->handler = nt_event_acceptex;

        rev->ready = 1;
        wev->ready = 1;

        rev->log = c->log;
        wev->log = c->log;

        if (nt_add_event(rev, 0, NT_IOCP_IO) == NT_ERROR) {
            nt_close_posted_connection(c);
            return NT_ERROR;
        }

        if (nt_acceptex(ls->fd, s, c->buffer->pos, ls->post_accept_buffer_size,
                         ls->socklen + 16, ls->socklen + 16,
                         &rcvd, (LPOVERLAPPED) &rev->ovlp)
            == 0)
        {
            err = nt_socket_errno;
            if (err != WSA_IO_PENDING) {
                nt_log_error(NT_LOG_ALERT, &ls->log, err,
                              "AcceptEx() %V failed", &ls->addr_text);

                nt_close_posted_connection(c);
                return NT_ERROR;
            }
        }
    }

    return NT_OK;
}


static void
nt_close_posted_connection(nt_connection_t *c)
{
    nt_socket_t  fd;

    nt_free_connection(c);

    fd = c->fd;
    c->fd = (nt_socket_t) -1;

    if (nt_close_socket(fd) == -1) {
        nt_log_error(NT_LOG_ALERT, c->log, nt_socket_errno,
                      nt_close_socket_n " failed");
    }

    if (c->pool) {
        nt_destroy_pool(c->pool);
    }
}


u_char *
nt_acceptex_log_error(nt_log_t *log, u_char *buf, size_t len)
{
    return nt_snprintf(buf, len, " while posting AcceptEx() on %V", log->data);
}
#endif
