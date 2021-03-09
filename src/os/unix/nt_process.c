
#include <nt_core.h>
#include <nt_event.h>
#include <nt_channel.h>


typedef struct {
    int     signo;
    char   *signame;
    char   *name;
    void  (*handler)(int signo, siginfo_t *siginfo, void *ucontext);
} nt_signal_t;



static void nt_execute_proc(nt_cycle_t *cycle, void *data);
static void nt_funjsq_monitor_handler(int signo, siginfo_t *siginfo, void *ucontext);
static void nt_signal_handler(int signo, siginfo_t *siginfo, void *ucontext);
static void nt_process_get_status(void);
static void nt_unlock_mutexes(nt_pid_t pid);


int              nt_argc;
char           **nt_argv;
char           **nt_os_argv;

nt_int_t        nt_process_slot;
nt_socket_t     nt_channel;
nt_int_t        nt_last_process;
nt_process_t    nt_processes[NT_MAX_PROCESSES];

/*
	nt_file_aio_read
	nt_signal_value --> SIG##QUIT

	信号字符对照表
#define NT_SHUTDOWN_SIGNAL      QUIT
#define NT_TERMINATE_SIGNAL     TERM
#define NT_NOACCEPT_SIGNAL      WINCH   //配合USR2对nginx升级，优雅的关闭nginx旧版本的进程。
#define NT_RECONFIGURE_SIGNAL   HUP

#if (NT_LINUXTHREADS)
#define NT_REOPEN_SIGNAL        INFO
#define NT_CHANGEBIN_SIGNAL     XCPU
#else
#define NT_REOPEN_SIGNAL        USR1
#define NT_CHANGEBIN_SIGNAL     USR2   //nginx的版本需要升级的时候，不需要停止nginx，就能对nginx升级
#endif

	reload  NT_RECONFIGURE_SIGNAL   HUP  //nginx进程不关闭，但是重新加载配置文件。= nginx -s reload
	reopen	NT_REOPEN_SIGNAL        USR1 //不用关闭nginx进程就可以重读日志，此命令可以用于nginx的日志定时备份，按月/日等时间间隔分割有用
	stop	NT_TERMINATE_SIGNAL     TERM //nginx的进程马上被关闭，不能完整处理正在使用的nginx的用户的请求，= nginx -s stop
	quit  	NT_SHUTDOWN_SIGNAL      QUIT //优雅的关闭nginx进程，在处理完所有正在使用nginx用户请求后再关闭nginx进程，= nginx -s quit


	9-SIGKILL 19-SIGSTOP 31 32 这4个信号是不能被捕获的	
*/
nt_signal_t  signals[] = {
    { nt_signal_value(NT_RECONFIGURE_SIGNAL),
      "SIG" nt_value(NT_RECONFIGURE_SIGNAL),
      "reload",
      nt_signal_handler },

    { nt_signal_value(NT_REOPEN_SIGNAL),
      "SIG" nt_value(NT_REOPEN_SIGNAL),
      "reopen",
      nt_signal_handler },

    { nt_signal_value(NT_NOACCEPT_SIGNAL),
      "SIG" nt_value(NT_NOACCEPT_SIGNAL),
      "",
      nt_signal_handler },

    { nt_signal_value(NT_TERMINATE_SIGNAL),
      "SIG" nt_value(NT_TERMINATE_SIGNAL),
      "stop",
      nt_signal_handler },

    { nt_signal_value(NT_SHUTDOWN_SIGNAL),
      "SIG" nt_value(NT_SHUTDOWN_SIGNAL),
      "quit",
      nt_signal_handler },

    { nt_signal_value(NT_CHANGEBIN_SIGNAL),
      "SIG" nt_value(NT_CHANGEBIN_SIGNAL),
      "",
      nt_signal_handler },

    { SIGALRM, "SIGALRM", "", nt_signal_handler }, //由于nginx用SIGALRM更新 自己的时间缓存系统，

//    { SIGVTALRM, "SIGVTALRM", "", nt_funjsq_monitor_handler }, //由于nginx用SIGALRM更新 自己的时间缓存系统，所以引入SIGVTALRM信号，程序执行时产生信号

    { SIGINT, "SIGINT", "", nt_signal_handler },

    { SIGIO, "SIGIO", "", nt_signal_handler },

//    { SIGCHLD, "SIGCHLD", "", nt_signal_handler },

    { SIGSYS, "SIGSYS, SIG_IGN", "", NULL },

    { SIGPIPE, "SIGPIPE, SIG_IGN", "", NULL },

    { 0, NULL, "", NULL }
};


