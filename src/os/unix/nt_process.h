#ifndef _NT_PROCESS_H_
#define _NT_PROCESS_H_


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
