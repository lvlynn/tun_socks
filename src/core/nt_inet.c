
#include <nt_core.h>


static nt_int_t nt_parse_unix_domain_url(nt_pool_t *pool, nt_url_t *u);
static nt_int_t nt_parse_inet_url(nt_pool_t *pool, nt_url_t *u);
static nt_int_t nt_parse_inet6_url(nt_pool_t *pool, nt_url_t *u);
static nt_int_t nt_inet_add_addr(nt_pool_t *pool, nt_url_t *u,
    struct sockaddr *sockaddr, socklen_t socklen, nt_uint_t total);

#if (T_NT_DNS_RESOLVE_BACKUP)
static nt_int_t nt_resolve_backup(nt_pool_t *pool, nt_url_t **u,
    nt_str_t path);
static nt_int_t nt_resolve_using_local(nt_pool_t *pool, nt_url_t **u,
    nt_str_t path);
#endif



in_addr_t
nt_inet_addr(u_char *text, size_t len)
{
    u_char      *p, c;
    in_addr_t    addr;
    nt_uint_t   octet, n;

    addr = 0;
    octet = 0;
    n = 0;

    for (p = text; p < text + len; p++) {
        c = *p;

        if (c >= '0' && c <= '9') {
            octet = octet * 10 + (c - '0');

            if (octet > 255) {
                return INADDR_NONE;
            }

            continue;
        }

        if (c == '.') {
            addr = (addr << 8) + octet;
            octet = 0;
            n++;
            continue;
        }

        return INADDR_NONE;
    }

    if (n == 3) {
        addr = (addr << 8) + octet;
        return htonl(addr);
    }

    return INADDR_NONE;
}


#if (NT_HAVE_INET6)

nt_int_t
nt_inet6_addr(u_char *p, size_t len, u_char *addr)
{
    u_char      c, *zero, *digit, *s, *d;
    size_t      len4;
    nt_uint_t  n, nibbles, word;

    if (len == 0) {
        return NT_ERROR;
    }

    zero = NULL;
    digit = NULL;
    len4 = 0;
    nibbles = 0;
    word = 0;
    n = 8;

    if (p[0] == ':') {
        p++;
        len--;
    }

    for (/* void */; len; len--) {
        c = *p++;

        if (c == ':') {
            if (nibbles) {
                digit = p;
                len4 = len;
                *addr++ = (u_char) (word >> 8);
                *addr++ = (u_char) (word & 0xff);

                if (--n) {
                    nibbles = 0;
                    word = 0;
                    continue;
                }

            } else {
                if (zero == NULL) {
                    digit = p;
                    len4 = len;
                    zero = addr;
                    continue;
                }
            }

            return NT_ERROR;
        }

        if (c == '.' && nibbles) {
            if (n < 2 || digit == NULL) {
                return NT_ERROR;
            }

            word = nt_inet_addr(digit, len4 - 1);
            if (word == INADDR_NONE) {
                return NT_ERROR;
            }

            word = ntohl(word);
            *addr++ = (u_char) ((word >> 24) & 0xff);
            *addr++ = (u_char) ((word >> 16) & 0xff);
            n--;
            break;
        }

        if (++nibbles > 4) {
            return NT_ERROR;
        }

        if (c >= '0' && c <= '9') {
            word = word * 16 + (c - '0');
            continue;
        }

        c |= 0x20;

        if (c >= 'a' && c <= 'f') {
            word = word * 16 + (c - 'a') + 10;
            continue;
        }

        return NT_ERROR;
    }

    if (nibbles == 0 && zero == NULL) {
        return NT_ERROR;
    }

    *addr++ = (u_char) (word >> 8);
    *addr++ = (u_char) (word & 0xff);

    if (--n) {
        if (zero) {
            n *= 2;
            s = addr - 1;
            d = s + n;
            while (s >= zero) {
                *d-- = *s--;
            }
            nt_memzero(zero, n);
            return NT_OK;
        }

    } else {
        if (zero == NULL) {
            return NT_OK;
        }
    }

    return NT_ERROR;
}

#endif


size_t
nt_sock_ntop(struct sockaddr *sa, socklen_t socklen, u_char *text, size_t len,
    nt_uint_t port)
{
    u_char               *p;
#if (NT_HAVE_INET6 || NT_HAVE_UNIX_DOMAIN)
    size_t                n;
#endif
    struct sockaddr_in   *sin;
#if (NT_HAVE_INET6)
    struct sockaddr_in6  *sin6;
#endif
#if (NT_HAVE_UNIX_DOMAIN)
    struct sockaddr_un   *saun;
#endif

    switch (sa->sa_family) {

    case AF_INET:

        sin = (struct sockaddr_in *) sa;
        p = (u_char *) &sin->sin_addr;

        if (port) {
            p = nt_snprintf(text, len, "%ud.%ud.%ud.%ud:%d",
                             p[0], p[1], p[2], p[3], ntohs(sin->sin_port));
        } else {
            p = nt_snprintf(text, len, "%ud.%ud.%ud.%ud",
                             p[0], p[1], p[2], p[3]);
        }

        return (p - text);

#if (NT_HAVE_INET6)

    case AF_INET6:

        sin6 = (struct sockaddr_in6 *) sa;

        n = 0;

        if (port) {
            text[n++] = '[';
        }

        n = nt_inet6_ntop(sin6->sin6_addr.s6_addr, &text[n], len);

        if (port) {
            n = nt_sprintf(&text[1 + n], "]:%d",
                            ntohs(sin6->sin6_port)) - text;
        }

        return n;
#endif

#if (NT_HAVE_UNIX_DOMAIN)

    case AF_UNIX:
        saun = (struct sockaddr_un *) sa;

        /* on Linux sockaddr might not include sun_path at all */

        if (socklen <= (socklen_t) offsetof(struct sockaddr_un, sun_path)) {
            p = nt_snprintf(text, len, "unix:%Z");

        } else {
            n = nt_strnlen((u_char *) saun->sun_path,
                            socklen - offsetof(struct sockaddr_un, sun_path));
            p = nt_snprintf(text, len, "unix:%*s%Z", n, saun->sun_path);
        }

        /* we do not include trailing zero in address length */

        return (p - text - 1);

#endif

    default:
        return 0;
    }
}


