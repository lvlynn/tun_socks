

#include <nt_core.h>
#include <nt_event.h>

#if (T_NT_SHOW_INFO)
extern nt_uint_t  nt_modules_n;
#endif

static void nt_destroy_cycle_pools(nt_conf_t *conf);
static nt_int_t nt_init_zone_pool(nt_cycle_t *cycle,
    nt_shm_zone_t *shm_zone);
static nt_int_t nt_test_lockfile(u_char *file, nt_log_t *log);
static void nt_clean_old_cycles(nt_event_t *ev);
static void nt_shutdown_timer_handler(nt_event_t *ev);


volatile nt_cycle_t  *nt_cycle;
nt_array_t            nt_old_cycles;

static nt_pool_t     *nt_temp_pool;
static nt_event_t     nt_cleaner_event;
static nt_event_t     nt_shutdown_event;

nt_uint_t             nt_test_config;
nt_uint_t             nt_dump_config;
nt_uint_t             nt_quiet_mode;
#if (T_NT_SHOW_INFO)
nt_uint_t             nt_show_modules;
nt_uint_t             nt_show_directives;
#endif


/* STUB NAME */
static nt_connection_t  dumb;
/* STUB */


nt_cycle_t *
nt_init_cycle(nt_cycle_t *old_cycle)
{
    void                *rv;
    char               **senv;
    nt_uint_t           i, n;
    nt_log_t           *log;
    nt_time_t          *tp;
    nt_conf_t           conf;
    nt_pool_t          *pool;
    nt_cycle_t         *cycle, **old;
    nt_shm_zone_t      *shm_zone, *oshm_zone;
    nt_list_part_t     *part, *opart;
    nt_open_file_t     *file;
    nt_listening_t     *ls, *nls;
    nt_core_conf_t     *ccf, *old_ccf;
    nt_core_module_t   *module;
    char                 hostname[NT_MAXHOSTNAMELEN];

    nt_timezone_update();

    /* force localtime update with a new timezone */

    tp = nt_timeofday();
    tp->sec = 0;

    nt_time_update();

#if (T_PIPES)
    nt_increase_pipe_generation();
#endif

    log = old_cycle->log;

    pool = nt_create_pool(NT_CYCLE_POOL_SIZE, log);
    if (pool == NULL) {
        return NULL;
    }
    pool->log = log;

    cycle = nt_pcalloc(pool, sizeof(nt_cycle_t));
    if (cycle == NULL) {
        nt_destroy_pool(pool);
        return NULL;
    }

    cycle->pool = pool;
    cycle->log = log;
    cycle->old_cycle = old_cycle;
#if (NT_SSL && NT_SSL_ASYNC)
    cycle->no_ssl_init = old_cycle->no_ssl_init;
#endif

    cycle->conf_prefix.len = old_cycle->conf_prefix.len;
    cycle->conf_prefix.data = nt_pstrdup(pool, &old_cycle->conf_prefix);
    if (cycle->conf_prefix.data == NULL) {
        nt_destroy_pool(pool);
        return NULL;
    }

    cycle->prefix.len = old_cycle->prefix.len;
    cycle->prefix.data = nt_pstrdup(pool, &old_cycle->prefix);
    if (cycle->prefix.data == NULL) {
        nt_destroy_pool(pool);
        return NULL;
    }

    cycle->conf_file.len = old_cycle->conf_file.len;
    cycle->conf_file.data = nt_pnalloc(pool, old_cycle->conf_file.len + 1);
    if (cycle->conf_file.data == NULL) {
        nt_destroy_pool(pool);
        return NULL;
    }
    nt_cpystrn(cycle->conf_file.data, old_cycle->conf_file.data,
                old_cycle->conf_file.len + 1);

    cycle->conf_param.len = old_cycle->conf_param.len;
    cycle->conf_param.data = nt_pstrdup(pool, &old_cycle->conf_param);
    if (cycle->conf_param.data == NULL) {
        nt_destroy_pool(pool);
        return NULL;
    }


    n = old_cycle->paths.nelts ? old_cycle->paths.nelts : 10;

    if (nt_array_init(&cycle->paths, pool, n, sizeof(nt_path_t *))
        != NT_OK)
    {
        nt_destroy_pool(pool);
        return NULL;
    }

    nt_memzero(cycle->paths.elts, n * sizeof(nt_path_t *));


    if (nt_array_init(&cycle->config_dump, pool, 1, sizeof(nt_conf_dump_t))
        != NT_OK)
    {
        nt_destroy_pool(pool);
        return NULL;
    }

    nt_rbtree_init(&cycle->config_dump_rbtree, &cycle->config_dump_sentinel,
                    nt_str_rbtree_insert_value);

    if (old_cycle->open_files.part.nelts) {
        n = old_cycle->open_files.part.nelts;
        for (part = old_cycle->open_files.part.next; part; part = part->next) {
            n += part->nelts;
        }

    } else {
        n = 20;
    }

    if (nt_list_init(&cycle->open_files, pool, n, sizeof(nt_open_file_t))
        != NT_OK)
    {
        nt_destroy_pool(pool);
        return NULL;
    }


    if (old_cycle->shared_memory.part.nelts) {
        n = old_cycle->shared_memory.part.nelts;
        for (part = old_cycle->shared_memory.part.next; part; part = part->next)
        {
            n += part->nelts;
        }

    } else {
        n = 1;
    }

    if (nt_list_init(&cycle->shared_memory, pool, n, sizeof(nt_shm_zone_t))
        != NT_OK)
    {
        nt_destroy_pool(pool);
        return NULL;
    }

    n = old_cycle->listening.nelts ? old_cycle->listening.nelts : 10;

    if (nt_array_init(&cycle->listening, pool, n, sizeof(nt_listening_t))
        != NT_OK)
    {
        nt_destroy_pool(pool);
        return NULL;
    }

    nt_memzero(cycle->listening.elts, n * sizeof(nt_listening_t));


    nt_queue_init(&cycle->reusable_connections_queue);


    cycle->conf_ctx = nt_pcalloc(pool, nt_max_module * sizeof(void *));
    if (cycle->conf_ctx == NULL) {
        nt_destroy_pool(pool);
        return NULL;
    }


    if (gethostname(hostname, NT_MAXHOSTNAMELEN) == -1) {
        nt_log_error(NT_LOG_EMERG, log, nt_errno, "gethostname() failed");
        nt_destroy_pool(pool);
        return NULL;
    }

    /* on Linux gethostname() silently truncates name that does not fit */

    hostname[NT_MAXHOSTNAMELEN - 1] = '\0';
    cycle->hostname.len = nt_strlen(hostname);

    cycle->hostname.data = nt_pnalloc(pool, cycle->hostname.len);
    if (cycle->hostname.data == NULL) {
        nt_destroy_pool(pool);
        return NULL;
    }

    nt_strlow(cycle->hostname.data, (u_char *) hostname, cycle->hostname.len);


    if (nt_cycle_modules(cycle) != NT_OK) {
        nt_destroy_pool(pool);
        return NULL;
    }


    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->type != NT_CORE_MODULE) {
            continue;
        }

        module = cycle->modules[i]->ctx;

        if (module->create_conf) {
            rv = module->create_conf(cycle);
            if (rv == NULL) {
                nt_destroy_pool(pool);
                return NULL;
            }
            cycle->conf_ctx[cycle->modules[i]->index] = rv;
        }
    }


    senv = environ;


    nt_memzero(&conf, sizeof(nt_conf_t));
    /* STUB: init array ? */
    conf.args = nt_array_create(pool, 10, sizeof(nt_str_t));
    if (conf.args == NULL) {
        nt_destroy_pool(pool);
        return NULL;
    }

    conf.temp_pool = nt_create_pool(NT_CYCLE_POOL_SIZE, log);
    if (conf.temp_pool == NULL) {
        nt_destroy_pool(pool);
        return NULL;
    }


    conf.ctx = cycle->conf_ctx;
    conf.cycle = cycle;
    conf.pool = pool;
    conf.log = log;
    conf.module_type = NT_CORE_MODULE;
    conf.cmd_type = NT_MAIN_CONF;
