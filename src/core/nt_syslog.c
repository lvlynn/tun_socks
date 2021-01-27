
#include <nt_core.h>
#include <nt_event.h>


#define NT_SYSLOG_MAX_STR                                                    \
    NT_MAX_ERROR_STR + sizeof("<255>Jan 01 00:00:00 ") - 1                   \
    + (NT_MAXHOSTNAMELEN - 1) + 1 /* space */                                \
    + 32 /* tag */ + 2 /* colon, space */


static char *nt_syslog_parse_args(nt_conf_t *cf, nt_syslog_peer_t *peer);
static nt_int_t nt_syslog_init_peer(nt_syslog_peer_t *peer);
static void nt_syslog_cleanup(void *data);


static char  *facilities[] = {
    "kern", "user", "mail", "daemon", "auth", "intern", "lpr", "news", "uucp",
    "clock", "authpriv", "ftp", "ntp", "audit", "alert", "cron", "local0",
    "local1", "local2", "local3", "local4", "local5", "local6", "local7",
    NULL
};

/* note 'error/warn' like in nginx.conf, not 'err/warning' */
static char  *severities[] = {
    "emerg", "alert", "crit", "error", "warn", "notice", "info", "debug", NULL
};

static nt_log_t    nt_syslog_dummy_log;
static nt_event_t  nt_syslog_dummy_event;


char *
nt_syslog_process_conf(nt_conf_t *cf, nt_syslog_peer_t *peer)
{
    nt_pool_cleanup_t  *cln;

    peer->facility = NT_CONF_UNSET_UINT;
    peer->severity = NT_CONF_UNSET_UINT;

    if (nt_syslog_parse_args(cf, peer) != NT_CONF_OK) {
        return NT_CONF_ERROR;
    }

    if (peer->server.sockaddr == NULL) {
        nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                           "no syslog server specified");
        return NT_CONF_ERROR;
    }

    if (peer->facility == NT_CONF_UNSET_UINT) {
        peer->facility = 23; /* local7 */
    }

    if (peer->severity == NT_CONF_UNSET_UINT) {
        peer->severity = 6; /* info */
    }

    if (peer->tag.data == NULL) {
        nt_str_set(&peer->tag, "nginx");
    }

    peer->conn.fd = (nt_socket_t) -1;

    peer->conn.read = &nt_syslog_dummy_event;
    peer->conn.write = &nt_syslog_dummy_event;

    nt_syslog_dummy_event.log = &nt_syslog_dummy_log;

    cln = nt_pool_cleanup_add(cf->pool, 0);
    if (cln == NULL) {
        return NT_CONF_ERROR;
    }

    cln->data = peer;
    cln->handler = nt_syslog_cleanup;

    return NT_CONF_OK;
}


static char *
nt_syslog_parse_args(nt_conf_t *cf, nt_syslog_peer_t *peer)
{
    u_char      *p, *comma, c;
    size_t       len;
    nt_str_t   *value;
    nt_url_t    u;
    nt_uint_t   i;

    value = cf->args->elts;

    p = value[1].data + sizeof("syslog:") - 1;

    for ( ;; ) {
        comma = (u_char *) nt_strchr(p, ',');

        if (comma != NULL) {
            len = comma - p;
            *comma = '\0';

        } else {
            len = value[1].data + value[1].len - p;
        }

        if (nt_strncmp(p, "server=", 7) == 0) {

            if (peer->server.sockaddr != NULL) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "duplicate syslog \"server\"");
                return NT_CONF_ERROR;
            }

            nt_memzero(&u, sizeof(nt_url_t));

            u.url.data = p + 7;
            u.url.len = len - 7;
            u.default_port = 514;

            if (nt_parse_url(cf->pool, &u) != NT_OK) {
                if (u.err) {
                    nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                       "%s in syslog server \"%V\"",
                                       u.err, &u.url);
                }

                return NT_CONF_ERROR;
            }

            peer->server = u.addrs[0];

        } else if (nt_strncmp(p, "facility=", 9) == 0) {

            if (peer->facility != NT_CONF_UNSET_UINT) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "duplicate syslog \"facility\"");
                return NT_CONF_ERROR;
            }

            for (i = 0; facilities[i] != NULL; i++) {

                if (nt_strcmp(p + 9, facilities[i]) == 0) {
                    peer->facility = i;
                    goto next;
                }
            }

            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "unknown syslog facility \"%s\"", p + 9);
            return NT_CONF_ERROR;

        } else if (nt_strncmp(p, "severity=", 9) == 0) {

            if (peer->severity != NT_CONF_UNSET_UINT) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "duplicate syslog \"severity\"");
                return NT_CONF_ERROR;
            }

            for (i = 0; severities[i] != NULL; i++) {

                if (nt_strcmp(p + 9, severities[i]) == 0) {
                    peer->severity = i;
                    goto next;
                }
            }

            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "unknown syslog severity \"%s\"", p + 9);
            return NT_CONF_ERROR;

        } else if (nt_strncmp(p, "tag=", 4) == 0) {

            if (peer->tag.data != NULL) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "duplicate syslog \"tag\"");
                return NT_CONF_ERROR;
            }

            /*
             * RFC 3164: the TAG is a string of ABNF alphanumeric characters
             * that MUST NOT exceed 32 characters.
             */
            if (len - 4 > 32) {
                nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                   "syslog tag length exceeds 32");
                return NT_CONF_ERROR;
            }

            for (i = 4; i < len; i++) {
                c = nt_tolower(p[i]);

                if (c < '0' || (c > '9' && c < 'a' && c != '_') || c > 'z') {
                    nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                                       "syslog \"tag\" only allows "
                                       "alphanumeric characters "
                                       "and underscore");
                    return NT_CONF_ERROR;
                }
            }

            peer->tag.data = p + 4;
            peer->tag.len = len - 4;

        } else if (len == 10 && nt_strncmp(p, "nohostname", 10) == 0) {
            peer->nohostname = 1;

        } else {
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "unknown syslog parameter \"%s\"", p);
            return NT_CONF_ERROR;
        }

    next:

        if (comma == NULL) {
            break;
        }

        p = comma + 1;
    }

    return NT_CONF_OK;
}


