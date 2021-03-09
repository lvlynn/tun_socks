

#ifndef _NT_EVENT_H_INCLUDED_
#define _NT_EVENT_H_INCLUDED_


#include <nt_core.h>


#define NT_INVALID_INDEX  0xd0d0d0d0


#if (NT_HAVE_IOCP)

typedef struct {
    WSAOVERLAPPED    ovlp;
    nt_event_t     *event;
    int              error;
} nt_event_ovlp_t;

#endif


struct nt_event_s {
    /*
     *     事件相关的对象。通常data都是指向nt_connection_t连接对象,见nt_get_connection。
     *     开启文件异步I/O时，它可能会指向nt_event_aio_t(nt_file_aio_init)结构体
     **/
    void            *data;

    //标志位，为1时表示事件是可写的。通常情况下，它表示对应的TCP连接目前状态是可写的，也就是连接处于可以发送网络包的状态
    unsigned         write:1;  //见nt_get_connection，可写事件ev默认为1  读ev事件应该默认还是0


#if (NT_SSL && NT_SSL_ASYNC)
    unsigned         async:1;
#endif

    /* 标志位，为1时表示为此事件可以建立新的连接。
     * 通常情况下，在nt_cycle_t中的listening动态数组中，
     * 每一个监听对象nt_listening_t对应的读事件中的accept标志位才会是1  nt_event_process_init中置1 */
    unsigned         accept:1;

	/*
	   		这个标志位用于区分当前事件是否是过期的，它仅仅是给事件驱动模块使用的，而事件消费模块可不用关心。为什么需要这个标志位呢？
	   当开始处理一批事件时，处理前面的事件可能会关闭一些连接，而这些连接有可能影响这批事件中还未处理到的后面的事件。这时，
	   可通过instance标志位来避免处理后面的已经过期的事件。将详细描述nt_epoll_module是如何使用instance标志位区分
	   过期事件的，这是一个巧妙的设计方法

	   		instance标志位为什么可以判断事件是否过期？instance标志位的使用其实很简单，它利用了指针的最后一位一定
	   是0这一特性。既然最后一位始终都是0，那么不如用来表示instance。这样，在使用nt_epoll_add_event方法向epoll中添加事件时，就把epoll_event中
	   联合成员data的ptr成员指向nt_connection_t连接的地址，同时把最后一位置为这个事件的instance标志。而在nt_epoll_process_events方法中取出指向连接的
	   ptr地址时，先把最后一位instance取出来，再把ptr还原成正常的地址赋给nt_connection_t连接。这样，instance究竟放在何处的问题也就解决了。
	   那么，过期事件又是怎么回事呢？举个例子，假设epoll_wait -次返回3个事件，在第
	   1个事件的处理过程中，由于业务的需要，所以关闭了一个连接，而这个连接恰好对应第3个事件。这样的话，在处理到第3个事件时，这个事件就
	   已经是过期辜件了，一旦处理必然出错。既然如此，把关闭的这个连接的fd套接字置为一1能解决问题吗？答案是不能处理所有情况。
	   下面先来看看这种貌似不可能发生的场景到底是怎么发生的：假设第3个事件对应的nt_connection_t连接中的fd套接字原先是50，处理第1个事件
	   时把这个连接的套接字关闭了，同时置为一1，并且调用nt_free_connection将该连接归还给连接池。在nt_epoll_process_events方法的循环中开始处
	   理第2个事件，恰好第2个事件是建立新连接事件，调用nt_get_connection从连接池中取出的连接非常可能就是刚刚释放的第3个事件对应的连接。由于套
	   接字50刚刚被释放，Linux内核非常有可能把刚刚释放的套接字50又分配给新建立的连接。因此，在循环中处理第3个事件时，这个事件就是过期的了！它对应
	   的事件是关闭的连接，而不是新建立的连接。
	   如何解决这个问题？依靠instance标志位。当调用nt_get_connection从连接池中获取一个新连接时，instance标志位就会置反
	   */
	/* used to detect the stale events in kqueue and epoll */
	unsigned         instance:1;

