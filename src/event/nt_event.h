#ifndef _NT_EVENT_H_
#define _NT_EVENT_H_

#include <nt_def.h>
#include <nt_core.h>


#define NT_INVALID_INDEX  0xd0d0d0d0

struct nt_event_s {

    /*  事件相关的对象。通常data都是指向nt_connection_t连接对象,见nt_get_connection。
     *  开启文件异步I/O时，它可能会指向nt_event_aio_t(nt_file_aio_init)结构体
     *  */
    void            *data;

    //标志位，为1时表示事件是可写的。通常情况下，它表示对应的TCP连接目前状态是可写的，也就是连接处于可以发送网络包的状态
    unsigned         write: 1;

    // 标志位，为1时表示当前事件是活跃的 为0时表示事件是不活跃的
    unsigned         active: 1;


    /*
     *     标志位，为1时表示禁用事件，仅在kqueue或者rtsig事件驱动模块中有效，而对于epoll事件驱动模块则无意义，这里不再详述
     **/
    unsigned         disabled: 1;


    unsigned         ready: 1;

    unsigned         accept: 1;


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
    unsigned         instance: 1;



    //标志位，为1时表示当前处理的字符流已经结束  例如内核缓冲区没有数据，你去读，则会返回0
    unsigned         eof: 1; //见nt_unix_recv
    //标志位，为1时表示事件在处理过程中出现错误
    unsigned         error: 1;

    //标志位，为I时表示这个事件已经超时，用以提示事件的消费模块做超时处理
    /*读客户端连接的数据，在nt_http_init_connection(nt_connection_t *c)中的nt_add_timer(rev, c->listening->post_ac  cept_timeout)把读事件添加到定时器中，如果超时则置1
      每次nt_unix_recv把内核数据读取完毕后，在重新启动add epoll，等待新的数据到来，同时会启动定时器nt_add_timer(rev  , c->listening->post_accept_timeout);
      如果在post_accept_timeout这么长事件内没有数据到来则超时，开始处理关闭TCP流程*/

    /*
        读超时是指的读取对端数据的超时时间，写超时指的是当数据包很大的时候，write返回NT_AGAIN，则会添加write定时器，从而
        判断是否超时，如果发往
        对端数据长度小，则一般write直接返回成功，则不会添加write超时定时器，也就不会有write超时，写定时器参考函数nt_http  _upstream_send_request
        */
    unsigned         timedout: 1; //定时器超时标记，见nt_event_expire_timers

    //标志位，为1时表示这个事件存在于定时器中
    unsigned         timer_set: 1; //nt_event_add_timer nt_add_timer 中置1   nt_event_expire_timers置0

    //标志位，delayed为1时表示需要延迟处理这个事件，它仅用于限速功能
    unsigned         delayed: 1; //限速见nt_http_write_filter


    //删除post队列的时候需要检查
    //表示延迟处理该事件，见nt_epoll_process_events -> nt_post_event  标记是否在延迟队列里面
    unsigned         posted: 1;

    //标志位，为1时表示当前事件已经关闭，epoll模块没有使用它
    unsigned         closed: 1; //nt_close_connection中置1


    /* to test on worker exit */
    unsigned         channel: 1;
    unsigned         resolver: 1;

    //计时器事件标志，指示在关闭工作人员时应忽略该事件。优雅的工作人员关闭被延迟，直到没有安排不可取消的计时器事件。
    unsigned         cancelable: 1;

    /*  epoll未使用
     *  select的事件序号
     * */
    nt_uint_t       index;

    //可用于记录error_log日志的nt_log_t对象
    nt_log_t       *log; //可以记录日志的nt_log_t对象 其实就是nt_listening_t中获取的log //赋值见nt_event_accept


    /* nt_event_accept，
     * nt_http_ssl_handshake
     * nt_ssl_handshake_handler
     * nt_http_v2_write_handler
     * nt_http_v2_read_handler
     * nt_http_wait_request_handler
     * nt_http_request_handler
     * nt_http_upstream_handler
     * nt_file_aio_event_handler */
    //由epoll读写事件在nt_epoll_process_events触发
    nt_event_handler_pt  handler;

    //定时器节点，用于定时器红黑树中
    nt_rbtree_node_t   timer;  //见nt_event_timer_rbtree

    /* the posted queue */
    /*post事件将会构成一个队列再统一处理，这个队列以next和prev作为链表指针，以此构成一个简易的双向链表，其中next指向后一个>  事件的地址，
     *       prev指向前一个事件的地址
     *              */
    nt_queue_t      queue;
};


