#ifndef _NT_ERRNO_H_
#define _NT_ERRNO_H_

#include <nt_def.h>
#include <nt_core.h>

typedef int               nt_err_t;


#define NT_EPERM         EPERM
#define NT_ENOENT        ENOENT
#define NT_ENOPATH       ENOENT
#define NT_ESRCH         ESRCH
#define NT_EINTR         EINTR
#define NT_ECHILD        ECHILD
#define NT_ENOMEM        ENOMEM
#define NT_EACCES        EACCES
#define NT_EBUSY         EBUSY
#define NT_EEXIST        EEXIST
#define NT_EEXIST_FILE   EEXIST
#define NT_EXDEV         EXDEV
#define NT_ENOTDIR       ENOTDIR
#define NT_EISDIR        EISDIR
#define NT_EINVAL        EINVAL
#define NT_ENFILE        ENFILE
#define NT_EMFILE        EMFILE
#define NT_ENOSPC        ENOSPC
#define NT_EPIPE         EPIPE
#define NT_EINPROGRESS   EINPROGRESS
#define NT_ENOPROTOOPT   ENOPROTOOPT
#define NT_EOPNOTSUPP    EOPNOTSUPP
#define NT_EADDRINUSE    EADDRINUSE
#define NT_ECONNABORTED  ECONNABORTED
#define NT_ECONNRESET    ECONNRESET
#define NT_ENOTCONN      ENOTCONN
#define NT_ETIMEDOUT     ETIMEDOUT
#define NT_ECONNREFUSED  ECONNREFUSED
#define NT_ENAMETOOLONG  ENAMETOOLONG
#define NT_ENETDOWN      ENETDOWN
#define NT_ENETUNREACH   ENETUNREACH
#define NT_EHOSTDOWN     EHOSTDOWN
#define NT_EHOSTUNREACH  EHOSTUNREACH
#define NT_ENOSYS        ENOSYS
#define NT_ECANCELED     ECANCELED
#define NT_EILSEQ        EILSEQ
#define NT_ENOMOREFILES  0
#define NT_ELOOP         ELOOP
#define NT_EBADF         EBADF

#if (NT_HAVE_OPENAT)
#define NT_EMLINK        EMLINK
#endif

#if (__hpux__)
#define NT_EAGAIN        EWOULDBLOCK
#else
#define NT_EAGAIN        EAGAIN
#endif


#define nt_errno                  errno
#define nt_socket_errno           errno
#define nt_set_errno(err)         errno = err
#define nt_set_socket_errno(err)  errno = err


#endif