	/*
	 * the event was passed or would be passed to a kernel;
	 * in aio mode - operation was posted.
	 */
	/* 
     * 标志位，为1时表示当前事件是活跃的，为0时表示事件是不活跃的。这个状态对应着事件驱动模块处理方式的不同。例如，在添加事件、
     * 删除事件和处理事件时，active标志位的不同都会对应着不同的处理方式。在使用事件时，一般不会直接改变active标志位
     * nt_epoll_add_event中也会置1  在调用该函数后，该值一直为1，除非调用nt_epoll_del_event
	 * 标记是否已经添加到事件驱动中，避免重复添加  在server端accept成功后，
	 * 或者在client端connect的时候把active置1，见nt_epoll_add_connection。第一次添加epoll_ctl为EPOLL_CTL_ADD,如果再次添加发
	 * 现active为1,则epoll_ctl为EPOLL_CTL_MOD
	 *  */
	unsigned         active:1;

    /*
     *     标志位，为1时表示禁用事件，仅在kqueue或者rtsig事件驱动模块中有效，而对于epoll事件驱动模块则无意义，这里不再详述
     **/
    unsigned         disabled:1;

    /* the ready event; in aio mode 0 means that no operation can be posted */

    /* 
     *  标志位，为1时表示当前事件已经淮备就绪，也就是说，允许这个事件的消费模块处理这个事件。在
     *  HTTP框架中，经常会检查事件的ready标志位以确定是否可以接收请求或者发送响应
     *  ready标志位，如果为1，则表示在与客户端的TCP连接上可以发送数据；如果为0，则表示暂不可发送数据。
     *  如果来自对端的数据内核缓冲区没有数据(返回NT_EAGAIN)，或者连接断开置0，见nt_unix_recv
     *  在发送数据的时候，nt_unix_send中的时候，如果希望发送1000字节，
     *  但是实际上send只返回了500字节(说明内核协议栈缓冲区满，需要通过epoll再次促发write的时候才能写)，
     *  或者链接异常，则把ready置0 
     *  */
    unsigned         ready:1;

    /*
     *     该标志位仅对kqueue，eventport等模块有意义，而对于Linux上的epoll事件驱动模块则是无意叉的，限于篇幅，不再详细说明
     **/
    unsigned         oneshot:1;

    /* aio operation is complete */
    //aio on | thread_pool方式下，如果读取数据完成，
    //则在nt_epoll_eventfd_handler(aio on)或者nt_thread_pool_handler(aio thread_pool)中置1
    //表示读取数据完成，通过epoll机制返回获取 ，见nt_epoll_eventfd_handler
    unsigned         complete:1; 

    //标志位，为1时表示当前处理的字符流已经结束  例如内核缓冲区没有数据，你去读，则会返回0
    unsigned         eof:1; //见nt_unix_recv
    //标志位，为1时表示事件在处理过程中出现错误
    unsigned         error:1; 

	//标志位，为I时表示这个事件已经超时，用以提示事件的消费模块做超时处理
    /*读客户端连接的数据，在nt_http_init_connection(nt_connection_t *c)中的nt_add_timer(rev, c->listening->post_accept_timeout)把读事件添加到定时器中，如果超时则置1
      每次nt_unix_recv把内核数据读取完毕后，在重新启动add epoll，等待新的数据到来，同时会启动定时器nt_add_timer(rev, c->listening->post_accept_timeout);
      如果在post_accept_timeout这么长事件内没有数据到来则超时，开始处理关闭TCP流程*/

    /*
    读超时是指的读取对端数据的超时时间，写超时指的是当数据包很大的时候，write返回NT_AGAIN，则会添加write定时器，从而判断是否超时，如果发往
    对端数据长度小，则一般write直接返回成功，则不会添加write超时定时器，也就不会有write超时，写定时器参考函数nt_http_upstream_send_request
     */
    unsigned         timedout:1; //定时器超时标记，见nt_event_expire_timers

	//标志位，为1时表示这个事件存在于定时器中
    unsigned         timer_set:1; //nt_event_add_timer nt_add_timer 中置1   nt_event_expire_timers置0