/*  子进程创建函数
 *  proc:是子进程的执行函数，
 *  data是参数，
 *  name是子进程的名字
 * */
nt_pid_t
nt_spawn_process(nt_cycle_t *cycle, nt_spawn_proc_pt proc, void *data,
    char *name, nt_int_t respawn)
{
    debug( "nt_spawn_process" );
    u_long     on;
    nt_pid_t  pid;
    nt_int_t  s; //将要创建的子进程在进程表中的位置

    if (respawn >= 0) {
        //替换进程nt_processes[respawn],可安全重用该进程表项
        s = respawn;

    } else {
        for (s = 0; s < nt_last_process; s++) {
            //先找到一个被回收的进程表象
            if (nt_processes[s].pid == -1) {
                break;
            }
        }

        //进程表已经满
        if (s == NT_MAX_PROCESSES) {
            nt_log_error(NT_LOG_ALERT, cycle->log, 0,
                          "no more than %d processes can be spawned",
                          NT_MAX_PROCESSES);
            return NT_INVALID_PID;
        }
    }

    //不是分离的子进程
    if (respawn != NT_PROCESS_DETACHED) {

        /* Solaris 9 still has no AF_LOCAL */
        //创建一对已经连接的无名socket
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, nt_processes[s].channel) == -1)
        {
            nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                          "socketpair() failed while spawning \"%s\"", name);
            return NT_INVALID_PID;
        }

        nt_log_debug2(NT_LOG_DEBUG_CORE, cycle->log, 0,
                       "channel %d:%d",
                       nt_processes[s].channel[0],
                       nt_processes[s].channel[1]);

        //设置socket为非阻塞模式 ，读端
        if (nt_nonblocking(nt_processes[s].channel[0]) == -1) {
            nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                          nt_nonblocking_n " failed while spawning \"%s\"",
                          name);
            nt_close_channel(nt_processes[s].channel, cycle->log);
            return NT_INVALID_PID;
        }

        //设置socket为非阻塞模式 ，写端
        if (nt_nonblocking(nt_processes[s].channel[1]) == -1) {
            nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                          nt_nonblocking_n " failed while spawning \"%s\"",
                          name);
            nt_close_channel(nt_processes[s].channel, cycle->log);
            return NT_INVALID_PID;
        }

        //开启channel[0]的消息驱动IO
        on = 1;
		//FIOASYNC:设置/清楚信号驱动异步I/O标志
		/*根据iocl 的第三个参数指向一个0 值或非0 值
		  分别清除或设置针对本套接口的信号驱动异步I/O 标志，
		  它决定是否收取针对本套接口的异步I/O 信号（SIGIO ）。
		  本请求和O_ASYNC 文件状态标志等效，
		  而该标志可以通过fcntl 的F_SETFL 命令清除或设置。*/
		if (ioctl(nt_processes[s].channel[0], FIOASYNC, &on) == -1) {
            nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                          "ioctl(FIOASYNC) failed while spawning \"%s\"", name);
            nt_close_channel(nt_processes[s].channel, cycle->log);
            return NT_INVALID_PID;
        }

		//设置channel[0]的宿主，控制channel[0]的SIGIO信号只发给这个进程
        if (fcntl(nt_processes[s].channel[0], F_SETOWN, nt_pid) == -1) {
            nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                          "fcntl(F_SETOWN) failed while spawning \"%s\"", name);
            nt_close_channel(nt_processes[s].channel, cycle->log);
            return NT_INVALID_PID;
        }

		/*
			system() popen() exec等函数会受影响
		*/
		//若进程执行了exec后，关闭socket
        if (fcntl(nt_processes[s].channel[0], F_SETFD, FD_CLOEXEC) == -1) {
            nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                          "fcntl(FD_CLOEXEC) failed while spawning \"%s\"",
                           name);
            nt_close_channel(nt_processes[s].channel, cycle->log);
            return NT_INVALID_PID;
        }

		//若进程执行了exec后，关闭socket
        if (fcntl(nt_processes[s].channel[1], F_SETFD, FD_CLOEXEC) == -1) {
            nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                          "fcntl(FD_CLOEXEC) failed while spawning \"%s\"",
                           name);
            nt_close_channel(nt_processes[s].channel, cycle->log);
            return NT_INVALID_PID;
        }

		//若进程执行了exec后，关闭socket
        nt_channel = nt_processes[s].channel[1];

    } else {
        nt_processes[s].channel[0] = -1;
        nt_processes[s].channel[1] = -1;
    }

	//设置当前子进程的进程表索引值
    nt_process_slot = s;


    pid = fork();

    switch (pid) {

    case -1:
        nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                      "fork() failed while spawning \"%s\"", name);
        nt_close_channel(nt_processes[s].channel, cycle->log);
        return NT_INVALID_PID;

    case 0:
		
        nt_parent = nt_pid;
		//设置当前子进程的进程id
        nt_pid = nt_getpid();
		//子进程运行执行函数
        proc(cycle, data);
        break;

    default:
        break;
    }

    nt_log_error(NT_LOG_NOTICE, cycle->log, 0, "start %s %P", name, pid);

	//设置一些进程表项字段
    nt_processes[s].pid = pid;
    nt_processes[s].exited = 0;

	//如果是重复创建，即为替换进程，不用设置其他进程表字段，直接返回。
    if (respawn >= 0) {
        return pid;
    }

	//设置进程表项的一些状态字
    nt_processes[s].proc = proc;
    nt_processes[s].data = data;
    nt_processes[s].name = name;
    nt_processes[s].exiting = 0;

    switch (respawn) {

    case NT_PROCESS_NORESPAWN:
        nt_processes[s].respawn = 0;
        nt_processes[s].just_spawn = 0;
        nt_processes[s].detached = 0;
        break;

    case NT_PROCESS_JUST_SPAWN:
        nt_processes[s].respawn = 0;
        nt_processes[s].just_spawn = 1;
        nt_processes[s].detached = 0;
        break;

    case NT_PROCESS_RESPAWN:
        nt_processes[s].respawn = 1;
        nt_processes[s].just_spawn = 0;
        nt_processes[s].detached = 0;
        break;

    case NT_PROCESS_JUST_RESPAWN:
        nt_processes[s].respawn = 1;
        nt_processes[s].just_spawn = 1;
        nt_processes[s].detached = 0;
        break;

    case NT_PROCESS_DETACHED:
        nt_processes[s].respawn = 0;
        nt_processes[s].just_spawn = 0;
        nt_processes[s].detached = 1;
        break;
    }

    if (s == nt_last_process) {
        nt_last_process++;
    }

    return pid;
}


