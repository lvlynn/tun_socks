


#ifndef _NT_SOCKET_H_
#define _NT_SOCKET_H_


#include <nt_def.h>


#define NT_WRITE_SHUTDOWN SHUT_WR

typedef int  nt_socket_t;

#define nt_socket          socket
#define nt_socket_n        "socket()"


#if (NT_HAVE_FIONBIO)

int nt_nonblocking(nt_socket_t s);
int nt_blocking(nt_socket_t s);

#define nt_nonblocking_n   "ioctl(FIONBIO)"
#define nt_blocking_n      "ioctl(!FIONBIO)"

#else

#define nt_nonblocking(s)  fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK)
#define nt_nonblocking_n   "fcntl(O_NONBLOCK)"

#define nt_blocking(s)     fcntl(s, F_SETFL, fcntl(s, F_GETFL) & ~O_NONBLOCK)
#define nt_blocking_n      "fcntl(!O_NONBLOCK)"

#endif

int nt_tcp_nopush(nt_socket_t s);
int nt_tcp_push(nt_socket_t s);

#if (NT_LINUX)

#define nt_tcp_nopush_n   "setsockopt(TCP_CORK)"
#define nt_tcp_push_n     "setsockopt(!TCP_CORK)"

#else

#define nt_tcp_nopush_n   "setsockopt(TCP_NOPUSH)"
#define nt_tcp_push_n     "setsockopt(!TCP_NOPUSH)"

#endif


#define nt_shutdown_socket    shutdown
#define nt_shutdown_socket_n  "shutdown()"

#define nt_close_socket    close
#define nt_close_socket_n  "close() socket"


#endif /* _NT_SOCKET_H_INCLUDED_ */
