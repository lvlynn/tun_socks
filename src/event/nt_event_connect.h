

#ifndef _NT_EVENT_CONNECT_H_INCLUDED_
#define _NT_EVENT_CONNECT_H_INCLUDED_


#include <nt_core.h>
#include <nt_event.h>


#define NT_PEER_KEEPALIVE           1
#define NT_PEER_NEXT                2
#define NT_PEER_FAILED              4


typedef struct nt_peer_connection_s  nt_peer_connection_t;

typedef nt_int_t (*nt_event_get_peer_pt)(nt_peer_connection_t *pc,
    void *data);
typedef void (*nt_event_free_peer_pt)(nt_peer_connection_t *pc, void *data,
    nt_uint_t state);
typedef void (*nt_event_notify_peer_pt)(nt_peer_connection_t *pc,
    void *data, nt_uint_t type);
typedef nt_int_t (*nt_event_set_peer_session_pt)(nt_peer_connection_t *pc,
    void *data);
typedef void (*nt_event_save_peer_session_pt)(nt_peer_connection_t *pc,
    void *data);


struct nt_peer_connection_s {
    nt_connection_t                *connection;

    struct sockaddr                 *sockaddr;
    socklen_t                        socklen;
    nt_str_t                       *name;
#if (T_NT_HTTP_DYNAMIC_RESOLVE)    
    nt_str_t                       *host;
#endif

    nt_uint_t                       tries;
    nt_msec_t                       start_time;

    nt_event_get_peer_pt            get;
    nt_event_free_peer_pt           free;
    nt_event_notify_peer_pt         notify;
    void                            *data;

#if (NT_SSL || NT_COMPAT)
    nt_event_set_peer_session_pt    set_session;
    nt_event_save_peer_session_pt   save_session;
#endif

    nt_addr_t                      *local;

    int                              type;
    int                              rcvbuf;

    nt_log_t                       *log;

    unsigned                         cached:1;
    unsigned                         transparent:1;
    unsigned                         so_keepalive:1;
    unsigned                         down:1;

#if (T_NT_HTTP_DYNAMIC_RESOLVE)    
    unsigned                         resolved:2;
#endif

                                     /* nt_connection_log_error_e */
    unsigned                         log_error:2;

    NT_COMPAT_BEGIN(2)
    NT_COMPAT_END
};


nt_int_t nt_event_connect_peer(nt_peer_connection_t *pc);
nt_int_t nt_event_get_peer(nt_peer_connection_t *pc, void *data);


#endif /* _NT_EVENT_CONNECT_H_INCLUDED_ */
