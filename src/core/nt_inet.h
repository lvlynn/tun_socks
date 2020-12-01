
#ifndef _NT_INET_H_INCLUDED_
#define _NT_INET_H_INCLUDED_


#include <nt_core.h>


#define NT_INET_ADDRSTRLEN   (sizeof("255.255.255.255") - 1)
#define NT_INET6_ADDRSTRLEN                                                 \
    (sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255") - 1)
#define NT_UNIX_ADDRSTRLEN                                                  \
    (sizeof("unix:") - 1 +                                                   \
     sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path))

#if (NT_HAVE_UNIX_DOMAIN)
#define NT_SOCKADDR_STRLEN   NT_UNIX_ADDRSTRLEN
#elif (NT_HAVE_INET6)
#define NT_SOCKADDR_STRLEN   (NT_INET6_ADDRSTRLEN + sizeof("[]:65535") - 1)
#else
#define NT_SOCKADDR_STRLEN   (NT_INET_ADDRSTRLEN + sizeof(":65535") - 1)
#endif

/* compatibility */
#define NT_SOCKADDRLEN       sizeof(nt_sockaddr_t)


typedef union {
    struct sockaddr           sockaddr;
    struct sockaddr_in        sockaddr_in;
#if (NT_HAVE_INET6)
    struct sockaddr_in6       sockaddr_in6;
#endif
#if (NT_HAVE_UNIX_DOMAIN)
    struct sockaddr_un        sockaddr_un;
#endif
} nt_sockaddr_t;


#if (T_NT_DNS_RESOLVE_BACKUP)
#define NT_DNS_RESOLVE_BACKUP_PATH             "NT_DNS_RESOLVE_BACKUP_PATH"
#endif


typedef struct {
    in_addr_t                 addr;
    in_addr_t                 mask;
} nt_in_cidr_t;


#if (NT_HAVE_INET6)

typedef struct {
    struct in6_addr           addr;
    struct in6_addr           mask;
} nt_in6_cidr_t;

#endif


typedef struct {
    nt_uint_t                family;
    union {
        nt_in_cidr_t         in;
#if (NT_HAVE_INET6)
        nt_in6_cidr_t        in6;
#endif
    } u;
} nt_cidr_t;


typedef struct {
    struct sockaddr          *sockaddr;
    socklen_t                 socklen;
    nt_str_t                 name;
} nt_addr_t;


typedef struct {
    nt_str_t                 url;
    nt_str_t                 host;
    nt_str_t                 port_text;
    nt_str_t                 uri;

    in_port_t                 port;
    in_port_t                 default_port;
    in_port_t                 last_port;
    int                       family;

    unsigned                  listen:1;
    unsigned                  uri_part:1;
    unsigned                  no_resolve:1;

    unsigned                  no_port:1;
    unsigned                  wildcard:1;

    socklen_t                 socklen;
    nt_sockaddr_t            sockaddr;

    nt_addr_t               *addrs;
    nt_uint_t                naddrs;

    char                     *err;
} nt_url_t;


in_addr_t nt_inet_addr(u_char *text, size_t len);
#if (NT_HAVE_INET6)
nt_int_t nt_inet6_addr(u_char *p, size_t len, u_char *addr);
size_t nt_inet6_ntop(u_char *p, u_char *text, size_t len);
#endif
size_t nt_sock_ntop(struct sockaddr *sa, socklen_t socklen, u_char *text,
    size_t len, nt_uint_t port);
size_t nt_inet_ntop(int family, void *addr, u_char *text, size_t len);
nt_int_t nt_ptocidr(nt_str_t *text, nt_cidr_t *cidr);
nt_int_t nt_cidr_match(struct sockaddr *sa, nt_array_t *cidrs);
nt_int_t nt_parse_addr(nt_pool_t *pool, nt_addr_t *addr, u_char *text,
    size_t len);
nt_int_t nt_parse_addr_port(nt_pool_t *pool, nt_addr_t *addr,
    u_char *text, size_t len);
nt_int_t nt_parse_url(nt_pool_t *pool, nt_url_t *u);
nt_int_t nt_inet_resolve_host(nt_pool_t *pool, nt_url_t *u);
nt_int_t nt_cmp_sockaddr(struct sockaddr *sa1, socklen_t slen1,
    struct sockaddr *sa2, socklen_t slen2, nt_uint_t cmp_port);
in_port_t nt_inet_get_port(struct sockaddr *sa);
void nt_inet_set_port(struct sockaddr *sa, in_port_t port);
nt_uint_t nt_inet_wildcard(struct sockaddr *sa);


#endif /* _NT_INET_H_INCLUDED_ */