size_t
nt_inet_ntop(int family, void *addr, u_char *text, size_t len)
{
    u_char  *p;

    switch (family) {

    case AF_INET:

        p = addr;

        return nt_snprintf(text, len, "%ud.%ud.%ud.%ud",
                            p[0], p[1], p[2], p[3])
               - text;

#if (NT_HAVE_INET6)

    case AF_INET6:
        return nt_inet6_ntop(addr, text, len);

#endif

    default:
        return 0;
    }
}


#if (NT_HAVE_INET6)

size_t
nt_inet6_ntop(u_char *p, u_char *text, size_t len)
{
    u_char      *dst;
    size_t       max, n;
    nt_uint_t   i, zero, last;

    if (len < NT_INET6_ADDRSTRLEN) {
        return 0;
    }

    zero = (nt_uint_t) -1;
    last = (nt_uint_t) -1;
    max = 1;
    n = 0;

    for (i = 0; i < 16; i += 2) {

        if (p[i] || p[i + 1]) {

            if (max < n) {
                zero = last;
                max = n;
            }

            n = 0;
            continue;
        }

        if (n++ == 0) {
            last = i;
        }
    }

    if (max < n) {
        zero = last;
        max = n;
    }

    dst = text;
    n = 16;

    if (zero == 0) {

        if ((max == 5 && p[10] == 0xff && p[11] == 0xff)
            || (max == 6)
            || (max == 7 && p[14] != 0 && p[15] != 1))
        {
            n = 12;
        }

        *dst++ = ':';
    }

    for (i = 0; i < n; i += 2) {

        if (i == zero) {
            *dst++ = ':';
            i += (max - 1) * 2;
            continue;
        }

        dst = nt_sprintf(dst, "%xd", p[i] * 256 + p[i + 1]);

        if (i < 14) {
            *dst++ = ':';
        }
    }

    if (n == 12) {
        dst = nt_sprintf(dst, "%ud.%ud.%ud.%ud", p[12], p[13], p[14], p[15]);
    }

    return dst - text;
}

#endif


nt_int_t
nt_ptocidr(nt_str_t *text, nt_cidr_t *cidr)
{
    u_char      *addr, *mask, *last;
    size_t       len;
    nt_int_t    shift;
#if (NT_HAVE_INET6)
    nt_int_t    rc;
    nt_uint_t   s, i;
#endif

    addr = text->data;
    last = addr + text->len;

    mask = nt_strlchr(addr, last, '/');
    len = (mask ? mask : last) - addr;

    cidr->u.in.addr = nt_inet_addr(addr, len);

    if (cidr->u.in.addr != INADDR_NONE) {
        cidr->family = AF_INET;

        if (mask == NULL) {
            cidr->u.in.mask = 0xffffffff;
            return NT_OK;
        }

#if (NT_HAVE_INET6)
    } else if (nt_inet6_addr(addr, len, cidr->u.in6.addr.s6_addr) == NT_OK) {
        cidr->family = AF_INET6;

        if (mask == NULL) {
            nt_memset(cidr->u.in6.mask.s6_addr, 0xff, 16);
            return NT_OK;
        }

#endif
    } else {
        return NT_ERROR;
    }

    mask++;

    shift = nt_atoi(mask, last - mask);
    if (shift == NT_ERROR) {
        return NT_ERROR;
    }

    switch (cidr->family) {

#if (NT_HAVE_INET6)
    case AF_INET6:
        if (shift > 128) {
            return NT_ERROR;
        }

        addr = cidr->u.in6.addr.s6_addr;
        mask = cidr->u.in6.mask.s6_addr;
        rc = NT_OK;

        for (i = 0; i < 16; i++) {

            s = (shift > 8) ? 8 : shift;
            shift -= s;

            mask[i] = (u_char) (0xffu << (8 - s));

            if (addr[i] != (addr[i] & mask[i])) {
                rc = NT_DONE;
                addr[i] &= mask[i];
            }
        }

        return rc;
#endif

    default: /* AF_INET */
        if (shift > 32) {
            return NT_ERROR;
        }

        if (shift) {
            cidr->u.in.mask = htonl((uint32_t) (0xffffffffu << (32 - shift)));

        } else {
            /* x86 compilers use a shl instruction that shifts by modulo 32 */
            cidr->u.in.mask = 0;
        }

        if (cidr->u.in.addr == (cidr->u.in.addr & cidr->u.in.mask)) {
            return NT_OK;
        }

        cidr->u.in.addr &= cidr->u.in.mask;

        return NT_DONE;
    }
}


