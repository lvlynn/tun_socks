
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <nt_core.h>


nt_int_t   nt_ncpu;
nt_int_t   nt_max_sockets;
nt_uint_t  nt_inherited_nonblocking;
nt_uint_t  nt_tcp_nodelay_and_tcp_nopush;


struct rlimit  rlmt;

#if 0
nt_os_io_t nt_os_io = {
    nt_unix_recv,
    nt_readv_chain,
    nt_udp_unix_recv,
    nt_unix_send,
    nt_udp_unix_send,
    nt_udp_unix_sendmsg_chain,
    nt_writev_chain,
    0
};
#else
nt_os_io_t nt_os_io = {
    nt_unix_recv,
    1,
    nt_udp_unix_recv,
    nt_unix_send,
    nt_udp_unix_send,
    NULL,
    NULL,
    0
};

#endif

nt_int_t
nt_os_init(nt_log_t *log)
{
    nt_time_t  *tp;
    nt_uint_t   n;
#if (NT_HAVE_LEVEL1_DCACHE_LINESIZE)
    long         size;
#endif

#if (NT_HAVE_OS_SPECIFIC_INIT)
    if (nt_os_specific_init(log) != NT_OK) {
        return NT_ERROR;
    }
#endif

    if (nt_init_setproctitle(log) != NT_OK) {
        return NT_ERROR;
    }

    nt_pagesize = getpagesize();
    nt_cacheline_size = NT_CPU_CACHE_LINE;

    for (n = nt_pagesize; n >>= 1; nt_pagesize_shift++) { /* void */ }

#if (NT_HAVE_SC_NPROCESSORS_ONLN)
    if (nt_ncpu == 0) {
        nt_ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    }
#endif

    if (nt_ncpu < 1) {
        nt_ncpu = 1;
    }

#if (NT_HAVE_LEVEL1_DCACHE_LINESIZE)
    size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    if (size > 0) {
        nt_cacheline_size = size;
    }
#endif

    nt_cpuinfo();

    if (getrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
        nt_log_error(NT_LOG_ALERT, log, errno,
                      "getrlimit(RLIMIT_NOFILE) failed");
        return NT_ERROR;
    }

    nt_max_sockets = (nt_int_t) rlmt.rlim_cur;

#if (NT_HAVE_INHERITED_NONBLOCK || NT_HAVE_ACCEPT4)
    nt_inherited_nonblocking = 1;
#else
    nt_inherited_nonblocking = 0;
#endif

    tp = nt_timeofday();
    srandom(((unsigned) nt_pid << 16) ^ tp->sec ^ tp->msec);

    return NT_OK;
}


void
nt_os_status(nt_log_t *log)
{
    nt_log_error(NT_LOG_NOTICE, log, 0, NGINX_VER_BUILD);
#if (T_NT_SERVER_INFO)
    nt_log_error(NT_LOG_NOTICE, log, 0, TENGINE_VER_BUILD);
#endif

#ifdef NT_COMPILER
    nt_log_error(NT_LOG_NOTICE, log, 0, "built by " NT_COMPILER);
#endif

#if (NT_HAVE_OS_SPECIFIC_INIT)
    nt_os_specific_status(log);
#endif

    nt_log_error(NT_LOG_NOTICE, log, 0,
                  "getrlimit(RLIMIT_NOFILE): %r:%r",
                  rlmt.rlim_cur, rlmt.rlim_max);
}


#if 0

nt_int_t
nt_posix_post_conf_init(nt_log_t *log)
{
    nt_fd_t  pp[2];

    if (pipe(pp) == -1) {
        nt_log_error(NT_LOG_EMERG, log, nt_errno, "pipe() failed");
        return NT_ERROR;
    }

    if (dup2(pp[1], STDERR_FILENO) == -1) {
        nt_log_error(NT_LOG_EMERG, log, errno, "dup2(STDERR) failed");
        return NT_ERROR;
    }

    if (pp[1] > STDERR_FILENO) {
        if (close(pp[1]) == -1) {
            nt_log_error(NT_LOG_EMERG, log, errno, "close() failed");
            return NT_ERROR;
        }
    }

    return NT_OK;
}

#endif
