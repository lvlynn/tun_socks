

#include <nt_core.h>
#include <nt_channel.h>


#if !(NT_WIN32)

static nt_uint_t       nt_pipe_generation;
static nt_uint_t       nt_last_pipe;
static nt_open_pipe_t  nt_pipes[NT_MAX_PROCESSES];

#define MAX_BACKUP_NUM          128
#define NT_PIPE_DIR_ACCESS     S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH
#define NT_PIPE_FILE_ACCESS    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH

typedef struct {
    nt_int_t       time_now;
    nt_int_t       last_open_time;
    nt_int_t       log_size;
    nt_int_t       last_suit_time;

    char           *logname;
    char           *backup[MAX_BACKUP_NUM];

    nt_int_t       backup_num;
    nt_int_t       log_max_size;
    nt_int_t       interval;
    char           *suitpath;
    nt_int_t       adjust_time;
    nt_int_t       adjust_time_raw;
} nt_pipe_rollback_conf_t;

static void nt_signal_pipe_broken(nt_log_t *log, nt_pid_t pid);
static nt_int_t nt_open_pipe(nt_cycle_t *cycle, nt_open_pipe_t *op);
static void nt_close_pipe(nt_open_pipe_t *pipe);

static void nt_pipe_log(nt_cycle_t *cycle, nt_open_pipe_t *op);
void nt_pipe_get_last_rollback_time(nt_pipe_rollback_conf_t *rbcf);
static void nt_pipe_do_rollback(nt_cycle_t *cycle, nt_pipe_rollback_conf_t *rbcf);
static nt_int_t nt_pipe_rollback_parse_args(nt_cycle_t *cycle,
    nt_open_pipe_t *op, nt_pipe_rollback_conf_t *rbcf);

nt_str_t nt_log_error_backup = nt_string(NT_ERROR_LOG_PATH);
/* nt_str_t nt_log_access_backup = nt_string(NT_HTTP_LOG_PATH); */
nt_str_t nt_log_access_backup = nt_string(NT_ERROR_LOG_PATH);

nt_str_t nt_pipe_dev_null_file = nt_string("/dev/null");

nt_open_pipe_t *
nt_conf_open_pipe(nt_cycle_t *cycle, nt_str_t *cmd, const char *type)
{
    u_char           *cp, *ct, *dup, **argi, **c1, **c2;
    nt_int_t         same, ti, use;
    nt_uint_t        i, j, numargs = 0;
    nt_array_t      *argv_out;

    dup = nt_pnalloc(cycle->pool, cmd->len + 1);
    if (dup == NULL) {
        return NULL;
    }

    (void) nt_cpystrn(dup, cmd->data, cmd->len + 1);

    for (cp = cmd->data; *cp == ' ' || *cp == '\t'; cp++);
    ct = cp;

    if (nt_strcmp(type, "r") == 0) {
        ti = NT_PIPE_READ;
    } else if (nt_strcmp(type, "w") == 0) {
        ti = NT_PIPE_WRITE;
    } else {
        return NULL;
    }

    numargs = 1;
    while (*ct != '\0') {
        for ( /* void */ ; *ct != '\0'; ct++) {
            if (*ct == ' ' || *ct == '\t') {
                break;
            }
        }

        if (*ct != '\0') {
            ct++;
        }

        numargs++;

        for ( /* void */ ; *ct == ' ' || *ct == '\t'; ct++);
    }

    argv_out = nt_array_create(cycle->pool, numargs, sizeof(u_char *));
    if (argv_out == NULL) {
        return NULL;
    }

    for (i = 0; i < (numargs - 1); i++) {
        for ( /* void */ ; *cp == ' ' || *cp == '\t'; cp++);

        for (ct = cp; *cp != '\0'; cp++) {
            if (*cp == ' ' || *cp == '\t') {
                break;
            }
        }

        *cp = '\0';
        argi = (u_char **) nt_array_push(argv_out);
        *argi = ct;
        cp++;
    }

    argi = (u_char **) nt_array_push(argv_out);
    if (argi == NULL) {
        return NULL;
    }

    *argi = NULL;

    for (i = 0, use = -1; i < nt_last_pipe; i++) {

        if (!nt_pipes[i].configured) {
            if (use == -1) {
                use = i;
            }
            continue;
        }

        if (nt_pipes[i].generation != nt_pipe_generation) {
            continue;
        }

        if (argv_out->nelts != nt_pipes[i].argv->nelts) {
            continue;
        }

        if (ti != nt_pipes[i].type) {
            continue;
        }

        same = 1;
        c1 = argv_out->elts;
        c2 = nt_pipes[i].argv->elts;
        for (j = 0; j < argv_out->nelts - 1; j++) {
            if (nt_strcmp(c1[j], c2[j]) != 0) {
                same = 0;
                break;
            }
        }
        if (same) {
            return &nt_pipes[i];
        }
    }

    if (use == -1) {
        if (nt_last_pipe < NT_MAX_PROCESSES) {
            use = nt_last_pipe++;
        } else {
            return NULL;
        }
    }

    nt_memzero(&nt_pipes[use], sizeof(nt_open_pipe_t));

    nt_pipes[use].open_fd = nt_list_push(&cycle->open_files);
    if (nt_pipes[use].open_fd == NULL) {
        return NULL;
    }

    nt_memzero(nt_pipes[use].open_fd, sizeof(nt_open_file_t));
    nt_pipes[use].open_fd->fd = NT_INVALID_FILE;

    nt_pipes[use].pid = -1;
    nt_pipes[use].cmd = dup;
    nt_pipes[use].argv = argv_out;
    nt_pipes[use].type = ti;
    nt_pipes[use].generation = nt_pipe_generation;
    nt_pipes[use].configured = 1;

    return &nt_pipes[use];
}


