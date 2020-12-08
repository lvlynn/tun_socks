
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

  #if !(NT_WIN32)
  
  /*
   * localtime() and localtime_r() are not Async-Signal-Safe functions, therefore,
   * they must not be called by a signal handler, so we use the cached
   * GMT offset value. Fortunately the value is changed only two times a year.
   */
  
  static nt_int_t         cached_gmtoff;
  #endif
  
  #if (T_NT_RET_CACHE)
  static nt_tm_t          cached_http_log_tm[NT_TIME_SLOTS];
  #endif
  static nt_time_t        cached_time[NT_TIME_SLOTS];
 static u_char            cached_err_log_time[NT_TIME_SLOTS]
                                      [sizeof("1970/09/28 12:00:00")];
 static u_char            cached_http_time[NT_TIME_SLOTS]
                                      [sizeof("Mon, 28 Sep 1970 06:00:00 GMT")];
 static u_char            cached_http_log_time[NT_TIME_SLOTS]
                                      [sizeof("28/Sep/1970:12:00:00 +0600")];
 static u_char            cached_http_log_iso8601[NT_TIME_SLOTS]
                                      [sizeof("1970-09-28T12:00:00+06:00")];
 static u_char            cached_syslog_time[NT_TIME_SLOTS]
                                      [sizeof("Sep 28 12:00:00")];


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

    nt_cached_time = &cached_time[0];
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
    printf( "msec=%u\n", sec );
    msec = tv.tv_usec / 1000;

    printf( "msec=%u\n", msec );
    #if (T_NT_RET_CACHE)
    usec = tv.tv_usec % 1000;
    #endif

    //转换为秒数
    nt_current_msec = nt_monotonic_time( sec, msec );
    tp = &cached_time[slot];

    /*
       由于nginx在读时间缓存比写时间缓存的次数远远要多，处于性能考虑所以没有对读进行加锁，看这前后几行代码，大牛是如何解决这>  个读写不一致的问题，
       当前面已经有一个读时间slot等于10，本来读时间是cached_time[10]的值，结果来了个信号中断读，但是这个信号中又写时间了，下>  面这个判断如果这个
       读时间变量和要写的时间一致直接返回，如果不一致，对slot++，写完后，上次本来要读取的时间是cached_time[10]，但是每次读取>  的时候是数组里面最新的那个时间
       也就是cached_time[11]。有没有觉得这种解决办法很简单，但有时候你解决的时候死活想不到。复杂问题，简单处理。精髓之处,也是
    我们平时值得学习借鉴应用的地方
       */
    //如果秒数一样，就直接返回
    if( tp->sec == sec ) {
        tp->msec = msec;
        #if (T_NT_RET_CACHE)
        tp->usec = usec;
        #endif
      //  nt_unlock( &nt_time_lock );
        return;
    }

    if( slot == NT_TIME_SLOTS - 1 ) {
        slot = 0;   //slot计数置零
    } else {
        slot++;
    }

    tp = &cached_time[slot];

    tp->sec = sec;
    tp->msec = msec;
    #if (T_NT_RET_CACHE)
    tp->usec = usec;
    #endif

    nt_gmtime( sec, &gmt );

    p0 = &cached_http_time[slot][0];

    ( void ) nt_sprintf( p0, "%s, %02d %s %4d %02d:%02d:%02d GMT",
                          week[gmt.nt_tm_wday], gmt.nt_tm_mday,
                          months[gmt.nt_tm_mon - 1], gmt.nt_tm_year,
                          gmt.nt_tm_hour, gmt.nt_tm_min, gmt.nt_tm_sec );

    #if (NT_HAVE_GETTIMEZONE)

    tp->gmtoff = nt_gettimezone();
    nt_gmtime( sec + tp->gmtoff * 60, &tm );

    #elif (NT_HAVE_GMTOFF)

    nt_localtime( sec, &tm );
    cached_gmtoff = ( nt_int_t )( tm.nt_tm_gmtoff / 60 );
    tp->gmtoff = cached_gmtoff;

    #else

    nt_localtime( sec, &tm );
    cached_gmtoff = nt_timezone( tm.nt_tm_isdst );
    tp->gmtoff = cached_gmtoff;

    #endif

    #if (T_NT_RET_CACHE)
    cached_http_log_tm[slot] = tm;
    #endif

    p1 = &cached_err_log_time[slot][0];

    ( void ) nt_sprintf( p1, "%4d/%02d/%02d %02d:%02d:%02d",
                          tm.nt_tm_year, tm.nt_tm_mon,
                          tm.nt_tm_mday, tm.nt_tm_hour,
                          tm.nt_tm_min, tm.nt_tm_sec );


    p2 = &cached_http_log_time[slot][0];

    ( void ) nt_sprintf( p2, "%02d/%s/%d:%02d:%02d:%02d %c%02i%02i",
                          tm.nt_tm_mday, months[tm.nt_tm_mon - 1],
                          tm.nt_tm_year, tm.nt_tm_hour,
                          tm.nt_tm_min, tm.nt_tm_sec,
                          tp->gmtoff < 0 ? '-' : '+',
                          nt_abs( tp->gmtoff / 60 ), nt_abs( tp->gmtoff % 60 ) );

    p3 = &cached_http_log_iso8601[slot][0];

    ( void ) nt_sprintf( p3, "%4d-%02d-%02dT%02d:%02d:%02d%c%02i:%02i",
                          tm.nt_tm_year, tm.nt_tm_mon,
                          tm.nt_tm_mday, tm.nt_tm_hour,
                          tm.nt_tm_min, tm.nt_tm_sec,
                          tp->gmtoff < 0 ? '-' : '+',
                          nt_abs( tp->gmtoff / 60 ), nt_abs( tp->gmtoff % 60 ) );

    p4 = &cached_syslog_time[slot][0];

    ( void ) nt_sprintf( p4, "%s %2d %02d:%02d:%02d",
                          months[tm.nt_tm_mon - 1], tm.nt_tm_mday,
                          tm.nt_tm_hour, tm.nt_tm_min, tm.nt_tm_sec );

    //这个函数也是宏定义，里面兼容了各种平台，volatile关键字就知道主要告诉编译器不要对后面的语句进行优化了
    nt_memory_barrier();

    #if (T_NT_RET_CACHE)
    nt_cached_tm = &cached_http_log_tm[slot];
    #endif
    nt_cached_time = tp;
    nt_cached_http_time.data = p0;
    nt_cached_err_log_time.data = p1;
    nt_cached_http_log_time.data = p2;
    nt_cached_http_log_iso8601.data = p3;
    nt_cached_syslog_time.data = p4;

