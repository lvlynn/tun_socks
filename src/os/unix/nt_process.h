

#ifndef _NT_PROCESS_H_INCLUDED_
#define _NT_PROCESS_H_INCLUDED_


#include <nt_setaffinity.h>
#include <nt_setproctitle.h>


typedef pid_t       nt_pid_t;

#define NT_INVALID_PID  -1

typedef void (*nt_spawn_proc_pt) (nt_cycle_t *cycle, void *data);

typedef struct {
    nt_pid_t           pid;
    int                 status;
    nt_socket_t        channel[2];

    nt_spawn_proc_pt   proc;
    void               *data;
    char               *name;

    unsigned            respawn:1;
    unsigned            just_spawn:1;
    unsigned            detached:1;
    unsigned            exiting:1;
    unsigned            exited:1;
} nt_process_t;


typedef struct {
    char         *path;
    char         *name;
    char *const  *argv;
    char *const  *envp;
} nt_exec_ctx_t;


#define NT_MAX_PROCESSES         1024

#define NT_PROCESS_NORESPAWN     -1
#define NT_PROCESS_JUST_SPAWN    -2
#define NT_PROCESS_RESPAWN       -3
#define NT_PROCESS_JUST_RESPAWN  -4
#define NT_PROCESS_DETACHED      -5


#define nt_getpid   getpid
#define nt_getppid  getppid

#ifndef nt_log_pid
#define nt_log_pid  nt_pid
#endif


nt_pid_t nt_spawn_process(nt_cycle_t *cycle,
    nt_spawn_proc_pt proc, void *data, char *name, nt_int_t respawn);
nt_pid_t nt_execute(nt_cycle_t *cycle, nt_exec_ctx_t *ctx);
nt_int_t nt_init_signals(nt_log_t *log);
void nt_debug_point(void);


#if (NT_HAVE_SCHED_YIELD)
#define nt_sched_yield()  sched_yield()
#else
#define nt_sched_yield()  usleep(1)
#endif


extern int            nt_argc;
extern char         **nt_argv;
extern char         **nt_os_argv;

extern nt_pid_t      nt_pid;
extern nt_pid_t      nt_parent;
extern nt_socket_t   nt_channel;
extern nt_int_t      nt_process_slot;
extern nt_int_t      nt_last_process;
extern nt_process_t  nt_processes[NT_MAX_PROCESSES];


#endif /* _NT_PROCESS_H_INCLUDED_ */
