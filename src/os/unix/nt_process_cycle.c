

#include <nt_core.h>
#include <nt_event.h>
#include <nt_channel.h>


#if (T_PIPES)
#define NT_PIPE_STILL_NEEDED     0x02
#endif

static void nt_quit_sub_process(  nt_cycle_t *cycle, int signo);
static void nt_start_worker_processes(nt_cycle_t *cycle, nt_int_t n,
    nt_int_t type);
static void nt_start_cache_manager_processes(nt_cycle_t *cycle,
    nt_uint_t respawn);
static void nt_pass_open_channel(nt_cycle_t *cycle, nt_channel_t *ch);
static void nt_signal_worker_processes(nt_cycle_t *cycle, int signo);
static nt_uint_t nt_reap_children(nt_cycle_t *cycle);

#ifdef NT_HAVE_FUNJSQ
static void nt_master_process_exit(struct F_hsearch_data *tab, nt_cycle_t *cycle);
#else
static void nt_master_process_exit( nt_cycle_t *cycle);
#endif
static void nt_worker_process_cycle(nt_cycle_t *cycle, void *data);
static void nt_worker_process_init(nt_cycle_t *cycle, nt_int_t worker);
static void nt_worker_process_exit(nt_cycle_t *cycle);
static void nt_channel_handler(nt_event_t *ev);
static void nt_cache_manager_process_cycle(nt_cycle_t *cycle, void *data);
static void nt_cache_manager_process_handler(nt_event_t *ev);
static void nt_cache_loader_process_handler(nt_event_t *ev);


nt_uint_t    nt_process;
nt_uint_t    nt_worker;
nt_pid_t     nt_pid;
nt_pid_t     nt_parent;

sig_atomic_t  nt_reap;
sig_atomic_t  nt_sigio;
sig_atomic_t  nt_sigalrm;
sig_atomic_t  nt_terminate;
sig_atomic_t  nt_quit;
sig_atomic_t  nt_debug_quit;
nt_uint_t    nt_exiting;
sig_atomic_t  nt_reconfigure;
sig_atomic_t  nt_reopen;

sig_atomic_t  nt_change_binary;
nt_pid_t     nt_new_binary;
nt_uint_t    nt_inherited;
nt_uint_t    nt_daemonized;

sig_atomic_t  nt_noaccept;
nt_uint_t    nt_noaccepting;
nt_uint_t    nt_restart;


static u_char  master_process[] = "master";


static nt_cache_manager_ctx_t  nt_cache_manager_ctx = {
    nt_cache_manager_process_handler, "cache manager process", 0
};

static nt_cache_manager_ctx_t  nt_cache_loader_ctx = {
    nt_cache_loader_process_handler, "cache loader process", 60000
};


static nt_cycle_t      nt_exit_cycle;
static nt_log_t        nt_exit_log;
static nt_open_file_t  nt_exit_log_file;

/*
	主进程循环函数
	当信号进来时会触发 nt_signal_handler函数
	然后设置全局变量
	nt_quit = 1;
	nt_terminate = 1;
	nt_noaccept = 1;
	nt_reconfigure = 1;
	nt_reopen = 1;
	ignore = 1;
	nt_change_binary = 1;
	nt_sigalrm = 1;
	nt_sigio = 1;
	nt_reap = 1;	
	nt_quit = 1;
	nt_terminate = 1;
	nt_reopen = 1;	

	主进程循环检测到变量变化执行相应的逻辑	

--------------------------------------------------------------------------------------------------------------------------
| 信号      | 对应进程中的全局标志位变量 | 意义
--------------------------------------------------------------------------------------------------------------------------
| QUIT      | nt_quit                   | 优雅地关闭整个服务
| TERM或INT | nt_terminate              | 强制关闭整个服务
| USR1      | nt_reopen                 | 重新打开服务中的所有文件
| WINCH     | nt_noaccept               | 所有子进程不再接受处理新的连接，实际相当于对所有子进程发送QUIT信号
| USR2      | nt_change_binary          | 平滑升级到新版本的Nginx程序
| HUP       | nt_reconfigure            | 重读配置文件并使服务对新配置项生效
| CHLD      | nt_reap                   | 有子进程意外结束，这时需要监控所有子进程，也就是nt_reap_children方法所做的工作
--------------------------------------------------------------------------------------------------------------------------


3. 在父子进程进程管道通信时，如果管道读端都关闭，会收到SIGPIPE信号，模拟场景，对该信号进行捕捉，并且使用捕捉函数回收子进程。


*/
void sigroutine(int signo){
	debug( "sigroutine");
	signal(SIGVTALRM, sigroutine);
}

extern int  funjsq_et_flag ;
	void
nt_master_process_cycle(nt_cycle_t *cycle)
{
	char              *title;
	u_char            *p;
	size_t             size;
	nt_int_t          i;
	nt_uint_t         n, sigio;
#if (T_PIPES)
	nt_uint_t         close_old_pipe;
#endif
	sigset_t           set;
	struct itimerval   itv;
	nt_uint_t         live;
	nt_msec_t         delay;
	nt_listening_t   *ls;
	nt_core_conf_t   *ccf;

	//将信号集初始化为空。
	sigemptyset(&set);

	//把信号signo添加到信号集set中，成功时返回0，失败时返回-1。
	//sigaddset(&set, SIGCHLD);
	sigaddset(&set, SIGALRM);
	//    sigaddset(&set, SIGVTALRM);
	sigaddset(&set, SIGIO);
	sigaddset(&set, SIGINT);
	sigaddset(&set, nt_signal_value(NT_RECONFIGURE_SIGNAL));
	sigaddset(&set, nt_signal_value(NT_REOPEN_SIGNAL));
	sigaddset(&set, nt_signal_value(NT_NOACCEPT_SIGNAL));
	sigaddset(&set, nt_signal_value(NT_TERMINATE_SIGNAL));
	sigaddset(&set, nt_signal_value(NT_SHUTDOWN_SIGNAL));
	sigaddset(&set, nt_signal_value(NT_CHANGEBIN_SIGNAL));


	/*
	   sigprocmask设定对信号屏蔽集内的信号的处理方式(阻塞或不阻塞)。

	   参数：
	   how：用于指定信号修改的方式，可能选择有三种
	   SIG_BLOCK //将set所指向的信号集中包含的信号加到当前的信号掩码中。即信号掩码和set信号集进行或操作。
	   SIG_UNBLOCK //将set所指向的信号集中包含的信号从当前的信号掩码中删除。即信号掩码和set进行与操作。
	   SIG_SETMASK //将set的值设定为新的进程信号掩码。即set对信号掩码进行了赋值操作。
	   set：为指向信号集的指针，在此专指新设的信号集，如果仅想读取现在的屏蔽值，可将其置为NULL。
	   oldset：也是指向信号集的指针，在此存放原来的信号集。可用来检测信号掩码中存在什么信号。

	   返回说明：
	   成功执行时，返回0。失败返回-1，errno被设为EINVAL。
	   */
	//    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
	if (sigprocmask(SIG_UNBLOCK, &set, NULL) == -1) {	//add by lvlai,改为非阻塞模式， 用来在for循环中增加监控代码
		nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
					  "sigprocmask() failed");
	}

	//将信号集初始化为空。
	sigemptyset(&set);

