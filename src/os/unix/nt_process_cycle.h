
#ifndef _NT_PROCESS_CYCLE_H_INCLUDED_
#define _NT_PROCESS_CYCLE_H_INCLUDED_


#include <nt_core.h>


#define NT_CMD_OPEN_CHANNEL   1
#define NT_CMD_CLOSE_CHANNEL  2
#define NT_CMD_QUIT           3
#define NT_CMD_TERMINATE      4
#define NT_CMD_REOPEN         5

#if (T_PIPES)
#define NT_CMD_PIPE_BROKEN    6
#endif


#define NT_PROCESS_SINGLE     0
#define NT_PROCESS_MASTER     1
#define NT_PROCESS_SIGNALLER  2
#define NT_PROCESS_WORKER     3
#define NT_PROCESS_HELPER     4

#if (NT_PROCS)
#define NT_PROCESS_PROC       5
#endif

#if (T_PIPES)
#define NT_PROCESS_PIPE       6
#endif


typedef struct {
    nt_event_handler_pt       handler;
    char                      *name;
    nt_msec_t                 delay;
} nt_cache_manager_ctx_t;


void nt_master_process_cycle(nt_cycle_t *cycle);
void nt_single_process_cycle(nt_cycle_t *cycle);


extern nt_uint_t      nt_process;
extern nt_uint_t      nt_worker;
extern nt_pid_t       nt_pid;
extern nt_pid_t       nt_new_binary;
extern nt_uint_t      nt_inherited;
extern nt_uint_t      nt_daemonized;
extern nt_uint_t      nt_exiting;

extern sig_atomic_t    nt_reap;
extern sig_atomic_t    nt_sigio;
extern sig_atomic_t    nt_sigalrm;
extern sig_atomic_t    nt_quit;
extern sig_atomic_t    nt_debug_quit;
extern sig_atomic_t    nt_terminate;
extern sig_atomic_t    nt_noaccept;
extern sig_atomic_t    nt_reconfigure;
extern sig_atomic_t    nt_reopen;
extern sig_atomic_t    nt_change_binary;

extern int funjsq_et_flag;

#endif /* _NT_PROCESS_CYCLE_H_INCLUDED_ */
