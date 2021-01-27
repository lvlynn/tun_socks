

#ifndef _NT_STREAM_VARIABLES_H_INCLUDED_
#define _NT_STREAM_VARIABLES_H_INCLUDED_


#include <nt_core.h>
#include <nt_stream.h>


typedef nt_variable_value_t  nt_stream_variable_value_t;

#define nt_stream_variable(v)     { sizeof(v) - 1, 1, 0, 0, 0, (u_char *) v }

typedef struct nt_stream_variable_s  nt_stream_variable_t;

typedef void (*nt_stream_set_variable_pt) (nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);
typedef nt_int_t (*nt_stream_get_variable_pt) (nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);


#define NT_STREAM_VAR_CHANGEABLE   1
#define NT_STREAM_VAR_NOCACHEABLE  2
#define NT_STREAM_VAR_INDEXED      4
#define NT_STREAM_VAR_NOHASH       8
#define NT_STREAM_VAR_WEAK         16
#define NT_STREAM_VAR_PREFIX       32


struct nt_stream_variable_s {
    nt_str_t                     name;   /* must be first to build the hash */
    nt_stream_set_variable_pt    set_handler;
    nt_stream_get_variable_pt    get_handler;
    uintptr_t                     data;
    nt_uint_t                    flags;
    nt_uint_t                    index;
};

#define nt_stream_null_variable  { nt_null_string, NULL, NULL, 0, 0, 0 }


nt_stream_variable_t *nt_stream_add_variable(nt_conf_t *cf, nt_str_t *name,
    nt_uint_t flags);
nt_int_t nt_stream_get_variable_index(nt_conf_t *cf, nt_str_t *name);
nt_stream_variable_value_t *nt_stream_get_indexed_variable(
    nt_stream_session_t *s, nt_uint_t index);
nt_stream_variable_value_t *nt_stream_get_flushed_variable(
    nt_stream_session_t *s, nt_uint_t index);

nt_stream_variable_value_t *nt_stream_get_variable(nt_stream_session_t *s,
    nt_str_t *name, nt_uint_t key);


#if (NT_PCRE)

typedef struct {
    nt_uint_t                    capture;
    nt_int_t                     index;
} nt_stream_regex_variable_t;


typedef struct {
    nt_regex_t                  *regex;
    nt_uint_t                    ncaptures;
    nt_stream_regex_variable_t  *variables;
    nt_uint_t                    nvariables;
    nt_str_t                     name;
} nt_stream_regex_t;


typedef struct {
    nt_stream_regex_t           *regex;
    void                         *value;
} nt_stream_map_regex_t;


nt_stream_regex_t *nt_stream_regex_compile(nt_conf_t *cf,
    nt_regex_compile_t *rc);
nt_int_t nt_stream_regex_exec(nt_stream_session_t *s, nt_stream_regex_t *re,
    nt_str_t *str);

#endif


typedef struct {
    nt_hash_combined_t           hash;
#if (NT_PCRE)
    nt_stream_map_regex_t       *regex;
    nt_uint_t                    nregex;
#endif
} nt_stream_map_t;


void *nt_stream_map_find(nt_stream_session_t *s, nt_stream_map_t *map,
    nt_str_t *match);


nt_int_t nt_stream_variables_add_core_vars(nt_conf_t *cf);
nt_int_t nt_stream_variables_init_vars(nt_conf_t *cf);


extern nt_stream_variable_value_t  nt_stream_variable_null_value;
extern nt_stream_variable_value_t  nt_stream_variable_true_value;


#endif /* _NT_STREAM_VARIABLES_H_INCLUDED_ */
