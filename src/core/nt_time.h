#ifndef _NT_TIME_H_
#define _NT_TIME_H_

#include <nt_def.h>
#include <nt_core.h>



/*
 *       封装nginx自己的时间缓存
 *             减少gettimeofday系统调用，从而提高效率
 *                   nginx更新缓存时间主要是通过nt_timer_resolution时间频率发送信号SIGALRM处理，从而达到更新缓存时间的目的  
 *                     */

struct nt_time_s{
    time_t      sec;
    nt_uint_t  msec;
#if (T_NT_RET_CACHE)
    nt_uint_t  usec;
#endif
    nt_int_t   gmtoff;
} ;

#if (T_NT_RET_CACHE)
    extern volatile nt_tm_t    *nt_cached_tm;
#endif

extern volatile nt_time_t  *nt_cached_time;


#define nt_time()           nt_cached_time->sec
#define nt_timeofday()      (nt_time_t *) nt_cached_time

extern volatile nt_str_t    nt_cached_err_log_time;
extern volatile nt_str_t    nt_cached_http_time;
extern volatile nt_str_t    nt_cached_http_log_time;
extern volatile nt_str_t    nt_cached_http_log_iso8601;
extern volatile nt_str_t    nt_cached_syslog_time;

/*
 *    * milliseconds elapsed since some unspecified point in the past
 *       * and truncated to nt_msec_t, used in event timers
 *          */
extern volatile nt_msec_t  nt_current_msec;

#endif