nt_int_t
nt_cidr_match(struct sockaddr *sa, nt_array_t *cidrs)
{
#if (NT_HAVE_INET6)
    u_char           *p;
#endif
    in_addr_t         inaddr;
    nt_cidr_t       *cidr;
    nt_uint_t        family, i;
#if (NT_HAVE_INET6)
    nt_uint_t        n;
    struct in6_addr  *inaddr6;
#endif

#if (NT_SUPPRESS_WARN)
    inaddr = 0;
#if (NT_HAVE_INET6)
    inaddr6 = NULL;
#endif
#endif

    family = sa->sa_family;

    if (family == AF_INET) {
        inaddr = ((struct sockaddr_in *) sa)->sin_addr.s_addr;
    }

#if (NT_HAVE_INET6)
    else if (family == AF_INET6) {
        inaddr6 = &((struct sockaddr_in6 *) sa)->sin6_addr;

        if (IN6_IS_ADDR_V4MAPPED(inaddr6)) {
            family = AF_INET;

            p = inaddr6->s6_addr;

            inaddr = p[12] << 24;
            inaddr += p[13] << 16;
            inaddr += p[14] << 8;
            inaddr += p[15];

            inaddr = htonl(inaddr);
        }
    }
#endif

    for (cidr = cidrs->elts, i = 0; i < cidrs->nelts; i++) {
        if (cidr[i].family != family) {
            goto next;
        }

        switch (family) {

#if (NT_HAVE_INET6)
        case AF_INET6:
            for (n = 0; n < 16; n++) {
                if ((inaddr6->s6_addr[n] & cidr[i].u.in6.mask.s6_addr[n])
                    != cidr[i].u.in6.addr.s6_addr[n])
                {
                    goto next;
                }
            }
            break;
#endif

#if (NT_HAVE_UNIX_DOMAIN)
        case AF_UNIX:
            break;
#endif

        default: /* AF_INET */
            if ((inaddr & cidr[i].u.in.mask) != cidr[i].u.in.addr) {
                goto next;
            }
            break;
        }

        return NT_OK;

    next:
        continue;
    }

    return NT_DECLINED;
}


nt_int_t
nt_parse_addr(nt_pool_t *pool, nt_addr_t *addr, u_char *text, size_t len)
{
    in_addr_t             inaddr;
    nt_uint_t            family;
    struct sockaddr_in   *sin;
#if (NT_HAVE_INET6)
    struct in6_addr       inaddr6;
    struct sockaddr_in6  *sin6;

    /*
     * prevent MSVC8 warning:
     *    potentially uninitialized local variable 'inaddr6' used
     */
    nt_memzero(&inaddr6, sizeof(struct in6_addr));
#endif

    inaddr = nt_inet_addr(text, len);

    if (inaddr != INADDR_NONE) {
        family = AF_INET;
        len = sizeof(struct sockaddr_in);

#if (NT_HAVE_INET6)
    } else if (nt_inet6_addr(text, len, inaddr6.s6_addr) == NT_OK) {
        family = AF_INET6;
        len = sizeof(struct sockaddr_in6);

#endif
    } else {
        return NT_DECLINED;
    }

    addr->sockaddr = nt_pcalloc(pool, len);
    if (addr->sockaddr == NULL) {
        return NT_ERROR;
    }

    addr->sockaddr->sa_family = (u_char) family;
    addr->socklen = len;

    switch (family) {

#if (NT_HAVE_INET6)
    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) addr->sockaddr;
        nt_memcpy(sin6->sin6_addr.s6_addr, inaddr6.s6_addr, 16);
        break;
#endif

    default: /* AF_INET */
        sin = (struct sockaddr_in *) addr->sockaddr;
        sin->sin_addr.s_addr = inaddr;
        break;
    }

    return NT_OK;
}


nt_int_t
nt_parse_addr_port(nt_pool_t *pool, nt_addr_t *addr, u_char *text,
    size_t len)
{
    u_char     *p, *last;
    size_t      plen;
    nt_int_t   rc, port;

    rc = nt_parse_addr(pool, addr, text, len);

    if (rc != NT_DECLINED) {
        return rc;
    }

    last = text + len;

#if (NT_HAVE_INET6)
    if (len && text[0] == '[') {

        p = nt_strlchr(text, last, ']');

        if (p == NULL || p == last - 1 || *++p != ':') {
            return NT_DECLINED;
        }

        text++;
        len -= 2;

    } else
#endif

    {
        p = nt_strlchr(text, last, ':');

        if (p == NULL) {
            return NT_DECLINED;
        }
    }

    p++;
    plen = last - p;

    port = nt_atoi(p, plen);

    if (port < 1 || port > 65535) {
        return NT_DECLINED;
    }

    len -= plen + 1;

    rc = nt_parse_addr(pool, addr, text, len);

    if (rc != NT_OK) {
        return rc;
    }

    nt_inet_set_port(addr->sockaddr, (in_port_t) port);

    return NT_OK;
}


nt_int_t
nt_parse_url(nt_pool_t *pool, nt_url_t *u)
{
    u_char  *p;
    size_t   len;

    p = u->url.data;
    len = u->url.len;

    if (len >= 5 && nt_strncasecmp(p, (u_char *) "unix:", 5) == 0) {
        return nt_parse_unix_domain_url(pool, u);
    }

    if (len && p[0] == '[') {
        return nt_parse_inet6_url(pool, u);
    }

    return nt_parse_inet_url(pool, u);
}


static nt_int_t
nt_parse_unix_domain_url(nt_pool_t *pool, nt_url_t *u)
{
#if (NT_HAVE_UNIX_DOMAIN)
    u_char              *path, *uri, *last;
    size_t               len;
    struct sockaddr_un  *saun;

    len = u->url.len;
    path = u->url.data;

    path += 5;
    len -= 5;

    if (u->uri_part) {

        last = path + len;
        uri = nt_strlchr(path, last, ':');

        if (uri) {
            len = uri - path;
            uri++;
            u->uri.len = last - uri;
            u->uri.data = uri;
        }
    }

    if (len == 0) {
        u->err = "no path in the unix domain socket";
        return NT_ERROR;
    }

    u->host.len = len++;
    u->host.data = path;

    if (len > sizeof(saun->sun_path)) {
        u->err = "too long path in the unix domain socket";
        return NT_ERROR;
    }

    u->socklen = sizeof(struct sockaddr_un);
    saun = (struct sockaddr_un *) &u->sockaddr;
    saun->sun_family = AF_UNIX;
    (void) nt_cpystrn((u_char *) saun->sun_path, path, len);

    u->addrs = nt_pcalloc(pool, sizeof(nt_addr_t));
    if (u->addrs == NULL) {
        return NT_ERROR;
    }

    saun = nt_pcalloc(pool, sizeof(struct sockaddr_un));
    if (saun == NULL) {
        return NT_ERROR;
    }

    u->family = AF_UNIX;
    u->naddrs = 1;

    saun->sun_family = AF_UNIX;
    (void) nt_cpystrn((u_char *) saun->sun_path, path, len);

    u->addrs[0].sockaddr = (struct sockaddr *) saun;
    u->addrs[0].socklen = sizeof(struct sockaddr_un);
    u->addrs[0].name.len = len + 4;
    u->addrs[0].name.data = u->url.data;

    return NT_OK;

#else

    u->err = "the unix domain sockets are not supported on this platform";

    return NT_ERROR;

#endif
}


