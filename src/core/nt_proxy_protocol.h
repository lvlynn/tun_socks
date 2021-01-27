

#ifndef _NT_PROXY_PROTOCOL_H_INCLUDED_
#define _NT_PROXY_PROTOCOL_H_INCLUDED_


#include <nt_core.h>


#define NT_PROXY_PROTOCOL_MAX_HEADER  107


u_char *nt_proxy_protocol_read(nt_connection_t *c, u_char *buf,
    u_char *last);
u_char *nt_proxy_protocol_write(nt_connection_t *c, u_char *buf,
    u_char *last);


#endif /* _NT_PROXY_PROTOCOL_H_INCLUDED_ */