static void
nt_close_pipe(nt_open_pipe_t *pipe)
{
    /*
     * No waitpid at this place, because it is called at
     * nt_process_get_status first.
     */

    if (pipe->pid != -1) {
        kill(pipe->pid, SIGTERM);
    }

    pipe->configured = 0;
}


void
nt_increase_pipe_generation(void)
{
    nt_pipe_generation++;
}


nt_int_t
nt_open_pipes(nt_cycle_t *cycle)
{
    nt_int_t          stat;
    nt_uint_t         i;
    nt_core_conf_t   *ccf;

    ccf = (nt_core_conf_t *) nt_get_conf(cycle->conf_ctx, nt_core_module);

    for (i = 0; i < nt_last_pipe; i++) {

        if (!nt_pipes[i].configured) {
            continue;
        }

        if (nt_pipes[i].generation != nt_pipe_generation) {
            continue;
        }

        nt_pipes[i].backup = nt_pipes[i].open_fd->name;
        nt_pipes[i].user = ccf->user;

        stat = nt_open_pipe(cycle, &nt_pipes[i]);

        nt_log_debug4(NT_LOG_DEBUG_CORE, cycle->log, 0,
                       "pipe: %ui(%d, %d) \"%s\"",
                       i, nt_pipes[i].pfd[0],
                       nt_pipes[i].pfd[1], nt_pipes[i].cmd);

        if (stat == NT_ERROR) {
            nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                          "open pipe \"%s\" failed",
                          nt_pipes[i].cmd);
            return NT_ERROR;
        }

        if (fcntl(nt_pipes[i].open_fd->fd, F_SETFD, FD_CLOEXEC) == -1) {
            nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                          "fcntl(FD_CLOEXEC) \"%s\" failed",
                          nt_pipes[i].cmd);
            return NT_ERROR;
        }

        if (nt_nonblocking(nt_pipes[i].open_fd->fd) == -1) {
            nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                          "nonblock \"%s\" failed",
                          nt_pipes[i].cmd);
            return NT_ERROR;
        }

        nt_pipes[i].open_fd->name.len = 0;
        nt_pipes[i].open_fd->name.data = NULL;
    }

    return NT_OK;
}