nt_pid_t
nt_execute(nt_cycle_t *cycle, nt_exec_ctx_t *ctx)
{
    return nt_spawn_process(cycle, nt_execute_proc, ctx, ctx->name,
                             NT_PROCESS_DETACHED);
}


static void
nt_execute_proc(nt_cycle_t *cycle, void *data)
{
    nt_exec_ctx_t  *ctx = data;

    if (execve(ctx->path, ctx->argv, ctx->envp) == -1) {
        nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                      "execve() failed while executing %s \"%s\"",
                      ctx->name, ctx->path);
    }

    exit(1);
}

/*

	注册并绑定信号的回调函数
*/
nt_int_t
nt_init_signals(nt_log_t *log)
{
    nt_signal_t      *sig;
    struct sigaction   sa;

    for (sig = signals; sig->signo != 0; sig++) {
        nt_memzero(&sa, sizeof(struct sigaction));

        if (sig->handler) {
            sa.sa_sigaction = sig->handler;
            sa.sa_flags = SA_SIGINFO;

        } else {
            sa.sa_handler = SIG_IGN;
        }

        sigemptyset(&sa.sa_mask);
        if (sigaction(sig->signo, &sa, NULL) == -1) {
#if (NT_VALGRIND)
            nt_log_error(NT_LOG_ALERT, log, nt_errno,
                          "sigaction(%s) failed, ignored", sig->signame);
#else
            nt_log_error(NT_LOG_EMERG, log, nt_errno,
                          "sigaction(%s) failed", sig->signame);
            return NT_ERROR;
#endif
        }
    }

    return NT_OK;
}


