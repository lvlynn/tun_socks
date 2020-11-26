#ifndef _NT_TIMES_H_
#define _NT_TIMES_H_

#include <nt_core.h>

typedef nt_rbtree_key_t      nt_msec_t;
#if (T_NT_RET_CACHE)
    typedef nt_rbtree_key_t      nt_usec_t;
#endif

typedef nt_rbtree_key_int_t  nt_msec_int_t;
#if (T_NT_VARS)
    typedef nt_rbtree_key_int_t  nt_usec_int_t;
#endif

typedef struct tm             nt_tm_t;

#define nt_tm_sec            tm_sec
#define nt_tm_min            tm_min
#define nt_tm_hour           tm_hour
#define nt_tm_mday           tm_mday
#define nt_tm_mon            tm_mon
#define nt_tm_year           tm_year
#define nt_tm_wday           tm_wday
#define nt_tm_isdst          tm_isdst

#define nt_tm_sec_t          int
#define nt_tm_min_t          int
#define nt_tm_hour_t         int
#define nt_tm_mday_t         int
#define nt_tm_mon_t          int
#define nt_tm_year_t         int
#define nt_tm_wday_t         int


#if (NT_HAVE_GMTOFF)
    #define nt_tm_gmtoff         tm_gmtoff
    #define nt_tm_zone           tm_zone
#endif

#if (NT_SOLARIS)

    #define nt_timezone(isdst) (- (isdst ? altzone : timezone) / 60)

#else

    #define nt_timezone(isdst) (- (isdst ? timezone + 3600 : timezone) / 60)

#endif


void nt_timezone_update( void );
 void nt_localtime( time_t s, nt_tm_t *tm );
 void nt_libc_localtime( time_t s, struct tm *tm );
 void nt_libc_gmtime( time_t s, struct tm *tm );

#define nt_gettimeofday(tp)  (void) gettimeofday(tp, NULL);
#define nt_msleep(ms)        (void) usleep(ms * 1000)
#define nt_sleep(s)          (void) sleep(s)


extern volatile nt_str_t    nt_cached_err_log_time;

#endif
