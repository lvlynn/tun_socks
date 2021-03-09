

#include <nt_core.h>


u_char  nt_linux_kern_ostype[50];
u_char  nt_linux_kern_osrelease[50];

#if 0
static nt_os_io_t nt_linux_io = {
    nt_unix_recv,
    nt_readv_chain,
    nt_udp_unix_recv,
    nt_unix_send,
    nt_udp_unix_send,
    nt_udp_unix_sendmsg_chain,
#if (NT_HAVE_SENDFILE)
    nt_linux_sendfile_chain,
    NT_IO_SENDFILE
#else
    nt_writev_chain,
    0
#endif
};
#else
static nt_os_io_t nt_linux_io = {
    nt_unix_recv,
    1,
    nt_udp_unix_recv,
    nt_unix_send,
    nt_udp_unix_send,
    1,
#if (NT_HAVE_SENDFILE)
    nt_linux_sendfile_chain,
    NT_IO_SENDFILE
#else
    0,
    0
#endif
};

#endif 

nt_int_t
nt_os_specific_init(nt_log_t *log)
{
    struct utsname  u;

    if (uname(&u) == -1) {
        nt_log_error(NT_LOG_ALERT, log, nt_errno, "uname() failed");
        return NT_ERROR;
    }

    (void) nt_cpystrn(nt_linux_kern_ostype, (u_char *) u.sysname,
                       sizeof(nt_linux_kern_ostype));

    (void) nt_cpystrn(nt_linux_kern_osrelease, (u_char *) u.release,
                       sizeof(nt_linux_kern_osrelease));

    nt_os_io = nt_linux_io;

    return NT_OK;
}


void
nt_os_specific_status(nt_log_t *log)
{
    nt_log_error(NT_LOG_NOTICE, log, 0, "OS: %s %s",
                  nt_linux_kern_ostype, nt_linux_kern_osrelease);
}
