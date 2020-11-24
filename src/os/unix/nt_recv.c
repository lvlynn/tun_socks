
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <nt_def.h>
#include <nt_core.h>
#include <nt_event.h>

/*
 * recv 接收
 *  
 * */
ssize_t
nt_unix_recv(nt_connection_t *c, u_char *buf, size_t size)
{
    ssize_t       n;
    nt_err_t     err;
    nt_event_t  *rev;

    rev = c->read;

#if (NT_HAVE_EPOLLRDHUP)
    if (nt_event_flags & NT_USE_EPOLL_EVENT) {
        nt_log_debug2(NT_LOG_DEBUG_EVENT, c->log, 0,
                       "recv: eof:%d, avail:%d",
                       rev->pending_eof, rev->available);

        if (!rev->available && !rev->pending_eof) {
            rev->ready = 0;
            return NT_AGAIN;
        }
    }
#endif

    do {
        n = recv(c->fd, buf, size, 0);

        nt_log_debug3(NT_LOG_DEBUG_EVENT, c->log, 0,
                       "recv: fd:%d %z of %uz", c->fd, n, size);

        if (n == 0) {
            rev->ready = 0;
            rev->eof = 1;
            return 0;
        }

        if (n > 0) {
#if (NT_HAVE_EPOLLRDHUP)
            if ((nt_event_flags & NT_USE_EPOLL_EVENT)
                && nt_use_epoll_rdhup)
            {
                if ((size_t) n < size) {
                    if (!rev->pending_eof) {
                        rev->ready = 0;
                    }

                    rev->available = 0;
                }

                return n;
            }
#endif

            if ((size_t) n < size
                && !(nt_event_flags & NT_USE_GREEDY_EVENT))
            {
                rev->ready = 0;
            }

            return n;
        }

        err = nt_socket_errno;

        if (err == NT_EAGAIN || err == NT_EINTR) {
            nt_log_debug0(NT_LOG_DEBUG_EVENT, c->log, err,
                           "recv() not ready");
            n = NT_AGAIN;

        } else {
            n = nt_connection_error(c, err, "recv() failed");
            break;
        }

    } while (err == NT_EINTR);

    rev->ready = 0;

    if (n == NT_ERROR) {
        rev->error = 1;
    }

    return n;
}
