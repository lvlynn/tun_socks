
#ifndef _NT_PARSE_H_INCLUDED_
#define _NT_PARSE_H_INCLUDED_


#include <nt_core.h>


ssize_t nt_parse_size(nt_str_t *line);
off_t nt_parse_offset(nt_str_t *line);
nt_int_t nt_parse_time(nt_str_t *line, nt_uint_t is_sec);


#endif /* _NT_PARSE_H_INCLUDED_ */