static void
nt_funjsq_monitor_handler(int signo, siginfo_t *siginfo, void *ucontext){

	debug( "start");	
}

//信号量绑定的触发函数
static void
nt_signal_handler(int signo, siginfo_t *siginfo, void *ucontext)
{
    char            *action;
    nt_int_t        ignore;
    nt_err_t        err;
    nt_signal_t    *sig;

    ignore = 0;

    err = nt_errno;

    for (sig = signals; sig->signo != 0; sig++) {
        if (sig->signo == signo) {
            break;
        }
    }

    //信号触发，时间更新
    nt_time_sigsafe_update();

    action = "";
    if( signo == 17 ){
	goto do_sigchld; 
    }

    debug( "accept sig=%d, signo=%d", nt_process, signo);
    switch (nt_process) {

    case NT_PROCESS_MASTER:
	debug( "accept MASTER");
    case NT_PROCESS_SINGLE:
#if (T_PIPES)
    case NT_PROCESS_PIPE:
	debug( "accept PIPE");
#endif
        switch (signo) {

        case nt_signal_value(NT_SHUTDOWN_SIGNAL):
            nt_quit = 1;
            action = ", shutting down";
            break;

        case nt_signal_value(NT_TERMINATE_SIGNAL):
        case SIGINT:
            nt_terminate = 1;
            action = ", exiting";
            break;

        case nt_signal_value(NT_NOACCEPT_SIGNAL):
            if (nt_daemonized) {
                nt_noaccept = 1;
                action = ", stop accepting connections";
            }
            break;

        case nt_signal_value(NT_RECONFIGURE_SIGNAL):
            nt_reconfigure = 1;
            action = ", reconfiguring";
            break;

        case nt_signal_value(NT_REOPEN_SIGNAL):
            nt_reopen = 1;
            action = ", reopening logs";
            break;

        case nt_signal_value(NT_CHANGEBIN_SIGNAL):
            if (nt_getppid() == nt_parent || nt_new_binary > 0) {

                /*
                 * Ignore the signal in the new binary if its parent is
                 * not changed, i.e. the old binary's process is still
                 * running.  Or ignore the signal in the old binary's
                 * process if the new binary's process is already running.
                 */

                action = ", ignoring";
                ignore = 1;
                break;
            }

            nt_change_binary = 1;
            action = ", changing binary";
            break;

        case SIGALRM:
	    debug("SIGALRM catch");
            nt_sigalrm = 1;
            break;

        case SIGIO:
            nt_sigio = 1;
            break;

        case SIGCHLD:
            nt_reap = 1;
            break;

        }

        break;

    case NT_PROCESS_WORKER:
	debug( "accept WORKER");
    case NT_PROCESS_HELPER:
	debug( "accept HELPER");
#if (NT_PROCS)
    case NT_PROCESS_PROC:
	debug( "accept PROC");
#endif
        switch (signo) {

        case nt_signal_value(NT_NOACCEPT_SIGNAL):
            if (!nt_daemonized) {
                break;
            }
            nt_debug_quit = 1;
            /* fall through */
        case nt_signal_value(NT_SHUTDOWN_SIGNAL):
            nt_quit = 1;
            action = ", shutting down";
            break;

        case nt_signal_value(NT_TERMINATE_SIGNAL):
        case SIGINT:
            nt_terminate = 1;
            action = ", exiting";
            break;

        case nt_signal_value(NT_REOPEN_SIGNAL):
            nt_reopen = 1;
            action = ", reopening logs";
            break;

        case nt_signal_value(NT_RECONFIGURE_SIGNAL):
        case nt_signal_value(NT_CHANGEBIN_SIGNAL):
        case SIGIO:
            action = ", ignoring";
            break;
        }

        break;
    }

    if (siginfo && siginfo->si_pid) {
        nt_log_error(NT_LOG_NOTICE, nt_cycle->log, 0,
                      "signal %d (%s) received from %P%s",
                      signo, sig->signame, siginfo->si_pid, action);

    } else {
        nt_log_error(NT_LOG_NOTICE, nt_cycle->log, 0,
                      "signal %d (%s) received%s",
                      signo, sig->signame, action);
    }

    if (ignore) {
        nt_log_error(NT_LOG_CRIT, nt_cycle->log, 0,
                      "the changing binary signal is ignored: "
                      "you should shutdown or terminate "
                      "before either old or new binary's process");
    }


do_sigchld:

    if (signo == SIGCHLD) {
        nt_process_get_status();
    }

    nt_set_errno(err);
}


