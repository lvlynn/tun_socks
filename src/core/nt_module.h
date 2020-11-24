#ifndef _NT_MODULE_H_
#define _NT_MODULE_H_

#include <nt_def.h>
#include <nt_core.h>
#include <nt.h>

#define NT_MODULE_UNSET_INDEX  (nt_uint_t) -1

#define NT_MODULE_SIGNATURE_0                                                \
    nt_value(NT_PTR_SIZE) ","                                               \
    nt_value(NT_SIG_ATOMIC_T_SIZE) ","                                      \
    nt_value(NT_TIME_T_SIZE) ","

#if (NT_HAVE_KQUEUE)
    #define NT_MODULE_SIGNATURE_1   "1"
#else
    #define NT_MODULE_SIGNATURE_1   "0"
#endif

#if (NT_HAVE_IOCP)
    #define NT_MODULE_SIGNATURE_2   "1"
#else
    #define NT_MODULE_SIGNATURE_2   "0"
#endif

#if (NT_HAVE_FILE_AIO || NT_COMPAT)
    #define NT_MODULE_SIGNATURE_3   "1"
#else
    #define NT_MODULE_SIGNATURE_3   "0"
#endif

#if (NT_HAVE_AIO_SENDFILE || NT_COMPAT)
    #define NT_MODULE_SIGNATURE_4   "1"
#else
    #define NT_MODULE_SIGNATURE_4   "0"
#endif

#if (NT_HAVE_EVENTFD)
    #define NT_MODULE_SIGNATURE_5   "1"
#else
    #define NT_MODULE_SIGNATURE_5   "0"
#endif

#if (NT_HAVE_EPOLL)
    #define NT_MODULE_SIGNATURE_6   "1"
#else
    #define NT_MODULE_SIGNATURE_6   "0"
#endif

#if (NT_HAVE_KEEPALIVE_TUNABLE)
    #define NT_MODULE_SIGNATURE_7   "1"
#else
    #define NT_MODULE_SIGNATURE_7   "0"
#endif

#if (NT_HAVE_INET6)
    #define NT_MODULE_SIGNATURE_8   "1"
#else
    #define NT_MODULE_SIGNATURE_8   "0"
#endif

#define NT_MODULE_SIGNATURE_9   "1"
#define NT_MODULE_SIGNATURE_10  "1"

#if (NT_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    #define NT_MODULE_SIGNATURE_11  "1"
#else
    #define NT_MODULE_SIGNATURE_11  "0"
#endif

#define NT_MODULE_SIGNATURE_12  "1"

#if (NT_HAVE_SETFIB)
    #define NT_MODULE_SIGNATURE_13  "1"
#else
    #define NT_MODULE_SIGNATURE_13  "0"
#endif

#if (NT_HAVE_TCP_FASTOPEN)
    #define NT_MODULE_SIGNATURE_14  "1"
#else
    #define NT_MODULE_SIGNATURE_14  "0"
#endif

#if (NT_HAVE_UNIX_DOMAIN)
    #define NT_MODULE_SIGNATURE_15  "1"
#else
    #define NT_MODULE_SIGNATURE_15  "0"
#endif

#if (NT_HAVE_VARIADIC_MACROS)
    #define NT_MODULE_SIGNATURE_16  "1"
#else
    #define NT_MODULE_SIGNATURE_16  "0"
#endif

#define NT_MODULE_SIGNATURE_17  "0"
#define NT_MODULE_SIGNATURE_18  "0"
#if (NT_HAVE_OPENAT)
    #define NT_MODULE_SIGNATURE_19  "1"
#else
    #define NT_MODULE_SIGNATURE_19  "0"
#endif

#if (NT_HAVE_ATOMIC_OPS)
    #define NT_MODULE_SIGNATURE_20  "1"
#else
    #define NT_MODULE_SIGNATURE_20  "0"
#endif

#if (NT_HAVE_POSIX_SEM)
    #define NT_MODULE_SIGNATURE_21  "1"
#else
    #define NT_MODULE_SIGNATURE_21  "0"
#endif

#if (NT_THREADS || NT_COMPAT)
    #define NT_MODULE_SIGNATURE_22  "1"
#else
    #define NT_MODULE_SIGNATURE_22  "0"
#endif

#if (NT_PCRE)
    #define NT_MODULE_SIGNATURE_23  "1"
#else
    #define NT_MODULE_SIGNATURE_23  "0"
#endif

#if (NT_HTTP_SSL || NT_COMPAT)
    #define NT_MODULE_SIGNATURE_24  "1"
#else
    #define NT_MODULE_SIGNATURE_24  "0"
#endif

#define NT_MODULE_SIGNATURE_25  "1"

