#ifndef _NT_LOG_H_
#define _NT_LOG_H_

#include <nt_core.h>

#define NT_LOG_STDERR            0
#define NT_LOG_EMERG             1
#define NT_LOG_ALERT             2
#define NT_LOG_CRIT              3
#define NT_LOG_ERR               4
#define NT_LOG_WARN              5
#define NT_LOG_NOTICE            6
#define NT_LOG_INFO              7
#define NT_LOG_DEBUG             8


#define NT_LOG_DEBUG_CORE        0x010
#define NT_LOG_DEBUG_ALLOC       0x020
#define NT_LOG_DEBUG_MUTEX       0x040
#define NT_LOG_DEBUG_EVENT       0x080
#define NT_LOG_DEBUG_HTTP        0x100
#define NT_LOG_DEBUG_MAIL        0x200
#define NT_LOG_DEBUG_STREAM      0x400


/*
  * do not forget to update debug_levels[] in src/core/nt_log.c
  * after the adding a new debug level
  */

#define NT_LOG_DEBUG_FIRST       NT_LOG_DEBUG_CORE
#define NT_LOG_DEBUG_LAST        NT_LOG_DEBUG_STREAM
#define NT_LOG_DEBUG_CONNECTION  0x80000000
#define NT_LOG_DEBUG_ALL         0x7ffffff0

typedef u_char *( *nt_log_handler_pt )( nt_log_t *log, u_char *buf, size_t len );
typedef void ( *nt_log_writer_pt )( nt_log_t *log, nt_uint_t level,
                                    u_char *buf, size_t len );


struct nt_log_s {
    nt_uint_t           log_level;
    nt_open_file_t     *file;

    size_t    connection;

    time_t               disk_full_time;

    nt_log_handler_pt   handler;
    void                *data;

    nt_log_writer_pt    writer;
    void                *wdata;

    /*
     *         * we declare "action" as "char *" because the actions are usually
     *                * the static strings and in the "u_char *" case we have to override
     *                       * their types all the time
     *                              */

    char                *action;

    nt_log_t           *next;

};

#define NT_MAX_ERROR_STR   2048

/*********************************/
//NT_HAVE_C99_VARIADIC_MACROS未定义
#if (NT_HAVE_C99_VARIADIC_MACROS)

#define NT_HAVE_VARIADIC_MACROS  1

#define nt_log_error(level, log, ...)                                        \
    if ((log)->log_level >= level) nt_log_error_core(level, log, __VA_ARGS__)

void nt_log_error_core( nt_uint_t level, nt_log_t *log, nt_err_t err,
                        const char *fmt, ... );

#define nt_log_debug(level, log, ...)                                        \
    if ((log)->log_level & level)                                             \
        nt_log_error_core(NT_LOG_DEBUG, log, __VA_ARGS__)

/*********************************/
//未定义
#elif (NT_HAVE_GCC_VARIADIC_MACROS)

#define NT_HAVE_VARIADIC_MACROS  1

#define nt_log_error(level, log, args...)                                    \
    if ((log)->log_level >= level) nt_log_error_core(level, log, args)

void nt_log_error_core( nt_uint_t level, nt_log_t *log, nt_err_t err,
                        const char *fmt, ... );

#define nt_log_debug(level, log, args...)                                    \
    if ((log)->log_level & level)                                             \
        nt_log_error_core(NT_LOG_DEBUG, log, args)

/*********************************/

#else /* no variadic macros */

#define NT_HAVE_VARIADIC_MACROS  0

void nt_cdecl nt_log_error( nt_uint_t level, nt_log_t *log, nt_err_t err,
                            const char *fmt, ... );
void nt_log_error_core( nt_uint_t level, nt_log_t *log, nt_err_t err,
                        const char *fmt, va_list args );
void nt_cdecl nt_log_debug_core( nt_log_t *log, nt_err_t err,
                                 const char *fmt, ... );


#endif /* variadic macros */


/*********************************/

#if (NT_DEBUG)


//未定义NT_HAVE_VARIADIC_MACROS
#if 0 
#if (NT_HAVE_VARIADIC_MACROS)

#define nt_log_debug0(level, log, err, fmt)                                  \
    nt_log_debug(level, log, err, fmt)

#define nt_log_debug1(level, log, err, fmt, arg1)                            \
    nt_log_debug(level, log, err, fmt, arg1)

#define nt_log_debug2(level, log, err, fmt, arg1, arg2)                      \
    nt_log_debug(level, log, err, fmt, arg1, arg2)

#define nt_log_debug3(level, log, err, fmt, arg1, arg2, arg3)                \
    nt_log_debug(level, log, err, fmt, arg1, arg2, arg3)

#define nt_log_debug4(level, log, err, fmt, arg1, arg2, arg3, arg4)          \
    nt_log_debug(level, log, err, fmt, arg1, arg2, arg3, arg4)

#define nt_log_debug5(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5)    \
    nt_log_debug(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5)