void
nt_close_old_pipes(void)
{
    nt_uint_t i, last;

    for (i = 0, last = -1; i < nt_last_pipe; i++) {

        if (!nt_pipes[i].configured) {
            continue;
        }

        if (nt_pipes[i].generation < nt_pipe_generation) {
            nt_close_pipe(&nt_pipes[i]);
        } else {
            last = i;
        }
    }

    nt_last_pipe = last + 1;
}


void
nt_close_pipes(void)
{
    nt_uint_t i, last;

    for (i = 0, last = -1; i < nt_last_pipe; i++) {

        if (!nt_pipes[i].configured) {
            continue;
        }

        if (nt_pipes[i].generation == nt_pipe_generation) {
            nt_close_pipe(&nt_pipes[i]);
        } else {
            last = i;
        }
    }

    nt_last_pipe = last + 1;
}


void
nt_pipe_broken_action(nt_log_t *log, nt_pid_t pid, nt_int_t master)
{
    nt_uint_t i;
#ifdef T_PIPES_OUTPUT_ON_BROKEN
    nt_uint_t is_stderr = 0;
#endif

    for (i = 0; i < nt_last_pipe; i++) {

        if (!nt_pipes[i].configured) {
            continue;
        }

        if (nt_pipes[i].generation != nt_pipe_generation) {
            continue;
        }

        if (nt_pipes[i].pid == pid) {

            if (close(nt_pipes[i].open_fd->fd) == NT_FILE_ERROR) {
                nt_log_error(NT_LOG_EMERG, log, nt_errno,
                              "close \"%s\" failed",
                              nt_pipes[i].cmd);
            }

#ifdef T_PIPES_OUTPUT_ON_BROKEN
            if (nt_pipes[i].open_fd == nt_cycle->log->file) {
                is_stderr = 1;
            }
#endif

            nt_pipes[i].open_fd->fd = NT_INVALID_FILE;

#ifdef T_PIPES_OUTPUT_ON_BROKEN
            if (nt_pipes[i].backup.len > 0 && nt_pipes[i].backup.data != NULL) {
#else
                //just write to /dev/null
                nt_pipes[i].backup.data = nt_pipe_dev_null_file.data;
                nt_pipes[i].backup.len = nt_pipe_dev_null_file.len;
#endif
                nt_pipes[i].open_fd->name.len = nt_pipes[i].backup.len;
                nt_pipes[i].open_fd->name.data = nt_pipes[i].backup.data;

                nt_pipes[i].open_fd->fd = nt_open_file(nt_pipes[i].backup.data,
                                                         NT_FILE_APPEND,
                                                         NT_FILE_CREATE_OR_OPEN,
                                                         NT_FILE_DEFAULT_ACCESS);

                if (nt_pipes[i].open_fd->fd == NT_INVALID_FILE) {
                    nt_log_error(NT_LOG_EMERG, log, nt_errno,
                                  nt_open_file_n " \"%s\" failed",
                                  nt_pipes[i].backup.data);
                }

                if (fcntl(nt_pipes[i].open_fd->fd, F_SETFD, FD_CLOEXEC) == -1) {
                    nt_log_error(NT_LOG_EMERG, log, nt_errno,
                                  "fcntl(FD_CLOEXEC) \"%s\" failed",
                                  nt_pipes[i].backup.data);
                }
#ifdef T_PIPES_OUTPUT_ON_BROKEN
            }

            if (is_stderr) {
                nt_set_stderr(nt_cycle->log->file->fd);
            }
#endif

            if (master) {
#ifdef T_PIPES_OUTPUT_ON_BROKEN
                if (nt_pipes[i].backup.len > 0 && nt_pipes[i].backup.data != NULL) {
                    if (chown((const char *) nt_pipes[i].backup.data,
                              nt_pipes[i].user, -1)
                        == -1)
                    {
                        nt_log_error(NT_LOG_EMERG, log, nt_errno,
                                      "chown() \"%s\" failed",
                                      nt_pipes[i].backup.data);
                    }
                }
#endif

                nt_signal_pipe_broken(log, pid);
            }
        }
    }
}