	//标志位，delayed为1时表示需要延迟处理这个事件，它仅用于限速功能 
    unsigned         delayed:1; //限速见nt_http_write_filter 

	/*
     标志位，为1时表示延迟建立TCP连接，也就是说，经过TCP三次握手后并不建立连接，而是要等到真正收到数据包后才会建立TCP连接
     */
    unsigned         deferred_accept:1; //通过listen的时候添加 deferred 参数来确定

    /* the pending eof reported by kqueue, epoll or in aio chain operation */
    //标志位，为1时表示等待字符流结束，它只与kqueue和aio事件驱动机制有关
    //一般在触发EPOLLRDHUP(当对端已经关闭，本端写数据，会引起该事件)的时候，会置1，见nt_epoll_process_events
    unsigned         pending_eof:1;

    //删除post队列的时候需要检查
    //表示延迟处理该事件，见nt_epoll_process_events -> nt_post_event  标记是否在延迟队列里面
    unsigned         posted:1;

    //标志位，为1时表示当前事件已经关闭，epoll模块没有使用它
    unsigned         closed:1; //nt_close_connection中置1

    /* to test on worker exit */
    //这两个该标志位目前无实际意义
    unsigned         channel:1;
    unsigned         resolver:1;


//    计时器事件标志，指示在关闭工作人员时应忽略该事件。优雅的工作人员关闭被延迟，直到没有安排不可取消的计时器事件。
    unsigned         cancelable:1;

#if (NT_HAVE_KQUEUE)
    unsigned         kq_vnode:1;

    /* the pending errno reported by kqueue */
    int              kq_errno;
#endif

    /*
     * kqueue only:
     *   accept:     number of sockets that wait to be accepted
     *   read:       bytes to read when event is ready
     *               or lowat when event is set with NT_LOWAT_EVENT flag
     *   write:      available space in buffer when event is ready
     *               or lowat when event is set with NT_LOWAT_EVENT flag
     *
     * epoll with EPOLLRDHUP:
     *   accept:     1 if accept many, 0 otherwise
     *   read:       1 if there can be data to read, 0 otherwise
     *
     * iocp: TODO
     *
     * otherwise:
     *   accept:     1 if accept many, 0 otherwise
     */

//标志住，在epoll事件驱动机制下表示一次尽可能多地建立TCP连接，它与multi_accept配置项对应，实现原理基见9.8.1节
#if (NT_HAVE_KQUEUE) || (NT_HAVE_IOCP)
    int              available;
#else
    unsigned         available:1;
#endif

  /*
    每一个事件最核心的部分是handler回调方法，它将由每一个事件消费模块实现，以此决定这个事件究竟如何“消费”
     */

    /*
    1.event可以是普通的epoll读写事件(参考nt_event_connect_peer->nt_add_conn或者nt_add_event)，通过读写事件触发
    
    2.也可以是普通定时器事件(参考nt_cache_manager_process_handler->nt_add_timer(nt_event_add_timer))，通过nt_process_events_and_timers中的
    epoll_wait返回，可以是读写事件触发返回，也可能是因为没获取到共享锁，从而等待0.5s返回重新获取锁来跟新事件并执行超时事件来跟新事件并且判断定
    时器链表中的超时事件，超时则执行从而指向event的handler，然后进一步指向对应r或者u的->write_event_handler  read_event_handler
    
    3.也可以是利用定时器expirt实现的读写事件(参考nt_http_set_write_handler->nt_add_timer(nt_event_add_timer)),触发过程见2，只是在handler中不会执行write_event_handler  read_event_handler
    */
     
    //这个事件发生时的处理方法，每个事件消费模块都会重新实现它
    //nt_epoll_process_events中执行accept
    /*
     赋值为nt_http_process_request_line     nt_event_process_init中初始化为nt_event_accept  如果是文件异步i/o，赋值为nt_epoll_eventfd_handler
     //当accept客户端连接后nt_http_init_connection中赋值为nt_http_wait_request_handler来读取客户端数据  
     在解析完客户端发送来的请求的请求行和头部行后，设置handler为nt_http_request_handler
     */ //一般与客户端的数据读写是 nt_http_request_handler;  与后端服务器读写为nt_http_upstream_handler(如fastcgi proxy memcache gwgi等)
    