#if (NT_SSL && NT_SSL_ASYNC)
    conf.no_ssl_init = cycle->no_ssl_init;
#endif

#if 0
    log->log_level = NT_LOG_DEBUG_ALL;
#endif

    if (nt_conf_param(&conf) != NT_CONF_OK) {
        environ = senv;
        nt_destroy_cycle_pools(&conf);
        return NULL;
    }

    if (nt_conf_parse(&conf, &cycle->conf_file) != NT_CONF_OK) {
        environ = senv;
        nt_destroy_cycle_pools(&conf);
        return NULL;
    }

#if (T_NT_SHOW_INFO)
    nt_command_t     *cmd;
    if (nt_show_directives) {
        nt_log_stderr(0, "all available directives:");

        for (i = 0; i < cycle->modules_n; i++) {
            nt_log_stderr(0, "%s:", cycle->modules[i]->name);

            cmd = cycle->modules[i]->commands;
            if(cmd == NULL) {
                continue;
            }

            for ( /* void */ ; cmd->name.len; cmd++) {
                nt_log_stderr(0, "    %V", &cmd->name);
            }
        }
    }

    if (nt_show_modules) {
        nt_log_stderr(0, "loaded modules:");

        for (i = 0; i < cycle->modules_n; i++) {
            if (cycle->modules[i]->index < nt_modules_n) {
                nt_log_stderr(0, "    %s (static)", cycle->modules[i]->name);

            } else {
                nt_log_stderr(0, "    %s (dynamic)", cycle->modules[i]->name);
            }
        }
    }
