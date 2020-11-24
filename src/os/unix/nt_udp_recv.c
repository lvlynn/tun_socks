
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <nt_def.h>
#include <nt_core.h>
#include <nt_event.h>


ssize_t
nt_udp_unix_recv(nt_connection_t *c, u_char *buf, size_t size)
{
    ssize_t       n;
    nt_err_t     err;
    nt_event_t  *rev;

    rev = c->read;

    do {
        n = recv(c->fd, buf, size, 0);

        nt_log_debug3(NT_LOG_DEBUG_EVENT, c->log, 0,
                       "recv: fd:%d %z of %uz", c->fd, n, size);

        if (n >= 0) {
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