//    nt_unlock( &nt_time_lock );

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

#if !(NT_WIN32)
//nt_time_sigsafe_update机制上和nt_time_update差不多，只是更新了一个缓存时间，原理一样。
void
nt_time_sigsafe_update( void )
{
    //  debug(2, "start");
    u_char          *p, *p2;
    nt_tm_t         tm;
    time_t           sec;
    nt_time_t      *tp;
    struct timeval   tv;

    /* if( !nt_trylock( &nt_time_lock ) ) {
        return;
    } */

    nt_gettimeofday( &tv );

    sec = tv.tv_sec;

    tp = &cached_time[slot];

    if( tp->sec == sec ) {
        /* nt_unlock( &nt_time_lock ); */
        return;
    }

    if( slot == NT_TIME_SLOTS - 1 ) {
        slot = 0;
    } else {
        slot++;
    }

    tp = &cached_time[slot];

    tp->sec = 0;

    nt_gmtime( sec + cached_gmtoff * 60, &tm );

    p = &cached_err_log_time[slot][0];

    ( void ) nt_sprintf( p, "%4d/%02d/%02d %02d:%02d:%02d",
                          tm.nt_tm_year, tm.nt_tm_mon,
                          tm.nt_tm_mday, tm.nt_tm_hour,
                          tm.nt_tm_min, tm.nt_tm_sec );

    p2 = &cached_syslog_time[slot][0];

    ( void ) nt_sprintf( p2, "%s %2d %02d:%02d:%02d",
                          months[tm.nt_tm_mon - 1], tm.nt_tm_mday,
                          tm.nt_tm_hour, tm.nt_tm_min, tm.nt_tm_sec );

    nt_memory_barrier();

    nt_cached_err_log_time.data = p;
    nt_cached_syslog_time.data = p2;

    /* nt_unlock( &nt_time_lock ); */
}

#endif


u_char *
nt_http_time( u_char *buf, time_t t )
{
    nt_tm_t  tm;

    nt_gmtime( t, &tm );

    return nt_sprintf( buf, "%s, %02d %s %4d %02d:%02d:%02d GMT",
                        week[tm.nt_tm_wday],
                        tm.nt_tm_mday,
                        months[tm.nt_tm_mon - 1],
                        tm.nt_tm_year,
                        tm.nt_tm_hour,
                        tm.nt_tm_min,
                        tm.nt_tm_sec );
}