#if 0
	size = sizeof(master_process);

	for (i = 0; i < nt_argc; i++) {
		size += nt_strlen(nt_argv[i]) + 1;
	}

	title = nt_pnalloc(cycle->pool, size);
	if (title == NULL) {
		/* fatal */
		exit(2);
	}

	p = nt_cpymem(title, master_process, sizeof(master_process) - 1);

	/*
	   for (i = 0; i < nt_argc; i++) {
	 *p++ = ' ';
	 p = nt_cpystrn(p, (u_char *) nt_argv[i], size);
	 }
	 */
	debug(2, "master process cycle title=%s", title );
	nt_setproctitle(title);
#endif

	ccf = (nt_core_conf_t *) nt_get_conf(cycle->conf_ctx, nt_core_module);

	//  开启work进程 /*创建子进程，接受请求，完成响应*/
//	    nt_start_worker_processes(cycle, ccf->worker_processes,
//	                               NT_PROCESS_RESPAWN);			//当前先不添加
	/*创建有关cache的子进程*/
	nt_start_cache_manager_processes(cycle, 0);

#if (NT_PROCS)
	nt_procs_start(cycle, 0);
#endif

	nt_new_binary = 0;
#if (T_PIPES)
	close_old_pipe = 0;
#endif
	delay = 0;
	sigio = 0;
	live = 0;
	// int funjsq_et_flag = 0;

	/*
	   设置监控时间间隔
	   */
	/*
	//    signal(SIGVTALRM, sigroutine);
	signal(SIGPROF, sigroutine);
	//	等价 itv = {{0, 0}, {delay / 1000, (delay % 1000 ) * 1000}};	
	struct itimerval  funjsq_timer ; //= {{0, 1}, {0, 1 }};
	funjsq_timer.it_interval.tv_sec = 0;		//不会触发定时
	funjsq_timer.it_interval.tv_usec = 1;	//
	funjsq_timer.it_value.tv_sec = 0;
	funjsq_timer.it_value.tv_usec = 1;

	//设置定时器
	//    if (setitimer(ITIMER_VIRTUAL, &funjsq_timer, NULL) == -1) {
	if (setitimer(ITIMER_PROF, &funjsq_timer, NULL) == -1) {
	debug(2, "set funjsq_timer failed");
	nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
	"setitimer() funjsq_timer failed");
	}
	*/

//	funjsq_dl_check();

	//    check_dns_ipset();
	int funjsq_count = 0;
	//    struct hsearch_data htab1 = {0} ;  
	//    struct hsearch_data htab2 = {0} ;  
	//    hcreate_r(5, &htab1);
	//    hcreate_r(5, &htab2);
//	struct F_hsearch_data htab = {0};  //必须初始化

//	hash_init( 5, &htab);

	nt_restart = 1;  
	for ( ;; ) {
		//	debug(2, "master process cycle delay=%d", delay);

		/*
		 * delay用来设置等待worker推出的时间，master接受了退出信号后，
		 * 首先发送退出信号给worker，而worker退出需要一些时间
		 * */
		if (delay) {
			if (nt_sigalrm) {
				sigio = 0;
				delay *= 2;
				nt_sigalrm = 0;
			}

			nt_log_debug1(NT_LOG_DEBUG_EVENT, cycle->log, 0,
						   "termination cycle: %M", delay);

			itv.it_interval.tv_sec = 0;		//不会触发定时
			itv.it_interval.tv_usec = 0;	//
			itv.it_value.tv_sec = delay / 1000;
			itv.it_value.tv_usec = (delay % 1000 ) * 1000;
			//	等价 itv = {{0, 0}, {delay / 1000, (delay % 1000 ) * 1000}};	


			//设置定时器
			//以系统真实时间来计算，送出SIGALRM信号
			if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
				nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
							  "setitimer() failed");
			}
		}

		nt_log_debug0(NT_LOG_DEBUG_EVENT, cycle->log, 0, "sigsuspend");

		/*
		   阻塞信号，当信号进来才触发
		   如果在等待信号发生时希望去休眠，则使用sigsuspend函数是非常合适的，
		   */
		// sigsuspend(&set);  //add by lvlai,取消阻塞

		//	debug(2, "master process cycle sigio %i", sigio);
		//	debug(2, "reap=%d,term=%d,quit=%d, reconf=%d, reopen=%d, restart=%d,  change_binary=%d, noaccept=%d", nt_reap, nt_terminate, nt_quit, nt_reconfigure , nt_reopen, nt_restart, nt_change_binary , nt_noaccept);

		//更新时间缓存
		nt_time_update();

		nt_log_debug1(NT_LOG_DEBUG_EVENT, cycle->log, 0,
					   "wake up, sigio %i", sigio);

		/* 
		   if(nt_kill){
		   debug(2, "get kill -9 ");
		   nt_kill = 0;
		   } */
#if 0
		if( funjsq_count >= 3610 ){
			funjsq_dl_check();
			debug(2, "funjsq_count=%d ", funjsq_count);

			//执行检测凌晨清除 ipset

			check_dns_ipset();
			funjsq_count = 0;
		}


		if( funjsq_count % 5 == 0){
			debug(2, "funjsq_count=%d ", funjsq_count);

			/*	
				hash_table_insert("5C:52:1E:85:08:AE", "1.1.1.1", 2, 2);
				hash_table_print(0);

				HashNode* p = hash_table_lookup( "5C:52:1E:85:08:AE" ) ;

				if( p != NULL ){
				debug(2, "p-skey=%s", p->sKey);
				char ip[18]={0};
				get_ip_by_mac(p->sKey, ip);
				debug(2, "ip=%s", ip);

				}	

				hash_table_insert("5C:52:1E:85:08:AE", "1.1.1.1", 2, 2);
				hash_table_print(0);
				hash_table_insert("5C:52:1E:85:08:AE", NULL, 1, 1);
				*/	
			check_nginx_rule_pro( &htab);
		}

		debug(2, "filled=%d, live=%d", htab.filled, live);	


		if( (htab.filled > 0) && live == 0 ){
			//		nt_reconfigure = 1;  //使用nt_reconfigure 主进程内存增加一倍
			if( funjsq_et_flag == 0)
				nt_restart = 1;  
		} 

		if( funjsq_et_flag == 1 && live == 1){
			nt_quit_sub_process(cycle,
								 nt_signal_value(NT_SHUTDOWN_SIGNAL));
			live = 0;
		}

		/* if( htab.filled <= 0 && live == 1){
			nt_quit_sub_process(cycle,
								 nt_signal_value(NT_SHUTDOWN_SIGNAL));
			live = 0;
		} */