    /* nt_event_accept，nt_http_ssl_handshake nt_ssl_handshake_handler nt_http_v2_write_handler nt_http_v2_read_handler 
    nt_http_wait_request_handler  nt_http_request_handler,nt_http_upstream_handler nt_file_aio_event_handler */
    nt_event_handler_pt  handler;  //由epoll读写事件在nt_epoll_process_events触发
#if (NT_SSL && NT_SSL_ASYNC)
    nt_event_handler_pt  saved_handler;
#endif



#if (NT_HAVE_IOCP)
    nt_event_ovlp_t ovlp;
#endif

	//由于epoll事件驱动方式不使用index，所以这里不再说明
    nt_uint_t       index;

	//可用于记录error_log日志的nt_log_t对象
    nt_log_t       *log; //可以记录日志的nt_log_t对象 其实就是nt_listening_t中获取的log //赋值见nt_event_accept

    //定时器节点，用于定时器红黑树中
    nt_rbtree_node_t   timer;  //见nt_event_timer_rbtree

    /* the posted queue */
	 /*post事件将会构成一个队列再统一处理，这个队列以next和prev作为链表指针，以此构成一个简易的双向链表，其中next指向后一个事件的地址，
    prev指向前一个事件的地址
     */
    nt_queue_t      queue;

#if 0

    /* the threads support */

    /*
     * the event thread context, we store it here
     * if $(CC) does not understand __thread declaration
     * and pthread_getspecific() is too costly
     */

    void            *thr_ctx;

#if (NT_EVENT_T_PADDING)

    /* event should not cross cache line in SMP */

    uint32_t         padding[NT_EVENT_T_PADDING];
#endif
#endif
};


#if (NT_HAVE_FILE_AIO)

struct nt_event_aio_s {
    void                      *data;
    nt_event_handler_pt       handler;
    nt_file_t                *file;

    nt_fd_t                   fd;

#if (NT_HAVE_AIO_SENDFILE || NT_COMPAT)
    ssize_t                  (*preload_handler)(nt_buf_t *file);
#endif

#if (NT_HAVE_EVENTFD)
    int64_t                    res;
#endif

#if !(NT_HAVE_EVENTFD) || (NT_TEST_BUILD_EPOLL)
    nt_err_t                  err;
    size_t                     nbytes;
#endif

    nt_aiocb_t                aiocb;
    nt_event_t                event;
};

#endif


typedef struct {
    nt_int_t  (*add)(nt_event_t *ev, nt_int_t event, nt_uint_t flags);
    nt_int_t  (*del)(nt_event_t *ev, nt_int_t event, nt_uint_t flags);

    nt_int_t  (*enable)(nt_event_t *ev, nt_int_t event, nt_uint_t flags);
    nt_int_t  (*disable)(nt_event_t *ev, nt_int_t event, nt_uint_t flags);

    nt_int_t  (*add_conn)(nt_connection_t *c);
    nt_int_t  (*del_conn)(nt_connection_t *c, nt_uint_t flags);

    nt_int_t  (*notify)(nt_event_handler_pt handler);

    nt_int_t  (*process_events)(nt_cycle_t *cycle, nt_msec_t timer,
                                 nt_uint_t flags);

    nt_int_t  (*init)(nt_cycle_t *cycle, nt_msec_t timer);
    void       (*done)(nt_cycle_t *cycle);

#if (NT_SSL && NT_SSL_ASYNC)
    nt_int_t  (*add_async_conn)(nt_connection_t *c);
    nt_int_t  (*del_async_conn)(nt_connection_t *c, nt_uint_t flags);
#endif
} nt_event_actions_t;