static void
nt_signal_pipe_broken(nt_log_t *log, nt_pid_t pid)
{
    nt_int_t      i;
    nt_channel_t  ch;

    ch.fd = -1;
    ch.pid = pid;
    ch.command = NT_CMD_PIPE_BROKEN;

    for (i = 0; i < nt_last_process; i++) {

        if (nt_processes[i].detached || nt_processes[i].pid == -1) {
            continue;
        }

        nt_write_channel(nt_processes[i].channel[0],
                          &ch, sizeof(nt_channel_t), log);
    }
}


static nt_int_t
nt_open_pipe(nt_cycle_t *cycle, nt_open_pipe_t *op)
{
    int               fd;
    u_char          **argv;
    nt_pid_t         pid;
    sigset_t          set;
#ifdef T_PIPE_USE_USER
    nt_core_conf_t  *ccf;

    ccf = (nt_core_conf_t *) nt_get_conf(cycle->conf_ctx, nt_core_module);
#endif

    if (pipe(op->pfd) < 0) {
        return NT_ERROR;
    }

    argv = op->argv->elts;

    if ((pid = fork()) < 0) {
        goto err;
    } else if (pid > 0) {
        op->pid = pid;

        if (op->open_fd->fd != NT_INVALID_FILE) {
            if (close(op->open_fd->fd) == NT_FILE_ERROR) {
                nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                              "close \"%s\" failed",
                              op->open_fd->name.data);
            }
        }

        if (op->type == NT_PIPE_WRITE) {
            op->open_fd->fd = op->pfd[1];
            close(op->pfd[0]);
        } else {
            op->open_fd->fd = op->pfd[0];
            close(op->pfd[1]);
        }
    } else {

       /*
        * Set correct process type since closing listening Unix domain socket
        * in a master process also removes the Unix domain socket file.
        */
        nt_process = NT_PROCESS_PIPE;
        nt_close_listening_sockets(cycle);

        if (op->type == 1) {
            close(op->pfd[1]);
            if (op->pfd[0] != STDIN_FILENO) {
                dup2(op->pfd[0], STDIN_FILENO);
                close(op->pfd[0]);
            }
        } else {
            close(op->pfd[0]);
            if (op->pfd[1] != STDOUT_FILENO) {
                dup2(op->pfd[1], STDOUT_FILENO);
                close(op->pfd[1]);
            }
        }
#ifdef T_PIPE_USE_USER
        if (geteuid() == 0) {
            if (setgid(ccf->group) == -1) {
                nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                              "setgid(%d) failed", ccf->group);
                exit(2);
            }

            if (initgroups(ccf->username, ccf->group) == -1) {
                nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                              "initgroups(%s, %d) failed",
                              ccf->username, ccf->group);
            }

            if (setuid(ccf->user) == -1) {
                nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                              "setuid(%d) failed", ccf->user);
                exit(2);
            }
        }
#endif

        /*
         * redirect stderr to /dev/null, because stderr will be connected with
         * fd used by the last pipe when error log is configured using pipe,
         * that will cause it no close
         */

        fd = nt_open_file("/dev/null", NT_FILE_WRONLY, NT_FILE_OPEN, 0);
        if (fd == -1) {
            nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                          "open(\"/dev/null\") failed");
            exit(2);
        }

        if (dup2(fd, STDERR_FILENO) == -1) {
            nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                          "dup2(STDERR) failed");
            exit(2);
        }

        if (fd > STDERR_FILENO && close(fd) == -1) {
            nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                          "close() failed");
            exit(2);
        }

        sigemptyset(&set);

        if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
            nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                          "sigprocmask() failed");
            exit(2);
        }

        if (nt_strncmp(argv[0], "rollback", sizeof("rollback") - 1) == 0) {
            nt_pipe_log(cycle, op);
            exit(0);

        } else {
            execv((const char *) argv[0], (char *const *) op->argv->elts);
            exit(0);
        }
    }

    return NT_OK;

err:

    close(op->pfd[0]);
    close(op->pfd[1]);

    return NT_ERROR;
}


