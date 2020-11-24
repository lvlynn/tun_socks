
#include <nt_def.h>
#include <nt_core.h>

nt_os_io_t nt_os_io = { 
    nt_unix_recv,
    nt_unix_send,
    nt_udp_unix_recv,
    nt_udp_unix_send,
    0   
};

nt_int_t
nt_os_init(nt_log_t *log){

    return NT_OK;
}
