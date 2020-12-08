
#ifndef _NT_PARSE_TIME_H_INCLUDED_
#define _NT_PARSE_TIME_H_INCLUDED_


#include <nt_core.h>


time_t nt_parse_http_time(u_char *value, size_t len);

/* compatibility */
#define nt_http_parse_time(value, len)  nt_parse_http_time(value, len)


#endif /* _NT_PARSE_TIME_H_INCLUDED_ */
