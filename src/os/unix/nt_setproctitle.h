
#ifndef _NT_SETPROCTITLE_H_INCLUDED_
#define _NT_SETPROCTITLE_H_INCLUDED_


#if (NT_HAVE_SETPROCTITLE)

/* FreeBSD, NetBSD, OpenBSD */

#define nt_init_setproctitle(log) NT_OK
#define nt_setproctitle(title)    setproctitle("%s", title)


#else /* !NT_HAVE_SETPROCTITLE */

#if !defined NT_SETPROCTITLE_USES_ENV

#if (NT_SOLARIS)

#define NT_SETPROCTITLE_USES_ENV  1
#define NT_SETPROCTITLE_PAD       ' '

nt_int_t nt_init_setproctitle(nt_log_t *log);
void nt_setproctitle(char *title);

#elif (NT_LINUX) || (NT_DARWIN)

#define NT_SETPROCTITLE_USES_ENV  1
#define NT_SETPROCTITLE_PAD       '\0'

nt_int_t nt_init_setproctitle(nt_log_t *log);
void nt_setproctitle(char *title);

#else

#define nt_init_setproctitle(log) NT_OK
#define nt_setproctitle(title)

#endif /* OSes */

#endif /* NT_SETPROCTITLE_USES_ENV */

#endif /* NT_HAVE_SETPROCTITLE */


#endif /* _NT_SETPROCTITLE_H_INCLUDED_ */