#endif


		nt_sub_process_status();

		nt_msleep(1000); //add by lvlai
		funjsq_count++;

		//收到了SIGCHLD信号，有worker退出(nt_reap == 1)
		if (nt_reap) {
			nt_reap = 0;
			nt_log_debug0(NT_LOG_DEBUG_EVENT, cycle->log, 0, "reap children");

			/*	nt_reap_children(会遍历nt_process数组，检查每个子进程的状态，对于非正常退出的子进程会重新拉起，
				最后，返回一个live标志位，如果所有的子进程都已经正常退出，则live为0，初次之外live为1。*/
			live = nt_reap_children(cycle);     //有子进程退出，需要重启
#if (T_PIPES)
			if (!(live & NT_PIPE_STILL_NEEDED) && close_old_pipe) {
				nt_close_old_pipes();
				close_old_pipe = 0;
			}
#endif
		}

		//	debug(2, "live=%d, nt_terminate=%d, nt_quit=%d", live, nt_terminate, nt_quit);
		// live 子进程是否存在的标志，如果存在，live为1.
		//如果worker都退出了
		//并且收到了NT_CMD_TERMINATE命令或者SIGTERM信号或SIGINT信号(nt_terminate ==1)
		//或NT_CMD_QUIT命令或SIGQUIT信号(nt_quit == 1),则master退出
		//if (!live && (nt_terminate || nt_quit)) {
		if ( (nt_terminate || nt_quit)) {
			debug( "do master exit");
#ifdef NT_HAVE_FUNJSQ
			nt_master_process_exit( &htab, cycle);
#else
			nt_master_process_exit( cycle);
#endif 
		}

		//收到了NT_CMD_TERMINATE命令或者SIGTERM信号或SIGINT信号(nt_terminate ==1)
		//通知所有worker退出，并且等待worker退出
		if (nt_terminate) {
			if (delay == 0) {
				delay = 50;
			}

			if (sigio) {
				sigio--;
				continue;
			}

			sigio = ccf->worker_processes + 2 /* cache processes */;

			//延时已到，给所有worker发送SIGKILL信号，强制杀死worker
			if (delay > 1000) {
				nt_signal_worker_processes(cycle, SIGKILL);
			} else {
				//给所有worker发送SIGTERM信号，通知worker退出
				nt_signal_worker_processes(cycle,
											nt_signal_value(NT_TERMINATE_SIGNAL));
			}

			continue;
		}

		//NT_CMD_QUIT命令或SIGQUIT信号(nt_quit == 1)
		if (nt_quit) {
			//给所有的worker发送SIGQUIT信号
			nt_signal_worker_processes(cycle,
										nt_signal_value(NT_SHUTDOWN_SIGNAL));

			//关闭所有监听socket
			ls = cycle->listening.elts;
			for (n = 0; n < cycle->listening.nelts; n++) {
				if (nt_close_socket(ls[n].fd) == -1) {
					nt_log_error(NT_LOG_EMERG, cycle->log, nt_socket_errno,
								  nt_close_socket_n " %V failed",
								  &ls[n].addr_text);
				}
			}
			cycle->listening.nelts = 0;

			continue;
		}

		//        debug(2, "nt_reconfigure=%d", nt_reconfigure);
		//收到SIGHUP信号
		if (nt_reconfigure) {
			nt_reconfigure = 0;

			//代码已被替换，重启worker，不需要重新初始化配置。
			if (nt_new_binary) {
				nt_start_worker_processes(cycle, ccf->worker_processes,
										   NT_PROCESS_RESPAWN);
				nt_start_cache_manager_processes(cycle, 0);

#if (NT_PROCS)
				nt_procs_start(cycle, 0);
#endif
				nt_noaccepting = 0;

				continue;
			}

			nt_log_error(NT_LOG_NOTICE, cycle->log, 0, "reconfiguring");

			//重新初始化配置
			cycle = nt_init_cycle(cycle);
			if (cycle == NULL) {
				cycle = (nt_cycle_t *) nt_cycle;
				continue;
			}

			//重启worker
			nt_cycle = cycle;
			ccf = (nt_core_conf_t *) nt_get_conf(cycle->conf_ctx,
												   nt_core_module);
			nt_start_worker_processes(cycle, ccf->worker_processes,
									   NT_PROCESS_JUST_RESPAWN);
			nt_start_cache_manager_processes(cycle, 1);

#if (NT_PROCS)
			nt_procs_start(cycle, 1);
#endif

			/* allow new processes to start */
			nt_msleep(100);

			live = 1;
#if (T_PIPES)
			close_old_pipe = 1;
#endif
			nt_signal_worker_processes(cycle,
										nt_signal_value(NT_SHUTDOWN_SIGNAL));
		}

		//当nt_noaccepting==1时，会把nt_restart设为1，重启worker
		if (nt_restart) {
			nt_restart = 0;
			nt_start_worker_processes(cycle, ccf->worker_processes,
									   NT_PROCESS_RESPAWN);
			nt_start_cache_manager_processes(cycle, 0);

#if (NT_PROCS)
			nt_procs_start(cycle, 0);
#endif

			live = 1;
		}

		//收到SIGUSR1信号，重新打开log文件
		if (nt_reopen) {
			nt_reopen = 0;
			nt_log_error(NT_LOG_NOTICE, cycle->log, 0, "reopening logs");
			nt_reopen_files(cycle, ccf->user);
			nt_signal_worker_processes(cycle,
										nt_signal_value(NT_REOPEN_SIGNAL));
		}

		//收到SIGUSER2，热代码替换
		if (nt_change_binary) {
			nt_change_binary = 0;
			nt_log_error(NT_LOG_NOTICE, cycle->log, 0, "changing binary");
			nt_new_binary = nt_exec_new_binary(cycle, nt_argv);
		}

		//收到SIGWINCH信号不在接受请求，worker退出，master不退出
		if (nt_noaccept) {
			nt_noaccept = 0;
			nt_noaccepting = 1;
			nt_signal_worker_processes(cycle,
										nt_signal_value(NT_SHUTDOWN_SIGNAL));
		}
	}
}


