

#ifndef _NT_CONFIG_H_INCLUDED_
#define _NT_CONFIG_H_INCLUDED_


#include <nt_auto_headers.h>


#if defined __DragonFly__ && !defined __FreeBSD__
#define __FreeBSD__        4
#define __FreeBSD_version  480101
#endif


#if (NT_FREEBSD)
#include <nt_freebsd_config.h>


#elif (NT_LINUX)
#include <nt_linux_config.h>


#elif (NT_SOLARIS)
#include <nt_solaris_config.h>


#elif (NT_DARWIN)
#include <nt_darwin_config.h>


#elif (NT_WIN32)
#include <nt_win32_config.h>


#else /* POSIX */
#include <nt_posix_config.h>

#endif


#ifndef NT_HAVE_SO_SNDLOWAT
#define NT_HAVE_SO_SNDLOWAT     1
#endif


#if !(NT_WIN32)

#define nt_signal_helper(n)     SIG##n
#define nt_signal_value(n)      nt_signal_helper(n)

#define nt_random               random

/* TODO: #ifndef */
#define NT_SHUTDOWN_SIGNAL      QUIT
#define NT_TERMINATE_SIGNAL     TERM
#define NT_NOACCEPT_SIGNAL      WINCH
#define NT_RECONFIGURE_SIGNAL   HUP

#if (NT_LINUXTHREADS)
#define NT_REOPEN_SIGNAL        INFO
#define NT_CHANGEBIN_SIGNAL     XCPU
#else
#define NT_REOPEN_SIGNAL        USR1
#define NT_CHANGEBIN_SIGNAL     USR2
#endif

#define nt_cdecl
#define nt_libc_cdecl

#endif

typedef intptr_t        nt_int_t;
typedef uintptr_t       nt_uint_t;
typedef intptr_t        nt_flag_t;


#define NT_INT32_LEN   (sizeof("-2147483648") - 1)
#define NT_INT64_LEN   (sizeof("-9223372036854775808") - 1)

#if (NT_PTR_SIZE == 4)
#define NT_INT_T_LEN   NT_INT32_LEN
#define NT_MAX_INT_T_VALUE  2147483647

#else
#define NT_INT_T_LEN   NT_INT64_LEN
#define NT_MAX_INT_T_VALUE  9223372036854775807
#endif


#ifndef NT_ALIGNMENT
#define NT_ALIGNMENT   sizeof(unsigned long)    /* platform word */
#endif

#define nt_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define nt_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))


#define nt_abort       abort


/* TODO: platform specific: array[NT_INVALID_ARRAY_INDEX] must cause SIGSEGV */
#define NT_INVALID_ARRAY_INDEX 0x80000000


/* TODO: auto_conf: nt_inline   inline __inline __inline__ */
#ifndef nt_inline
#define nt_inline      inline
#endif

#ifndef INADDR_NONE  /* Solaris */
#define INADDR_NONE  ((unsigned int) -1)
#endif

#ifdef MAXHOSTNAMELEN
#define NT_MAXHOSTNAMELEN  MAXHOSTNAMELEN
#else
#define NT_MAXHOSTNAMELEN  256
#endif


#define NT_MAX_UINT32_VALUE  (uint32_t) 0xffffffff
#define NT_MAX_INT32_VALUE   (uint32_t) 0x7fffffff


#if (NT_COMPAT)

#define NT_COMPAT_BEGIN(slots)  uint64_t spare[slots];
#define NT_COMPAT_END

#else

#define NT_COMPAT_BEGIN(slots)
#define NT_COMPAT_END

#endif



#ifndef NT_AF_TUN
    #define NT_AF_TUN 55 
#endif

//重做 inet_ntoa， 可直接打印 uint32_t 的ip，而不用重新构造 struct sin_addr
#define nt_inet_ntoa( a  )  inet_ntoa( *( struct in_addr * )(&a) )


#endif /* _NT_CONFIG_H_INCLUDED_ */