u_char *
nt_syslog_add_header(nt_syslog_peer_t *peer, u_char *buf)
{
    nt_uint_t  pri;

    pri = peer->facility * 8 + peer->severity;

    if (peer->nohostname) {
        return nt_sprintf(buf, "<%ui>%V %V: ", pri, &nt_cached_syslog_time,
                           &peer->tag);
    }

    return nt_sprintf(buf, "<%ui>%V %V %V: ", pri, &nt_cached_syslog_time,
                       &nt_cycle->hostname, &peer->tag);
}


void
nt_syslog_writer(nt_log_t *log, nt_uint_t level, u_char *buf,
    size_t len)
{
    u_char             *p, msg[NT_SYSLOG_MAX_STR];
    nt_uint_t          head_len;
    nt_syslog_peer_t  *peer;

    peer = log->wdata;

    if (peer->busy) {
        return;
    }

    peer->busy = 1;
    peer->severity = level - 1;

    p = nt_syslog_add_header(peer, msg);
    head_len = p - msg;

    len -= NT_LINEFEED_SIZE;

    if (len > NT_SYSLOG_MAX_STR - head_len) {
        len = NT_SYSLOG_MAX_STR - head_len;
    }

    p = nt_snprintf(p, len, "%s", buf);

    (void) nt_syslog_send(peer, msg, p - msg);

    peer->busy = 0;
}


ssize_t
nt_syslog_send(nt_syslog_peer_t *peer, u_char *buf, size_t len)
{
    ssize_t  n;

    if (peer->conn.fd == (nt_socket_t) -1) {
        if (nt_syslog_init_peer(peer) != NT_OK) {
            return NT_ERROR;
        }
    }

    /* log syslog socket events with valid log */
    peer->conn.log = nt_cycle->log;

    if (nt_send) {
        n = nt_send(&peer->conn, buf, len);

    } else {
        /* event module has not yet set nt_io */
        n = nt_os_io.send(&peer->conn, buf, len);
    }

    if (n == NT_ERROR) {

        if (nt_close_socket(peer->conn.fd) == -1) {
            nt_log_error(NT_LOG_ALERT, nt_cycle->log, nt_socket_errno,
                          nt_close_socket_n " failed");
        }

        peer->conn.fd = (nt_socket_t) -1;
    }

    return n;
}


static nt_int_t
nt_syslog_init_peer(nt_syslog_peer_t *peer)
{
    nt_socket_t  fd;

    fd = nt_socket(peer->server.sockaddr->sa_family, SOCK_DGRAM, 0);
    if (fd == (nt_socket_t) -1) {
        nt_log_error(NT_LOG_ALERT, nt_cycle->log, nt_socket_errno,
                      nt_socket_n " failed");
        return NT_ERROR;
    }

    if (nt_nonblocking(fd) == -1) {
        nt_log_error(NT_LOG_ALERT, nt_cycle->log, nt_socket_errno,
                      nt_nonblocking_n " failed");
        goto failed;
    }

    if (connect(fd, peer->server.sockaddr, peer->server.socklen) == -1) {
        nt_log_error(NT_LOG_ALERT, nt_cycle->log, nt_socket_errno,
                      "connect() failed");
        goto failed;
    }

    peer->conn.fd = fd;

    /* UDP sockets are always ready to write */
    peer->conn.write->ready = 1;

    return NT_OK;

failed:

    if (nt_close_socket(fd) == -1) {
        nt_log_error(NT_LOG_ALERT, nt_cycle->log, nt_socket_errno,
                      nt_close_socket_n " failed");
    }

    return NT_ERROR;
}


static void
nt_syslog_cleanup(void *data)
{
    nt_syslog_peer_t  *peer = data;

    /* prevents further use of this peer */
    peer->busy = 1;

    if (peer->conn.fd == (nt_socket_t) -1) {
        return;
    }

    if (nt_close_socket(peer->conn.fd) == -1) {
        nt_log_error(NT_LOG_ALERT, nt_cycle->log, nt_socket_errno,
                      nt_close_socket_n " failed");
    }
}
