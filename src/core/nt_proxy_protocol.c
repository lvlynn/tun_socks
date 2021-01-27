#include <nt_core.h>


#define NT_PROXY_PROTOCOL_AF_INET          1
#define NT_PROXY_PROTOCOL_AF_INET6         2


#define nt_proxy_protocol_parse_uint16(p)  ((p)[0] << 8 | (p)[1])


typedef struct {
    u_char                                  signature[12];
    u_char                                  version_command;
    u_char                                  family_transport;
    u_char                                  len[2];
} nt_proxy_protocol_header_t;


typedef struct {
    u_char                                  src_addr[4];
    u_char                                  dst_addr[4];
    u_char                                  src_port[2];
    u_char                                  dst_port[2];
} nt_proxy_protocol_inet_addrs_t;


typedef struct {
    u_char                                  src_addr[16];
    u_char                                  dst_addr[16];
    u_char                                  src_port[2];
    u_char                                  dst_port[2];
} nt_proxy_protocol_inet6_addrs_t;


static u_char *nt_proxy_protocol_v2_read(nt_connection_t *c, u_char *buf,
    u_char *last);


u_char *
nt_proxy_protocol_read(nt_connection_t *c, u_char *buf, u_char *last)
{
    size_t     len;
    u_char     ch, *p, *addr, *port;
    nt_int_t  n;

    static const u_char signature[] = "\r\n\r\n\0\r\nQUIT\n";

    p = buf;
    len = last - buf;

    if (len >= sizeof(nt_proxy_protocol_header_t)
        && memcmp(p, signature, sizeof(signature) - 1) == 0)
    {
        return nt_proxy_protocol_v2_read(c, buf, last);
    }

    if (len < 8 || nt_strncmp(p, "PROXY ", 6) != 0) {
        goto invalid;
    }

    p += 6;
    len -= 6;

    if (len >= 7 && nt_strncmp(p, "UNKNOWN", 7) == 0) {
        nt_log_debug0(NT_LOG_DEBUG_CORE, c->log, 0,
                       "PROXY protocol unknown protocol");
        p += 7;
        goto skip;
    }

    if (len < 5 || nt_strncmp(p, "TCP", 3) != 0
        || (p[3] != '4' && p[3] != '6') || p[4] != ' ')
    {
        goto invalid;
    }

    p += 5;
    addr = p;

    for ( ;; ) {
        if (p == last) {
            goto invalid;
        }

        ch = *p++;

        if (ch == ' ') {
            break;
        }

        if (ch != ':' && ch != '.'
            && (ch < 'a' || ch > 'f')
            && (ch < 'A' || ch > 'F')
            && (ch < '0' || ch > '9'))
        {
            goto invalid;
        }
    }

    len = p - addr - 1;
    c->proxy_protocol_addr.data = nt_pnalloc(c->pool, len);

    if (c->proxy_protocol_addr.data == NULL) {
        return NULL;
    }

    nt_memcpy(c->proxy_protocol_addr.data, addr, len);
    c->proxy_protocol_addr.len = len;

    for ( ;; ) {
        if (p == last) {
            goto invalid;
        }

        if (*p++ == ' ') {
            break;
        }
    }

    port = p;

    for ( ;; ) {
        if (p == last) {
            goto invalid;
        }

        if (*p++ == ' ') {
            break;
        }
    }

    len = p - port - 1;

    n = nt_atoi(port, len);

    if (n < 0 || n > 65535) {
        goto invalid;
    }

    c->proxy_protocol_port = (in_port_t) n;

    nt_log_debug2(NT_LOG_DEBUG_CORE, c->log, 0,
                   "PROXY protocol address: %V %d", &c->proxy_protocol_addr,
                   c->proxy_protocol_port);

skip:

    for ( /* void */ ; p < last - 1; p++) {
        if (p[0] == CR && p[1] == LF) {
            return p + 2;
        }
    }

invalid:

    nt_log_error(NT_LOG_ERR, c->log, 0,
                  "broken header: \"%*s\"", (size_t) (last - buf), buf);

    return NULL;
}


u_char *
nt_proxy_protocol_write(nt_connection_t *c, u_char *buf, u_char *last)
{
    nt_uint_t  port, lport;

    if (last - buf < NT_PROXY_PROTOCOL_MAX_HEADER) {
        return NULL;
    }

    if (nt_connection_local_sockaddr(c, NULL, 0) != NT_OK) {
        return NULL;
    }

    switch (c->sockaddr->sa_family) {

    case AF_INET:
        buf = nt_cpymem(buf, "PROXY TCP4 ", sizeof("PROXY TCP4 ") - 1);
        break;

#if (NT_HAVE_INET6)
    case AF_INET6:
        buf = nt_cpymem(buf, "PROXY TCP6 ", sizeof("PROXY TCP6 ") - 1);
        break;
#endif

    default:
        return nt_cpymem(buf, "PROXY UNKNOWN" CRLF,
                          sizeof("PROXY UNKNOWN" CRLF) - 1);
    }

    buf += nt_sock_ntop(c->sockaddr, c->socklen, buf, last - buf, 0);

    *buf++ = ' ';

    buf += nt_sock_ntop(c->local_sockaddr, c->local_socklen, buf, last - buf,
                         0);

    port = nt_inet_get_port(c->sockaddr);
    lport = nt_inet_get_port(c->local_sockaddr);

    return nt_slprintf(buf, last, " %ui %ui" CRLF, port, lport);
}