static void
nt_pipe_create_subdirs(char *filename, nt_cycle_t *cycle)
{
    nt_file_info_t stat_buf;
    char            dirname[1024];
    char           *p;

    for (p = filename; (p = strchr(p, '/')); p++)
    {
        if (p == filename) {
            continue;       // Don't bother with the root directory
        }

        nt_memcpy(dirname, filename, p - filename);
        dirname[p-filename] = '\0';

        if (nt_file_info(dirname, &stat_buf) < 0) {
            if (errno != ENOENT) {
                nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                              "stat [%s] failed", dirname);
                exit(2);

            } else {
                if ((nt_create_dir(dirname, NT_PIPE_DIR_ACCESS) < 0) && (errno != EEXIST)) {
                    nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                                  "mkdir [%s] failed", dirname);
                    exit(2);
                }
            }
        }
    }
}

static void
nt_pipe_create_suitpath(nt_cycle_t *cycle, nt_pipe_rollback_conf_t *rbcf)
{
    char        realpath[256];
    struct      tm *tm_now;
    int         result;
    time_t      t = nt_time();

    tm_now = localtime(&t);
    if (tm_now == NULL) {
        nt_log_error(NT_LOG_EMERG, cycle->log, 0,
                      "get now time failed");
        return;
    }
    if (0 >= strftime(realpath, sizeof(realpath), rbcf->suitpath, tm_now)) {
        nt_log_error(NT_LOG_EMERG, cycle->log, 0,
                      "parse suitpath with time now failed");
        return;
    }

    nt_pipe_create_subdirs(realpath, cycle);

    result = unlink(realpath);
    if (0 == result) {
        nt_log_error(NT_LOG_INFO, cycle->log, 0,
                      "unlink [%s] success", realpath);
    } else {
        nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                      "unlink [%s] failed", realpath);
    }

    result = symlink(rbcf->logname, realpath);
    if (0 == result) {
        nt_log_error(NT_LOG_INFO, cycle->log, 0,
                      "symlink [%s] success", realpath);
    } else {
        nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                      "symlink [%s] failed", realpath);
    }

    rbcf->last_suit_time = rbcf->time_now;
}

static time_t
nt_pipe_get_now_sec()
{
    nt_time_update();

    return nt_time() + nt_cached_time->gmtoff * 60;
}