extern nt_event_actions_t   nt_event_actions;
#if (NT_HAVE_EPOLLRDHUP)
extern nt_uint_t            nt_use_epoll_rdhup;
#endif
#if (T_NT_ACCEPT_FILTER)
typedef nt_int_t (*nt_event_accept_filter_pt) (nt_connection_t *c);
void nt_close_accepted_connection(nt_connection_t *c);
extern nt_event_accept_filter_pt nt_event_top_accept_filter;
#endif


/*
 * The event filter requires to read/write the whole data:
 * select, poll, /dev/poll, kqueue, epoll.
 */
#define NT_USE_LEVEL_EVENT      0x00000001

/*
 * The event filter is deleted after a notification without an additional
 * syscall: kqueue, epoll.
 */
#define NT_USE_ONESHOT_EVENT    0x00000002

/*
 * The event filter notifies only the changes and an initial level:
 * kqueue, epoll.
 */
#define NT_USE_CLEAR_EVENT      0x00000004

/*
 * The event filter has kqueue features: the eof flag, errno,
 * available data, etc.
 */
#define NT_USE_KQUEUE_EVENT     0x00000008

/*
 * The event filter supports low water mark: kqueue's NOTE_LOWAT.
 * kqueue in FreeBSD 4.1-4.2 has no NOTE_LOWAT so we need a separate flag.
 */
#define NT_USE_LOWAT_EVENT      0x00000010

/*
 * The event filter requires to do i/o operation until EAGAIN: epoll.
 */
#define NT_USE_GREEDY_EVENT     0x00000020

/*
 * The event filter is epoll.
 */
#define NT_USE_EPOLL_EVENT      0x00000040

/*
 * Obsolete.
 */
#define NT_USE_RTSIG_EVENT      0x00000080

/*
 * Obsolete.
 */
#define NT_USE_AIO_EVENT        0x00000100

/*
 * Need to add socket or handle only once: i/o completion port.
 */
#define NT_USE_IOCP_EVENT       0x00000200

/*
 * The event filter has no opaque data and requires file descriptors table:
 * poll, /dev/poll.
 */
#define NT_USE_FD_EVENT         0x00000400

/*
 * The event module handles periodic or absolute timer event by itself:
 * kqueue in FreeBSD 4.4, NetBSD 2.0, and MacOSX 10.4, Solaris 10's event ports.
 */
#define NT_USE_TIMER_EVENT      0x00000800

/*
 * All event filters on file descriptor are deleted after a notification:
 * Solaris 10's event ports.
 */
#define NT_USE_EVENTPORT_EVENT  0x00001000

/*
 * The event filter support vnode notifications: kqueue.
 */
#define NT_USE_VNODE_EVENT      0x00002000


/*
 * The event filter is deleted just before the closing file.
 * Has no meaning for select and poll.
 * kqueue, epoll, eventport:         allows to avoid explicit delete,
 *                                   because filter automatically is deleted
 *                                   on file close,
 *
 * /dev/poll:                        we need to flush POLLREMOVE event
 *                                   before closing file.
 */
#define NT_CLOSE_EVENT    1

/*
 * disable temporarily event filter, this may avoid locks
 * in kernel malloc()/free(): kqueue.
 */
#define NT_DISABLE_EVENT  2

/*
 * event must be passed to kernel right now, do not wait until batch processing.
 */
#define NT_FLUSH_EVENT    4


/* these flags have a meaning only for kqueue */
#define NT_LOWAT_EVENT    0
#define NT_VNODE_EVENT    0


#if (NT_HAVE_EPOLL) && !(NT_HAVE_EPOLLRDHUP)
#define EPOLLRDHUP         0
#endif


#if (NT_HAVE_KQUEUE)

#define NT_READ_EVENT     EVFILT_READ
#define NT_WRITE_EVENT    EVFILT_WRITE

#undef  NT_VNODE_EVENT
#define NT_VNODE_EVENT    EVFILT_VNODE

/*
 * NT_CLOSE_EVENT, NT_LOWAT_EVENT, and NT_FLUSH_EVENT are the module flags
 * and they must not go into a kernel so we need to choose the value
 * that must not interfere with any existent and future kqueue flags.
 * kqueue has such values - EV_FLAG1, EV_EOF, and EV_ERROR:
 * they are reserved and cleared on a kernel entrance.
 */