#endif

    if (nt_test_config && !nt_quiet_mode) {
        nt_log_stderr(0, "the configuration file %s syntax is ok",
                       cycle->conf_file.data);
    }

    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->type != NT_CORE_MODULE) {
            continue;
        }

        module = cycle->modules[i]->ctx;

        if (module->init_conf) {
            if (module->init_conf(cycle,
                                  cycle->conf_ctx[cycle->modules[i]->index])
                == NT_CONF_ERROR)
            {
                environ = senv;
                nt_destroy_cycle_pools(&conf);
                return NULL;
            }
        }
    }

    if (nt_process == NT_PROCESS_SIGNALLER) {
        return cycle;
    }

    ccf = (nt_core_conf_t *) nt_get_conf(cycle->conf_ctx, nt_core_module);

    if (nt_test_config) {

        if (nt_create_pidfile(&ccf->pid, log) != NT_OK) {
            goto failed;
        }

    } else if (!nt_is_init_cycle(old_cycle)) {

        /*
         * we do not create the pid file in the first nt_init_cycle() call
         * because we need to write the demonized process pid
         */

        old_ccf = (nt_core_conf_t *) nt_get_conf(old_cycle->conf_ctx,
                                                   nt_core_module);
        if (ccf->pid.len != old_ccf->pid.len
            || nt_strcmp(ccf->pid.data, old_ccf->pid.data) != 0)
        {
            /* new pid file name */

            if (nt_create_pidfile(&ccf->pid, log) != NT_OK) {
                goto failed;
            }

            nt_delete_pidfile(old_cycle);
        }
    }


    if (nt_test_lockfile(cycle->lock_file.data, log) != NT_OK) {
        goto failed;
    }


    if (nt_create_paths(cycle, ccf->user) != NT_OK) {
        goto failed;
    }


    if (nt_log_open_default(cycle) != NT_OK) {
        goto failed;
    }

    /* open the new files */

    part = &cycle->open_files.part;
    file = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            file = part->elts;
            i = 0;
        }

        if (file[i].name.len == 0) {
            continue;
        }

        file[i].fd = nt_open_file(file[i].name.data,
                                   NT_FILE_APPEND,
                                   NT_FILE_CREATE_OR_OPEN,
                                   NT_FILE_DEFAULT_ACCESS);

        nt_log_debug3(NT_LOG_DEBUG_CORE, log, 0,
                       "log: %p %d \"%s\"",
                       &file[i], file[i].fd, file[i].name.data);

        if (file[i].fd == NT_INVALID_FILE) {
            nt_log_error(NT_LOG_EMERG, log, nt_errno,
                          nt_open_file_n " \"%s\" failed",
                          file[i].name.data);
            goto failed;
        }

#if !(NT_WIN32)
        if (fcntl(file[i].fd, F_SETFD, FD_CLOEXEC) == -1) {
            nt_log_error(NT_LOG_EMERG, log, nt_errno,
                          "fcntl(FD_CLOEXEC) \"%s\" failed",
                          file[i].name.data);
            goto failed;
        }
