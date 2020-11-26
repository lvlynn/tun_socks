
#include <nt_core.h>


volatile nt_time_t     *nt_cached_time;                                                                                         
volatile nt_str_t       nt_cached_err_log_time;
volatile nt_str_t       nt_cached_http_time;
volatile nt_str_t       nt_cached_http_log_time;
volatile nt_str_t       nt_cached_http_log_iso8601;
volatile nt_str_t       nt_cached_syslog_time;


static char  *week[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"  };
static char  *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