u_char *
nt_http_cookie_time( u_char *buf, time_t t )
{
    nt_tm_t  tm;

    nt_gmtime( t, &tm );

    /*
     * Netscape 3.x does not understand 4-digit years at all and
     * 2-digit years more than "37"
     */

    return nt_sprintf( buf,
                        ( tm.nt_tm_year > 2037 ) ?
                        "%s, %02d-%s-%d %02d:%02d:%02d GMT" :
                        "%s, %02d-%s-%02d %02d:%02d:%02d GMT",
                        week[tm.nt_tm_wday],
                        tm.nt_tm_mday,
                        months[tm.nt_tm_mon - 1],
                        ( tm.nt_tm_year > 2037 ) ? tm.nt_tm_year :
                        tm.nt_tm_year % 100,
                        tm.nt_tm_hour,
                        tm.nt_tm_min,
                        tm.nt_tm_sec );
}

void
nt_gmtime( time_t t, nt_tm_t *tp )
{
    nt_int_t   yday;
    nt_uint_t  sec, min, hour, mday, mon, year, wday, days, leap;

    /* the calculation is valid for positive time_t only */

    if( t < 0 ) {
        t = 0;
    }

    days = t / 86400;
    sec = t % 86400;

    /*
     * no more than 4 year digits supported,
     * truncate to December 31, 9999, 23:59:59
     */

    if( days > 2932896 ) {
        days = 2932896;
        sec = 86399;
    }

    /* January 1, 1970 was Thursday */

    wday = ( 4 + days ) % 7;

    hour = sec / 3600;
    sec %= 3600;
    min = sec / 60;
    sec %= 60;

    /*
     * the algorithm based on Gauss' formula,
     * see src/core/nt_parse_time.c
     */

    /* days since March 1, 1 BC */
    days = days - ( 31 + 28 ) + 719527;

    /*
     * The "days" should be adjusted to 1 only, however, some March 1st's go
     * to previous year, so we adjust them to 2.  This causes also shift of the
     * last February days to next year, but we catch the case when "yday"
     * becomes negative.
     */
    year = ( days + 2 ) * 400 / ( 365 * 400 + 100 - 4 + 1 );

    yday = days - ( 365 * year + year / 4 - year / 100 + year / 400 );

    if( yday < 0 ) {
        leap = ( year % 4 == 0 ) && ( year % 100 || ( year % 400 == 0 ) );
        yday = 365 + leap + yday;
        year--;
    }

    /*
     * The empirical formula that maps "yday" to month.
     * There are at least 10 variants, some of them are:
     *     mon = (yday + 31) * 15 / 459
     *     mon = (yday + 31) * 17 / 520
     *     mon = (yday + 31) * 20 / 612
     */

    mon = ( yday + 31 ) * 10 / 306;

    /* the Gauss' formula that evaluates days before the month */

    mday = yday - ( 367 * mon / 12 - 30 ) + 1;

    if( yday >= 306 ) {

        year++;
        mon -= 10;

        /*
         * there is no "yday" in Win32 SYSTEMTIME
         *
         * yday -= 306;
         */

    } else {

        mon += 2;

        /*
         * there is no "yday" in Win32 SYSTEMTIME
         *
         * yday += 31 + 28 + leap;
         */
    }
    tp->nt_tm_sec = ( nt_tm_sec_t ) sec;
    tp->nt_tm_min = ( nt_tm_min_t ) min;
    tp->nt_tm_hour = ( nt_tm_hour_t ) hour;
    tp->nt_tm_mday = ( nt_tm_mday_t ) mday;
    tp->nt_tm_mon = ( nt_tm_mon_t ) mon;
    tp->nt_tm_year = ( nt_tm_year_t ) year;
    tp->nt_tm_wday = ( nt_tm_wday_t ) wday;
}


time_t
nt_next_time( time_t when )
{
    time_t     now, next;
    struct tm  tm;

    now = nt_time();

    nt_libc_localtime( now, &tm );

    tm.tm_hour = ( int )( when / 3600 );
    when %= 3600;
    tm.tm_min = ( int )( when / 60 );
    tm.tm_sec = ( int )( when % 60 );

    next = mktime( &tm );

    if( next == -1 ) {
        return -1;
    }

    if( next - now > 0 ) {
        return next;
    }

    tm.tm_mday++;

    /* mktime() should normalize a date (Jan 32, etc) */

    next = mktime( &tm );

    if( next != -1 ) {
        return next;
    }

    return -1;
}