static nt_int_t
nt_parse_inet_url(nt_pool_t *pool, nt_url_t *u)
{
    u_char              *host, *port, *last, *uri, *args, *dash;
    size_t               len;
    nt_int_t            n;
    struct sockaddr_in  *sin;

    u->socklen = sizeof(struct sockaddr_in);
    sin = (struct sockaddr_in *) &u->sockaddr;
    sin->sin_family = AF_INET;

    u->family = AF_INET;

    host = u->url.data;

    last = host + u->url.len;

    port = nt_strlchr(host, last, ':');

    uri = nt_strlchr(host, last, '/');

    args = nt_strlchr(host, last, '?');

    if (args) {
        if (uri == NULL || args < uri) {
            uri = args;
        }
    }

    if (uri) {
        if (u->listen || !u->uri_part) {
            u->err = "invalid host";
            return NT_ERROR;
        }

        u->uri.len = last - uri;
        u->uri.data = uri;

        last = uri;

        if (uri < port) {
            port = NULL;
        }
    }

    if (port) {
        port++;

        len = last - port;

        if (u->listen) {
            dash = nt_strlchr(port, last, '-');

            if (dash) {
                dash++;

                n = nt_atoi(dash, last - dash);

                if (n < 1 || n > 65535) {
                    u->err = "invalid port";
                    return NT_ERROR;
                }

                u->last_port = (in_port_t) n;

                len = dash - port - 1;
            }
        }

        n = nt_atoi(port, len);

        if (n < 1 || n > 65535) {
            u->err = "invalid port";
            return NT_ERROR;
        }

        if (u->last_port && n > u->last_port) {
            u->err = "invalid port range";
            return NT_ERROR;
        }

        u->port = (in_port_t) n;
        sin->sin_port = htons((in_port_t) n);

        u->port_text.len = last - port;
        u->port_text.data = port;

        last = port - 1;

    } else {
        if (uri == NULL) {

            if (u->listen) {

                /* test value as port only */

                len = last - host;

                dash = nt_strlchr(host, last, '-');

                if (dash) {
                    dash++;

                    n = nt_atoi(dash, last - dash);

                    if (n == NT_ERROR) {
                        goto no_port;
                    }

                    if (n < 1 || n > 65535) {
                        u->err = "invalid port";

                    } else {
                        u->last_port = (in_port_t) n;
                    }

                    len = dash - host - 1;
                }

                n = nt_atoi(host, len);

                if (n != NT_ERROR) {

                    if (u->err) {
                        return NT_ERROR;
                    }

                    if (n < 1 || n > 65535) {
                        u->err = "invalid port";
                        return NT_ERROR;
                    }

                    if (u->last_port && n > u->last_port) {
                        u->err = "invalid port range";
                        return NT_ERROR;
                    }

                    u->port = (in_port_t) n;
                    sin->sin_port = htons((in_port_t) n);
                    sin->sin_addr.s_addr = INADDR_ANY;

                    u->port_text.len = last - host;
                    u->port_text.data = host;

                    u->wildcard = 1;

                    return nt_inet_add_addr(pool, u, &u->sockaddr.sockaddr,
                                             u->socklen, 1);
                }
            }
        }

no_port:

        u->err = NULL;
        u->no_port = 1;
        u->port = u->default_port;
        sin->sin_port = htons(u->default_port);
        u->last_port = 0;
    }

    len = last - host;

    if (len == 0) {
        u->err = "no host";
        return NT_ERROR;
    }

    u->host.len = len;
    u->host.data = host;

    if (u->listen && len == 1 && *host == '*') {
        sin->sin_addr.s_addr = INADDR_ANY;
        u->wildcard = 1;
        return nt_inet_add_addr(pool, u, &u->sockaddr.sockaddr, u->socklen, 1);
    }

    sin->sin_addr.s_addr = nt_inet_addr(host, len);

    if (sin->sin_addr.s_addr != INADDR_NONE) {

        if (sin->sin_addr.s_addr == INADDR_ANY) {
            u->wildcard = 1;
        }

        return nt_inet_add_addr(pool, u, &u->sockaddr.sockaddr, u->socklen, 1);
    }

    if (u->no_resolve) {
        return NT_OK;
    }

    if (nt_inet_resolve_host(pool, u) != NT_OK) {
        return NT_ERROR;
    }

    u->family = u->addrs[0].sockaddr->sa_family;
    u->socklen = u->addrs[0].socklen;
    nt_memcpy(&u->sockaddr, u->addrs[0].sockaddr, u->addrs[0].socklen);
    u->wildcard = nt_inet_wildcard(&u->sockaddr.sockaddr);

    return NT_OK;
}


