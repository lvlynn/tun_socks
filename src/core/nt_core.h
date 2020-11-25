#ifndef _NT_CORE_H_
#define _NT_CORE_H_
#include <nt_def.h>

typedef struct nt_module_s          nt_module_t;
typedef struct nt_cycle_s           nt_cycle_t;
typedef struct nt_log_s           nt_log_t;
typedef struct nt_event_s           nt_event_t;
typedef struct nt_connection_s      nt_connection_t;
typedef struct nt_open_file_s       nt_open_file_t;

typedef void ( *nt_event_handler_pt )( nt_event_t *ev );

#define  NT_OK          0
#define  NT_ERROR      -1
#define  NT_AGAIN      -2
#define  NT_BUSY       -3
#define  NT_DONE       -4
#define  NT_DECLINED   -5
#define  NT_ABORT      -6
#if (T_NT_HTTP_DYNAMIC_RESOLVE)
    #define  NT_YIELD      -7
#endif


#define LF     (u_char) '\n'
#define CR     (u_char) '\r'
#define CRLF   "\r\n"

//引入模块头文件
//os
#include <nt_socket.h>
#include <nt_errno.h>
#include <nt_os.h>
#include <nt_process.h>

//core
#include <nt_log.h>
#include <nt_files.h>
#include <nt_cycle.h>

#include <nt_string.h>
#include <nt_module.h>
#include <nt_connection.h>
#include <nt_conf.h>
//event


#include <nt_time.h>
#endif
