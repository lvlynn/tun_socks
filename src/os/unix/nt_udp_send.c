
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <nt_def.h>
#include <nt_core.h>
#include <nt_event.h>


ssize_t
nt_udp_unix_send(nt_connection_t *c, u_char *buf, size_t size)
{
    ssize_t       n;
    nt_err_t     err;
    nt_event_t  *wev;

    wev = c->write;

    for ( ;; ) {
        n = sendto(c->fd, buf, size, 0, c->sockaddr, c->socklen);

        nt_log_debug4(NT_LOG_DEBUG_EVENT, c->log, 0,
                       "sendto: fd:%d %z of %uz to \"%V\"",
                       c->fd, n, size, &c->addr_text);

        if (n >= 0) {
            if ((size_t) n != size) {
                wev->error = 1;
                (void) nt_connection_error(c, 0, "sendto() incomplete");
                return NT_ERROR;
            }

            c->sent += n;

            return n;
        }

        err = nt_socket_errno;

        if (err == NT_EAGAIN) {
            wev->ready = 0;
            nt_log_debug0(NT_LOG_DEBUG_EVENT, c->log, NT_EAGAIN,
                           "sendto() not ready");
            return NT_AGAIN;
        }

        if (err != NT_EINTR) {
            wev->error = 1;
            (void) nt_connection_error(c, err, "sendto() failed");
            return NT_ERROR;
        }
    }
}
