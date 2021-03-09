
#ifndef _NT_CHANNEL_H_INCLUDED_
#define _NT_CHANNEL_H_INCLUDED_


#include <nt_core.h>
#include <nt_event.h>


typedef struct {
    nt_uint_t  command;
    nt_pid_t   pid;
    nt_int_t   slot;
    nt_fd_t    fd;
} nt_channel_t;


nt_int_t nt_write_channel(nt_socket_t s, nt_channel_t *ch, size_t size,
    nt_log_t *log);
nt_int_t nt_read_channel(nt_socket_t s, nt_channel_t *ch, size_t size,
    nt_log_t *log);
nt_int_t nt_add_channel_event(nt_cycle_t *cycle, nt_fd_t fd,
    nt_int_t event, nt_event_handler_pt handler);
void nt_close_channel(nt_fd_t *fd, nt_log_t *log);


#endif /* _NT_CHANNEL_H_INCLUDED_ */