static void
nt_pipe_log(nt_cycle_t *cycle, nt_open_pipe_t *op)
{
    nt_int_t                   n_bytes_read;
    u_char                     *read_buf;
    size_t                      read_buf_len = 65536;
    nt_fd_t                    log_fd = NT_INVALID_FILE;
#ifdef T_PIPE_NO_DISPLAY
    size_t                      title_len;
#endif
    nt_pipe_rollback_conf_t    rbcf;
    nt_file_info_t             sb;
    size_t                      one_day = 24 * 60 * 60;

    nt_pid = nt_getpid();

    rbcf.last_open_time = 0;
    rbcf.last_suit_time = 0;
    rbcf.log_size = 0;

    if (nt_pipe_rollback_parse_args(cycle, op, &rbcf) != NT_OK) {
        nt_log_error(NT_LOG_EMERG, cycle->log, 0, "log rollback parse error");
        return;
    }

    read_buf = nt_pcalloc(cycle->pool, read_buf_len);
    if (read_buf == NULL) {
        return;
    }

    //set title
    nt_setproctitle((char *) op->cmd);
#ifdef T_PIPE_NO_DISPLAY
    title_len = nt_strlen(nt_os_argv[0]);
#if (NT_SOLARIS)
#else
    nt_memset((u_char *) nt_os_argv[0], NT_SETPROCTITLE_PAD, title_len);
    nt_cpystrn((u_char *) nt_os_argv[0], op->cmd, title_len);
#endif
#endif

    for (;;)
    {
        if (nt_terminate == 1) {
            return;
        }

        n_bytes_read = nt_read_fd(0, read_buf, read_buf_len);
        if (n_bytes_read == 0) {
            return;
        }
        if (errno == EINTR) {
            continue;

        } else if (n_bytes_read < 0) {
            return;
        }

        rbcf.time_now = nt_pipe_get_now_sec();

        if (NULL != rbcf.suitpath) {
            if (rbcf.time_now / one_day >
                    rbcf.last_suit_time / one_day) {
                nt_pipe_create_suitpath(cycle, &rbcf);
            }
        }

        if (log_fd >= 0) {
            if (rbcf.interval > 0) {
                if (((rbcf.time_now - rbcf.adjust_time) / rbcf.interval) >
                        (rbcf.last_open_time / rbcf.interval)) {
                    //need check rollback
                    nt_close_file(log_fd);
                    log_fd = NT_INVALID_FILE;
                    nt_log_error(NT_LOG_INFO, cycle->log, 0,
                                  "need check rollback time [%s]", rbcf.logname);
                    nt_pipe_do_rollback(cycle, &rbcf);
                }
            }
        }

        if (log_fd >= 0 && rbcf.log_max_size > 0 &&
                           rbcf.log_size >= rbcf.log_max_size) {
            nt_close_file(log_fd);
            log_fd = NT_INVALID_FILE;
            nt_log_error(NT_LOG_INFO, cycle->log, 0,
                          "need check rollback size [%s] [%d]",
                          rbcf.logname, rbcf.log_size);
            nt_pipe_do_rollback(cycle, &rbcf);
        }

        /* If there is no log file open then open a new one.
         *   */
        if (log_fd < 0) {
            nt_pipe_create_subdirs(rbcf.logname, cycle);
            if (rbcf.interval > 0 && rbcf.last_open_time == 0 
                    && ((rbcf.time_now - rbcf.adjust_time_raw) / rbcf.interval < rbcf.time_now / rbcf.interval)) { /*just check
                when time is after interval and befor adjust_time_raw, fix when no backup file, do rollback
                */
                //need check last rollback time, because other process may do rollback after when it adjust time more than this process
                nt_pipe_get_last_rollback_time(&rbcf);
            } else {
                rbcf.last_open_time = nt_pipe_get_now_sec();
            }

            log_fd = nt_open_file(rbcf.logname, NT_FILE_APPEND, NT_FILE_CREATE_OR_OPEN,
                          NT_PIPE_FILE_ACCESS);
            if (log_fd < 0) {
                nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                              "open [%s] failed", rbcf.logname);
                return;
            }

            if (0 == nt_fd_info(log_fd, &sb)) {
                rbcf.log_size = sb.st_size;
            }
        }

        if (nt_write_fd(log_fd, read_buf, n_bytes_read) != n_bytes_read) {
            if (errno != EINTR) {
                nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                              "write to [%s] failed", rbcf.logname);
                return;
            }
        }
        rbcf.log_size += n_bytes_read;
    }

}

