
#include <nt_core.h>

static nt_msec_t nt_monotonic_time( time_t sec, nt_uint_t msec );


/*
 *    * The time may be updated by signal handler or by several threads.
 *       * The time update operations are rare and require to hold the nt_time_lock.
 *          * The time read operations are frequent, so they are lock-free and get time
 *             * values and strings from the current slot.  Thus thread may get the corrupted
 *                * values only if it is preempted while copying and then it is not scheduled
 *                   * to run more than NT_TIME_SLOTS seconds.
 *                      */

#define NT_TIME_SLOTS   64

static nt_uint_t        slot;
//static nt_atomic_t      nt_time_lock;

volatile nt_msec_t      nt_current_msec;
volatile nt_time_t     *nt_cached_time;
volatile nt_str_t       nt_cached_err_log_time;
volatile nt_str_t       nt_cached_http_time;
volatile nt_str_t       nt_cached_http_log_time;
volatile nt_str_t       nt_cached_http_log_iso8601;
volatile nt_str_t       nt_cached_syslog_time;
#if (T_NT_RET_CACHE)
    volatile nt_tm_t       *nt_cached_tm;
#endif


static char  *week[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"  };
static char  *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
                         };


void
nt_time_init( void )
{
    nt_cached_err_log_time.len = sizeof( "1970/09/28 12:00:00" ) - 1;
    nt_cached_http_time.len = sizeof( "Mon, 28 Sep 1970 06:00:00 GMT" ) - 1;
    nt_cached_http_log_time.len = sizeof( "28/Sep/1970:12:00:00 +0600" ) - 1;
    nt_cached_http_log_iso8601.len = sizeof( "1970-09-28T12:00:00+06:00" ) - 1;
    nt_cached_syslog_time.len = sizeof( "Sep 28 12:00:00" ) - 1;

//    nt_cached_time = &cached_time[0];
    #if (T_NT_RET_CACHE)
    nt_cached_tm = &cached_http_log_tm[0];
    #endif

    nt_time_update();
}

/*
  nt_time_update 是nginx时间管理的核心函数 
      1）时间数据的一致性主要是读写上，首先要防止写冲突，对写时间缓存加锁，因为读时间操作远大于写时间，所以读操作没有加锁
      2）还有另外一种是，在读的时候，另外有个信号中断了读操作，而这个信号里面更新了时间缓存变量导致读的时间不一致，nginx通过时间数组变量来
  解决这个问题
  */
void
nt_time_update( void )
{

    //    debug(2, "start");
    u_char          *p0, *p1, *p2, *p3, *p4;
    nt_tm_t         tm, gmt;
    time_t           sec;
    nt_uint_t       msec;
    nt_time_t      *tp;
    struct timeval   tv;
    #if (T_NT_RET_CACHE)
    nt_uint_t       usec;
    #endif

    //nt_time_lock加锁是为了解决信号处理过程中更新缓存时间产生的时间不一致的问题。
    /* if (!nt_trylock(&nt_time_lock)) {
        return;
    }
    */
    //该函数时间更新实际就是调用系统函数，只是对#define nt_gettimeofday(tp)  (void) gettimeofday(tp, NULL);
    nt_gettimeofday( &tv );

    sec = tv.tv_sec;
    printf("msec=%u\n", sec);
    msec = tv.tv_usec / 1000;

    printf("msec=%u\n", msec);
    #if (T_NT_RET_CACHE)
    usec = tv.tv_usec % 1000;
    #endif

    //转换为秒数
    nt_current_msec = nt_monotonic_time( sec, msec );
}

static nt_msec_t
nt_monotonic_time( time_t sec, nt_uint_t msec )
{
    #if (NT_HAVE_CLOCK_MONOTONIC)
    struct timespec  ts;

    #if defined(CLOCK_MONOTONIC_FAST)
    clock_gettime( CLOCK_MONOTONIC_FAST, &ts );

    #elif defined(CLOCK_MONOTONIC_COARSE)
    clock_gettime( CLOCK_MONOTONIC_COARSE, &ts );

    #else
    clock_gettime( CLOCK_MONOTONIC, &ts );
    #endif

    sec = ts.tv_sec;
    msec = ts.tv_nsec / 1000000;

    #endif

    return ( nt_msec_t ) sec * 1000 + msec;
}
