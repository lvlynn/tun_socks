
#include <nt_core.h>


nt_int_t
nt_daemon(nt_log_t *log)
{
    int  fd;

    switch (fork()) {
    case -1:
        nt_log_error(NT_LOG_EMERG, log, nt_errno, "fork() failed");
        return NT_ERROR;

    case 0:
        break;

    default:
        exit(0);
    }

    nt_parent = nt_pid;
    nt_pid = nt_getpid();

    if (setsid() == -1) {
        nt_log_error(NT_LOG_EMERG, log, nt_errno, "setsid() failed");
        return NT_ERROR;
    }

    umask(0);

    fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
        nt_log_error(NT_LOG_EMERG, log, nt_errno,
                      "open(\"/dev/null\") failed");
        return NT_ERROR;
    }

    if (dup2(fd, STDIN_FILENO) == -1) {
        nt_log_error(NT_LOG_EMERG, log, nt_errno, "dup2(STDIN) failed");
        return NT_ERROR;
    }

    if (dup2(fd, STDOUT_FILENO) == -1) {
        nt_log_error(NT_LOG_EMERG, log, nt_errno, "dup2(STDOUT) failed");
        return NT_ERROR;
    }

#if 0
    if (dup2(fd, STDERR_FILENO) == -1) {
        nt_log_error(NT_LOG_EMERG, log, nt_errno, "dup2(STDERR) failed");
        return NT_ERROR;
    }
#endif

    if (fd > STDERR_FILENO) {
        if (close(fd) == -1) {
            nt_log_error(NT_LOG_EMERG, log, nt_errno, "close() failed");
            return NT_ERROR;
        }
    }

    return NT_OK;
}