#endif
    }

    cycle->log = &cycle->new_log;
    pool->log = &cycle->new_log;


    /* create shared memory */

    part = &cycle->shared_memory.part;
    shm_zone = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            shm_zone = part->elts;
            i = 0;
        }

        if (shm_zone[i].shm.size == 0) {
            nt_log_error(NT_LOG_EMERG, log, 0,
                          "zero size shared memory zone \"%V\"",
                          &shm_zone[i].shm.name);
            goto failed;
        }

        shm_zone[i].shm.log = cycle->log;

        opart = &old_cycle->shared_memory.part;
        oshm_zone = opart->elts;

        for (n = 0; /* void */ ; n++) {

            if (n >= opart->nelts) {
                if (opart->next == NULL) {
                    break;
                }
                opart = opart->next;
                oshm_zone = opart->elts;
                n = 0;
            }

            if (shm_zone[i].shm.name.len != oshm_zone[n].shm.name.len) {
                continue;
            }

            if (nt_strncmp(shm_zone[i].shm.name.data,
                            oshm_zone[n].shm.name.data,
                            shm_zone[i].shm.name.len)
                != 0)
            {
                continue;
            }

            if (shm_zone[i].tag == oshm_zone[n].tag
                && shm_zone[i].shm.size == oshm_zone[n].shm.size
                && !shm_zone[i].noreuse)
            {
                shm_zone[i].shm.addr = oshm_zone[n].shm.addr;
#if (NT_WIN32)
                shm_zone[i].shm.handle = oshm_zone[n].shm.handle;
#endif

                if (shm_zone[i].init(&shm_zone[i], oshm_zone[n].data)
                    != NT_OK)
                {
                    goto failed;
                }

                goto shm_zone_found;
            }

            break;
        }

        if (nt_shm_alloc(&shm_zone[i].shm) != NT_OK) {
            goto failed;
        }

        if (nt_init_zone_pool(cycle, &shm_zone[i]) != NT_OK) {
            goto failed;
        }

        if (shm_zone[i].init(&shm_zone[i], NULL) != NT_OK) {
            goto failed;
        }

    shm_zone_found:

        continue;
    }


    /* handle the listening sockets */

    if (old_cycle->listening.nelts) {
        ls = old_cycle->listening.elts;
        for (i = 0; i < old_cycle->listening.nelts; i++) {
            ls[i].remain = 0;
        }

        nls = cycle->listening.elts;
        for (n = 0; n < cycle->listening.nelts; n++) {

            for (i = 0; i < old_cycle->listening.nelts; i++) {
                if (ls[i].ignore) {
                    continue;
                }

                if (ls[i].remain) {
                    continue;
                }

                if (ls[i].type != nls[n].type) {
                    continue;
                }

                if (nt_cmp_sockaddr(nls[n].sockaddr, nls[n].socklen,
                                     ls[i].sockaddr, ls[i].socklen, 1)
                    == NT_OK)
                {
                    nls[n].fd = ls[i].fd;
                    nls[n].previous = &ls[i];
                    ls[i].remain = 1;

                    if (ls[i].backlog != nls[n].backlog) {
                        nls[n].listen = 1;
                    }

#if (NT_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)

                    /*
                     * FreeBSD, except the most recent versions,
                     * could not remove accept filter
                     */
                    nls[n].deferred_accept = ls[i].deferred_accept;

                    if (ls[i].accept_filter && nls[n].accept_filter) {
                        if (nt_strcmp(ls[i].accept_filter,
                                       nls[n].accept_filter)
                            != 0)
                        {
                            nls[n].delete_deferred = 1;
                            nls[n].add_deferred = 1;
                        }

                    } else if (ls[i].accept_filter) {
                        nls[n].delete_deferred = 1;

                    } else if (nls[n].accept_filter) {
                        nls[n].add_deferred = 1;
                    }
#endif

#if (NT_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)

                    if (ls[i].deferred_accept && !nls[n].deferred_accept) {
                        nls[n].delete_deferred = 1;

                    } else if (ls[i].deferred_accept != nls[n].deferred_accept)
                    {
                        nls[n].add_deferred = 1;
                    }
#endif

#if (NT_HAVE_REUSEPORT)
                    if (nls[n].reuseport && !ls[i].reuseport) {
                        nls[n].add_reuseport = 1;
                    }
#endif

                    break;
                }
            }

            if (nls[n].fd == (nt_socket_t) -1) {
                nls[n].open = 1;
#if (NT_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
                if (nls[n].accept_filter) {
                    nls[n].add_deferred = 1;
                }
#endif
#if (NT_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)
                if (nls[n].deferred_accept) {
                    nls[n].add_deferred = 1;
                }
#endif
            }
        }

    } else {
        ls = cycle->listening.elts;
        for (i = 0; i < cycle->listening.nelts; i++) {
            ls[i].open = 1;
#if (NT_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
            if (ls[i].accept_filter) {
                ls[i].add_deferred = 1;
            }
#endif
#if (NT_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)
            if (ls[i].deferred_accept) {
                ls[i].add_deferred = 1;
            }
#endif
        }
    }

    if (nt_open_listening_sockets(cycle) != NT_OK) {
        goto failed;
    }

    if (!nt_test_config) {
        nt_configure_listening_sockets(cycle);
    }


    /* commit the new cycle configuration */

    if (!nt_use_stderr) {
        (void) nt_log_redirect_stderr(cycle);
    }

    pool->log = cycle->log;

    if (nt_init_modules(cycle) != NT_OK) {
        /* fatal */
        exit(1);
    }


    /* close and delete stuff that lefts from an old cycle */

    /* free the unnecessary shared memory */

    opart = &old_cycle->shared_memory.part;
    oshm_zone = opart->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= opart->nelts) {
            if (opart->next == NULL) {
                goto old_shm_zone_done;
            }
            opart = opart->next;
            oshm_zone = opart->elts;
            i = 0;
        }

        part = &cycle->shared_memory.part;
        shm_zone = part->elts;

        for (n = 0; /* void */ ; n++) {

            if (n >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }
                part = part->next;
                shm_zone = part->elts;
                n = 0;
            }

            if (oshm_zone[i].shm.name.len != shm_zone[n].shm.name.len) {
                continue;
            }

            if (nt_strncmp(oshm_zone[i].shm.name.data,
                            shm_zone[n].shm.name.data,
                            oshm_zone[i].shm.name.len)
                != 0)
            {
                continue;
            }

            if (oshm_zone[i].tag == shm_zone[n].tag
                && oshm_zone[i].shm.size == shm_zone[n].shm.size
                && !oshm_zone[i].noreuse)
            {
                goto live_shm_zone;
            }

            break;
        }

        nt_shm_free(&oshm_zone[i].shm);

    live_shm_zone:

        continue;
    }

