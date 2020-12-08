
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <nt_def.h>
#include <nt_core.h>
#include <nt_event.h>


ssize_t
nt_unix_write(nt_connection_t *c, u_char *buf, size_t size)
{
    ssize_t       n;
    nt_err_t     err;
    nt_event_t  *wev;

    wev = c->write;

    for ( ;; ) {
        n = write(c->fd, buf, size);

        nt_log_debug3(NT_LOG_DEBUG_EVENT, c->log, 0,
                       "send: fd:%d %z of %uz", c->fd, n, size);

        if (n > 0) {
            if (n < (ssize_t) size) {
                wev->ready = 0;
            }

            c->sent += n;

            return n;
        }

        err = nt_socket_errno;

        if (n == 0) {
            nt_log_error(NT_LOG_ALERT, c->log, err, "send() returned zero");
            wev->ready = 0;
            return n;
        }

        if (err == NT_EAGAIN || err == NT_EINTR) {
            wev->ready = 0;

            nt_log_debug0(NT_LOG_DEBUG_EVENT, c->log, err,
                           "send() not ready");

            if (err == NT_EAGAIN) {
                return NT_AGAIN;
            }

        } else {
            wev->error = 1;
            (void) nt_connection_error(c, err, "send() failed");
            return NT_ERROR;
        }
    }
}
