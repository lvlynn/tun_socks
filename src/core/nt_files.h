#ifndef _NT_FILES_H_
#define _NT_FILES_H_

 #include <nt_def.h>
 #include <nt_core.h>


typedef int                      nt_fd_t;


#define nt_read_fd              read
#define nt_read_fd_n            "read()"

/*
 * we use inlined function instead of simple #define
 * because glibc 2.3 sets warn_unused_result attribute for write()
 * and in this case gcc 4.3 ignores (void) cast
 */
static nt_inline ssize_t
nt_write_fd( nt_fd_t fd, void *buf, size_t n )
{
    return write( fd, buf, n );
}

#define nt_write_fd_n           "write()"


#define nt_write_console        nt_write_fd


#define nt_linefeed(p)          *p++ = LF;
#define NT_LINEFEED_SIZE        1
#define NT_LINEFEED             "\x0a"


#define nt_stdout               STDOUT_FILENO
#define nt_stderr               STDERR_FILENO
#define nt_set_stderr(fd)       dup2(fd, STDERR_FILENO)
#define nt_set_stderr_n         "dup2(STDERR_FILENO)"

#endif