old_shm_zone_done:


    /* close the unnecessary listening sockets */

    ls = old_cycle->listening.elts;
    for (i = 0; i < old_cycle->listening.nelts; i++) {

        if (ls[i].remain || ls[i].fd == (nt_socket_t) -1) {
            continue;
        }

        if (nt_close_socket(ls[i].fd) == -1) {
            nt_log_error(NT_LOG_EMERG, log, nt_socket_errno,
                          nt_close_socket_n " listening socket on %V failed",
                          &ls[i].addr_text);
        }

#if (NT_HAVE_UNIX_DOMAIN)

        if (ls[i].sockaddr->sa_family == AF_UNIX) {
            u_char  *name;

            name = ls[i].addr_text.data + sizeof("unix:") - 1;

            nt_log_error(NT_LOG_WARN, cycle->log, 0,
                          "deleting socket %s", name);

            if (nt_delete_file(name) == NT_FILE_ERROR) {
                nt_log_error(NT_LOG_EMERG, cycle->log, nt_socket_errno,
                              nt_delete_file_n " %s failed", name);
            }
        }

#endif
    }

#if (T_PIPES)
    /* open pipes */

    if (!nt_is_init_cycle(old_cycle)) {
        if (nt_open_pipes(cycle) == NT_ERROR) {
            nt_log_error(NT_LOG_EMERG, cycle->log, 0, "can not open pipes");
            goto failed;
        }
    }
#endif

    /* close the unnecessary open files */

    part = &old_cycle->open_files.part;
    file = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            file = part->elts;
            i = 0;
        }

        if (file[i].fd == NT_INVALID_FILE || file[i].fd == nt_stderr) {
            continue;
        }

        if (nt_close_file(file[i].fd) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_EMERG, log, nt_errno,
                          nt_close_file_n " \"%s\" failed",
                          file[i].name.data);
        }
    }

    nt_destroy_pool(conf.temp_pool);

    if (nt_process == NT_PROCESS_MASTER || nt_is_init_cycle(old_cycle)) {

        nt_destroy_pool(old_cycle->pool);
        cycle->old_cycle = NULL;

        return cycle;
    }


    if (nt_temp_pool == NULL) {
        nt_temp_pool = nt_create_pool(128, cycle->log);
        if (nt_temp_pool == NULL) {
            nt_log_error(NT_LOG_EMERG, cycle->log, 0,
                          "could not create nt_temp_pool");
            exit(1);
        }

        n = 10;

        if (nt_array_init(&nt_old_cycles, nt_temp_pool, n,
                           sizeof(nt_cycle_t *))
            != NT_OK)
        {
            exit(1);
        }

        nt_memzero(nt_old_cycles.elts, n * sizeof(nt_cycle_t *));

        nt_cleaner_event.handler = nt_clean_old_cycles;
        nt_cleaner_event.log = cycle->log;
        nt_cleaner_event.data = &dumb;
        dumb.fd = (nt_socket_t) -1;
    }

    nt_temp_pool->log = cycle->log;

    old = nt_array_push(&nt_old_cycles);
    if (old == NULL) {
        exit(1);
    }
    *old = old_cycle;

    if (!nt_cleaner_event.timer_set) {
        nt_add_timer(&nt_cleaner_event, 30000);
        nt_cleaner_event.timer_set = 1;
    }

    return cycle;


failed:

    if (!nt_is_init_cycle(old_cycle)) {
        old_ccf = (nt_core_conf_t *) nt_get_conf(old_cycle->conf_ctx,
                                                   nt_core_module);
        if (old_ccf->environment) {
            environ = old_ccf->environment;
        }
    }

    /* rollback the new cycle configuration */

    part = &cycle->open_files.part;
    file = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            file = part->elts;
            i = 0;
        }

        if (file[i].fd == NT_INVALID_FILE || file[i].fd == nt_stderr) {
            continue;
        }

        if (nt_close_file(file[i].fd) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_EMERG, log, nt_errno,
                          nt_close_file_n " \"%s\" failed",
                          file[i].name.data);
        }
    }

#if (T_PIPES)
    nt_close_pipes();
#endif

    /* free the newly created shared memory */

    part = &cycle->shared_memory.part;
    shm_zone = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            shm_zone = part->elts;
            i = 0;
        }

        if (shm_zone[i].shm.addr == NULL) {
            continue;
        }

        opart = &old_cycle->shared_memory.part;
        oshm_zone = opart->elts;

        for (n = 0; /* void */ ; n++) {

            if (n >= opart->nelts) {
                if (opart->next == NULL) {
                    break;
                }
                opart = opart->next;
                oshm_zone = opart->elts;
                n = 0;
            }

            if (shm_zone[i].shm.name.len != oshm_zone[n].shm.name.len) {
                continue;
            }

            if (nt_strncmp(shm_zone[i].shm.name.data,
                            oshm_zone[n].shm.name.data,
                            shm_zone[i].shm.name.len)
                != 0)
            {
                continue;
            }

            if (shm_zone[i].tag == oshm_zone[n].tag
                && shm_zone[i].shm.size == oshm_zone[n].shm.size
                && !shm_zone[i].noreuse)
            {
                goto old_shm_zone_found;
            }

            break;
        }

        nt_shm_free(&shm_zone[i].shm);

    old_shm_zone_found:

        continue;
    }

    if (nt_test_config) {
        nt_destroy_cycle_pools(&conf);
        return NULL;
    }

    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {
        if (ls[i].fd == (nt_socket_t) -1 || !ls[i].open) {
            continue;
        }

        if (nt_close_socket(ls[i].fd) == -1) {
            nt_log_error(NT_LOG_EMERG, log, nt_socket_errno,
                          nt_close_socket_n " %V failed",
                          &ls[i].addr_text);
        }
    }

    nt_destroy_cycle_pools(&conf);

    return NULL;
}


