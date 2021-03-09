#include <nt_core.h>
#include <nt_def.h>
#include <nt_channel.h>


nt_int_t
nt_write_channel(nt_socket_t s, nt_channel_t *ch, size_t size,
    nt_log_t *log)
{
    ssize_t             n;
    nt_err_t           err;
    struct iovec        iov[1];
    struct msghdr       msg;

#if (NT_HAVE_MSGHDR_MSG_CONTROL)

    union {
        struct cmsghdr  cm;
        char            space[CMSG_SPACE(sizeof(int))];
    } cmsg;

    if (ch->fd == -1) {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;

    } else {
        msg.msg_control = (caddr_t) &cmsg;
        msg.msg_controllen = sizeof(cmsg);

        nt_memzero(&cmsg, sizeof(cmsg));

        cmsg.cm.cmsg_len = CMSG_LEN(sizeof(int));
        cmsg.cm.cmsg_level = SOL_SOCKET;
        cmsg.cm.cmsg_type = SCM_RIGHTS;

        /*
         * We have to use nt_memcpy() instead of simple
         *   *(int *) CMSG_DATA(&cmsg.cm) = ch->fd;
         * because some gcc 4.4 with -O2/3/s optimization issues the warning:
         *   dereferencing type-punned pointer will break strict-aliasing rules
         *
         * Fortunately, gcc with -O1 compiles this nt_memcpy()
         * in the same simple assignment as in the code above
         */

        nt_memcpy(CMSG_DATA(&cmsg.cm), &ch->fd, sizeof(int));
    }

    msg.msg_flags = 0;

#else

    if (ch->fd == -1) {
        msg.msg_accrights = NULL;
        msg.msg_accrightslen = 0;

    } else {
        msg.msg_accrights = (caddr_t) &ch->fd;
        msg.msg_accrightslen = sizeof(int);
    }

#endif

    iov[0].iov_base = (char *) ch;
    iov[0].iov_len = size;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    n = sendmsg(s, &msg, 0);

    if (n == -1) {
        err = nt_errno;
        if (err == NT_EAGAIN) {
            return NT_AGAIN;
        }

        nt_log_error(NT_LOG_ALERT, log, err, "sendmsg() failed");
        return NT_ERROR;
    }

    return NT_OK;
}


nt_int_t
nt_read_channel(nt_socket_t s, nt_channel_t *ch, size_t size, nt_log_t *log)
{
    ssize_t             n;
    nt_err_t           err;
    struct iovec        iov[1];
    struct msghdr       msg;

#if (NT_HAVE_MSGHDR_MSG_CONTROL)
    union {
        struct cmsghdr  cm;
        char            space[CMSG_SPACE(sizeof(int))];
    } cmsg;
#else
    int                 fd;
#endif

    iov[0].iov_base = (char *) ch;
    iov[0].iov_len = size;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

#if (NT_HAVE_MSGHDR_MSG_CONTROL)
    msg.msg_control = (caddr_t) &cmsg;
    msg.msg_controllen = sizeof(cmsg);
#else
    msg.msg_accrights = (caddr_t) &fd;
    msg.msg_accrightslen = sizeof(int);
#endif

    n = recvmsg(s, &msg, 0);

    if (n == -1) {
        err = nt_errno;
        if (err == NT_EAGAIN) {
            return NT_AGAIN;
        }

        nt_log_error(NT_LOG_ALERT, log, err, "recvmsg() failed");
        return NT_ERROR;
    }

    if (n == 0) {
        nt_log_debug0(NT_LOG_DEBUG_CORE, log, 0, "recvmsg() returned zero");
        return NT_ERROR;
    }

    if ((size_t) n < sizeof(nt_channel_t)) {
        nt_log_error(NT_LOG_ALERT, log, 0,
                      "recvmsg() returned not enough data: %z", n);
        return NT_ERROR;
    }

#if (NT_HAVE_MSGHDR_MSG_CONTROL)

    if (ch->command == NT_CMD_OPEN_CHANNEL) {

        if (cmsg.cm.cmsg_len < (socklen_t) CMSG_LEN(sizeof(int))) {
            nt_log_error(NT_LOG_ALERT, log, 0,
                          "recvmsg() returned too small ancillary data");
            return NT_ERROR;
        }

        if (cmsg.cm.cmsg_level != SOL_SOCKET || cmsg.cm.cmsg_type != SCM_RIGHTS)
        {
            nt_log_error(NT_LOG_ALERT, log, 0,
                          "recvmsg() returned invalid ancillary data "
                          "level %d or type %d",
                          cmsg.cm.cmsg_level, cmsg.cm.cmsg_type);
            return NT_ERROR;
        }

        /* ch->fd = *(int *) CMSG_DATA(&cmsg.cm); */

        nt_memcpy(&ch->fd, CMSG_DATA(&cmsg.cm), sizeof(int));
    }

    if (msg.msg_flags & (MSG_TRUNC|MSG_CTRUNC)) {
        nt_log_error(NT_LOG_ALERT, log, 0,
                      "recvmsg() truncated data");
    }

#else

    if (ch->command == NT_CMD_OPEN_CHANNEL) {
        if (msg.msg_accrightslen != sizeof(int)) {
            nt_log_error(NT_LOG_ALERT, log, 0,
                          "recvmsg() returned no ancillary data");
            return NT_ERROR;
        }

        ch->fd = fd;
    }

#endif

    return n;
}


nt_int_t
nt_add_channel_event(nt_cycle_t *cycle, nt_fd_t fd, nt_int_t event,
    nt_event_handler_pt handler)
{
    nt_event_t       *ev, *rev, *wev;
    nt_connection_t  *c;

    c = nt_get_connection(fd, cycle->log);

    if (c == NULL) {
        return NT_ERROR;
    }

    c->pool = cycle->pool;

    rev = c->read;
    wev = c->write;

    rev->log = cycle->log;
    wev->log = cycle->log;

    rev->channel = 1;
    wev->channel = 1;

    ev = (event == NT_READ_EVENT) ? rev : wev;

    ev->handler = handler;

    if (nt_add_conn && (nt_event_flags & NT_USE_EPOLL_EVENT) == 0) {
        if (nt_add_conn(c) == NT_ERROR) {
            nt_free_connection(c);
            return NT_ERROR;
        }

    } else {
        if (nt_add_event(ev, event, 0) == NT_ERROR) {
            nt_free_connection(c);
            return NT_ERROR;
        }
    }

    return NT_OK;
}


void
nt_close_channel(nt_fd_t *fd, nt_log_t *log)
{
    if (close(fd[0]) == -1) {
        nt_log_error(NT_LOG_ALERT, log, nt_errno, "close() channel failed");
    }

    if (close(fd[1]) == -1) {
        nt_log_error(NT_LOG_ALERT, log, nt_errno, "close() channel failed");
    }
}