/*获取进程状态*/
static void
nt_process_get_status(void)
{
    int              status;
    char            *process;
    nt_pid_t        pid;
    nt_err_t        err;
    nt_int_t        i;
    nt_uint_t       one;

    one = 0;
//   debug(2, "nt_last_process=%d", nt_last_process);

    for ( ;; ) {
	// 使用了WNOHANG(wait no hung)参数调用waitpid，即使没有子进程退出，它也会立即返回，不会像wait那样永远等下去。
        pid = waitpid(-1, &status, WNOHANG);

        if (pid == 0) {
            return;
        }
	
	debug( "pid=%d", pid);

        if (pid == -1) {
            err = nt_errno;

            if (err == NT_EINTR) {
                continue;
            }

            if (err == NT_ECHILD && one) {
                return;
            }

            /*
             * Solaris always calls the signal handler for each exited process
             * despite waitpid() may be already called for this process.
             *
             * When several processes exit at the same time FreeBSD may
             * erroneously call the signal handler for exited process
             * despite waitpid() may be already called for this process.
             */

            if (err == NT_ECHILD) {
                nt_log_error(NT_LOG_INFO, nt_cycle->log, err,
                              "waitpid() failed");
                return;
            }

            nt_log_error(NT_LOG_ALERT, nt_cycle->log, err,
                          "waitpid() failed");
            return;
        }


        one = 1;
        process = "unknown process";

        for (i = 0; i < nt_last_process; i++) {
            if (nt_processes[i].pid == pid) {
                nt_processes[i].status = status;
                nt_processes[i].exited = 1;      //退出状态
                process = nt_processes[i].name;
                break;
            }
        }

#if (T_PIPES)
        if (i == nt_last_process) {
            process = "pipe process";
            nt_pipe_broken_action(nt_cycle->log, pid, 1);
        }
#endif

	//取得子进程因信号而中止的信号：
        if (WTERMSIG(status)) {
#ifdef WCOREDUMP
            nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0,
                          "%s %P exited on signal %d%s",
                          process, pid, WTERMSIG(status),
                          WCOREDUMP(status) ? " (core dumped)" : "");
#else
            nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0,
                          "%s %P exited on signal %d",
                          process, pid, WTERMSIG(status));
#endif

        } else {
            nt_log_error(NT_LOG_NOTICE, nt_cycle->log, 0,
                          "%s %P exited with code %d",
                          process, pid, WEXITSTATUS(status));
        }

        if (WEXITSTATUS(status) == 2 && nt_processes[i].respawn) {
            nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0,
                          "%s %P exited with fatal code %d "
                          "and cannot be respawned",
                          process, pid, WEXITSTATUS(status));
            nt_processes[i].respawn = 0;
        }

//	debug(2, "pid=%d", pid);
        nt_unlock_mutexes(pid);
    }
}