#define nt_log_debug6(level, log, err, fmt,                                  \
                      arg1, arg2, arg3, arg4, arg5, arg6)                    \
nt_log_debug(level, log, err, fmt,                                   \
             arg1, arg2, arg3, arg4, arg5, arg6)

#define nt_log_debug7(level, log, err, fmt,                                  \
                      arg1, arg2, arg3, arg4, arg5, arg6, arg7)              \
nt_log_debug(level, log, err, fmt,                                   \
             arg1, arg2, arg3, arg4, arg5, arg6, arg7)

#define nt_log_debug8(level, log, err, fmt,                                  \
                      arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)        \
nt_log_debug(level, log, err, fmt,                                   \
             arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)

#endif 
#else /* no variadic macros */

#define nt_log_debug0(level, log, err, fmt)                                  \
    if ((log)->log_level & level)                                             \
        nt_log_debug_core(log, err, fmt)

#define nt_log_debug1(level, log, err, fmt, arg1)                            \
    if ((log)->log_level & level)                                             \
        nt_log_debug_core(log, err, fmt, arg1)

#define nt_log_debug2(level, log, err, fmt, arg1, arg2)                      \
    if ((log)->log_level & level)                                             \
        nt_log_debug_core(log, err, fmt, arg1, arg2)

#define nt_log_debug3(level, log, err, fmt, arg1, arg2, arg3)                \
    if ((log)->log_level & level)                                             \
        nt_log_debug_core(log, err, fmt, arg1, arg2, arg3)

#define nt_log_debug4(level, log, err, fmt, arg1, arg2, arg3, arg4)          \
    if ((log)->log_level & level)                                             \
        nt_log_debug_core(log, err, fmt, arg1, arg2, arg3, arg4)

#define nt_log_debug5(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5)    \
    if ((log)->log_level & level)                                             \
        nt_log_debug_core(log, err, fmt, arg1, arg2, arg3, arg4, arg5)

#define nt_log_debug6(level, log, err, fmt,                                  \
                      arg1, arg2, arg3, arg4, arg5, arg6)                    \
if ((log)->log_level & level)                                             \
    nt_log_debug_core(log, err, fmt, arg1, arg2, arg3, arg4, arg5, arg6)

#define nt_log_debug7(level, log, err, fmt,                                  \
                      arg1, arg2, arg3, arg4, arg5, arg6, arg7)              \
if ((log)->log_level & level)                                             \
    nt_log_debug_core(log, err, fmt,                                     \
                      arg1, arg2, arg3, arg4, arg5, arg6, arg7)

#define nt_log_debug8(level, log, err, fmt,                                  \
                      arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)        \
if ((log)->log_level & level)                                             \
    nt_log_debug_core(log, err, fmt,                                     \
                      arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)

#endif
#else /* !NT_DEBUG */

#define nt_log_debug0(level, log, err, fmt)
#define nt_log_debug1(level, log, err, fmt, arg1)
#define nt_log_debug2(level, log, err, fmt, arg1, arg2)
#define nt_log_debug3(level, log, err, fmt, arg1, arg2, arg3)
#define nt_log_debug4(level, log, err, fmt, arg1, arg2, arg3, arg4)
#define nt_log_debug5(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5)
#define nt_log_debug6(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5, arg6)
#define nt_log_debug7(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5,    \
                      arg6, arg7)
#define nt_log_debug8(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5,    \
                      arg6, arg7, arg8)

#endif

/*********************************/

nt_log_t *nt_log_init( u_char *prefix );
void nt_cdecl nt_log_abort( nt_err_t err, const char *fmt, ... );
void nt_cdecl nt_log_stderr( nt_err_t err, const char *fmt, ... );
u_char *nt_log_errno( u_char *buf, u_char *last, nt_err_t err );
nt_int_t nt_log_open_default( nt_cycle_t *cycle );
nt_int_t nt_log_redirect_stderr( nt_cycle_t *cycle );
nt_log_t *nt_log_get_file_log( nt_log_t *head );
char *nt_log_set_log( nt_conf_t *cf, nt_log_t **head );


/*
 *    * nt_write_stderr() cannot be implemented as macro, since
 *       * MSVC does not allow to use #ifdef inside macro parameters.
 *          *
 *             * nt_write_fd() is used instead of nt_write_console(), since
 *                * CharToOemBuff() inside nt_write_console() cannot be used with
 *                   * read only buffer as destination and CharToOemBuff() is not needed
 *                      * for nt_write_stderr() anyway.
 *                         */
static nt_inline void
nt_write_stderr( char *text )
{
    ( void ) nt_write_fd( nt_stderr, text, nt_strlen( text ) );

}


static nt_inline void
nt_write_stdout( char *text )
{
    ( void ) nt_write_fd( nt_stdout, text, nt_strlen( text ) );

}


extern nt_module_t  nt_errlog_module;
extern nt_uint_t    nt_use_stderr;



#endif
