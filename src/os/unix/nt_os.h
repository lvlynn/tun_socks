#ifndef _NT_OS_H_
#define _NT_OS_H_

#include <nt_def.h>
#include <nt_core.h>


typedef ssize_t ( *nt_recv_pt )( nt_connection_t *c, u_char *buf, size_t size );
typedef ssize_t ( *nt_send_pt )( nt_connection_t *c, u_char *buf, size_t size );


typedef struct {
    nt_recv_pt        recv;
    nt_send_pt        send;
    nt_recv_pt        udp_recv;
    nt_send_pt        udp_send;
    nt_uint_t         flags;

} nt_os_io_t;

extern nt_os_io_t  nt_os_io;

nt_int_t nt_os_init( nt_log_t *log );

ssize_t nt_unix_recv( nt_connection_t *c, u_char *buf, size_t size );
ssize_t nt_udp_unix_recv( nt_connection_t *c, u_char *buf, size_t size );
ssize_t nt_unix_send( nt_connection_t *c, u_char *buf, size_t size );
ssize_t nt_udp_unix_send( nt_connection_t *c, u_char *buf, size_t size );


#endif
