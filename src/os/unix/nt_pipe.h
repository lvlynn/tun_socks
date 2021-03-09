
#ifndef _NT_PIPE_H_INCLUDED_
#define _NT_PIPE_H_INCLUDED_


#include <nt_core.h>


#if !(NT_WIN32)

typedef struct {
    u_char           *cmd;
    nt_fd_t          pfd[2];
    nt_pid_t         pid;
    nt_str_t         backup;         /* when pipe is broken, log into it */
    nt_uid_t         user;
    nt_uint_t        generation;
    nt_array_t      *argv;
    nt_open_file_t  *open_fd;        /* the fd of pipe left open in master */

    unsigned          type:1;         /* 1: write, 0: read */
    unsigned          configured:1;
} nt_open_pipe_t;


#define NT_PIPE_WRITE    1
#define NT_PIPE_READ     0


nt_open_pipe_t *nt_conf_open_pipe(nt_cycle_t *cycle, nt_str_t *cmd,
    const char *type);
void nt_increase_pipe_generation(void);
void nt_close_old_pipes(void);
nt_int_t nt_open_pipes(nt_cycle_t *cycle);
void nt_close_pipes(void);
void nt_pipe_broken_action(nt_log_t *log, nt_pid_t pid, nt_int_t master);


extern nt_str_t nt_log_error_backup;
extern nt_str_t nt_log_access_backup;

#else

#define nt_increase_pipe_generation
#define nt_close_old_pipes
#define nt_open_pipes(cycle)
#define nt_close_pipes
#define nt_pipe_broken_action(log, pid, master)

#endif


#endif /* _NT_PIPE_H_INCLUDED_ */
