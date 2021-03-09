
#ifndef _NT_OS_H_INCLUDED_
#define _NT_OS_H_INCLUDED_


#include <nt_core.h>
#include <nt_process.h>


#define NT_IO_SENDFILE    1


typedef ssize_t (*nt_recv_pt)(nt_connection_t *c, u_char *buf, size_t size);
typedef ssize_t (*nt_recv_chain_pt)(nt_connection_t *c, nt_chain_t *in,
    off_t limit);
typedef ssize_t (*nt_send_pt)(nt_connection_t *c, u_char *buf, size_t size);
typedef nt_chain_t *(*nt_send_chain_pt)(nt_connection_t *c, nt_chain_t *in,
    off_t limit);

typedef struct {
    nt_recv_pt        recv;
    nt_recv_chain_pt  recv_chain;
    nt_recv_pt        udp_recv;
    nt_send_pt        send;
    nt_send_pt        udp_send;
    nt_send_chain_pt  udp_send_chain;
    nt_send_chain_pt  send_chain;
    nt_uint_t         flags;
} nt_os_io_t;


nt_int_t nt_os_init(nt_log_t *log);
void nt_os_status(nt_log_t *log);
nt_int_t nt_os_specific_init(nt_log_t *log);
void nt_os_specific_status(nt_log_t *log);
nt_int_t nt_daemon(nt_log_t *log);
nt_int_t nt_os_signal_process(nt_cycle_t *cycle, char *sig, nt_pid_t pid);


ssize_t nt_unix_recv(nt_connection_t *c, u_char *buf, size_t size);
ssize_t nt_readv_chain(nt_connection_t *c, nt_chain_t *entry, off_t limit);
ssize_t nt_udp_unix_recv(nt_connection_t *c, u_char *buf, size_t size);
ssize_t nt_unix_send(nt_connection_t *c, u_char *buf, size_t size);
nt_chain_t *nt_writev_chain(nt_connection_t *c, nt_chain_t *in,
    off_t limit);
ssize_t nt_udp_unix_send(nt_connection_t *c, u_char *buf, size_t size);
nt_chain_t *nt_udp_unix_sendmsg_chain(nt_connection_t *c, nt_chain_t *in,
    off_t limit);


#if (IOV_MAX > 64)
#define NT_IOVS_PREALLOCATE  64
#else
#define NT_IOVS_PREALLOCATE  IOV_MAX
#endif


typedef struct {
    struct iovec  *iovs;
    nt_uint_t     count;
    size_t         size;
    nt_uint_t     nalloc;
} nt_iovec_t;

nt_chain_t *nt_output_chain_to_iovec(nt_iovec_t *vec, nt_chain_t *in,
    size_t limit, nt_log_t *log);


ssize_t nt_writev(nt_connection_t *c, nt_iovec_t *vec);


extern nt_os_io_t  nt_os_io;
extern nt_int_t    nt_ncpu;
extern nt_int_t    nt_max_sockets;
extern nt_uint_t   nt_inherited_nonblocking;
extern nt_uint_t   nt_tcp_nodelay_and_tcp_nopush;


#if (NT_FREEBSD)
#include <nt_freebsd.h>


#elif (NT_LINUX)
#include <nt_linux.h>


#elif (NT_SOLARIS)
#include <nt_solaris.h>


#elif (NT_DARWIN)
#include <nt_darwin.h>
#endif


#endif /* _NT_OS_H_INCLUDED_ */