nt_int_t
nt_pipe_rollback_parse_args(nt_cycle_t *cycle, nt_open_pipe_t *op,
    nt_pipe_rollback_conf_t *rbcf)
{
    u_char         **argv;
    nt_uint_t       i;
    nt_int_t        j;
    size_t           len;
    nt_str_t        filename;
    nt_str_t        value;
    uint32_t         hash;

    if (op->argv->nelts < 3) {
        //no logname
        return NT_ERROR;
    }

    //parse args
    argv = op->argv->elts;

    //set default param
    filename.data = (u_char *) argv[1];
    filename.len = nt_strlen(filename.data);
    if (nt_conf_full_name(cycle, &filename, 0) != NT_OK) {
        nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                      "get fullname failed");
        return NT_ERROR;
    }
    rbcf->logname = (char *) filename.data;
    rbcf->backup_num = 1;
    rbcf->log_max_size = -1;
    rbcf->interval = -1;
    rbcf->suitpath = NULL;
    rbcf->adjust_time = 60;
    rbcf->adjust_time_raw = 60;
    memset(rbcf->backup, 0, sizeof(rbcf->backup));

    for (i = 2; i < op->argv->nelts; i++) {
        if (argv[i] == NULL) {
            break;
        }
        if (nt_strncmp((u_char *) "interval=", argv[i], 9) == 0) {
            value.data = argv[i] + 9;
            value.len = nt_strlen((char *) argv[i]) - 9;
#ifdef T_PIPE_OLD_CONF
            //just compatibility
            rbcf->interval = nt_atoi(value.data, value.len);
            if (rbcf->interval > 0) {
                rbcf->interval = rbcf->interval * 60;   //convert minute to second
                continue;
            }
#endif
            rbcf->interval = nt_parse_time(&value, 1);
            if (rbcf->interval <= 0) {
                rbcf->interval = -1;
            }

        } else if (nt_strncmp((u_char *) "baknum=", argv[i], 7) == 0) {
            rbcf->backup_num = nt_atoi(argv[i] + 7,
                                        nt_strlen((char *) argv[i]) - 7);
            if (rbcf->backup_num <= 0) {
                rbcf->backup_num = 1;

            } else if (MAX_BACKUP_NUM < (size_t)rbcf->backup_num) {
                rbcf->backup_num = MAX_BACKUP_NUM;
            }

        } else if (nt_strncmp((u_char *) "maxsize=", argv[i], 8) == 0) {
            value.data = argv[i] + 8;
            value.len = nt_strlen((char *) argv[i]) - 8;
#ifdef T_PIPE_OLD_CONF
            //just compatibility
            rbcf->log_max_size = nt_atoi(value.data, value.len);
            if (rbcf->log_max_size > 0) {
                rbcf->log_max_size = rbcf->log_max_size * 1024 * 1024; //M
                continue;
            }
#endif
            rbcf->log_max_size = nt_parse_offset(&value);
            if (rbcf->log_max_size <= 0) {
                rbcf->log_max_size = -1;
            } 
        } else if (nt_strncmp((u_char *) "suitpath=", argv[i], 9) == 0) {
            filename.data = (u_char*)(argv[i] + 9);
            filename.len = nt_strlen(filename.data);
            if (nt_conf_full_name(cycle, &filename, 0) != NT_OK) {
                nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                              "get fullname failed");
                return NT_ERROR;
            }
            rbcf->suitpath = (char*)filename.data;

            rbcf->time_now = nt_pipe_get_now_sec();
            nt_pipe_create_suitpath(cycle, rbcf);
        } else if (nt_strncmp((u_char *) "adjust=", argv[i], 7) == 0) {
            value.data =argv[i] + 7;
            value.len = nt_strlen((char *) argv[i]) - 7;
            rbcf->adjust_time_raw = nt_parse_time(&value, 1);
            if (rbcf->adjust_time_raw < 1) {
                rbcf->adjust_time_raw = 1;
            }
        }
    } 

    len = nt_strlen(rbcf->logname) + 5; //max is ".128"
    for (j = 0; j < rbcf->backup_num; j++) {
        rbcf->backup[j] = nt_pcalloc(cycle->pool, len);
        if (rbcf->backup[j] == NULL) {
            return NT_ERROR;
        }
        nt_snprintf((u_char *) rbcf->backup[j], len, "%s.%i%Z", rbcf->logname, j + 1);
    }

    //use same seed for same log file, when reload may have same adjust time
    hash = nt_crc32_short((u_char*)rbcf->logname, nt_strlen(rbcf->logname));
    srand(hash);
    rbcf->adjust_time = rand() % rbcf->adjust_time_raw;

    nt_log_error(NT_LOG_INFO, cycle->log, 0,
                  "log rollback param: num [%i], interval %i(S), size %i(B), adjust %i/%i(S)", 
                  rbcf->backup_num, rbcf->interval, rbcf->log_max_size, rbcf->adjust_time, rbcf->adjust_time_raw);

    return NT_OK;
}