static nt_int_t
nt_parse_inet6_url(nt_pool_t *pool, nt_url_t *u)
{
#if (NT_HAVE_INET6)
    u_char               *p, *host, *port, *last, *uri, *dash;
    size_t                len;
    nt_int_t             n;
    struct sockaddr_in6  *sin6;

    u->socklen = sizeof(struct sockaddr_in6);
    sin6 = (struct sockaddr_in6 *) &u->sockaddr;
    sin6->sin6_family = AF_INET6;

    host = u->url.data + 1;

    last = u->url.data + u->url.len;

    p = nt_strlchr(host, last, ']');

    if (p == NULL) {
        u->err = "invalid host";
        return NT_ERROR;
    }

    port = p + 1;

    uri = nt_strlchr(port, last, '/');

    if (uri) {
        if (u->listen || !u->uri_part) {
            u->err = "invalid host";
            return NT_ERROR;
        }

        u->uri.len = last - uri;
        u->uri.data = uri;

        last = uri;
    }

    if (port < last) {
        if (*port != ':') {
            u->err = "invalid host";
            return NT_ERROR;
        }

        port++;

        len = last - port;

        if (u->listen) {
            dash = nt_strlchr(port, last, '-');

            if (dash) {
                dash++;

                n = nt_atoi(dash, last - dash);

                if (n < 1 || n > 65535) {
                    u->err = "invalid port";
                    return NT_ERROR;
                }

                u->last_port = (in_port_t) n;

                len = dash - port - 1;
            }
        }

        n = nt_atoi(port, len);

        if (n < 1 || n > 65535) {
            u->err = "invalid port";
            return NT_ERROR;
        }

        if (u->last_port && n > u->last_port) {
            u->err = "invalid port range";
            return NT_ERROR;
        }

        u->port = (in_port_t) n;
        sin6->sin6_port = htons((in_port_t) n);

        u->port_text.len = last - port;
        u->port_text.data = port;

    } else {
        u->no_port = 1;
        u->port = u->default_port;
        sin6->sin6_port = htons(u->default_port);
    }

    len = p - host;

    if (len == 0) {
        u->err = "no host";
        return NT_ERROR;
    }

    u->host.len = len + 2;
    u->host.data = host - 1;

    if (nt_inet6_addr(host, len, sin6->sin6_addr.s6_addr) != NT_OK) {
        u->err = "invalid IPv6 address";
        return NT_ERROR;
    }

    if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
        u->wildcard = 1;
    }

    u->family = AF_INET6;

    return nt_inet_add_addr(pool, u, &u->sockaddr.sockaddr, u->socklen, 1);

#else

    u->err = "the INET6 sockets are not supported on this platform";

    return NT_ERROR;

#endif
}


#if (T_NT_DNS_RESOLVE_BACKUP)
static nt_int_t
nt_resolve_backup(nt_pool_t *pool, nt_url_t **u, nt_str_t path)
{
    u_char                           buf[70], *p, *pos;
    size_t                           len;
    nt_url_t                       *url;
    nt_uint_t                       i;
    nt_fd_t                         fd;
    nt_int_t                        err;
    nt_str_t                        tpathf, dpathf, q;
    static nt_int_t                 create_dir_flag = 1;

    url = *u;
    if (path.data && path.data[path.len - 1] != '/') {
        p = nt_pcalloc(pool, path.len + 1);
        if (p == NULL) {
            return NT_ERROR;
        }

        path.len = nt_sprintf(p, "%V/", &path) - p;
        path.data = p;
    }

    if (nt_conf_full_name((nt_cycle_t *) nt_cycle, &path, 0) != NT_OK) {
        return NT_ERROR;
    }

    if (create_dir_flag) {
        err = nt_create_full_path(path.data, 0755);
        if (err != 0) {
            nt_log_error(NT_LOG_ERR, nt_cycle->log, nt_errno,
                          nt_create_dir_n " \"%s\" failed",
                          path.data);
            return NT_ERROR;
        }
        create_dir_flag = 0;
    }

    tpathf.len = path.len + url->host.len + NT_INT_T_LEN + 2;
    tpathf.data = nt_pcalloc(pool, tpathf.len);
    if (tpathf.data == NULL) {
        return NT_ERROR;
    }

    tpathf.len = nt_snprintf(tpathf.data, tpathf.len, "%V%V.%d%Z", &path,
                              &url->host, nt_pid) - tpathf.data;

    dpathf.len = path.len + url->host.len;
    dpathf.data = nt_pstrdup(pool, &tpathf);
    dpathf.data[dpathf.len] = '\0';

    fd = nt_open_file(tpathf.data, NT_FILE_WRONLY,
                       NT_FILE_TRUNCATE, NT_FILE_DEFAULT_ACCESS);
    if (fd == NT_INVALID_FILE) {
        nt_log_error(NT_LOG_ERR, nt_cycle->log, nt_errno,
                      nt_open_file_n " \"%s\" failed",
                      tpathf.data);

        return NT_ERROR;
    }

    for (i = 0; i < url->naddrs; i++) {
        if (url->addrs[i].sockaddr->sa_family == AF_INET) {
            pos = nt_strlchr(url->addrs[i].name.data, url->addrs[i].name.data
                              + url->addrs[i].name.len, ':');
            if (pos == NULL) {
                pos = url->addrs[i].name.data + url->addrs[i].name.len;
            }

            q.data = url->addrs[i].name.data;
            q.len = pos - url->addrs[i].name.data;

        } else if (url->addrs[i].sockaddr->sa_family == AF_INET6) {
            pos = nt_strlchr(url->addrs[i].name.data, url->addrs[i].name.data
                              + url->addrs[i].name.len, ']');
            if (pos == NULL) {
                continue;
            }
            q.data = url->addrs[i].name.data + 1;
            q.len = pos - q.data;
        }

        len = nt_snprintf(buf, 69, "%V|%d%N", &q,
                           url->addrs[i].sockaddr->sa_family) - buf;
        nt_write_fd(fd, buf, len);
    }

    nt_close_file(fd);

    if (nt_rename_file(tpathf.data, dpathf.data) == NT_FILE_ERROR) {
        nt_log_error(NT_LOG_EMERG, nt_cycle->log, nt_errno,
                      nt_rename_file_n" from \"%s\" to \"%s\" failed",
                      tpathf.data, dpathf.data);
        return NT_ERROR;
    }

    return NT_OK;
}