#if (NT_HTTP_GZIP)
    #define NT_MODULE_SIGNATURE_26  "1"
#else
    #define NT_MODULE_SIGNATURE_26  "0"
#endif

#define NT_MODULE_SIGNATURE_27  "1"

#if (NT_HTTP_X_FORWARDED_FOR)
    #define NT_MODULE_SIGNATURE_28  "1"
#else
    #define NT_MODULE_SIGNATURE_28  "0"
#endif

#if (NT_HTTP_REALIP)
    #define NT_MODULE_SIGNATURE_29  "1"
#else
    #define NT_MODULE_SIGNATURE_29  "0"
#endif

#if (NT_HTTP_HEADERS)
    #define NT_MODULE_SIGNATURE_30  "1"
#else
    #define NT_MODULE_SIGNATURE_30  "0"
#endif

#if (NT_HTTP_DAV)
    #define NT_MODULE_SIGNATURE_31  "1"
#else
    #define NT_MODULE_SIGNATURE_31  "0"
#endif

#if (NT_HTTP_CACHE)
    #define NT_MODULE_SIGNATURE_32  "1"
#else
    #define NT_MODULE_SIGNATURE_32  "0"
#endif

#if (NT_HTTP_UPSTREAM_ZONE)
    #define NT_MODULE_SIGNATURE_33  "1"
#else
    #define NT_MODULE_SIGNATURE_33  "0"
#endif

#if (NT_COMPAT)
    #define NT_MODULE_SIGNATURE_34  "1"
#else
    #define NT_MODULE_SIGNATURE_34  "0"
#endif

#define NT_MODULE_SIGNATURE                                                  \
    NT_MODULE_SIGNATURE_0 NT_MODULE_SIGNATURE_1 NT_MODULE_SIGNATURE_2      \
    NT_MODULE_SIGNATURE_3 NT_MODULE_SIGNATURE_4 NT_MODULE_SIGNATURE_5      \
    NT_MODULE_SIGNATURE_6 NT_MODULE_SIGNATURE_7 NT_MODULE_SIGNATURE_8      \
    NT_MODULE_SIGNATURE_9 NT_MODULE_SIGNATURE_10 NT_MODULE_SIGNATURE_11    \
    NT_MODULE_SIGNATURE_12 NT_MODULE_SIGNATURE_13 NT_MODULE_SIGNATURE_14   \
    NT_MODULE_SIGNATURE_15 NT_MODULE_SIGNATURE_16 NT_MODULE_SIGNATURE_17   \
    NT_MODULE_SIGNATURE_18 NT_MODULE_SIGNATURE_19 NT_MODULE_SIGNATURE_20   \
    NT_MODULE_SIGNATURE_21 NT_MODULE_SIGNATURE_22 NT_MODULE_SIGNATURE_23   \
    NT_MODULE_SIGNATURE_24 NT_MODULE_SIGNATURE_25 NT_MODULE_SIGNATURE_26   \
    NT_MODULE_SIGNATURE_27 NT_MODULE_SIGNATURE_28 NT_MODULE_SIGNATURE_29   \
    NT_MODULE_SIGNATURE_30 NT_MODULE_SIGNATURE_31 NT_MODULE_SIGNATURE_32   \
    NT_MODULE_SIGNATURE_33 NT_MODULE_SIGNATURE_34



#define NT_MODULE_V1                                                         \
    NT_MODULE_UNSET_INDEX, NT_MODULE_UNSET_INDEX,                           \
    NULL, 0, 0, nt_version, NT_MODULE_SIGNATURE

#define NT_MODULE_V1_PADDING  0, 0, 0, 0, 0, 0, 0, 0

struct nt_module_s {
    nt_uint_t            ctx_index;
    nt_uint_t            index;

    char                 *name;

    nt_uint_t            spare0;
    nt_uint_t            spare1;

    nt_uint_t            version;
    const char           *signature;

    void                 *ctx;
//    nt_command_t        *commands;
    nt_uint_t            type;

    nt_int_t ( *init_master )( nt_log_t *log );

    nt_int_t ( *init_module )( nt_cycle_t *cycle );

    nt_int_t ( *init_process )( nt_cycle_t *cycle );
    nt_int_t ( *init_thread )( nt_cycle_t *cycle );
    void ( *exit_thread )( nt_cycle_t *cycle );
    void ( *exit_process )( nt_cycle_t *cycle );

    void ( *exit_master )( nt_cycle_t *cycle );

    uintptr_t             spare_hook0;
    uintptr_t             spare_hook1;
    uintptr_t             spare_hook2;
    uintptr_t             spare_hook3;
    uintptr_t             spare_hook4;
    uintptr_t             spare_hook5;
    uintptr_t             spare_hook6;
    uintptr_t             spare_hook7;

};

#endif
