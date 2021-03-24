
#include <nt_core.h>


static void nt_show_version_info( void );
static nt_int_t nt_add_inherited_sockets( nt_cycle_t *cycle );
static void nt_cleanup_environment( void *data );
static nt_int_t nt_get_options( int argc, char *const *argv );
static nt_int_t nt_process_options( nt_cycle_t *cycle );
nt_int_t nt_save_argv( nt_cycle_t *cycle, int argc, char *const *argv );
static void *nt_core_module_create_conf( nt_cycle_t *cycle );
static char *nt_core_module_init_conf( nt_cycle_t *cycle, void *conf );
static char *nt_set_user( nt_conf_t *cf, nt_command_t *cmd, void *conf );
static char *nt_set_env( nt_conf_t *cf, nt_command_t *cmd, void *conf );
static char *nt_set_priority( nt_conf_t *cf, nt_command_t *cmd, void *conf );
static char *nt_set_cpu_affinity( nt_conf_t *cf, nt_command_t *cmd,
                                  void *conf );
static char *nt_set_worker_processes( nt_conf_t *cf, nt_command_t *cmd,
                                      void *conf );
static char *nt_load_module( nt_conf_t *cf, nt_command_t *cmd, void *conf );
#if (NT_HAVE_DLOPEN)
    static void nt_unload_module( void *data );
#endif
#if (T_NT_MASTER_ENV)
    static char *nt_master_set_env( nt_conf_t *cf, nt_command_t *cmd, void *conf );
#endif


static nt_conf_enum_t  nt_debug_points[] = {
    { nt_string( "stop" ), NT_DEBUG_POINTS_STOP },
    { nt_string( "abort" ), NT_DEBUG_POINTS_ABORT },
    { nt_null_string, 0 }
};