static nt_int_t
nt_resolve_using_local(nt_pool_t *pool, nt_url_t **u, nt_str_t path)
{
    u_char                              *buf, *p, *s, *q, *l;
    size_t                               len;
    nt_uint_t                           i;
    nt_str_t                            pf;
    nt_fd_t                             fd;
    nt_uint_t                           size, ip, sin_family;
    nt_file_info_t                      info;
    nt_url_t                           *url;
    struct sockaddr_in                  *sin;
#if (NT_HAVE_INET6)
    struct sockaddr_in6                 *sin6;
    u_char                              *v6;
#endif

    url = *u;
    if (path.data && path.data[path.len - 1] != '/') {
        p = nt_pcalloc(pool, path.len + 1);
        if (p == NULL) {
            return NT_ERROR;
        }

        path.len = nt_sprintf(p, "%V/", &path) - p;
        path.data = p;
    }

    pf.len = path.len + url->host.len;
    pf.data = nt_pcalloc(pool, pf.len + 1);
    if (pf.data == NULL) {
        return NT_ERROR;
    }

    nt_snprintf(pf.data, pf.len, "%V%*s",
                 &path, url->host.len, url->host.data);

    if (nt_conf_full_name((nt_cycle_t *) nt_cycle, &pf, 0) != NT_OK) {
        return NT_ERROR;
    }

    fd = nt_open_file(pf.data, NT_FILE_RDONLY, NT_FILE_OPEN, 0);
    if (fd == NT_INVALID_FILE) {
        return NT_ERROR;
    }

    if (nt_fd_info(fd, &info) == -1) {
        nt_close_file(fd);
        nt_log_error(NT_LOG_ERR, nt_cycle->log, nt_errno,
                      nt_fd_info_n " \"%V\" failed", &pf);
        return NT_ERROR;
    }

    /* fix: maybe disk full */
    size = nt_file_size(&info);
    if (size == 0) {
        nt_close_file(fd);
        nt_log_error(NT_LOG_ERR, nt_cycle->log, 0,
                      "dns : the cache \"%V\" is empty file", &pf);
        return NT_ERROR;
    }

    buf = nt_palloc(pool, size);
    if (buf == NULL) {
        nt_close_file(fd);
        return NT_ERROR;
    }

    if (nt_read_fd(fd, buf, size) == -1) {
        nt_close_file(fd);
        return NT_ERROR;
    }

    nt_close_file(fd);

    for(i = 0, p = buf; p < buf + size; i++) {
        s = nt_strlchr(p, buf + size, '\n');
        if (s == NULL) {
            break;
        }

        p = s + 1;
    }

    url->addrs = nt_pcalloc(pool, i * sizeof(nt_addr_t));
    if (url->addrs == NULL) {
        return NT_ERROR;
    }

    url->naddrs = 0;

    for (i = 0, p = buf; p < buf + size; ) {
        s = nt_strlchr(p, buf + size, '\n');
        if (s == NULL) {
            s = buf + size + 1;
        }

        l = nt_strlchr(p, s, '|');
        if (l == NULL) {
            l = s;
        }

        if (l != s) {
            sin_family = nt_atoi(l + 1, s - l - 1);
            if (sin_family != AF_INET && sin_family != AF_INET6 ) {
                p = s + 1;
                continue;
            }

        } else {
            p = s + 1;
            continue;
        }

        if (sin_family == AF_INET) {
            ip = nt_inet_addr(p, l - p);
            if (ip == INADDR_NONE) {
                nt_log_error(NT_LOG_ERR, nt_cycle->log, 0,
                              "dns: failed to parse ip \"%*s\"",
                              s - p, p);
                p = s + 1;
                continue;
            }

            sin = nt_pcalloc(pool, sizeof(struct sockaddr_in));
            if (sin == NULL) {
                return NT_ERROR;
            }

            sin->sin_family = sin_family;
            sin->sin_port = htons(url->port);
            sin->sin_addr.s_addr = ip;

            url->addrs[i].sockaddr = (struct sockaddr *) sin;
            url->addrs[i].socklen = sizeof(struct sockaddr_in);

            len = NT_INET_ADDRSTRLEN + sizeof(":65535") - 1;

            q = nt_pnalloc(pool, len);
            if (q == NULL) {
                return NT_ERROR;
            }

            len = nt_sock_ntop((struct sockaddr *) sin,
                                sizeof(struct sockaddr_in), q, len, 1);

            url->addrs[i].name.len = len;
            url->addrs[i].name.data = q;
            i++;

        } else if (sin_family == AF_INET6) {
#if (NT_HAVE_INET6)
            v6 = p;
            len = l - p;

            sin6 = nt_pcalloc(pool, sizeof(struct sockaddr_in6));
            if (sin6 == NULL) {
                return NT_ERROR;
            }

            if (nt_inet6_addr(v6, len, sin6->sin6_addr.s6_addr) != NT_OK) {
                p = s + 1;
                continue;
            }

            sin6->sin6_family = sin_family;
            sin6->sin6_port = htons(url->port);

            url->addrs[i].sockaddr = (struct sockaddr *) sin6;
            url->addrs[i].socklen = sizeof(struct sockaddr_in6);

            len = NT_INET6_ADDRSTRLEN + sizeof("[]:65535") - 1;

            q = nt_pnalloc(pool, len);
            if (q == NULL) {
                return NT_ERROR;
            }

            len = nt_sock_ntop((struct sockaddr *) sin6,
                                sizeof(struct sockaddr), q, len, 1);

            url->addrs[i].name.len = len;
            url->addrs[i].name.data = q;
            i++;
#endif
        }

        p = s + 1;

    }

    if (i == 0) {
        return NT_ERROR;
    }

    url->naddrs = i;

    return NT_OK;

}
#endif


