#ifndef _NT_PROCESS_H_
#define _NT_PROCESS_H_

#include <nt_core.h>

typedef pid_t       nt_pid_t;

#define NT_INVALID_PID  -1

typedef void ( *nt_spawn_proc_pt )( nt_cycle_t *cycle, void *data );

typedef struct {
    nt_pid_t           pid;
    int                 status;
    nt_socket_t        channel[2];

    nt_spawn_proc_pt   proc;
    void               *data;
    char               *name;

    unsigned            respawn: 1;
    unsigned            just_spawn: 1;
    unsigned            detached: 1;
    unsigned            exiting: 1;
    unsigned            exited: 1;

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

#define nt_pid 1

#define nt_getpid   getpid
#define nt_getppid  getppid

#ifndef nt_log_pid
    #define nt_log_pid  nt_pid
#endif



#endif