static u_char *
nt_proxy_protocol_v2_read(nt_connection_t *c, u_char *buf, u_char *last)
{
    u_char                             *end;
    size_t                              len;
    socklen_t                           socklen;
    nt_uint_t                          version, command, family, transport;
    nt_sockaddr_t                      sockaddr;
    nt_proxy_protocol_header_t        *header;
    nt_proxy_protocol_inet_addrs_t    *in;
#if (NT_HAVE_INET6)
    nt_proxy_protocol_inet6_addrs_t   *in6;
#endif

    header = (nt_proxy_protocol_header_t *) buf;

    buf += sizeof(nt_proxy_protocol_header_t);

    version = header->version_command >> 4;

    if (version != 2) {
        nt_log_error(NT_LOG_ERR, c->log, 0,
                      "unknown PROXY protocol version: %ui", version);
        return NULL;
    }

    len = nt_proxy_protocol_parse_uint16(header->len);

    if ((size_t) (last - buf) < len) {
        nt_log_error(NT_LOG_ERR, c->log, 0, "header is too large");
        return NULL;
    }

    end = buf + len;

    command = header->version_command & 0x0f;

    /* only PROXY is supported */
    if (command != 1) {
        nt_log_debug1(NT_LOG_DEBUG_CORE, c->log, 0,
                       "PROXY protocol v2 unsupported command %ui", command);
        return end;
    }

    transport = header->family_transport & 0x0f;

    /* only STREAM is supported */
    if (transport != 1) {
        nt_log_debug1(NT_LOG_DEBUG_CORE, c->log, 0,
                       "PROXY protocol v2 unsupported transport %ui",
                       transport);
        return end;
    }

    family = header->family_transport >> 4;

    switch (family) {

    case NT_PROXY_PROTOCOL_AF_INET:

        if ((size_t) (end - buf) < sizeof(nt_proxy_protocol_inet_addrs_t)) {
            return NULL;
        }

        in = (nt_proxy_protocol_inet_addrs_t *) buf;

        sockaddr.sockaddr_in.sin_family = AF_INET;
        sockaddr.sockaddr_in.sin_port = 0;
        memcpy(&sockaddr.sockaddr_in.sin_addr, in->src_addr, 4);

        c->proxy_protocol_port = nt_proxy_protocol_parse_uint16(in->src_port);

        socklen = sizeof(struct sockaddr_in);

        buf += sizeof(nt_proxy_protocol_inet_addrs_t);

        break;

#if (NT_HAVE_INET6)

    case NT_PROXY_PROTOCOL_AF_INET6:

        if ((size_t) (end - buf) < sizeof(nt_proxy_protocol_inet6_addrs_t)) {
            return NULL;
        }

        in6 = (nt_proxy_protocol_inet6_addrs_t *) buf;

        sockaddr.sockaddr_in6.sin6_family = AF_INET6;
        sockaddr.sockaddr_in6.sin6_port = 0;
        memcpy(&sockaddr.sockaddr_in6.sin6_addr, in6->src_addr, 16);

        c->proxy_protocol_port = nt_proxy_protocol_parse_uint16(in6->src_port);

        socklen = sizeof(struct sockaddr_in6);

        buf += sizeof(nt_proxy_protocol_inet6_addrs_t);

        break;

#endif

    default:
        nt_log_debug1(NT_LOG_DEBUG_CORE, c->log, 0,
                       "PROXY protocol v2 unsupported address family %ui",
                       family);
        return end;
    }

    c->proxy_protocol_addr.data = nt_pnalloc(c->pool, NT_SOCKADDR_STRLEN);
    if (c->proxy_protocol_addr.data == NULL) {
        return NULL;
    }

    c->proxy_protocol_addr.len = nt_sock_ntop(&sockaddr.sockaddr, socklen,
                                               c->proxy_protocol_addr.data,
                                               NT_SOCKADDR_STRLEN, 0);

    nt_log_debug2(NT_LOG_DEBUG_CORE, c->log, 0,
                   "PROXY protocol v2 address: %V %d", &c->proxy_protocol_addr,
                   c->proxy_protocol_port);

    if (buf < end) {
        nt_log_debug1(NT_LOG_DEBUG_CORE, c->log, 0,
                       "PROXY protocol v2 %z bytes of tlv ignored", end - buf);
    }

    return end;
}