/*
   单进程循环，暂时不用
*/
void
nt_single_process_cycle(nt_cycle_t *cycle)
{
    nt_uint_t  i;

    if (nt_set_environment(cycle, NULL) == NULL) {
        /* fatal */
        exit(2);
    }

    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->init_process) {
            if (cycle->modules[i]->init_process(cycle) == NT_ERROR) {
                /* fatal */
                exit(2);
            }
        }
    }

#ifdef NT_HAVE_FUNJSQ
    struct F_hsearch_data tab = {0};  //必须初始化
    hash_init( 6, &tab);
#endif


    for ( ;; ) {
        nt_log_debug0(NT_LOG_DEBUG_EVENT, cycle->log, 0, "worker cycle");

        nt_process_events_and_timers(cycle);

        if (nt_terminate || nt_quit) {

            for (i = 0; cycle->modules[i]; i++) {
                if (cycle->modules[i]->exit_process) {
                    cycle->modules[i]->exit_process(cycle);
                }
            }

#ifdef NT_HAVE_FUNJSQ
            nt_master_process_exit( &tab, cycle);
#else
            nt_master_process_exit(  cycle);
#endif
        }

        if (nt_reconfigure) {
            nt_reconfigure = 0;
            nt_log_error(NT_LOG_NOTICE, cycle->log, 0, "reconfiguring");

            cycle = nt_init_cycle(cycle);
            if (cycle == NULL) {
                cycle = (nt_cycle_t *) nt_cycle;
                continue;
            }

            nt_cycle = cycle;
        }

        if (nt_reopen) {
            nt_reopen = 0;
            nt_log_error(NT_LOG_NOTICE, cycle->log, 0, "reopening logs");
            nt_reopen_files(cycle, (nt_uid_t) -1);
        }
    }
}


static void nt_quit_sub_process(  nt_cycle_t *cycle, int signo){
	
	int i = 0;
	for (i = 0; i < nt_last_process; i++) {
		nt_processes[i].status = 0;
		nt_processes[i].exited = 1;      //退出状态
		nt_processes[i].exiting = 0;


		if (kill(nt_processes[i].pid, signo) == -1) {
			//			err = nt_errno;
			//			nt_log_error(NT_LOG_ALERT, cycle->log, err,
			//					"kill(%P, %d) failed", nt_processes[i].pid, signo);

			nt_processes[i].exited = 1;
			nt_processes[i].exiting = 0;
			//		nt_reap = 1;
		}
		nt_processes[i].pid = -1;
	}	

}

/*
	开启worker进程
*/
static void
nt_start_worker_processes(nt_cycle_t *cycle, nt_int_t n, nt_int_t type)
{
//    return ;
    nt_int_t      i;
    nt_channel_t  ch;

    nt_log_error(NT_LOG_NOTICE, cycle->log, 0, "start worker processes");

    nt_memzero(&ch, sizeof(nt_channel_t));

	//传递给其他worker子进程的命令，打开通信管道
    ch.command = NT_CMD_OPEN_CHANNEL;

	//创建n个worker子进程
	for (i = 0; i < n; i++) {
		debug( "nt_start_worker_processes for i=%d", i);
		//nt_spawn_process创建worker子进程并初始化相关资源和属性
		//然后执行子进程的执行函数nt_worker_process_cycle
		nt_spawn_process(cycle, nt_worker_process_cycle,
						  (void *) (intptr_t) i, "proxy", type);

		//向已经创建的worker进程广播当前创建worker进程信息。
		ch.pid = nt_processes[nt_process_slot].pid;
		ch.slot = nt_process_slot;
        ch.fd = nt_processes[nt_process_slot].channel[0];

        nt_pass_open_channel(cycle, &ch);
    }
}


static void
nt_start_cache_manager_processes(nt_cycle_t *cycle, nt_uint_t respawn)
{
    nt_uint_t       i, manager, loader;
    nt_path_t     **path;
    nt_channel_t    ch;

    manager = 0;
    loader = 0;

    path = nt_cycle->paths.elts;
    for (i = 0; i < nt_cycle->paths.nelts; i++) {

        if (path[i]->manager) {
            manager = 1;
        }

        if (path[i]->loader) {
            loader = 1;
        }
    }

    if (manager == 0) {
        return;
    }

    nt_spawn_process(cycle, nt_cache_manager_process_cycle,
                      &nt_cache_manager_ctx, "cache manager process",
                      respawn ? NT_PROCESS_JUST_RESPAWN : NT_PROCESS_RESPAWN);

    nt_memzero(&ch, sizeof(nt_channel_t));

    ch.command = NT_CMD_OPEN_CHANNEL;
    ch.pid = nt_processes[nt_process_slot].pid;
    ch.slot = nt_process_slot;
    ch.fd = nt_processes[nt_process_slot].channel[0];

    nt_pass_open_channel(cycle, &ch);

    if (loader == 0) {
        return;
    }

    nt_spawn_process(cycle, nt_cache_manager_process_cycle,
                      &nt_cache_loader_ctx, "cache loader process",
                      respawn ? NT_PROCESS_JUST_SPAWN : NT_PROCESS_NORESPAWN);

    ch.command = NT_CMD_OPEN_CHANNEL;
    ch.pid = nt_processes[nt_process_slot].pid;
    ch.slot = nt_process_slot;
    ch.fd = nt_processes[nt_process_slot].channel[0];

    nt_pass_open_channel(cycle, &ch);
}


static void
nt_pass_open_channel(nt_cycle_t *cycle, nt_channel_t *ch)
{
	nt_int_t  i;

	for (i = 0; i < nt_last_process; i++) {

		//跳过自己和异常的worker
		if (i == nt_process_slot
			|| nt_processes[i].pid == -1
			|| nt_processes[i].channel[0] == -1)
		{
			continue;
		}

		nt_log_debug6(NT_LOG_DEBUG_CORE, cycle->log, 0,
					   "pass channel s:%i pid:%P fd:%d to s:%i pid:%P fd:%d",
					   ch->slot, ch->pid, ch->fd,
					   i, nt_processes[i].pid,
					   nt_processes[i].channel[0]);

		/* TODO: NT_AGAIN */
		//发送消息给其他的worker
		nt_write_channel(nt_processes[i].channel[0],
						  ch, sizeof(nt_channel_t), cycle->log);
	}
}