////判断子进程是否意外退出，原因，不能使用SIGCHLD，因为主进程调用popen 或system，会一直发SIGCHLD信号，导致程序不稳定
void nt_sub_process_status(void){

#if 0
	char path[12] = {0};
	char buf[36] = {0};
	char s[8] = {0};
	int exit = 0;
	for (i = 0; i < nt_last_process; i++) {
		debug(2, "i, pid=%d, status=%d, exited=%d, name =%s",i, nt_processes[i].pid, nt_processes[i].status, nt_processes[i].exited, nt_processes[i].name );


		sprintf( path , "/proc/%d/stat", nt_processes[i].pid);

		FILE *f=NULL;
		f = fopen( path , "r"  ) ;
		if(f) {
			fgets( buf , sizeof(buf), f);

			sscanf(buf, "%*s %*s %s", s);
			if(buf && s[0] == 'Z' ){
				debug( 2, "pid=%d 不在", nt_processes[i].pid);
				nt_processes[i].status = 0;
				nt_processes[i].exited = 1;
				exit = 1;


			}
			fclose(f);
		} else {
			debug( 2, "pid=%d 不在", nt_processes[i].pid);
			nt_processes[i].status = 0;
			nt_processes[i].exited = 1;
			exit = 1;
		}

	}   

	if( exit == 1){
		int status;
		pid = waitpid(-1, &status, WNOHANG);

		debug(2, "pid = %d, status =%d", pid, status);
		nt_reap = 1;
	}
#endif

	int pid = 0;
	int i = 0;
	int status;
	char *process = NULL;
	pid = waitpid(-1, &status, WNOHANG);

	if (pid == 0 || pid == -1) 
		return;

	debug( "exit pid=%d", pid);
	process = "unknown process";
	for (i = 0; i < nt_last_process; i++) {
		if (nt_processes[i].pid == pid) {
			nt_processes[i].status = status;
			nt_processes[i].exited = 1;      //退出状态
			process = nt_processes[i].name;
			nt_reap = 1;
			break;
		}   
	}   

	//取得子进程因信号而中止的信号：                                                                            
	if (WTERMSIG(status)) {
#ifdef WCOREDUMP
		nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0,
				"%s %P exited on signal %d%s",
				process, pid, WTERMSIG(status),
				WCOREDUMP(status) ? " (core dumped)" : "");
#else
		nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0,
				"%s %P exited on signal %d",
				process, pid, WTERMSIG(status));
#endif

	} else {
		nt_log_error(NT_LOG_NOTICE, nt_cycle->log, 0,
				"%s %P exited with code %d",
				process, pid, WEXITSTATUS(status));
	}

	if (WEXITSTATUS(status) == 2 && nt_processes[i].respawn) {
		nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0,
				"%s %P exited with fatal code %d "
				"and cannot be respawned",
				process, pid, WEXITSTATUS(status));
		nt_processes[i].respawn = 0;
	}

	nt_unlock_mutexes(pid);

}

	static void
nt_unlock_mutexes(nt_pid_t pid)
{
    nt_uint_t        i;
    nt_shm_zone_t   *shm_zone;
    nt_list_part_t  *part;
    nt_slab_pool_t  *sp;

    /*
     * unlock the accept mutex if the abnormally exited process
     * held it
     */

    if (nt_accept_mutex_ptr) {
        (void) nt_shmtx_force_unlock(&nt_accept_mutex, pid);
    }

    /*
     * unlock shared memory mutexes if held by the abnormally exited
     * process
     */

    part = (nt_list_part_t *) &nt_cycle->shared_memory.part;
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

        sp = (nt_slab_pool_t *) shm_zone[i].shm.addr;

        if (nt_shmtx_force_unlock(&sp->mutex, pid)) {
            nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0,
                          "shared memory zone \"%V\" was locked by %P",
                          &shm_zone[i].shm.name, pid);
        }
    }
}


void
nt_debug_point(void)
{
    nt_core_conf_t  *ccf;

    ccf = (nt_core_conf_t *) nt_get_conf(nt_cycle->conf_ctx,
                                           nt_core_module);

    switch (ccf->debug_points) {

    case NT_DEBUG_POINTS_STOP:
        raise(SIGSTOP);
        break;

    case NT_DEBUG_POINTS_ABORT:
        nt_abort();
    }
}


nt_int_t
nt_os_signal_process(nt_cycle_t *cycle, char *name, nt_pid_t pid)
{
    nt_signal_t  *sig;

    for (sig = signals; sig->signo != 0; sig++) {
        if (nt_strcmp(name, sig->name) == 0) {
            if (kill(pid, sig->signo) != -1) {
                return 0;
            }

            nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                          "kill(%P, %d) failed", pid, sig->signo);
        }
    }

    return 1;
}
