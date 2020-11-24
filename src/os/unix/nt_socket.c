
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <nt_def.h>
#include <nt_core.h>


/*
 * ioctl(FIONBIO) sets a non-blocking mode with the single syscall
 * while fcntl(F_SETFL, O_NONBLOCK) needs to learn the current state
 * using fcntl(F_GETFL).
 *
 * ioctl() and fcntl() are syscalls at least in FreeBSD 2.x, Linux 2.2
 * and Solaris 7.
 *
 * ioctl() in Linux 2.4 and 2.6 uses BKL, however, fcntl(F_SETFL) uses it too.
 */


#if (NT_HAVE_FIONBIO)

int
nt_nonblocking(nt_socket_t s)
{
    int  nb;

    nb = 1;

    return ioctl(s, FIONBIO, &nb);
}


int
nt_blocking(nt_socket_t s)
{
    int  nb;

    nb = 0;

    return ioctl(s, FIONBIO, &nb);
}

#endif


#if (NT_FREEBSD)

int
nt_tcp_nopush(nt_socket_t s)
{
    int  tcp_nopush;

    tcp_nopush = 1;

    return setsockopt(s, IPPROTO_TCP, TCP_NOPUSH,
                      (const void *) &tcp_nopush, sizeof(int));
}


int
nt_tcp_push(nt_socket_t s)
{
    int  tcp_nopush;

    tcp_nopush = 0;

    return setsockopt(s, IPPROTO_TCP, TCP_NOPUSH,
                      (const void *) &tcp_nopush, sizeof(int));
}

#elif (NT_LINUX)


int
nt_tcp_nopush(nt_socket_t s)
{
    int  cork;

    cork = 1;

    return setsockopt(s, IPPROTO_TCP, TCP_CORK,
                      (const void *) &cork, sizeof(int));
}


int
nt_tcp_push(nt_socket_t s)
{
    int  cork;

    cork = 0;

    return setsockopt(s, IPPROTO_TCP, TCP_CORK,
                      (const void *) &cork, sizeof(int));
}

#else

int
nt_tcp_nopush(nt_socket_t s)
{
    return 0;
}


int
nt_tcp_push(nt_socket_t s)
{
    return 0;
}

#endif