static void
nt_signal_worker_processes(nt_cycle_t *cycle, int signo)
{
    nt_int_t      i;
    nt_err_t      err;
    nt_channel_t  ch;

    nt_memzero(&ch, sizeof(nt_channel_t));

#if (NT_BROKEN_SCM_RIGHTS)

    ch.command = 0;

#else

    switch (signo) {

    case nt_signal_value(NT_SHUTDOWN_SIGNAL):
        ch.command = NT_CMD_QUIT;
        break;

    case nt_signal_value(NT_TERMINATE_SIGNAL):
        ch.command = NT_CMD_TERMINATE;
        break;

    case nt_signal_value(NT_REOPEN_SIGNAL):
        ch.command = NT_CMD_REOPEN;
        break;

    default:
        ch.command = 0;
    }

#endif

    ch.fd = -1;


    for (i = 0; i < nt_last_process; i++) {

        nt_log_debug7(NT_LOG_DEBUG_EVENT, cycle->log, 0,
                       "child: %i %P e:%d t:%d d:%d r:%d j:%d",
                       i,
                       nt_processes[i].pid,
                       nt_processes[i].exiting,
                       nt_processes[i].exited,
                       nt_processes[i].detached,
                       nt_processes[i].respawn,
                       nt_processes[i].just_spawn);

        if (nt_processes[i].detached || nt_processes[i].pid == -1) {
            continue;
        }

        if (nt_processes[i].just_spawn) {
            nt_processes[i].just_spawn = 0;
            continue;
        }

        if (nt_processes[i].exiting
            && signo == nt_signal_value(NT_SHUTDOWN_SIGNAL))
        {
            continue;
        }

        if (ch.command) {
            if (nt_write_channel(nt_processes[i].channel[0],
                                  &ch, sizeof(nt_channel_t), cycle->log)
                == NT_OK)
            {
                if (signo != nt_signal_value(NT_REOPEN_SIGNAL)) {
                    nt_processes[i].exiting = 1;
                }

                continue;
            }
        }

        nt_log_debug2(NT_LOG_DEBUG_CORE, cycle->log, 0,
                       "kill (%P, %d)", nt_processes[i].pid, signo);

        if (kill(nt_processes[i].pid, signo) == -1) {
            err = nt_errno;
            nt_log_error(NT_LOG_ALERT, cycle->log, err,
                          "kill(%P, %d) failed", nt_processes[i].pid, signo);

            if (err == NT_ESRCH) {
                nt_processes[i].exited = 1;
                nt_processes[i].exiting = 0;
                nt_reap = 1;
            }

            continue;
        }

        if (signo != nt_signal_value(NT_REOPEN_SIGNAL)) {
            nt_processes[i].exiting = 1;
        }
    }
}

//有子进程意外结束，这时需要监控所有子进程
static nt_uint_t
nt_reap_children(nt_cycle_t *cycle)
{
    nt_int_t         i, n;
    nt_uint_t        live;
    nt_channel_t     ch;
    nt_core_conf_t  *ccf;

    nt_memzero(&ch, sizeof(nt_channel_t));

    ch.command = NT_CMD_CLOSE_CHANNEL;
    ch.fd = -1;

    live = 0;
    for (i = 0; i < nt_last_process; i++) {

	    debug( "child: %i %P e:%d t:%d d:%d r:%d j:%d", 
			    i,
			    nt_processes[i].pid,
			    nt_processes[i].exiting,
			    nt_processes[i].exited,
			    nt_processes[i].detached,
			    nt_processes[i].respawn,
			    nt_processes[i].just_spawn
		 );
        nt_log_debug7(NT_LOG_DEBUG_EVENT, cycle->log, 0,
                       "child: %i %P e:%d t:%d d:%d r:%d j:%d",
                       i,
                       nt_processes[i].pid,
                       nt_processes[i].exiting,
                       nt_processes[i].exited,
                       nt_processes[i].detached,
                       nt_processes[i].respawn,
                       nt_processes[i].just_spawn);

        if (nt_processes[i].pid == -1) {
            continue;
        }

        if (nt_processes[i].exited) {

            if (!nt_processes[i].detached) {
                nt_close_channel(nt_processes[i].channel, cycle->log);

                nt_processes[i].channel[0] = -1;
                nt_processes[i].channel[1] = -1;

                ch.pid = nt_processes[i].pid;
                ch.slot = i;

                for (n = 0; n < nt_last_process; n++) {
                    if (nt_processes[n].exited
                        || nt_processes[n].pid == -1
                        || nt_processes[n].channel[0] == -1)
                    {
                        continue;
                    }

                    nt_log_debug3(NT_LOG_DEBUG_CORE, cycle->log, 0,
                                   "pass close channel s:%i pid:%P to:%P",
                                   ch.slot, ch.pid, nt_processes[n].pid);

                    /* TODO: NT_AGAIN */

                    nt_write_channel(nt_processes[n].channel[0],
                                      &ch, sizeof(nt_channel_t), cycle->log);
                }
            }

            if (nt_processes[i].respawn
                && !nt_processes[i].exiting
                && !nt_terminate
                && !nt_quit)
            {

#if (NT_SSL && NT_SSL_ASYNC)
                /* Delay added to give Quickassist Driver time to cleanup
                * if worker exit with non-zero code. */
                if(nt_processes[i].status != 0) {
                    usleep(2000000);
                }
#endif
                if (nt_spawn_process(cycle, nt_processes[i].proc,
                                      nt_processes[i].data,
                                      nt_processes[i].name, i)
                    == NT_INVALID_PID)
                {
                    nt_log_error(NT_LOG_ALERT, cycle->log, 0,
                                  "could not respawn %s",
                                  nt_processes[i].name);
                    continue;
                }


                ch.command = NT_CMD_OPEN_CHANNEL;
                ch.pid = nt_processes[nt_process_slot].pid;
                ch.slot = nt_process_slot;
                ch.fd = nt_processes[nt_process_slot].channel[0];

                nt_pass_open_channel(cycle, &ch);
#if (T_PIPES)
                live |= 1;
#else
                live = 1;
#endif

                continue;
            }

            if (nt_processes[i].pid == nt_new_binary) {

                ccf = (nt_core_conf_t *) nt_get_conf(cycle->conf_ctx,
                                                       nt_core_module);

                if (nt_rename_file((char *) ccf->oldpid.data,
                                    (char *) ccf->pid.data)
                    == NT_FILE_ERROR)
                {
                    nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                                  nt_rename_file_n " %s back to %s failed "
                                  "after the new binary process \"%s\" exited",
                                  ccf->oldpid.data, ccf->pid.data, nt_argv[0]);
                }

                nt_new_binary = 0;
                if (nt_noaccepting) {
                    nt_restart = 1;
                    nt_noaccepting = 0;
                }
            }

            if (i == nt_last_process - 1) {
                nt_last_process--;

            } else {
                nt_processes[i].pid = -1;
            }

        } else if (nt_processes[i].exiting || !nt_processes[i].detached) {
#if (T_PIPES)
            live |= 1;

            if (nt_processes[i].exiting) {
                live |= NT_PIPE_STILL_NEEDED;
            }
#else
            live = 1;
#endif
        }
    }

    return live;
}