static void
nt_destroy_cycle_pools(nt_conf_t *conf)
{
    nt_destroy_pool(conf->temp_pool);
    nt_destroy_pool(conf->pool);
}


static nt_int_t
nt_init_zone_pool(nt_cycle_t *cycle, nt_shm_zone_t *zn)
{
    u_char           *file;
    nt_slab_pool_t  *sp;

    sp = (nt_slab_pool_t *) zn->shm.addr;

    if (zn->shm.exists) {

        if (sp == sp->addr) {
            return NT_OK;
        }

#if (NT_WIN32)

        /* remap at the required address */

        if (nt_shm_remap(&zn->shm, sp->addr) != NT_OK) {
            return NT_ERROR;
        }

        sp = (nt_slab_pool_t *) zn->shm.addr;

        if (sp == sp->addr) {
            return NT_OK;
        }

#endif

        nt_log_error(NT_LOG_EMERG, cycle->log, 0,
                      "shared zone \"%V\" has no equal addresses: %p vs %p",
                      &zn->shm.name, sp->addr, sp);
        return NT_ERROR;
    }

    sp->end = zn->shm.addr + zn->shm.size;
    sp->min_shift = 3;
    sp->addr = zn->shm.addr;

#if (NT_HAVE_ATOMIC_OPS)

    file = NULL;

#else

    file = nt_pnalloc(cycle->pool,
                       cycle->lock_file.len + zn->shm.name.len + 1);
    if (file == NULL) {
        return NT_ERROR;
    }

    (void) nt_sprintf(file, "%V%V%Z", &cycle->lock_file, &zn->shm.name);

#endif

    if (nt_shmtx_create(&sp->mutex, &sp->lock, file) != NT_OK) {
        return NT_ERROR;
    }

    nt_slab_init(sp);

    return NT_OK;
}


nt_int_t
nt_create_pidfile(nt_str_t *name, nt_log_t *log)
{
    size_t      len;
    nt_uint_t  create;
    nt_file_t  file;
    u_char      pid[NT_INT64_LEN + 2];

    if (nt_process > NT_PROCESS_MASTER) {
        return NT_OK;
    }

    nt_memzero(&file, sizeof(nt_file_t));

    file.name = *name;
    file.log = log;

    create = nt_test_config ? NT_FILE_CREATE_OR_OPEN : NT_FILE_TRUNCATE;

    file.fd = nt_open_file(file.name.data, NT_FILE_RDWR,
                            create, NT_FILE_DEFAULT_ACCESS);

    if (file.fd == NT_INVALID_FILE) {
        nt_log_error(NT_LOG_EMERG, log, nt_errno,
                      nt_open_file_n " \"%s\" failed", file.name.data);
        return NT_ERROR;
    }

    if (!nt_test_config) {
        len = nt_snprintf(pid, NT_INT64_LEN + 2, "%P%N", nt_pid) - pid;

        if (nt_write_file(&file, pid, len, 0) == NT_ERROR) {
            return NT_ERROR;
        }
    }

    if (nt_close_file(file.fd) == NT_FILE_ERROR) {
        nt_log_error(NT_LOG_ALERT, log, nt_errno,
                      nt_close_file_n " \"%s\" failed", file.name.data);
    }

    return NT_OK;
}


void
nt_delete_pidfile(nt_cycle_t *cycle)
{
    u_char           *name;
    nt_core_conf_t  *ccf;

    ccf = (nt_core_conf_t *) nt_get_conf(cycle->conf_ctx, nt_core_module);

    name = nt_new_binary ? ccf->oldpid.data : ccf->pid.data;

    if (nt_delete_file(name) == NT_FILE_ERROR) {
        nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                      nt_delete_file_n " \"%s\" failed", name);
    }
}