typedef struct {
    nt_int_t ( *add )( nt_event_t *ev, nt_int_t event, nt_uint_t flags );
    nt_int_t ( *del )( nt_event_t *ev, nt_int_t event, nt_uint_t flags );

    nt_int_t ( *enable )( nt_event_t *ev, nt_int_t event, nt_uint_t flags );
    nt_int_t ( *disable )( nt_event_t *ev, nt_int_t event, nt_uint_t flags );

    nt_int_t ( *add_conn )( nt_connection_t *c );
    nt_int_t ( *del_conn )( nt_connection_t *c, nt_uint_t flags );

    nt_int_t ( *notify )( nt_event_handler_pt handler );

    nt_int_t ( *process_events )( nt_cycle_t *cycle, nt_msec_t  timer,
                                  nt_uint_t flags );

    nt_int_t ( *init )( nt_cycle_t *cycle );
    void ( *done )( nt_cycle_t *cycle );

    #if (NT_SSL && NT_SSL_ASYNC)
    nt_int_t ( *add_async_conn )( nt_connection_t *c );
    nt_int_t ( *del_async_conn )( nt_connection_t *c, nt_uint_t flags );
    #endif
} nt_event_actions_t;

extern nt_event_actions_t   nt_event_actions;



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

//E->45, V->56, N->4E, E->54
#define NT_EVENT_MODULE      0x544E5645  /* "EVNT" */
#define NT_EVENT_CONF        0x02000000


typedef struct {
    nt_str_t              *name;

    void                 *( *create_conf )( nt_cycle_t *cycle );
    char                 *( *init_conf )( nt_cycle_t *cycle, void *conf );

    nt_event_actions_t     actions;

} nt_event_module_t;



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


#if (NT_HAVE_EPOLL) && !(NT_TEST_BUILD_EPOLL)

    #define NT_READ_EVENT     (EPOLLIN|EPOLLRDHUP)
    #define NT_WRITE_EVENT    EPOLLOUT

    #define NT_LEVEL_EVENT    0
    #define NT_CLEAR_EVENT    EPOLLET
    #define NT_ONESHOT_EVENT  0x70000000
    #if 0
        #define NT_ONESHOT_EVENT  EPOLLONESHOT
    #endif

    #if (NT_HAVE_EPOLLEXCLUSIVE)
        #define NT_EXCLUSIVE_EVENT  EPOLLEXCLUSIVE
    #endif

#elif (NT_HAVE_POLL)

    #define NT_READ_EVENT     POLLIN
    #define NT_WRITE_EVENT    POLLOUT

    #define NT_LEVEL_EVENT    0
    #define NT_ONESHOT_EVENT  1


#else /* select */

    #define NT_READ_EVENT     0
    #define NT_WRITE_EVENT    1

    #define NT_LEVEL_EVENT    0
    #define NT_ONESHOT_EVENT  1

#endif

#define NT_UPDATE_TIME         1
#define NT_POST_EVENTS         2

//声明
extern nt_atomic_t          *nt_connection_counter;
extern nt_uint_t             nt_event_flags;
extern nt_uint_t             nt_accept_mutex_held;
extern nt_uint_t             nt_use_accept_mutex;
//得到event模块对应的配置
#define nt_event_get_conf(conf_ctx, module)                                  \
    (*(nt_get_conf(conf_ctx, nt_events_module))) [module.ctx_index]

void nt_event_accept( nt_event_t *ev );
void nt_event_no_accept( nt_event_t *ev );
#if !(NT_WIN32)
void nt_event_recvmsg( nt_event_t *ev );
void nt_udp_rbtree_insert_value( nt_rbtree_node_t *temp,
                                 nt_rbtree_node_t *node, nt_rbtree_node_t *sentinel );
#endif
void nt_delete_udp_connection( void *data );
nt_int_t nt_trylock_accept_mutex( nt_cycle_t *cycle );
nt_int_t nt_enable_accept_events( nt_cycle_t *cycle );
u_char *nt_accept_log_error( nt_log_t *log, u_char *buf, size_t len );
/* #if (NT_DEBUG)
    void nt_debug_accepted_connection( nt_event_conf_t *ecf, nt_connection_t *c );
#endif */


void nt_process_events_and_timers( nt_cycle_t *cycle );
nt_int_t nt_handle_read_event( nt_event_t *rev, nt_uint_t flags );
nt_int_t nt_handle_write_event( nt_event_t *wev, size_t lowat );


#if (NT_WIN32)
    void nt_event_acceptex( nt_event_t *ev );
    nt_int_t nt_event_post_acceptex( nt_listening_t *ls, nt_uint_t n );
    u_char *nt_acceptex_log_error( nt_log_t *log, u_char *buf, size_t len );
#endif


nt_int_t nt_send_lowat( nt_connection_t *c, size_t lowat );


/* used in nt_log_debugX() */
#define nt_event_ident(p)  ((nt_connection_t *) (p))->fd

nt_int_t nt_event_process_init( nt_cycle_t *cycle );

extern nt_queue_t  nt_posted_accept_events;
extern nt_queue_t  nt_posted_events;


#include <nt_event_posted.h>
#include <nt_event_timer.h>



#endif