static void
#ifdef NT_HAVE_FUNJSQ
nt_master_process_exit(  struct F_hsearch_data *tab ,nt_cycle_t *cycle)
#else
nt_master_process_exit(  nt_cycle_t *cycle )
#endif
{
#ifdef NT_HAVE_FUNJSQ
    check_dl_rule( NULL , 0);
#endif

    debug( "start " );

    nt_uint_t  i;

    nt_delete_pidfile(cycle);

    nt_log_error(NT_LOG_NOTICE, cycle->log, 0, "exit");

    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->exit_master) {
            cycle->modules[i]->exit_master(cycle);
        }
    }

    nt_close_listening_sockets(cycle);

    /*
     * Copy nt_cycle->log related data to the special static exit cycle,
     * log, and log file structures enough to allow a signal handler to log.
     * The handler may be called when standard nt_cycle->log allocated from
     * nt_cycle->pool is already destroyed.
     */


    nt_exit_log = *nt_log_get_file_log(nt_cycle->log);

    nt_exit_log_file.fd = nt_exit_log.file->fd;
    nt_exit_log.file = &nt_exit_log_file;
    nt_exit_log.next = NULL;
    nt_exit_log.writer = NULL;

    nt_exit_cycle.log = &nt_exit_log;
    nt_exit_cycle.files = nt_cycle->files;
    nt_exit_cycle.files_n = nt_cycle->files_n;
    nt_cycle = &nt_exit_cycle;

    nt_destroy_pool(cycle->pool);
#if (T_PIPES)
    /* Terminate all pipe processes */

    nt_increase_pipe_generation();
    nt_close_old_pipes();
#endif

#ifdef NT_HAVE_FUNJSQ
    hash_free( &tab);
#endif

    nt_destroy_pool( process_pool );

    exit(0);
}

/*
 *  worker子进程 大循环位置
 *  所谓惊群问题，就是指的像Nginx这种多进程的服务器，在fork后同时监听同一个端口时，如果有一个外部连接进来，会导致所有休眠的子进程被唤醒，而最终只有一个子进程能够成功处理accept事件，其他进程都会重新进入休眠中。这就导致出现了很多不必要的schedule和上下文切换，而这些开销是完全不必要的。
 *  accept锁就是nginx的解决方案，本质上这是一个跨进程的互斥锁，以这个互斥锁来保证只有一个进程具备监听accept事件的能力。
 *  要解决epoll带来的accept锁的问题也很简单，只需要保证同一时间只有一个进程注册了accept的epoll事件即可。
 *  Nginx采用了将事件处理延后的方式。即在ngx_process_events的处理中，仅仅将事件放入两个队列中：
 *  即ngx_process_events仅对epoll_wait进行处理，事件的消费则放到accept锁释放之后，来最大限度地缩短占有accept的时间，来让其他进程也有足够的时机处理accept事件。
 *  https://blog.csdn.net/weixin_44400506/article/details/86231334
 * */
static void
nt_worker_process_cycle(nt_cycle_t *cycle, void *data)
{

    debug( "start");
    nt_int_t worker = (intptr_t) data;

    //在master中，nt_process被设置为NT_PROCESS_MASTER
    nt_process = NT_PROCESS_WORKER;
    nt_worker = worker;

    //初始化
    nt_worker_process_init(cycle, worker);

    //设置进程名
    nt_setproctitle("proxy");

    for ( ;; ) {

        //如果进程退出,关闭所有连接
        if (nt_exiting) {
            if (nt_event_no_timers_left() == NT_OK) {
                nt_log_error(NT_LOG_NOTICE, cycle->log, 0, "exiting");
                nt_worker_process_exit(cycle);
            }
        }

        nt_log_debug0(NT_LOG_DEBUG_EVENT, cycle->log, 0, "worker cycle");

        //处理时间和计时
        nt_process_events_and_timers(cycle);

        //收到NT_CMD_TERMINATE命令
        if (nt_terminate) {
            nt_log_error(NT_LOG_NOTICE, cycle->log, 0, "exiting");
            //清理后进程退出，会调用所有模块的钩子exit_process
            nt_worker_process_exit(cycle);
        }

        //收到NT_CMD_QUIT命令
        if (nt_quit) {
            nt_quit = 0;
            nt_log_error(NT_LOG_NOTICE, cycle->log, 0,
                          "gracefully shutting down");
            nt_setproctitle("worker process is shutting down");

             //如果进程没有"正在退出"
            if (!nt_exiting) {
                //关闭监听socket，设置退出正在状态
                nt_exiting = 1;
                nt_set_shutdown_timer(cycle);
                nt_close_listening_sockets(cycle);
                nt_close_idle_connections(cycle);
            }
        }

        //收到NT_CMD_REOPEN命令，重新打开log
        if (nt_reopen) {
            nt_reopen = 0;
            nt_log_error(NT_LOG_NOTICE, cycle->log, 0, "reopening logs");
            nt_reopen_files(cycle, -1);
        }
    }
}