nt_int_t
nt_signal_process(nt_cycle_t *cycle, char *sig)
{
    ssize_t           n;
    nt_pid_t         pid;
    nt_file_t        file;
    nt_core_conf_t  *ccf;
    u_char            buf[NT_INT64_LEN + 2];

    nt_log_error(NT_LOG_NOTICE, cycle->log, 0, "signal process started");

    ccf = (nt_core_conf_t *) nt_get_conf(cycle->conf_ctx, nt_core_module);

    nt_memzero(&file, sizeof(nt_file_t));

    file.name = ccf->pid;
    file.log = cycle->log;

    file.fd = nt_open_file(file.name.data, NT_FILE_RDONLY,
                            NT_FILE_OPEN, NT_FILE_DEFAULT_ACCESS);

    if (file.fd == NT_INVALID_FILE) {
        nt_log_error(NT_LOG_ERR, cycle->log, nt_errno,
                      nt_open_file_n " \"%s\" failed", file.name.data);
        return 1;
    }

    n = nt_read_file(&file, buf, NT_INT64_LEN + 2, 0);

    if (nt_close_file(file.fd) == NT_FILE_ERROR) {
        nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                      nt_close_file_n " \"%s\" failed", file.name.data);
    }

    if (n == NT_ERROR) {
        return 1;
    }

    while (n-- && (buf[n] == CR || buf[n] == LF)) { /* void */ }

    pid = nt_atoi(buf, ++n);

    if (pid == (nt_pid_t) NT_ERROR) {
        nt_log_error(NT_LOG_ERR, cycle->log, 0,
                      "invalid PID number \"%*s\" in \"%s\"",
                      n, buf, file.name.data);
        return 1;
    }

    return nt_os_signal_process(cycle, sig, pid);

}


static nt_int_t
nt_test_lockfile(u_char *file, nt_log_t *log)
{
#if !(NT_HAVE_ATOMIC_OPS)
    nt_fd_t  fd;

    fd = nt_open_file(file, NT_FILE_RDWR, NT_FILE_CREATE_OR_OPEN,
                       NT_FILE_DEFAULT_ACCESS);

    if (fd == NT_INVALID_FILE) {
        nt_log_error(NT_LOG_EMERG, log, nt_errno,
                      nt_open_file_n " \"%s\" failed", file);
        return NT_ERROR;
    }

    if (nt_close_file(fd) == NT_FILE_ERROR) {
        nt_log_error(NT_LOG_ALERT, log, nt_errno,
                      nt_close_file_n " \"%s\" failed", file);
    }

    if (nt_delete_file(file) == NT_FILE_ERROR) {
        nt_log_error(NT_LOG_ALERT, log, nt_errno,
                      nt_delete_file_n " \"%s\" failed", file);
    }

#endif

    return NT_OK;
}


void
nt_reopen_files(nt_cycle_t *cycle, nt_uid_t user)
{
    nt_fd_t          fd;
    nt_uint_t        i;
    nt_list_part_t  *part;
    nt_open_file_t  *file;

    part = &cycle->open_files.part;
    file = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            file = part->elts;
            i = 0;
        }

        if (file[i].name.len == 0) {
            continue;
        }

        if (file[i].flush) {
            file[i].flush(&file[i], cycle->log);
        }

        fd = nt_open_file(file[i].name.data, NT_FILE_APPEND,
                           NT_FILE_CREATE_OR_OPEN, NT_FILE_DEFAULT_ACCESS);

        nt_log_debug3(NT_LOG_DEBUG_EVENT, cycle->log, 0,
                       "reopen file \"%s\", old:%d new:%d",
                       file[i].name.data, file[i].fd, fd);

        if (fd == NT_INVALID_FILE) {
            nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                          nt_open_file_n " \"%s\" failed", file[i].name.data);
            continue;
        }

#if !(NT_WIN32)
        if (user != (nt_uid_t) NT_CONF_UNSET_UINT) {
            nt_file_info_t  fi;

            if (nt_file_info(file[i].name.data, &fi) == NT_FILE_ERROR) {
                nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                              nt_file_info_n " \"%s\" failed",
                              file[i].name.data);

                if (nt_close_file(fd) == NT_FILE_ERROR) {
                    nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                                  nt_close_file_n " \"%s\" failed",
                                  file[i].name.data);
                }

                continue;
            }

            if (fi.st_uid != user) {
                if (chown((const char *) file[i].name.data, user, -1) == -1) {
                    nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                                  "chown(\"%s\", %d) failed",
                                  file[i].name.data, user);

                    if (nt_close_file(fd) == NT_FILE_ERROR) {
                        nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                                      nt_close_file_n " \"%s\" failed",
                                      file[i].name.data);
                    }

                    continue;
                }
            }

            if ((fi.st_mode & (S_IRUSR|S_IWUSR)) != (S_IRUSR|S_IWUSR)) {

                fi.st_mode |= (S_IRUSR|S_IWUSR);

                if (chmod((const char *) file[i].name.data, fi.st_mode) == -1) {
                    nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                                  "chmod() \"%s\" failed", file[i].name.data);

                    if (nt_close_file(fd) == NT_FILE_ERROR) {
                        nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                                      nt_close_file_n " \"%s\" failed",
                                      file[i].name.data);
                    }

                    continue;
                }
            }
        }

        if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
            nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                          "fcntl(FD_CLOEXEC) \"%s\" failed",
                          file[i].name.data);

            if (nt_close_file(fd) == NT_FILE_ERROR) {
                nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                              nt_close_file_n " \"%s\" failed",
                              file[i].name.data);
            }

            continue;
        }