#undef  NT_CLOSE_EVENT
#define NT_CLOSE_EVENT    EV_EOF

#undef  NT_LOWAT_EVENT
#define NT_LOWAT_EVENT    EV_FLAG1

#undef  NT_FLUSH_EVENT
#define NT_FLUSH_EVENT    EV_ERROR

#define NT_LEVEL_EVENT    0
#define NT_ONESHOT_EVENT  EV_ONESHOT
#define NT_CLEAR_EVENT    EV_CLEAR

#undef  NT_DISABLE_EVENT
#define NT_DISABLE_EVENT  EV_DISABLE

//DEVPOLL
#elif (NT_HAVE_DEVPOLL && !(NT_TEST_BUILD_DEVPOLL)) \
      || (NT_HAVE_EVENTPORT && !(NT_TEST_BUILD_EVENTPORT))

#define NT_READ_EVENT     POLLIN
#define NT_WRITE_EVENT    POLLOUT

#define NT_LEVEL_EVENT    0
#define NT_ONESHOT_EVENT  1

//使用EPOLL 模式
#elif (NT_HAVE_EPOLL) && !(NT_TEST_BUILD_EPOLL)

#define NT_READ_EVENT     (EPOLLIN|EPOLLRDHUP)
#define NT_WRITE_EVENT    EPOLLOUT

#define NT_LEVEL_EVENT    0

#if 1
//nginx 的epoll 默认使用ET模式
#define NT_CLEAR_EVENT    EPOLLET  
#else
//测试改用LT模式
#define NT_CLEAR_EVENT   0 
#endif

#define NT_ONESHOT_EVENT  0x70000000
#if 0
#define NT_ONESHOT_EVENT  EPOLLONESHOT
#endif

#if (NT_HAVE_EPOLLEXCLUSIVE)
#define NT_EXCLUSIVE_EVENT  EPOLLEXCLUSIVE
#endif

//使用POLL
#elif (NT_HAVE_POLL)

#define NT_READ_EVENT     POLLIN
#define NT_WRITE_EVENT    POLLOUT

#define NT_LEVEL_EVENT    0
#define NT_ONESHOT_EVENT  1

//使用select
#else /* select */

#define NT_READ_EVENT     0
#define NT_WRITE_EVENT    1

#define NT_LEVEL_EVENT    0
#define NT_ONESHOT_EVENT  1

#endif /* NT_HAVE_KQUEUE */


#if (NT_HAVE_IOCP)
#define NT_IOCP_ACCEPT      0
#define NT_IOCP_IO          1
#define NT_IOCP_CONNECT     2
#endif


#if (NT_TEST_BUILD_EPOLL)
#define NT_EXCLUSIVE_EVENT  0
#endif


#ifndef NT_CLEAR_EVENT
#define NT_CLEAR_EVENT    0    /* dummy declaration */
#endif


#define nt_process_events   nt_event_actions.process_events
#define nt_done_events      nt_event_actions.done

#define nt_add_event        nt_event_actions.add
#define nt_del_event        nt_event_actions.del
#define nt_add_conn         nt_event_actions.add_conn
#define nt_del_conn         nt_event_actions.del_conn

#if (NT_SSL && NT_SSL_ASYNC)
#define nt_add_async_conn   nt_event_actions.add_async_conn
#define nt_del_async_conn   nt_event_actions.del_async_conn
#endif

#define nt_notify           nt_event_actions.notify

#define nt_add_timer        nt_event_add_timer
#define nt_del_timer        nt_event_del_timer


extern nt_os_io_t  nt_io;

#define nt_recv             nt_io.recv
#define nt_recv_chain       nt_io.recv_chain
#define nt_udp_recv         nt_io.udp_recv
#define nt_send             nt_io.send
#define nt_send_chain       nt_io.send_chain
#define nt_udp_send         nt_io.udp_send
#define nt_udp_send_chain   nt_io.udp_send_chain


