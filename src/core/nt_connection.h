#ifndef _NT_CONNECTION_H_
#define _NT_CONNECTION_H_

#include <nt_def.h>
#include <nt_core.h>

/*用于存储一个新连进来的连接*/
struct nt_connection_s {

    //存储内容
    void               *data;

    //日志
    nt_log_t           *log;

    //发送个数
    off_t               sent;

    nt_socket_t        fd;
    //read event事件
    nt_event_t        *read;
    //write event事件
    nt_event_t        *write;

    //recv回调
    nt_recv_pt         recv;

    //send回调
    nt_send_pt         send;

    struct sockaddr    *sockaddr;
    socklen_t           socklen;
    nt_str_t           addr_text;

    //队列
    nt_queue_t         queue;

    unsigned            log_error:3;     /* nt_connection_log_error_e */

};

typedef enum {
    NT_ERROR_ALERT = 0,
    NT_ERROR_ERR,
    NT_ERROR_INFO,
    NT_ERROR_IGNORE_ECONNRESET,
    NT_ERROR_IGNORE_EINVAL

} nt_connection_log_error_e;


extern nt_int_t nt_connection_error( nt_connection_t *c, nt_err_t err, char *text );


#endif
