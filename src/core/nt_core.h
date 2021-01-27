#ifndef _NT_CORE_H_
#define _NT_CORE_H_
#include <nt_def.h>

typedef struct nt_module_s          nt_module_t;
typedef struct nt_conf_s            nt_conf_t;
typedef struct nt_cycle_s           nt_cycle_t;
typedef struct nt_pool_s            nt_pool_t;
typedef struct nt_chain_s           nt_chain_t;
typedef struct nt_file_s            nt_file_t;
typedef struct nt_command_s         nt_command_t;
typedef struct nt_time_s            nt_time_t;
typedef struct nt_log_s             nt_log_t;
typedef struct nt_event_s           nt_event_t;
typedef struct nt_udp_connection_s  nt_udp_connection_t;
typedef struct nt_connection_s      nt_connection_t;
typedef struct nt_open_file_s       nt_open_file_t;
// typedef struct pid_t            nt_pid_t;

typedef void ( *nt_event_handler_pt )( nt_event_t *ev );
typedef void ( *nt_connection_handler_pt )( nt_connection_t *c );

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



#define nt_abs(value)       (((value) >= 0) ? (value) : - (value))
#define nt_max(val1, val2)  ((val1 < val2) ? (val2) : (val1))
#define nt_min(val1, val2)  ((val1 > val2) ? (val2) : (val1))


//引入模块头文件
#include <nt_errno.h>
#include <nt_atomic.h>
#include <nt_rbtree.h>
#include <nt_string.h>
#include <nt_times.h>
#include <nt_socket.h>
#include <nt_files.h>
#include <nt_os.h>
#include <nt_log.h>
#include <nt_alloc.h>
#include <nt_palloc.h>
#include <nt_buf.h>
#include <nt_queue.h>
#include <nt_array.h>
#include <nt_list.h>
#include <nt_static_hash.h>
#include <nt_process.h>

#include <nt_linux.h>
#include <nt_file.h>
#include <nt_inet.h>
#include <nt_cycle.h>

#include <nt_module.h>
#include <nt_connection.h>
#include <nt_conf.h>
#include <nt_time.h>

#include <nt_syslog.h>
#include <nt_crc32.h>
#include <nt_resolver.h>
#include <nt_proxy_protocol.h>

#endif
