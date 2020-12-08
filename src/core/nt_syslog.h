
/*
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NT_SYSLOG_H_INCLUDED_
#define _NT_SYSLOG_H_INCLUDED_


typedef struct {
    nt_uint_t        facility;
    nt_uint_t        severity;
    nt_str_t         tag;

    nt_addr_t        server;
    nt_connection_t  conn;
    unsigned          busy:1;
    unsigned          nohostname:1;
} nt_syslog_peer_t;


char *nt_syslog_process_conf(nt_conf_t *cf, nt_syslog_peer_t *peer);
u_char *nt_syslog_add_header(nt_syslog_peer_t *peer, u_char *buf);
void nt_syslog_writer(nt_log_t *log, nt_uint_t level, u_char *buf,
    size_t len);
ssize_t nt_syslog_send(nt_syslog_peer_t *peer, u_char *buf, size_t len);


#endif /* _NT_SYSLOG_H_INCLUDED_ */