static void
nt_worker_process_init(nt_cycle_t *cycle, nt_int_t worker)
{
    sigset_t          set;
    nt_int_t         n;
    nt_time_t       *tp;
    nt_uint_t        i;
    nt_cpuset_t     *cpu_affinity;
    struct rlimit     rlmt;
    nt_core_conf_t  *ccf;
    nt_listening_t  *ls;

    // 1.全局性的设置，根据全局的配置信息设置执行环境、优先级、限制、setgid、setuid、信号初始化等；
    if (nt_set_environment(cycle, NULL) == NULL) {
        /* fatal */
        exit(2);
    }

    ccf = (nt_core_conf_t *) nt_get_conf(cycle->conf_ctx, nt_core_module);

    if (worker >= 0 && ccf->priority != 0) {
        if (setpriority(PRIO_PROCESS, 0, ccf->priority) == -1) {
            nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                          "setpriority(%d) failed", ccf->priority);
        }
    }

    if (ccf->rlimit_nofile != NT_CONF_UNSET) {
        rlmt.rlim_cur = (rlim_t) ccf->rlimit_nofile;
        rlmt.rlim_max = (rlim_t) ccf->rlimit_nofile;

        if (setrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
            nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                          "setrlimit(RLIMIT_NOFILE, %i) failed",
                          ccf->rlimit_nofile);
        }
    }

    if (ccf->rlimit_core != NT_CONF_UNSET) {
        rlmt.rlim_cur = (rlim_t) ccf->rlimit_core;
        rlmt.rlim_max = (rlim_t) ccf->rlimit_core;

        if (setrlimit(RLIMIT_CORE, &rlmt) == -1) {
            nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                          "setrlimit(RLIMIT_CORE, %O) failed",
                          ccf->rlimit_core);
        }
    }

    if (geteuid() == 0) {
        if (setgid(ccf->group) == -1) {
            nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                          "setgid(%d) failed", ccf->group);
            /* fatal */
            exit(2);
        }

        if (initgroups(ccf->username, ccf->group) == -1) {
            nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                          "initgroups(%s, %d) failed",
                          ccf->username, ccf->group);
        }

#if (NT_HAVE_PR_SET_KEEPCAPS && NT_HAVE_CAPABILITIES)
        if (ccf->transparent && ccf->user) {
            if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) == -1) {
                nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                              "prctl(PR_SET_KEEPCAPS, 1) failed");
                /* fatal */
                exit(2);
            }
        }
#endif

        if (setuid(ccf->user) == -1) {
            nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                          "setuid(%d) failed", ccf->user);
            /* fatal */
            exit(2);
        }

#if (NT_HAVE_CAPABILITIES)
        if (ccf->transparent && ccf->user) {
            struct __user_cap_data_struct    data;
            struct __user_cap_header_struct  header;

            nt_memzero(&header, sizeof(struct __user_cap_header_struct));
            nt_memzero(&data, sizeof(struct __user_cap_data_struct));

            header.version = _LINUX_CAPABILITY_VERSION_1;
            data.effective = CAP_TO_MASK(CAP_NET_RAW);
            data.permitted = data.effective;

            if (syscall(SYS_capset, &header, &data) == -1) {
                nt_log_error(NT_LOG_EMERG, cycle->log, nt_errno,
                              "capset() failed");
                /* fatal */
                exit(2);
            }
        }
#endif
    }

    if (worker >= 0) {
        cpu_affinity = nt_get_cpu_affinity(worker);

        if (cpu_affinity) {
            nt_setaffinity(cpu_affinity, cycle->log);
        }
    }

#if (NT_HAVE_PR_SET_DUMPABLE)

    /* allow coredump after setuid() in Linux 2.4.x */

    if (prctl(PR_SET_DUMPABLE, 1, 0, 0, 0) == -1) {
        nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                      "prctl(PR_SET_DUMPABLE) failed");
    }

#endif

    if (ccf->working_directory.len) {
        if (chdir((char *) ccf->working_directory.data) == -1) {
            nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                          "chdir(\"%s\") failed", ccf->working_directory.data);
            /* fatal */
            exit(2);
        }
    }

    sigemptyset(&set);

    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
        nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                      "sigprocmask() failed");
    }

    tp = nt_timeofday();
    srandom(((unsigned) nt_pid << 16) ^ tp->sec ^ tp->msec);

    /*
     * disable deleting previous events for the listening sockets because
     * in the worker processes there are no events at all at this point
     */
    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {
        ls[i].previous = NULL;
    }

    //2.调用所有模块的钩子init_process；
    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->init_process) {
            if (cycle->modules[i]->init_process(cycle) == NT_ERROR) {
                /* fatal */
                exit(2);
            }
        }
    }

   /* 3.关闭不使用的socket，关闭当前worker的channel[0]句柄和其他worker的channel[1]句柄，
    * 当前worker会使用其他worker的channel[0]句柄发送消息，
    * 使用当前worker的channel[1]句柄监听可读事件： 
    * */
    for (n = 0; n < nt_last_process; n++) {

        if (nt_processes[n].pid == -1) {
            continue;
        }

        if (n == nt_process_slot) {
            continue;
        }

        if (nt_processes[n].channel[1] == -1) {
            continue;
        }

        //关闭所有其他worker进程channel[1]句柄(用于监听)
        if (close(nt_processes[n].channel[1]) == -1) {
            nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                          "close() channel failed");
        }
    }

    //关闭自己的channel[0]句柄(用于发送信息)
    if (close(nt_processes[nt_process_slot].channel[0]) == -1) {
        nt_log_error(NT_LOG_ALERT, cycle->log, nt_errno,
                      "close() channel failed");
    }

    //这也就是为什么，用其他worker的channel[0]句柄发消息
    //用当前的worker的channel[1]句柄监听可读时间
#if 0
    nt_last_process = 0;
#endif

    /* 4.在当前worker的channel[1]句柄监听可读事件：
     * nt_add_channel_event把句柄nt_channel(当前worker的channel[1])上建立的连接
     * 的可读事件加入事件监控队列，事件处理函数为nt_channel_hanlder(nt_event_t *ev)。
     * 当有可读事件的时候，nt_channel_handler负责处理消息
     * */
    if (nt_add_channel_event(cycle, nt_channel, NT_READ_EVENT,
                              nt_channel_handler)
        == NT_ERROR)
    {
        /* fatal */
        exit(2);
    }
}