#if (NT_HAVE_GETADDRINFO && NT_HAVE_INET6)

nt_int_t
nt_inet_resolve_host(nt_pool_t *pool, nt_url_t *u)
{
    u_char           *host;
    nt_uint_t        n;
    struct addrinfo   hints, *res, *rp;
#if (T_NT_DNS_RESOLVE_BACKUP)
    u_char           *ph;
    nt_str_t         path;
#endif

    host = nt_alloc(u->host.len + 1, pool->log);
    if (host == NULL) {
        return NT_ERROR;
    }

    (void) nt_cpystrn(host, u->host.data, u->host.len + 1);

    nt_memzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
#ifdef AI_ADDRCONFIG
    hints.ai_flags = AI_ADDRCONFIG;
#endif

    if (getaddrinfo((char *) host, NULL, &hints, &res) != 0) {
#if (T_NT_DNS_RESOLVE_BACKUP)
        ph = (u_char *) getenv(NT_DNS_RESOLVE_BACKUP_PATH);
        if (ph != NULL) {
            path.data = ph;
            path.len = nt_strlen(ph);
            if (nt_resolve_using_local(pool, &u, path) == NT_OK) {
                nt_log_error(NT_LOG_WARN, nt_cycle->log, 0,
                              "dom %V using local dns cache successed",
                              &u->host);
                nt_free(host);
                return NT_OK;
            }
        }
#endif
        u->err = "host not found";
        nt_free(host);
        return NT_ERROR;
    }

    nt_free(host);

    for (n = 0, rp = res; rp != NULL; rp = rp->ai_next) {

        switch (rp->ai_family) {

        case AF_INET:
        case AF_INET6:
            break;

        default:
            continue;
        }

        n++;
    }

    if (n == 0) {
        u->err = "host not found";
        goto failed;
    }

    /* MP: nt_shared_palloc() */

    for (rp = res; rp != NULL; rp = rp->ai_next) {

        switch (rp->ai_family) {

        case AF_INET:
        case AF_INET6:
            break;

        default:
            continue;
        }

        if (nt_inet_add_addr(pool, u, rp->ai_addr, rp->ai_addrlen, n)
            != NT_OK)
        {
            goto failed;
        }
    }

    freeaddrinfo(res);

#if (T_NT_DNS_RESOLVE_BACKUP)
    ph = (u_char *) getenv(NT_DNS_RESOLVE_BACKUP_PATH);
    if (ph != NULL) {
        path.data = ph;
        path.len = nt_strlen(ph);
        if (nt_resolve_backup(pool, &u, path) != NT_OK) {
            nt_log_error(NT_LOG_WARN, nt_cycle->log, 0,
                          "dom %V backup local dns cache failed", &u->host);
        }
    }
#endif

    return NT_OK;

failed:

    freeaddrinfo(res);
    return NT_ERROR;
}

#else /* !NT_HAVE_GETADDRINFO || !NT_HAVE_INET6 */

nt_int_t
nt_inet_resolve_host(nt_pool_t *pool, nt_url_t *u)
{
    u_char              *host;
    nt_uint_t           i, n;
    struct hostent      *h;
    struct sockaddr_in   sin;
#if (T_NT_DNS_RESOLVE_BACKUP)
    u_char              *ph;
    nt_str_t            path;
#endif

    /* AF_INET only */

    nt_memzero(&sin, sizeof(struct sockaddr_in));

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = nt_inet_addr(u->host.data, u->host.len);

    if (sin.sin_addr.s_addr == INADDR_NONE) {
        host = nt_alloc(u->host.len + 1, pool->log);
        if (host == NULL) {
            return NT_ERROR;
        }

        (void) nt_cpystrn(host, u->host.data, u->host.len + 1);

        h = gethostbyname((char *) host);

#if (T_NT_DNS_RESOLVE_BACKUP)
        ph = (u_char *) getenv(NT_DNS_RESOLVE_BACKUP_PATH);
        if (h == NULL && ph != NULL) {
            path.data = ph;
            path.len = nt_strlen(ph);
            if (nt_resolve_using_local(pool, &u, path) == NT_OK) {
                nt_log_error(NT_LOG_WARN, nt_cycle->log, 0,
                              "dom %V using local dns cache successed",
                              &u->host);
                nt_free(host);
                return NT_OK;
            }
        }
#endif

        nt_free(host);

        if (h == NULL || h->h_addr_list[0] == NULL) {
            u->err = "host not found";
            return NT_ERROR;
        }

        for (n = 0; h->h_addr_list[n] != NULL; n++) { /* void */ }

        /* MP: nt_shared_palloc() */

        for (i = 0; i < n; i++) {
            sin.sin_addr.s_addr = *(in_addr_t *) (h->h_addr_list[i]);

            if (nt_inet_add_addr(pool, u, (struct sockaddr *) &sin,
                                  sizeof(struct sockaddr_in), n)
                != NT_OK)
            {
                return NT_ERROR;
            }
        }

#if (T_NT_DNS_RESOLVE_BACKUP)
        ph = (u_char *) getenv(NT_DNS_RESOLVE_BACKUP_PATH);
        if (ph != NULL) {
            path.data = ph;
            path.len = nt_strlen(ph);
            if (nt_resolve_backup(pool, &u, path) != NT_OK) {
                nt_log_error(NT_LOG_WARN, nt_cycle->log, 0,
                              "dom %V backup local dns cache failed",
                              &u->host);
            }
        }
#endif

    } else {

        /* MP: nt_shared_palloc() */

        if (nt_inet_add_addr(pool, u, (struct sockaddr *) &sin,
                              sizeof(struct sockaddr_in), 1)
            != NT_OK)
        {
            return NT_ERROR;
        }
    }

    return NT_OK;
}