void nt_pipe_get_last_rollback_time(nt_pipe_rollback_conf_t *rbcf)
{
    int             fd;
    struct flock    lock;
    int             ret;

    struct stat     sb;

    fd = nt_open_file(rbcf->logname, NT_FILE_RDWR, NT_FILE_OPEN, 0);
    if (fd < 0) {
        //open lock file failed just use now
        rbcf->last_open_time = rbcf->time_now;
        return;
    }

    lock.l_type     = F_WRLCK;
    lock.l_whence   = SEEK_SET;
    lock.l_start    = 0;
    lock.l_len      = 0;

    ret = fcntl(fd, F_SETLKW, &lock);
    if (ret < 0) {
        close(fd);
        //lock failed just use now
        rbcf->last_open_time = rbcf->time_now;
        return;
    }

    //check time
    if (rbcf->interval > 0) {
        if (nt_file_info(rbcf->backup[0], &sb) == -1) {
            //no backup file ,so need rollback just set 1
            rbcf->last_open_time = 1;
        } else if (sb.st_ctime / rbcf->interval < nt_time() / rbcf->interval) {
            //need rollback just set 1
            rbcf->last_open_time = 1;
        } else {
            //just no need rollback
            rbcf->last_open_time = rbcf->time_now;
        }
    } else {
        rbcf->last_open_time = rbcf->time_now;
    }

    close(fd);
}

void
nt_pipe_do_rollback(nt_cycle_t *cycle, nt_pipe_rollback_conf_t *rbcf)
{
    int             fd;
    struct flock    lock;
    int             ret;
    nt_int_t       i;
    nt_file_info_t sb;
    nt_int_t       need_do = 0;

    fd = nt_open_file(rbcf->logname, NT_FILE_RDWR, NT_FILE_OPEN, 0);
    if (fd < 0) {
        //open lock file failed just no need rollback
        return;
    }

    lock.l_type     = F_WRLCK;
    lock.l_whence   = SEEK_SET;
    lock.l_start    = 0;
    lock.l_len      = 0;

    ret = fcntl(fd, F_SETLKW, &lock);
    if (ret < 0) {
        nt_close_file(fd);
        //lock failed just no need rollback
        return;
    }

    //check time
    if (rbcf->interval >= 0) {
        if (nt_file_info(rbcf->backup[0], &sb) == -1) {
            need_do = 1;
            nt_log_error(NT_LOG_INFO, cycle->log, 0,
                          "need rollback [%s]: cannot open backup", rbcf->logname);

        } else if (sb.st_ctime / rbcf->interval < nt_time() / rbcf->interval) {
            need_do = 1;
            nt_log_error(NT_LOG_INFO, cycle->log, 0,
                          "need rollback [%s]: time on [%d] [%d]",
                          rbcf->logname, sb.st_ctime, rbcf->time_now);

        } else {
            nt_log_error(NT_LOG_INFO, cycle->log, 0,
                          "no need rollback [%s]: time not on [%d] [%d]",
                          rbcf->logname, sb.st_ctime, rbcf->time_now);
        }

    } else {
        nt_log_error(NT_LOG_INFO, cycle->log, 0,
                      "no need check rollback [%s] time: no interval", rbcf->logname);
    }

    //check size
    if (rbcf->log_max_size > 0) {
        if (nt_file_info(rbcf->logname, &sb) == 0 && (sb.st_size >= rbcf->log_max_size)) {
            need_do = 1;
            nt_log_error(NT_LOG_INFO, cycle->log, 0,
                          "need rollback [%s]: size on [%d]", rbcf->logname, sb.st_size);

        } else {
            nt_log_error(NT_LOG_INFO, cycle->log, 0,
                          "no need rollback [%s]: size not on", rbcf->logname);
        }

    } else {
        nt_log_error(NT_LOG_INFO, cycle->log, 0,
                      "no need check rollback [%s] size: no max size", rbcf->logname);
    }

    if (need_do) {
        for (i = 1; i < rbcf->backup_num; i++) {
            nt_rename_file(rbcf->backup[rbcf->backup_num - i - 1],
                   rbcf->backup[rbcf->backup_num - i]);
        }
        if (nt_rename_file(rbcf->logname, rbcf->backup[0]) < 0) {
            nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                          "rname %s to %s failed", rbcf->logname, rbcf->backup[0]);
        } else {
            nt_log_error(NT_LOG_WARN, cycle->log, 0,
                          "rollback [%s] success", rbcf->logname);
        }
    }
    nt_close_file(fd);
}

#endif