static nt_command_t  nt_core_commands[] = {

    {
        nt_string( "daemon" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_FLAG,
        nt_conf_set_flag_slot,
        0,
        offsetof( nt_core_conf_t, daemon ),
        NULL
    },

    {
        nt_string( "master_process" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_FLAG,
        nt_conf_set_flag_slot,
        0,
        offsetof( nt_core_conf_t, master ),
        NULL
    },

    {
        nt_string( "timer_resolution" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_TAKE1,
        nt_conf_set_msec_slot,
        0,
        offsetof( nt_core_conf_t, timer_resolution ),
        NULL
    },

    {
        nt_string( "pid" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_TAKE1,
        nt_conf_set_str_slot,
        0,
        offsetof( nt_core_conf_t, pid ),
        NULL
    },

    {
        nt_string( "lock_file" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_TAKE1,
        nt_conf_set_str_slot,
        0,
        offsetof( nt_core_conf_t, lock_file ),
        NULL
    },

    {
        nt_string( "worker_processes" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_TAKE1,
        nt_set_worker_processes,
        0,
        0,
        NULL
    },

    {
        nt_string( "debug_points" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_TAKE1,
        nt_conf_set_enum_slot,
        0,
        offsetof( nt_core_conf_t, debug_points ),
        &nt_debug_points
    },

    {
        nt_string( "user" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_TAKE12,
        nt_set_user,
        0,
        0,
        NULL
    },

    {
        nt_string( "worker_priority" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_TAKE1,
        nt_set_priority,
        0,
        0,
        NULL
    },

    {
        nt_string( "worker_cpu_affinity" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_1MORE,
        nt_set_cpu_affinity,
        0,
        0,
        NULL
    },

    {
        nt_string( "worker_rlimit_nofile" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_TAKE1,
        nt_conf_set_num_slot,
        0,
        offsetof( nt_core_conf_t, rlimit_nofile ),
        NULL
    },

    {
        nt_string( "worker_rlimit_core" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_TAKE1,
        nt_conf_set_off_slot,
        0,
        offsetof( nt_core_conf_t, rlimit_core ),
        NULL
    },

    {
        nt_string( "worker_shutdown_timeout" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_TAKE1,
        nt_conf_set_msec_slot,
        0,
        offsetof( nt_core_conf_t, shutdown_timeout ),
        NULL
    },

    {
        nt_string( "working_directory" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_TAKE1,
        nt_conf_set_str_slot,
        0,
        offsetof( nt_core_conf_t, working_directory ),
        NULL
    },

    {
        nt_string( "env" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_TAKE1,
        nt_set_env,
        0,
        0,
        NULL
    },

    {
        nt_string( "load_module" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_TAKE1,
        nt_load_module,
        0,
        0,
        NULL
    },

    #if (T_NT_MASTER_ENV)

    {
        nt_string( "master_env" ),
        NT_MAIN_CONF | NT_DIRECT_CONF | NT_CONF_TAKE1,
        nt_master_set_env,
        0,
        0,
        NULL
    },

    #endif

    nt_null_command
};


static nt_core_module_t  nt_core_module_ctx = {
    nt_string( "core" ),
    nt_core_module_create_conf,
    nt_core_module_init_conf
};


nt_module_t  nt_core_module = {
    NT_MODULE_V1,
    &nt_core_module_ctx,                  /* module context */
    nt_core_commands,                     /* module directives */
    NT_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NT_MODULE_V1_PADDING
};


static nt_uint_t   nt_show_help;
static nt_uint_t   nt_show_version;
static nt_uint_t   nt_show_configure;
#if (NT_SSL && NT_SSL_ASYNC)
    /* indicate that nginx start without nt_ssl_init()
    * which will involve OpenSSL configuration file to
    * start OpenSSL engine */
    static nt_uint_t   nt_no_ssl_init;
#endif
static u_char      *nt_prefix;
static u_char      *nt_conf_file;
static u_char      *nt_conf_params;
static char        *nt_signal;


static char **nt_os_environ;


static nt_int_t
nt_add_inherited_sockets( nt_cycle_t *cycle )
{
    u_char           *p, *v, *inherited;
    nt_int_t         s;
    nt_listening_t  *ls;

    //获取环境变量 env
    inherited = ( u_char * ) getenv( NGINX_VAR );

    //如果环境变量不存在，说明是新启动的程序，无需继承
    if( inherited == NULL ) {
        return NT_OK;
    }

    nt_log_error( NT_LOG_NOTICE, cycle->log, 0,
                  "using inherited sockets from \"%s\"", inherited );

    if( nt_array_init( &cycle->listening, cycle->pool, 10,
                       sizeof( nt_listening_t ) )
        != NT_OK ) {
        return NT_ERROR;
    }

    for( p = inherited, v = p; *p; p++ ) {
        if( *p == ':' || *p == ';' ) {
            s = nt_atoi( v, p - v );
            if( s == NT_ERROR ) {
                nt_log_error( NT_LOG_EMERG, cycle->log, 0,
                              "invalid socket number \"%s\" in " NGINX_VAR
                              " environment variable, ignoring the rest"
                              " of the variable", v );
                break;
            }

            v = p + 1;

            ls = nt_array_push( &cycle->listening );
            if( ls == NULL ) {
                return NT_ERROR;
            }

            nt_memzero( ls, sizeof( nt_listening_t ) );

            ls->fd = ( nt_socket_t ) s;
        }
    }

    if( v != p ) {
        nt_log_error( NT_LOG_EMERG, cycle->log, 0,
                      "invalid socket number \"%s\" in " NGINX_VAR
                      " environment variable, ignoring", v );
    }

    nt_inherited = 1;

    return nt_set_inherited_sockets( cycle );
}


char **
nt_set_environment( nt_cycle_t *cycle, nt_uint_t *last )
{
    char                **p, **env;
    nt_str_t            *var;
    nt_uint_t            i, n;
    nt_core_conf_t      *ccf;
    nt_pool_cleanup_t   *cln;

    ccf = ( nt_core_conf_t * ) nt_get_conf( cycle->conf_ctx, nt_core_module );

    if( last == NULL && ccf->environment ) {
        return ccf->environment;
    }

    var = ccf->env.elts;

    for( i = 0; i < ccf->env.nelts; i++ ) {
        if( nt_strcmp( var[i].data, "TZ" ) == 0
            || nt_strncmp( var[i].data, "TZ=", 3 ) == 0 ) {
            goto tz_found;
        }
    }

    var = nt_array_push( &ccf->env );
    if( var == NULL ) {
        return NULL;
    }

    var->len = 2;
    var->data = ( u_char * ) "TZ";

    var = ccf->env.elts;

tz_found:

    n = 0;

    for( i = 0; i < ccf->env.nelts; i++ ) {

        if( var[i].data[var[i].len] == '=' ) {
            n++;
            continue;
        }

        for( p = nt_os_environ; *p; p++ ) {

            if( nt_strncmp( *p, var[i].data, var[i].len ) == 0
                && ( *p )[var[i].len] == '=' ) {
                n++;
                break;
            }
        }
    }

    if( last ) {
#if 1
        env = nt_palloc( cycle->pool,  ( *last + n + 1 ) * sizeof( char * ) );
#else
        env = nt_alloc( ( *last + n + 1 ) * sizeof( char * ), cycle->log );
#endif
        if( env == NULL ) {
            return NULL;
        }

        *last = n;

    } else {
        cln = nt_pool_cleanup_add( cycle->pool, 0 );
        if( cln == NULL ) {
            return NULL;
        }

#if 1
        env = nt_pcalloc( cycle->pool, ( n + 1 ) * sizeof( char * ) );
#else
        env = nt_alloc( ( n + 1 ) * sizeof( char * ), cycle->log );
#endif
        if( env == NULL ) {
            return NULL;
        }

        cln->handler = nt_cleanup_environment;
        cln->data = env;
    }

    n = 0;

    for( i = 0; i < ccf->env.nelts; i++ ) {

        if( var[i].data[var[i].len] == '=' ) {
            env[n++] = ( char * ) var[i].data;
            continue;
        }

        for( p = nt_os_environ; *p; p++ ) {

            if( nt_strncmp( *p, var[i].data, var[i].len ) == 0
                && ( *p )[var[i].len] == '=' ) {
                env[n++] = *p;
                break;
            }
        }
    }

    env[n] = NULL;

    if( last == NULL ) {
        ccf->environment = env;
        environ = env;
    }

    return env;
}


static void
nt_cleanup_environment( void *data )
{
    char  **env = data;

    if( environ == env ) {

        /*
         * if the environment is still used, as it happens on exit,
         * the only option is to leak it
         */

        return;
    }

    nt_free( env );
}


nt_pid_t
nt_exec_new_binary( nt_cycle_t *cycle, char *const *argv )
{
    char             **env, *var;
    u_char            *p;
    nt_uint_t         i, n;
    nt_pid_t          pid;
    nt_exec_ctx_t     ctx;
    nt_core_conf_t   *ccf;
    nt_listening_t   *ls;

    nt_memzero( &ctx, sizeof( nt_exec_ctx_t ) );

    ctx.path = argv[0];
    ctx.name = "new binary process";
    ctx.argv = argv;

    n = 2;
    env = nt_set_environment( cycle, &n );
    if( env == NULL ) {
        return NT_INVALID_PID;
    }

    var = nt_alloc( sizeof( NGINX_VAR )
                    + cycle->listening.nelts * ( NT_INT32_LEN + 1 ) + 2,
                    cycle->log );
    if( var == NULL ) {
        nt_free( env );
        return NT_INVALID_PID;
    }

    p = nt_cpymem( var, NGINX_VAR "=", sizeof( NGINX_VAR ) );

    ls = cycle->listening.elts;
    for( i = 0; i < cycle->listening.nelts; i++ ) {
        p = nt_sprintf( p, "%ud;", ls[i].fd );
    }

    *p = '\0';

    env[n++] = var;

    #if (NT_SETPROCTITLE_USES_ENV)

    /* allocate the spare 300 bytes for the new binary process title */

    env[n++] = "SPARE=XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
               "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
               "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
               "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
               "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

    #endif

    env[n] = NULL;

    #if (NT_DEBUG)
    {
        char  **e;
        for( e = env; *e; e++ ) {
            nt_log_debug1( NT_LOG_DEBUG_CORE, cycle->log, 0, "env: %s", *e );
        }
    }
    #endif

    ctx.envp = ( char *const * ) env;

    ccf = ( nt_core_conf_t * ) nt_get_conf( cycle->conf_ctx, nt_core_module );

    if( nt_rename_file( ccf->pid.data, ccf->oldpid.data ) == NT_FILE_ERROR ) {
        nt_log_error( NT_LOG_ALERT, cycle->log, nt_errno,
                      nt_rename_file_n " %s to %s failed "
                      "before executing new binary process \"%s\"",
                      ccf->pid.data, ccf->oldpid.data, argv[0] );

        nt_free( env );
        nt_free( var );

        return NT_INVALID_PID;
    }

    pid = nt_execute( cycle, &ctx );

    if( pid == NT_INVALID_PID ) {
        if( nt_rename_file( ccf->oldpid.data, ccf->pid.data )
            == NT_FILE_ERROR ) {
            nt_log_error( NT_LOG_ALERT, cycle->log, nt_errno,
                          nt_rename_file_n " %s back to %s failed after "
                          "an attempt to execute new binary process \"%s\"",
                          ccf->oldpid.data, ccf->pid.data, argv[0] );
        }
    }

    nt_free( env );
    nt_free( var );

    return pid;
}


static nt_int_t
nt_get_options( int argc, char *const *argv )
{
    u_char     *p;
    nt_int_t   i;

    for( i = 1; i < argc; i++ ) {

        p = ( u_char * ) argv[i];

        if( *p++ != '-' ) {
            nt_log_stderr( 0, "invalid option: \"%s\"", argv[i] );
            return NT_ERROR;
        }

        while( *p ) {

            switch( *p++ ) {

            case '?':
            case 'h':
                return NT_ERROR;
                nt_show_version = 1;
                nt_show_help = 1;
                break;

            case 'v':
                return NT_ERROR;
                nt_show_version = 1;
                break;

            case 'V':
                return NT_ERROR;
                nt_show_version = 1;
                nt_show_configure = 1;
                break;

            case 't':
                return NT_ERROR;
                nt_test_config = 1;
                #if (NT_SSL && NT_SSL_ASYNC)
                nt_no_ssl_init = 1;
                #endif
                break;

            case 'T':
                return NT_ERROR;
                nt_test_config = 1;
                nt_dump_config = 1;
                #if (NT_SSL && NT_SSL_ASYNC)
                nt_no_ssl_init = 1;
                #endif
                break;

            case 'q':
                nt_quiet_mode = 1;
                break;

                #if (T_NT_SHOW_INFO)
            case 'l':
                return NT_ERROR;
                nt_test_config = 1;
                nt_show_version = 1;
                nt_show_directives = 1;
                break;

            case 'm':
                return NT_ERROR;
                nt_test_config = 1;
                nt_show_version = 1;
                nt_show_modules = 1;
                break;
                #endif

            case 'p':
                return NT_ERROR;
                if( *p ) {
                    nt_prefix = p;
                    goto next;
                }

                if( argv[++i] ) {
                    nt_prefix = ( u_char * ) argv[i];
                    goto next;
                }

                nt_log_stderr( 0, "option \"-p\" requires directory name" );
                return NT_ERROR;

            case 'c':
                if( *p ) {
                    nt_conf_file = p;
                    goto next;
                }

                if( argv[++i] ) {
                    nt_conf_file = ( u_char * ) argv[i];
                    goto next;
                }

                nt_log_stderr( 0, "option \"-c\" requires file name" );
                return NT_ERROR;

            case 'g':
                if( *p ) {
                    nt_conf_params = p;
                    goto next;
                }

                if( argv[++i] ) {
                    nt_conf_params = ( u_char * ) argv[i];
                    goto next;
                }

                nt_log_stderr( 0, "option \"-g\" requires parameter" );
                return NT_ERROR;

            case 's':
                return NT_ERROR;
                #if (NT_SSL && NT_SSL_ASYNC)
                nt_no_ssl_init = 1;
                #endif
                if( *p ) {
                    nt_signal = ( char * ) p;

                } else if( argv[++i] ) {
                    nt_signal = argv[i];

                } else {
                    nt_log_stderr( 0, "option \"-s\" requires parameter" );
                    return NT_ERROR;
                }

                if( nt_strcmp( nt_signal, "stop" ) == 0
                    || nt_strcmp( nt_signal, "quit" ) == 0
                    || nt_strcmp( nt_signal, "reopen" ) == 0
                    || nt_strcmp( nt_signal, "reload" ) == 0 ) {
                    nt_process = NT_PROCESS_SIGNALLER;
                    goto next;
                }

                nt_log_stderr( 0, "invalid option: \"-s %s\"", nt_signal );
                return NT_ERROR;

            default:
                nt_log_stderr( 0, "invalid option: \"%c\"", *( p - 1 ) );
                #if (NT_SSL && NT_SSL_ASYNC)
                nt_no_ssl_init = 1;
                #endif
                return NT_ERROR;
            }
        }

next:

        continue;
    }

    return NT_OK;
}


nt_int_t
nt_save_argv( nt_cycle_t *cycle, int argc, char *const *argv )
{
    #if (NT_FREEBSD)

    nt_os_argv = ( char ** ) argv;
    nt_argc = argc;
    nt_argv = ( char ** ) argv;

    #else
    size_t     len;
    nt_int_t  i;

    nt_os_argv = ( char ** ) argv;
    nt_argc = argc;

#if 1
    nt_argv = nt_pcalloc( cycle->pool,  ( argc + 1 ) * sizeof( char * ) );
#else
    nt_argv = nt_alloc( ( argc + 1 ) * sizeof( char * ), cycle->log );
#endif

    if( nt_argv == NULL ) {
        return NT_ERROR;
    }

    for( i = 0; i < argc; i++ ) {
        len = nt_strlen( argv[i] ) + 1;

#if 1
        nt_argv[i] = nt_palloc(  cycle->pool, len );
#else
        nt_argv[i] = nt_alloc( len, cycle->log );
#endif
        if( nt_argv[i] == NULL ) {
            return NT_ERROR;
        }

        ( void ) nt_cpystrn( ( u_char * ) nt_argv[i], ( u_char * ) argv[i], len );
    }

    nt_argv[i] = NULL;

    #endif

    nt_os_environ = environ;

    return NT_OK;
}


static nt_int_t
nt_process_options( nt_cycle_t *cycle )
{
    u_char  *p;
    size_t   len;

    if( nt_prefix ) {
        len = nt_strlen( nt_prefix );
        p = nt_prefix;

        if( len && !nt_path_separator( p[len - 1] ) ) {
            p = nt_pnalloc( cycle->pool, len + 1 );
            if( p == NULL ) {
                return NT_ERROR;
            }

            nt_memcpy( p, nt_prefix, len );
            p[len++] = '/';
        }

        cycle->conf_prefix.len = len;
        cycle->conf_prefix.data = p;
        cycle->prefix.len = len;
        cycle->prefix.data = p;

    } else {

        #ifndef NT_PREFIX

        p = nt_pnalloc( cycle->pool, NT_MAX_PATH );
        if( p == NULL ) {
            return NT_ERROR;
        }

        if( nt_getcwd( p, NT_MAX_PATH ) == 0 ) {
            nt_log_stderr( nt_errno, "[emerg]: " nt_getcwd_n " failed" );
            return NT_ERROR;
        }

        len = nt_strlen( p );

        p[len++] = '/';

        cycle->conf_prefix.len = len;
        cycle->conf_prefix.data = p;
        cycle->prefix.len = len;
        cycle->prefix.data = p;

        #else

        #ifdef NT_CONF_PREFIX
        nt_str_set( &cycle->conf_prefix, NT_CONF_PREFIX );
        #else
        nt_str_set( &cycle->conf_prefix, NT_PREFIX );
        #endif
        nt_str_set( &cycle->prefix, NT_PREFIX );

        #endif
    }

    if( nt_conf_file ) {
        cycle->conf_file.len = nt_strlen( nt_conf_file );
        cycle->conf_file.data = nt_conf_file;

    } else {
        nt_str_set( &cycle->conf_file, NT_CONF_PATH );
    }

    if( nt_conf_full_name( cycle, &cycle->conf_file, 0 ) != NT_OK ) {
        return NT_ERROR;
    }

    for( p = cycle->conf_file.data + cycle->conf_file.len - 1;
         p > cycle->conf_file.data;
         p-- ) {
        if( nt_path_separator( *p ) ) {
            cycle->conf_prefix.len = p - cycle->conf_file.data + 1;
            cycle->conf_prefix.data = cycle->conf_file.data;
            break;
        }
    }

    if( nt_conf_params ) {
        cycle->conf_param.len = nt_strlen( nt_conf_params );
        cycle->conf_param.data = nt_conf_params;
    }

    if( nt_test_config ) {
        cycle->log->log_level = NT_LOG_INFO;
    }

    return NT_OK;
}


static void *
nt_core_module_create_conf( nt_cycle_t *cycle )
{
    nt_core_conf_t  *ccf;

    ccf = nt_pcalloc( cycle->pool, sizeof( nt_core_conf_t ) );
    if( ccf == NULL ) {
        return NULL;
    }

    /*
     * set by nt_pcalloc()
     *
     *     ccf->pid = NULL;
     *     ccf->oldpid = NULL;
     *     ccf->priority = 0;
     *     ccf->cpu_affinity_auto = 0;
     *     ccf->cpu_affinity_n = 0;
     *     ccf->cpu_affinity = NULL;
     */

    ccf->daemon = NT_CONF_UNSET;
    ccf->master = NT_CONF_UNSET;
    ccf->timer_resolution = NT_CONF_UNSET_MSEC;
    ccf->shutdown_timeout = NT_CONF_UNSET_MSEC;

    ccf->worker_processes = NT_CONF_UNSET;
    ccf->debug_points = NT_CONF_UNSET;

    ccf->rlimit_nofile = NT_CONF_UNSET;
    ccf->rlimit_core = NT_CONF_UNSET;

    ccf->user = ( nt_uid_t ) NT_CONF_UNSET_UINT;
    ccf->group = ( nt_gid_t ) NT_CONF_UNSET_UINT;

    if( nt_array_init( &ccf->env, cycle->pool, 1, sizeof( nt_str_t ) )
        != NT_OK ) {
        return NULL;
    }

    return ccf;
}


static char *
nt_core_module_init_conf( nt_cycle_t *cycle, void *conf )
{
    nt_core_conf_t  *ccf = conf;

    nt_conf_init_value( ccf->daemon, 1 );
    nt_conf_init_value( ccf->master, 1 );
    nt_conf_init_msec_value( ccf->timer_resolution, 0 );
    nt_conf_init_msec_value( ccf->shutdown_timeout, 0 );

    nt_conf_init_value( ccf->worker_processes,
                        #if (T_NT_MODIFY_DEFAULT_VALUE)
                        nt_ncpu );
                        #else
                        1 );
                        #endif
    nt_conf_init_value( ccf->debug_points, 0 );

    #if (NT_HAVE_CPU_AFFINITY)

    if( !ccf->cpu_affinity_auto
        && ccf->cpu_affinity_n
        && ccf->cpu_affinity_n != 1
        && ccf->cpu_affinity_n != ( nt_uint_t ) ccf->worker_processes ) {
        nt_log_error( NT_LOG_WARN, cycle->log, 0,
                      "the number of \"worker_processes\" is not equal to "
                      "the number of \"worker_cpu_affinity\" masks, "
                      "using last mask for remaining worker processes" );
    }

    #endif


    if( ccf->pid.len == 0 ) {
        nt_str_set( &ccf->pid, NT_PID_PATH );
    }

    if( nt_conf_full_name( cycle, &ccf->pid, 0 ) != NT_OK ) {
        return NT_CONF_ERROR;
    }

    ccf->oldpid.len = ccf->pid.len + sizeof( NT_OLDPID_EXT );

    ccf->oldpid.data = nt_pnalloc( cycle->pool, ccf->oldpid.len );
    if( ccf->oldpid.data == NULL ) {
        return NT_CONF_ERROR;
    }

    nt_memcpy( nt_cpymem( ccf->oldpid.data, ccf->pid.data, ccf->pid.len ),
               NT_OLDPID_EXT, sizeof( NT_OLDPID_EXT ) );


    #if !(NT_WIN32)


//   lvlai add , 去掉指定用户启动
    if( ccf->user == ( uid_t ) NT_CONF_UNSET_UINT && geteuid() == 0 ) {
        struct group   *grp;
        struct passwd  *pwd;

        nt_set_errno( 0 );

//        pwd = getpwnam(NT_USER);
//        pwd = getpwnam(getuid());
        pwd = getpwuid( getuid() );

        if( pwd == NULL ) {
            nt_log_error( NT_LOG_EMERG, cycle->log, nt_errno,
                          "getpwnam(\"" NT_USER "\") failed" );
            return NT_CONF_ERROR;
        }

//        ccf->username = NT_USER;
        ccf->username = pwd->pw_name;
        ccf->user = pwd->pw_uid;


        nt_set_errno( 0 );
//        grp = getgrnam(NT_GROUP);
        grp = getgrgid( getgid() );

        if( grp == NULL ) {
            //      nt_log_error( NT_LOG_EMERG, cycle->log, nt_errno,
            //                     "getgrnam(\"" NT_GROUP "\") failed" );
            //      return NT_CONF_ERROR;
            //mifon 类86u机器会这样
            //若grp失败，直接设置为 0
            ccf->group = 0;
        } else
            ccf->group = grp->gr_gid;

    }


    if( ccf->lock_file.len == 0 ) {
        nt_str_set( &ccf->lock_file, NT_LOCK_PATH );
    }

    if( nt_conf_full_name( cycle, &ccf->lock_file, 0 ) != NT_OK ) {
        return NT_CONF_ERROR;
    }

    {
        nt_str_t  lock_file;

        lock_file = cycle->old_cycle->lock_file;

        if( lock_file.len ) {
            lock_file.len--;

            if( ccf->lock_file.len != lock_file.len
                || nt_strncmp( ccf->lock_file.data, lock_file.data, lock_file.len )
                != 0 ) {
                nt_log_error( NT_LOG_EMERG, cycle->log, 0,
                              "\"lock_file\" could not be changed, ignored" );
            }

            cycle->lock_file.len = lock_file.len + 1;
            lock_file.len += sizeof( ".accept" );

            cycle->lock_file.data = nt_pstrdup( cycle->pool, &lock_file );
            if( cycle->lock_file.data == NULL ) {
                return NT_CONF_ERROR;
            }

        } else {
            cycle->lock_file.len = ccf->lock_file.len + 1;
            cycle->lock_file.data = nt_pnalloc( cycle->pool,
                                                ccf->lock_file.len + sizeof( ".accept" ) );
            if( cycle->lock_file.data == NULL ) {
                return NT_CONF_ERROR;
            }

            nt_memcpy( nt_cpymem( cycle->lock_file.data, ccf->lock_file.data,
                                  ccf->lock_file.len ),
                       ".accept", sizeof( ".accept" ) );
        }
    }

    #endif

    return NT_CONF_OK;
}


static char *
nt_set_user( nt_conf_t *cf, nt_command_t *cmd, void *conf )
{
    #if (NT_WIN32)

    nt_conf_log_error( NT_LOG_WARN, cf, 0,
                       "\"user\" is not supported, ignored" );

    return NT_CONF_OK;

    #else

    nt_core_conf_t  *ccf = conf;

    char             *group;
    struct passwd    *pwd;
    struct group     *grp;
    nt_str_t        *value;

    if( ccf->user != ( uid_t ) NT_CONF_UNSET_UINT ) {
        return "is duplicate";
    }

    if( geteuid() != 0 ) {
        nt_conf_log_error( NT_LOG_WARN, cf, 0,
                           "the \"user\" directive makes sense only "
                           "if the master process runs "
                           "with super-user privileges, ignored" );
        return NT_CONF_OK;
    }

    value = cf->args->elts;

    ccf->username = ( char * ) value[1].data;

    nt_set_errno( 0 );
    pwd = getpwnam( ( const char * ) value[1].data );
    if( pwd == NULL ) {
        nt_conf_log_error( NT_LOG_EMERG, cf, nt_errno,
                           "getpwnam(\"%s\") failed", value[1].data );
        return NT_CONF_ERROR;
    }

    ccf->user = pwd->pw_uid;

    group = ( char * )( ( cf->args->nelts == 2 ) ? value[1].data : value[2].data );

    nt_set_errno( 0 );
    grp = getgrnam( group );
    if( grp == NULL ) {
        nt_conf_log_error( NT_LOG_EMERG, cf, nt_errno,
                           "getgrnam(\"%s\") failed", group );
        return NT_CONF_ERROR;
    }

    ccf->group = grp->gr_gid;

    return NT_CONF_OK;

    #endif
}


static char *
nt_set_env( nt_conf_t *cf, nt_command_t *cmd, void *conf )
{
    nt_core_conf_t  *ccf = conf;

    nt_str_t   *value, *var;
    nt_uint_t   i;

    var = nt_array_push( &ccf->env );
    if( var == NULL ) {
        return NT_CONF_ERROR;
    }

    value = cf->args->elts;
    *var = value[1];

    for( i = 0; i < value[1].len; i++ ) {

        if( value[1].data[i] == '=' ) {

            var->len = i;

            return NT_CONF_OK;
        }
    }

    return NT_CONF_OK;
}


static char *
nt_set_priority( nt_conf_t *cf, nt_command_t *cmd, void *conf )
{
    nt_core_conf_t  *ccf = conf;

    nt_str_t        *value;
    nt_uint_t        n, minus;

    if( ccf->priority != 0 ) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if( value[1].data[0] == '-' ) {
        n = 1;
        minus = 1;

    } else if( value[1].data[0] == '+' ) {
        n = 1;
        minus = 0;

    } else {
        n = 0;
        minus = 0;
    }

    ccf->priority = nt_atoi( &value[1].data[n], value[1].len - n );
    if( ccf->priority == NT_ERROR ) {
        return "invalid number";
    }

    if( minus ) {
        ccf->priority = -ccf->priority;
    }

    return NT_CONF_OK;
}


static char *
nt_set_cpu_affinity( nt_conf_t *cf, nt_command_t *cmd, void *conf )
{
    #if (NT_HAVE_CPU_AFFINITY)
    nt_core_conf_t  *ccf = conf;

    u_char            ch, *p;
    nt_str_t        *value;
    nt_uint_t        i, n;
    nt_cpuset_t     *mask;

    if( ccf->cpu_affinity ) {
        return "is duplicate";
    }

    mask = nt_palloc( cf->pool, ( cf->args->nelts - 1 ) * sizeof( nt_cpuset_t ) );
    if( mask == NULL ) {
        return NT_CONF_ERROR;
    }

    ccf->cpu_affinity_n = cf->args->nelts - 1;
    ccf->cpu_affinity = mask;

    value = cf->args->elts;

    if( nt_strcmp( value[1].data, "auto" ) == 0 ) {

        if( cf->args->nelts > 3 ) {
            nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                               "invalid number of arguments in "
                               "\"worker_cpu_affinity\" directive" );
            return NT_CONF_ERROR;
        }

        ccf->cpu_affinity_auto = 1;

        CPU_ZERO( &mask[0] );
        for( i = 0; i < ( nt_uint_t ) nt_min( nt_ncpu, CPU_SETSIZE ); i++ ) {
            CPU_SET( i, &mask[0] );
        }

        n = 2;

    } else {
        n = 1;
    }

    for( /* void */ ; n < cf->args->nelts; n++ ) {

        if( value[n].len > CPU_SETSIZE ) {
            nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                               "\"worker_cpu_affinity\" supports up to %d CPUs only",
                               CPU_SETSIZE );
            return NT_CONF_ERROR;
        }

        i = 0;
        CPU_ZERO( &mask[n - 1] );

        for( p = value[n].data + value[n].len - 1;
             p >= value[n].data;
             p-- ) {
            ch = *p;

            if( ch == ' ' ) {
                continue;
            }

            i++;

            if( ch == '0' ) {
                continue;
            }

            if( ch == '1' ) {
                CPU_SET( i - 1, &mask[n - 1] );
                continue;
            }

            nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                               "invalid character \"%c\" in \"worker_cpu_affinity\"",
                               ch );
            return NT_CONF_ERROR;
        }
    }

    #else

    nt_conf_log_error( NT_LOG_WARN, cf, 0,
                       "\"worker_cpu_affinity\" is not supported "
                       "on this platform, ignored" );
    #endif

    return NT_CONF_OK;
}


nt_cpuset_t *
nt_get_cpu_affinity( nt_uint_t n )
{
    #if (NT_HAVE_CPU_AFFINITY)
    nt_uint_t        i, j;
    nt_cpuset_t     *mask;
    nt_core_conf_t  *ccf;

    static nt_cpuset_t  result;

    ccf = ( nt_core_conf_t * ) nt_get_conf( nt_cycle->conf_ctx,
                                            nt_core_module );

    if( ccf->cpu_affinity == NULL ) {
        return NULL;
    }

    if( ccf->cpu_affinity_auto ) {
        mask = &ccf->cpu_affinity[ccf->cpu_affinity_n - 1];

        for( i = 0, j = n; /* void */ ; i++ ) {

            if( CPU_ISSET( i % CPU_SETSIZE, mask ) && j-- == 0 ) {
                break;
            }

            if( i == CPU_SETSIZE && j == n ) {
                /* empty mask */
                return NULL;
            }

            /* void */
        }

        CPU_ZERO( &result );
        CPU_SET( i % CPU_SETSIZE, &result );

        return &result;
    }

    if( ccf->cpu_affinity_n > n ) {
        return &ccf->cpu_affinity[n];
    }

    return &ccf->cpu_affinity[ccf->cpu_affinity_n - 1];

    #else

    return NULL;

    #endif
}


static char *
nt_set_worker_processes( nt_conf_t *cf, nt_command_t *cmd, void *conf )
{
    nt_str_t        *value;
    nt_core_conf_t  *ccf;

    ccf = ( nt_core_conf_t * ) conf;

    if( ccf->worker_processes != NT_CONF_UNSET ) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if( nt_strcmp( value[1].data, "auto" ) == 0 ) {
        ccf->worker_processes = nt_ncpu;
        return NT_CONF_OK;
    }

    ccf->worker_processes = nt_atoi( value[1].data, value[1].len );

    if( ccf->worker_processes == NT_ERROR ) {
        return "invalid value";
    }

    return NT_CONF_OK;
}


static char *
nt_load_module( nt_conf_t *cf, nt_command_t *cmd, void *conf )
{
    #if (NT_HAVE_DLOPEN)
    void                *handle;
    char               **names, **order;
    nt_str_t           *value, file;
    nt_uint_t           i;
    nt_module_t        *module, **modules;
    nt_pool_cleanup_t  *cln;

    if( cf->cycle->modules_used ) {
        return "is specified too late";
    }

    value = cf->args->elts;

    file = value[1];

    if( nt_conf_full_name( cf->cycle, &file, 0 ) != NT_OK ) {
        return NT_CONF_ERROR;
    }

    cln = nt_pool_cleanup_add( cf->cycle->pool, 0 );
    if( cln == NULL ) {
        return NT_CONF_ERROR;
    }

    handle = nt_dlopen( file.data );
    if( handle == NULL ) {
        nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                           nt_dlopen_n " \"%s\" failed (%s)",
                           file.data, nt_dlerror() );
        return NT_CONF_ERROR;
    }

    cln->handler = nt_unload_module;
    cln->data = handle;

    modules = nt_dlsym( handle, "nt_modules" );
    if( modules == NULL ) {
        nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                           nt_dlsym_n " \"%V\", \"%s\" failed (%s)",
                           &value[1], "nt_modules", nt_dlerror() );
        return NT_CONF_ERROR;
    }

    names = nt_dlsym( handle, "nt_module_names" );
    if( names == NULL ) {
        nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                           nt_dlsym_n " \"%V\", \"%s\" failed (%s)",
                           &value[1], "nt_module_names", nt_dlerror() );
        return NT_CONF_ERROR;
    }

    order = nt_dlsym( handle, "nt_module_order" );

    for( i = 0; modules[i]; i++ ) {
        module = modules[i];
        module->name = names[i];

        if( nt_add_module( cf, &file, module, order ) != NT_OK ) {
            return NT_CONF_ERROR;
        }

        nt_log_debug2( NT_LOG_DEBUG_CORE, cf->log, 0, "module: %s i:%ui",
                       module->name, module->index );
    }

    return NT_CONF_OK;

    #else

    nt_conf_log_error( NT_LOG_EMERG, cf, 0,
                       "\"load_module\" is not supported "
                       "on this platform" );
    return NT_CONF_ERROR;

    #endif
}


#if (NT_HAVE_DLOPEN)

static void
nt_unload_module( void *data )
{
    void  *handle = data;

    if( nt_dlclose( handle ) != 0 ) {
        nt_log_error( NT_LOG_ALERT, nt_cycle->log, 0,
                      nt_dlclose_n " failed (%s)", nt_dlerror() );
    }
}

#endif


#if (T_NT_MASTER_ENV)

static char *
nt_master_set_env( nt_conf_t *cf, nt_command_t *cmd, void *conf )
{
    size_t       klen, vlen;
    u_char      *key, *v, *p;
    nt_str_t   *value;

    value = cf->args->elts;
    p = nt_strlchr( value[1].data, value[1].data + value[1].len, '=' );
    if( p == NULL ) {
        klen = value[1].len;
        v = ( u_char * ) "1";

    } else {
        klen = p - value[1].data;
        vlen = value[1].len - klen - 1;
        v = nt_palloc( cf->pool, vlen + 1 );
        if( v == NULL ) {
            return NT_CONF_ERROR;
        }

        nt_memcpy( v, p + 1, vlen );
        v[vlen] = '\0';
    }

    key = nt_palloc( cf->pool, klen + 1 );
    if( key == NULL ) {
        return NT_CONF_ERROR;
    }

    nt_memcpy( key, value[1].data, klen );
    key[klen] = '\0';

    setenv( ( char * ) key, ( char * ) v, 0 );

    return NT_CONF_OK;
}

#endif


nt_pool_t *process_pool;
nt_int_t nt_main_init( int argc, char *const *argv )
{


    nt_buf_t        *b;
    nt_log_t        *log;
    nt_uint_t        i;
    nt_cycle_t      *cycle, init_cycle;
    nt_conf_dump_t  *cd;
    nt_core_conf_t  *ccf;

    

    /*
     * 这个函数主要是作为初始化debug用的，nginx中的debug，主要是对内存池分配管理方面的debug，
     * 因为作为一个应用程序，最容易出现bug的地方也是内存管理这块
     * */
    nt_debug_init();


    //获得运行时选项
    if( nt_get_options( argc, argv ) != NT_OK ) {
        return 1;
    }

    // 显示版本信息和帮助信息
    if( nt_show_version ) {
        nt_show_version_info();

        if( !nt_test_config ) {
            return 0;
        }
    }


    /* TODO */ nt_max_sockets = -1;

    //初始化系统时间
    nt_time_init();

    #if (NT_PCRE)
    nt_regex_init();
    #endif

    nt_pid = nt_getpid();
    nt_parent = nt_getppid();

    //初始化日志信息
    log = nt_log_init( nt_prefix );
    if( log == NULL ) {
        return 1;
    }


    process_pool = nt_create_pool( 1024, log );
    if( process_pool == NULL )
        return 1;

    /*
     *     我们来到程序的第208行，nt_strerror_init()，该函数的定义在文件src/os/unix/nt_errno.c中。
     *     该函数主要初始化系统中错误编号对应的含义，这样初始化中进行对应的好处是，当出现错误，
     *     不用再去调用strerror()函数来获取错误原因，而直接可以根据错误编号找到对应的错误原因，
     *     可以提高运行时的执行效率。从这里可以看到，nginx开发者的别有用心，对于微小的性能提升也毫不放过。
     *  */
    if( nt_strerror_init() != NT_OK ) {
        return 1;
    }


    /* STUB */
    #if (NT_OPENSSL)
    #if (NT_SSL && NT_SSL_ASYNC)
    if( !nt_no_ssl_init ) {
    #endif
        nt_ssl_init( log );
        #if (NT_SSL && NT_SSL_ASYNC)
    }
        #endif
    #endif

    /*
        * init_cycle->log is required for signal handlers and
        * nt_process_options()
        */

    /*
    nt_string.h 第86行
    #define nt_memzero(buf, n)       (void) memset(buf, 0, n)
    nt_cycle_s 机构体:nt_cycle.h 第37行
    */
    //开始初始化nt_cycle_s *init_cycle对象
    nt_memzero( &init_cycle, sizeof( nt_cycle_t ) );
    init_cycle.log = log;
    #if (NT_SSL && NT_SSL_ASYNC)
    init_cycle.no_ssl_init = nt_no_ssl_init;
    #endif
    nt_cycle = &init_cycle;

    // 为nt_cycle 创建内存池
    init_cycle.pool = nt_create_pool( 1024, log );
    if( init_cycle.pool == NULL ) {
        return 1;
    }

    // 保存进程参数到init_cycle中
    if( nt_save_argv( &init_cycle, argc, argv ) != NT_OK ) {
        return 1;
    }

    // 处理进程参数
    if( nt_process_options( &init_cycle ) != NT_OK ) {
        return 1;
    }

    // 获取系统配置信息 (分页大小，CPU 个数，CPU 制造商，文件符最大数等)
    if( nt_os_init( log ) != NT_OK ) {
        return 1;
    }

    /*
     * nt_crc32_table_init() requires nt_cacheline_size set in nt_os_init()
     */
    // 初始化循环冗余检验表,验证接收数据
    if( nt_crc32_table_init() != NT_OK ) {
        return 1;
    }


    /*
    * nt_slab_sizes_init() requires nt_pagesize set in nt_os_init()
    */

    //slab大小的初始化
    nt_slab_sizes_init();

    //通过环境变量NGINX完成socket的继承，这些继承来的socket会放到init_cycyle的listening数组中
    if( nt_add_inherited_sockets( &init_cycle ) != NT_OK ) {
        return 1;
    }

    //初始化nginx的扩展模块
    /**
     * nginx把所有模块（nt_module_t）存放到nt_modules数组中，这个数组在nginx源码路
     * 径的objs/nt_modules.c中，是在运行configure脚本后生成的。index属性就是该模块
     * 在nt_modules数组中的下标。同时nginx把所有的core module的配置结构存放到nt_cycle的
     * conf_ctx数组中，index也是该模块的配置结构在nt_cycle->conf_ctx数组中的下标。
     **/
    if( nt_preinit_modules() != NT_OK ) {
        return 1;
    }


    /*
     * 对nt_cycle结构进行初始化,这里是nginx启动核心之处！
     * 以及配置文件的读取初始化等
     * 这里面会把init_cycle 作为old_cycle, 并释放掉，重新生成一个新的cycle
     * */
    cycle = nt_init_cycle( &init_cycle );
    if( cycle == NULL ) {
        if( nt_test_config ) {
            nt_log_stderr( 0, "configuration file %s test failed",
                            init_cycle.conf_file.data );
        }

        return 1;
    }


    //测试配置是否正确
    if( nt_test_config ) {
        if( !nt_quiet_mode ) {
            nt_log_stderr( 0, "configuration file %s test is successful",
                            cycle->conf_file.data );
        }

        //打印配置信息
        if( nt_dump_config ) {
            cd = cycle->config_dump.elts;

            for( i = 0; i < cycle->config_dump.nelts; i++ ) {

                nt_write_stdout( "# configuration file " );
                ( void ) nt_write_fd( nt_stdout, cd[i].name.data,
                                       cd[i].name.len );
                nt_write_stdout( ":" NT_LINEFEED );

                b = cd[i].buffer;

                ( void ) nt_write_fd( nt_stdout, b->pos, b->last - b->pos );
                nt_write_stdout( NT_LINEFEED );
            }
        }

        return 0;
    }

    // 检查是否有设置信号处理，如有，进入nt_signal_process处理
    if( nt_signal ) {
        return nt_signal_process( cycle, nt_signal );
    }


    debug( "run nt_os_status" );
    nt_cycle = cycle;
    //nginx编译信息
    nt_os_status( cycle->log );
    debug( "run nt_os_status end" );


    //获取全局配置中的core配置
    ccf = ( nt_core_conf_t * ) nt_get_conf( cycle->conf_ctx, nt_core_module );

    if( ccf->master && nt_process == NT_PROCESS_SINGLE ) {
        nt_process = NT_PROCESS_MASTER;
    }

    #if !(NT_WIN32)
    //初始化信号
    if( nt_init_signals( cycle->log ) != NT_OK ) {
        return 1;
    }

    //设置nginx为daemon
    if( !nt_inherited && ccf->daemon ) {
        if( nt_daemon( cycle->log ) != NT_OK ) {
            return 1;
        }

        nt_daemonized = 1;
    }

    if( nt_inherited ) {
        nt_daemonized = 1;
    }

    #endif

    #if (T_PIPES)
    //pipe
    if( nt_open_pipes( cycle ) == NT_ERROR ) {
        nt_log_error( NT_LOG_EMERG, cycle->log, 0, "can not open pipes" );
        return 1;
    }
    #endif

    //创建pid文件
    if( nt_create_pidfile( &ccf->pid, cycle->log ) != NT_OK ) {
        return 1;
    }

    //检查log文件句柄
    if( nt_log_redirect_stderr( cycle ) != NT_OK ) {
        return 1;
    }

    if( log->file->fd != nt_stderr ) {
        if( nt_close_file( log->file->fd ) == NT_FILE_ERROR ) {
            nt_log_error( NT_LOG_ALERT, cycle->log, nt_errno,
                           nt_close_file_n " built-in log failed" );
        }
    }

    nt_use_stderr = 0;

    //判断nginx是否执行单进程
    if( nt_process == NT_PROCESS_SINGLE ) {
        //  fprintf(stderr,"nt_single_process_cycle\n");
        //        nt_log_error(0 , log, 0, "main nt_single_process_cycle");
        //  debug(2, "main nt_single_process_cycle");
        nt_single_process_cycle( cycle );

    } else {
          debug( "main nt_master_process_cycle");
        //  fprintf(stderr,"nt_master_process_cycle\n");
        //        nt_log_error(0 , log, 0, "main nt_master_process_cycle");
        nt_master_process_cycle( cycle );
    }

    return NT_OK;

}