#endif /* NT_HAVE_GETADDRINFO && NT_HAVE_INET6 */

static nt_int_t
nt_inet_add_addr(nt_pool_t *pool, nt_url_t *u, struct sockaddr *sockaddr,
    socklen_t socklen, nt_uint_t total)
{
    u_char           *p;
    size_t            len;
    nt_uint_t        i, nports;
    nt_addr_t       *addr;
    struct sockaddr  *sa;

    nports = u->last_port ? u->last_port - u->port + 1 : 1;

    if (u->addrs == NULL) {
        u->addrs = nt_palloc(pool, total * nports * sizeof(nt_addr_t));
        if (u->addrs == NULL) {
            return NT_ERROR;
        }
    }

    for (i = 0; i < nports; i++) {
        sa = nt_pcalloc(pool, socklen);
        if (sa == NULL) {
            return NT_ERROR;
        }

        nt_memcpy(sa, sockaddr, socklen);

        nt_inet_set_port(sa, u->port + i);

        switch (sa->sa_family) {

#if (NT_HAVE_INET6)
        case AF_INET6:
            len = NT_INET6_ADDRSTRLEN + sizeof("[]:65536") - 1;
            break;
#endif

        default: /* AF_INET */
            len = NT_INET_ADDRSTRLEN + sizeof(":65535") - 1;
        }

        p = nt_pnalloc(pool, len);
        if (p == NULL) {
            return NT_ERROR;
        }

        len = nt_sock_ntop(sa, socklen, p, len, 1);

        addr = &u->addrs[u->naddrs++];

        addr->sockaddr = sa;
        addr->socklen = socklen;

        addr->name.len = len;
        addr->name.data = p;
    }

    return NT_OK;
}


nt_int_t
nt_cmp_sockaddr(struct sockaddr *sa1, socklen_t slen1,
    struct sockaddr *sa2, socklen_t slen2, nt_uint_t cmp_port)
{
    struct sockaddr_in   *sin1, *sin2;
#if (NT_HAVE_INET6)
    struct sockaddr_in6  *sin61, *sin62;
#endif
#if (NT_HAVE_UNIX_DOMAIN)
    size_t                len;
    struct sockaddr_un   *saun1, *saun2;
#endif

    if (sa1->sa_family != sa2->sa_family) {
        return NT_DECLINED;
    }

    switch (sa1->sa_family) {

#if (NT_HAVE_INET6)
    case AF_INET6:

        sin61 = (struct sockaddr_in6 *) sa1;
        sin62 = (struct sockaddr_in6 *) sa2;

        if (cmp_port && sin61->sin6_port != sin62->sin6_port) {
            return NT_DECLINED;
        }

        if (nt_memcmp(&sin61->sin6_addr, &sin62->sin6_addr, 16) != 0) {
            return NT_DECLINED;
        }

        break;
#endif

#if (NT_HAVE_UNIX_DOMAIN)
    case AF_UNIX:

        saun1 = (struct sockaddr_un *) sa1;
        saun2 = (struct sockaddr_un *) sa2;

        if (slen1 < slen2) {
            len = slen1 - offsetof(struct sockaddr_un, sun_path);

        } else {
            len = slen2 - offsetof(struct sockaddr_un, sun_path);
        }

        if (len > sizeof(saun1->sun_path)) {
            len = sizeof(saun1->sun_path);
        }

        if (nt_memcmp(&saun1->sun_path, &saun2->sun_path, len) != 0) {
            return NT_DECLINED;
        }

        break;
#endif

    default: /* AF_INET */

        sin1 = (struct sockaddr_in *) sa1;
        sin2 = (struct sockaddr_in *) sa2;

        if (cmp_port && sin1->sin_port != sin2->sin_port) {
            return NT_DECLINED;
        }

        if (sin1->sin_addr.s_addr != sin2->sin_addr.s_addr) {
            return NT_DECLINED;
        }

        break;
    }

    return NT_OK;
}


in_port_t
nt_inet_get_port(struct sockaddr *sa)
{
    struct sockaddr_in   *sin;
#if (NT_HAVE_INET6)
    struct sockaddr_in6  *sin6;
#endif

    switch (sa->sa_family) {

#if (NT_HAVE_INET6)
    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) sa;
        return ntohs(sin6->sin6_port);
#endif

#if (NT_HAVE_UNIX_DOMAIN)
    case AF_UNIX:
        return 0;
#endif

    default: /* AF_INET */
        sin = (struct sockaddr_in *) sa;
        return ntohs(sin->sin_port);
    }
}


void
nt_inet_set_port(struct sockaddr *sa, in_port_t port)
{
    struct sockaddr_in   *sin;
#if (NT_HAVE_INET6)
    struct sockaddr_in6  *sin6;
#endif

    switch (sa->sa_family) {

#if (NT_HAVE_INET6)
    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) sa;
        sin6->sin6_port = htons(port);
        break;
#endif

#if (NT_HAVE_UNIX_DOMAIN)
    case AF_UNIX:
        break;
#endif

    default: /* AF_INET */
        sin = (struct sockaddr_in *) sa;
        sin->sin_port = htons(port);
        break;
    }
}


nt_uint_t
nt_inet_wildcard(struct sockaddr *sa)
{
    struct sockaddr_in   *sin;
#if (NT_HAVE_INET6)
    struct sockaddr_in6  *sin6;
#endif

    switch (sa->sa_family) {

    case AF_INET:
        sin = (struct sockaddr_in *) sa;

        if (sin->sin_addr.s_addr == INADDR_ANY) {
            return 1;
        }

        break;

#if (NT_HAVE_INET6)

    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) sa;

        if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
            return 1;
        }

        break;

#endif
    }

    return 0;
}