#endif

        if (nt_close_file(file[i].fd) == NT_FILE_ERROR) {
            nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                          nt_close_file_n " \"%s\" failed",
                          file[i].name.data);
        }

        file[i].fd = fd;
    }

    (void) nt_log_redirect_stderr(cycle);
}


nt_shm_zone_t *
nt_shared_memory_add(nt_conf_t *cf, nt_str_t *name, size_t size, void *tag)
{
    nt_uint_t        i;
    nt_shm_zone_t   *shm_zone;
    nt_list_part_t  *part;

    part = &cf->cycle->shared_memory.part;
    shm_zone = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            shm_zone = part->elts;
            i = 0;
        }

        if (name->len != shm_zone[i].shm.name.len) {
            continue;
        }

        if (nt_strncmp(name->data, shm_zone[i].shm.name.data, name->len)
            != 0)
        {
            continue;
        }

        if (tag != shm_zone[i].tag) {
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                            "the shared memory zone \"%V\" is "
                            "already declared for a different use",
                            &shm_zone[i].shm.name);
            return NULL;
        }

        if (shm_zone[i].shm.size == 0) {
            shm_zone[i].shm.size = size;
        }

        if (size && size != shm_zone[i].shm.size) {
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                            "the size %uz of shared memory zone \"%V\" "
                            "conflicts with already declared size %uz",
                            size, &shm_zone[i].shm.name, shm_zone[i].shm.size);
            return NULL;
        }

        return &shm_zone[i];
    }

    shm_zone = nt_list_push(&cf->cycle->shared_memory);

    if (shm_zone == NULL) {
        return NULL;
    }

    shm_zone->data = NULL;
    shm_zone->shm.log = cf->cycle->log;
    shm_zone->shm.addr = NULL;
    shm_zone->shm.size = size;
    shm_zone->shm.name = *name;
    shm_zone->shm.exists = 0;
    shm_zone->init = NULL;
    shm_zone->tag = tag;
    shm_zone->noreuse = 0;

    return shm_zone;
}


static void
nt_clean_old_cycles(nt_event_t *ev)
{
    nt_uint_t     i, n, found, live;
    nt_log_t     *log;
    nt_cycle_t  **cycle;

    log = nt_cycle->log;
    nt_temp_pool->log = log;

    nt_log_debug0(NT_LOG_DEBUG_CORE, log, 0, "clean old cycles");

    live = 0;

    cycle = nt_old_cycles.elts;
    for (i = 0; i < nt_old_cycles.nelts; i++) {

        if (cycle[i] == NULL) {
            continue;
        }

        found = 0;

        for (n = 0; n < cycle[i]->connection_n; n++) {
            if (cycle[i]->connections[n].fd != (nt_socket_t) -1) {
                found = 1;

                nt_log_debug1(NT_LOG_DEBUG_CORE, log, 0, "live fd:%ui", n);

                break;
            }
        }

        if (found) {
            live = 1;
            continue;
        }

        nt_log_debug1(NT_LOG_DEBUG_CORE, log, 0, "clean old cycle: %ui", i);

        nt_destroy_pool(cycle[i]->pool);
        cycle[i] = NULL;
    }

    nt_log_debug1(NT_LOG_DEBUG_CORE, log, 0, "old cycles status: %ui", live);

    if (live) {
        nt_add_timer(ev, 30000);

    } else {
        nt_destroy_pool(nt_temp_pool);
        nt_temp_pool = NULL;
        nt_old_cycles.nelts = 0;
    }
}


void
nt_set_shutdown_timer(nt_cycle_t *cycle)
{
    nt_core_conf_t  *ccf;

    ccf = (nt_core_conf_t *) nt_get_conf(cycle->conf_ctx, nt_core_module);

    if (ccf->shutdown_timeout) {
        nt_shutdown_event.handler = nt_shutdown_timer_handler;
        nt_shutdown_event.data = cycle;
        nt_shutdown_event.log = cycle->log;
        nt_shutdown_event.cancelable = 1;

        nt_add_timer(&nt_shutdown_event, ccf->shutdown_timeout);
    }
}


static void
nt_shutdown_timer_handler(nt_event_t *ev)
{
    nt_uint_t         i;
    nt_cycle_t       *cycle;
    nt_connection_t  *c;

    cycle = ev->data;

    c = cycle->connections;

    for (i = 0; i < cycle->connection_n; i++) {

        if (c[i].fd == (nt_socket_t) -1
            || c[i].read == NULL
            || c[i].read->accept
            || c[i].read->channel
            || c[i].read->resolver)
        {
            continue;
        }

        nt_log_debug1(NT_LOG_DEBUG_CORE, ev->log, 0,
                       "*%uA shutdown timeout", c[i].number);

        c[i].close = 1;
        c[i].error = 1;

        c[i].read->handler(c[i].read);
    }
}
