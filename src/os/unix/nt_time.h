#ifndef _NT_TIME_H_
#define _NT_TIME_H_

#include <nt_def.h>
#include <nt_core.h>

//typedef nt_rbtree_key_t      nt_msec_t;
#if (T_NGX_RET_CACHE)
    typedef nt_rbtree_key_t      nt_usec_t;
#endif

//typedef nt_rbtree_key_int_t  nt_msec_int_t;
#if (T_NGX_VARS)
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


#if (NGX_HAVE_GMTOFF)
    #define nt_tm_gmtoff         tm_gmtoff
    #define nt_tm_zone           tm_zone
#endif


extern volatile nt_str_t    nt_cached_err_log_time;

#endif