static void
nt_worker_process_exit(nt_cycle_t *cycle)
{
    nt_uint_t         i;
    nt_connection_t  *c;

    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->exit_process) {
            cycle->modules[i]->exit_process(cycle);
        }
    }

    if (nt_exiting) {
        c = cycle->connections;
        for (i = 0; i < cycle->connection_n; i++) {
            if (c[i].fd != -1
                && c[i].read
                && !c[i].read->accept
                && !c[i].read->channel
                && !c[i].read->resolver)
            {
                nt_log_error(NT_LOG_ALERT, cycle->log, 0,
                              "*%uA open socket #%d left in connection %ui",
                              c[i].number, c[i].fd, i);
                nt_debug_quit = 1;
            }
        }

        if (nt_debug_quit) {
            nt_log_error(NT_LOG_ALERT, cycle->log, 0, "aborting");
            nt_debug_point();
        }
    }

    /*
     * Copy nt_cycle->log related data to the special static exit cycle,
     * log, and log file structures enough to allow a signal handler to log.
     * The handler may be called when standard nt_cycle->log allocated from
     * nt_cycle->pool is already destroyed.
     */

    nt_exit_log = *nt_log_get_file_log(nt_cycle->log);

    nt_exit_log_file.fd = nt_exit_log.file->fd;
    nt_exit_log.file = &nt_exit_log_file;
    nt_exit_log.next = NULL;
    nt_exit_log.writer = NULL;

    nt_exit_cycle.log = &nt_exit_log;
    nt_exit_cycle.files = nt_cycle->files;
    nt_exit_cycle.files_n = nt_cycle->files_n;
    nt_cycle = &nt_exit_cycle;

    nt_destroy_pool(cycle->pool);

    nt_log_error(NT_LOG_NOTICE, nt_cycle->log, 0, "exit");

    exit(0);
}


static void
nt_channel_handler(nt_event_t *ev)
{
    nt_int_t          n;
    nt_channel_t      ch;
    nt_connection_t  *c;

    if (ev->timedout) {
        ev->timedout = 0;
        return;
    }

    c = ev->data;

    nt_log_debug0(NT_LOG_DEBUG_CORE, ev->log, 0, "channel handler");

    for ( ;; ) {

        n = nt_read_channel(c->fd, &ch, sizeof(nt_channel_t), ev->log);

        nt_log_debug1(NT_LOG_DEBUG_CORE, ev->log, 0, "channel: %i", n);

        if (n == NT_ERROR) {

            if (nt_event_flags & NT_USE_EPOLL_EVENT) {
#if (NT_HTTP_SSL && NT_SSL_ASYNC)
            if (c->async_enable && nt_del_async_conn) {
                if (c->num_async_fds) {
                    nt_del_async_conn(c, NT_DISABLE_EVENT);
                    c->num_async_fds--;
                }
            }
#endif
                nt_del_conn(c, 0);
            }

            nt_close_connection(c);
            return;
        }

        if (nt_event_flags & NT_USE_EVENTPORT_EVENT) {
            if (nt_add_event(ev, NT_READ_EVENT, 0) == NT_ERROR) {
                return;
            }
        }

        if (n == NT_AGAIN) {
            return;
        }

        nt_log_debug1(NT_LOG_DEBUG_CORE, ev->log, 0,
                       "channel command: %ui", ch.command);

        switch (ch.command) {

        case NT_CMD_QUIT:
            nt_quit = 1;
            break;

        case NT_CMD_TERMINATE:
            nt_terminate = 1;
            break;

        case NT_CMD_REOPEN:
            nt_reopen = 1;
            break;

        case NT_CMD_OPEN_CHANNEL:

            nt_log_debug3(NT_LOG_DEBUG_CORE, ev->log, 0,
                           "get channel s:%i pid:%P fd:%d",
                           ch.slot, ch.pid, ch.fd);

            nt_processes[ch.slot].pid = ch.pid;
            nt_processes[ch.slot].channel[0] = ch.fd;
            break;

        case NT_CMD_CLOSE_CHANNEL:

            nt_log_debug4(NT_LOG_DEBUG_CORE, ev->log, 0,
                           "close channel s:%i pid:%P our:%P fd:%d",
                           ch.slot, ch.pid, nt_processes[ch.slot].pid,
                           nt_processes[ch.slot].channel[0]);

            if (close(nt_processes[ch.slot].channel[0]) == -1) {
                nt_log_error(NT_LOG_ALERT, ev->log, nt_errno,
                              "close() channel failed");
            }

            nt_processes[ch.slot].channel[0] = -1;
            break;

#if (T_PIPES)
        case NT_CMD_PIPE_BROKEN:
            nt_pipe_broken_action(ev->log, ch.pid, 0);
            break;
#endif
        }
    }
}


static void
nt_cache_manager_process_cycle(nt_cycle_t *cycle, void *data)
{
    nt_cache_manager_ctx_t *ctx = data;

    void         *ident[4];
    nt_event_t   ev;

    /*
     * Set correct process type since closing listening Unix domain socket
     * in a master process also removes the Unix domain socket file.
     */
    nt_process = NT_PROCESS_HELPER;

    nt_close_listening_sockets(cycle);

    /* Set a moderate number of connections for a helper process. */
    cycle->connection_n = 512;

    nt_worker_process_init(cycle, -1);

    nt_memzero(&ev, sizeof(nt_event_t));
    ev.handler = ctx->handler;
    ev.data = ident;
    ev.log = cycle->log;
    ident[3] = (void *) -1;

    nt_use_accept_mutex = 0;

    nt_setproctitle(ctx->name);

    nt_add_timer(&ev, ctx->delay);

    for ( ;; ) {

        if (nt_terminate || nt_quit) {
            nt_log_error(NT_LOG_NOTICE, cycle->log, 0, "exiting");
            exit(0);
        }

        if (nt_reopen) {
            nt_reopen = 0;
            nt_log_error(NT_LOG_NOTICE, cycle->log, 0, "reopening logs");
            nt_reopen_files(cycle, -1);
        }

        nt_process_events_and_timers(cycle);
    }
}


static void
nt_cache_manager_process_handler(nt_event_t *ev)
{
    nt_uint_t    i;
    nt_msec_t    next, n;
    nt_path_t  **path;

    next = 60 * 60 * 1000;

    path = nt_cycle->paths.elts;
    for (i = 0; i < nt_cycle->paths.nelts; i++) {

        if (path[i]->manager) {
            n = path[i]->manager(path[i]->data);

            next = (n <= next) ? n : next;

            nt_time_update();
        }
    }

    if (next == 0) {
        next = 1;
    }

    nt_add_timer(ev, next);
}


static void
nt_cache_loader_process_handler(nt_event_t *ev)
{
    nt_uint_t     i;
    nt_path_t   **path;
    nt_cycle_t   *cycle;

    cycle = (nt_cycle_t *) nt_cycle;

    path = cycle->paths.elts;
    for (i = 0; i < cycle->paths.nelts; i++) {

        if (nt_terminate || nt_quit) {
            break;
        }

        if (path[i]->loader) {
            path[i]->loader(path[i]->data);
            nt_time_update();
        }
    }

    exit(0);
}