#define NT_EVENT_MODULE      0x544E5645  /* "EVNT" */
#define NT_EVENT_CONF        0x02000000


typedef struct {
    nt_uint_t    connections;
    nt_uint_t    use;

    nt_flag_t    multi_accept;
    nt_flag_t    accept_mutex;

    nt_msec_t    accept_mutex_delay;

    u_char       *name;

#if (NT_DEBUG)
    nt_array_t   debug_connection;
#endif
} nt_event_conf_t;


typedef struct {
    nt_str_t              *name;

    void                 *(*create_conf)(nt_cycle_t *cycle);
    char                 *(*init_conf)(nt_cycle_t *cycle, void *conf);

    nt_event_actions_t     actions;
} nt_event_module_t;


extern nt_atomic_t          *nt_connection_counter;

extern nt_atomic_t          *nt_accept_mutex_ptr;
extern nt_shmtx_t            nt_accept_mutex;
extern nt_uint_t             nt_use_accept_mutex;
extern nt_uint_t             nt_accept_events;
extern nt_uint_t             nt_accept_mutex_held;
extern nt_msec_t             nt_accept_mutex_delay;
extern nt_int_t              nt_accept_disabled;


#if (NT_STAT_STUB)

extern nt_atomic_t  *nt_stat_accepted;
extern nt_atomic_t  *nt_stat_handled;
extern nt_atomic_t  *nt_stat_requests;
extern nt_atomic_t  *nt_stat_active;
extern nt_atomic_t  *nt_stat_reading;
extern nt_atomic_t  *nt_stat_writing;
extern nt_atomic_t  *nt_stat_waiting;
#if (T_NT_HTTP_STUB_STATUS)
extern nt_atomic_t  *nt_stat_request_time;
#endif
#endif


#define NT_UPDATE_TIME         1
#define NT_POST_EVENTS         2


extern sig_atomic_t           nt_event_timer_alarm;
extern nt_uint_t             nt_event_flags;
extern nt_module_t           nt_events_module;
extern nt_module_t           nt_event_core_module;


#define nt_event_get_conf(conf_ctx, module)                                  \
             (*(nt_get_conf(conf_ctx, nt_events_module))) [module.ctx_index]



void nt_event_accept(nt_event_t *ev);
#if !(NT_WIN32)
void nt_event_recvmsg(nt_event_t *ev);
void nt_udp_rbtree_insert_value(nt_rbtree_node_t *temp,
    nt_rbtree_node_t *node, nt_rbtree_node_t *sentinel);
#endif
void nt_delete_udp_connection(void *data);
nt_int_t nt_trylock_accept_mutex(nt_cycle_t *cycle);
nt_int_t nt_enable_accept_events(nt_cycle_t *cycle);
u_char *nt_accept_log_error(nt_log_t *log, u_char *buf, size_t len);
#if (NT_DEBUG)
void nt_debug_accepted_connection(nt_event_conf_t *ecf, nt_connection_t *c);
#endif


void nt_process_events_and_timers(nt_cycle_t *cycle);
nt_int_t nt_handle_read_event(nt_event_t *rev, nt_uint_t flags);
nt_int_t nt_handle_write_event(nt_event_t *wev, size_t lowat);


#if (NT_WIN32)
void nt_event_acceptex(nt_event_t *ev);
nt_int_t nt_event_post_acceptex(nt_listening_t *ls, nt_uint_t n);
u_char *nt_acceptex_log_error(nt_log_t *log, u_char *buf, size_t len);
#endif


nt_int_t nt_send_lowat(nt_connection_t *c, size_t lowat);


/* used in nt_log_debugX() */
#define nt_event_ident(p)  ((nt_connection_t *) (p))->fd


#include <nt_event_timer.h>
#include <nt_event_posted.h>

#if (NT_WIN32)
#include <nt_iocp_module.h>
#endif

ssize_t nt_unix_read(nt_connection_t *c, u_char *buf, size_t size);
ssize_t nt_unix_write(nt_connection_t *c, u_char *buf, size_t size);


#endif /* _NT_EVENT_H_INCLUDED_ */
