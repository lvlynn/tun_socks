

#ifndef _NT_STREAM_SCRIPT_H_INCLUDED_
#define _NT_STREAM_SCRIPT_H_INCLUDED_


#include <nt_core.h>
#include <nt_stream.h>


typedef struct {
    u_char                       *ip;
    u_char                       *pos;
    nt_stream_variable_value_t  *sp;

    nt_str_t                     buf;
    nt_str_t                     line;

    unsigned                      flushed:1;
    unsigned                      skip:1;

    nt_stream_session_t         *session;
} nt_stream_script_engine_t;


typedef struct {
    nt_conf_t                   *cf;
    nt_str_t                    *source;

    nt_array_t                 **flushes;
    nt_array_t                 **lengths;
    nt_array_t                 **values;

    nt_uint_t                    variables;
    nt_uint_t                    ncaptures;
    nt_uint_t                    size;

    void                         *main;

    unsigned                      complete_lengths:1;
    unsigned                      complete_values:1;
    unsigned                      zero:1;
    unsigned                      conf_prefix:1;
    unsigned                      root_prefix:1;
} nt_stream_script_compile_t;


typedef struct {
    nt_str_t                     value;
    nt_uint_t                   *flushes;
    void                         *lengths;
    void                         *values;

    union {
        size_t                    size;
    } u;
} nt_stream_complex_value_t;


typedef struct {
    nt_conf_t                   *cf;
    nt_str_t                    *value;
    nt_stream_complex_value_t   *complex_value;

    unsigned                      zero:1;
    unsigned                      conf_prefix:1;
    unsigned                      root_prefix:1;
} nt_stream_compile_complex_value_t;


typedef void (*nt_stream_script_code_pt) (nt_stream_script_engine_t *e);
typedef size_t (*nt_stream_script_len_code_pt) (nt_stream_script_engine_t *e);


typedef struct {
    nt_stream_script_code_pt     code;
    uintptr_t                     len;
} nt_stream_script_copy_code_t;


typedef struct {
    nt_stream_script_code_pt     code;
    uintptr_t                     index;
} nt_stream_script_var_code_t;


typedef struct {
    nt_stream_script_code_pt     code;
    uintptr_t                     n;
} nt_stream_script_copy_capture_code_t;


typedef struct {
    nt_stream_script_code_pt     code;
    uintptr_t                     conf_prefix;
} nt_stream_script_full_name_code_t;


void nt_stream_script_flush_complex_value(nt_stream_session_t *s,
    nt_stream_complex_value_t *val);
nt_int_t nt_stream_complex_value(nt_stream_session_t *s,
    nt_stream_complex_value_t *val, nt_str_t *value);
size_t nt_stream_complex_value_size(nt_stream_session_t *s,
    nt_stream_complex_value_t *val, size_t default_value);
nt_int_t nt_stream_compile_complex_value(
    nt_stream_compile_complex_value_t *ccv);
char *nt_stream_set_complex_value_slot(nt_conf_t *cf, nt_command_t *cmd,
    void *conf);
char *nt_stream_set_complex_value_size_slot(nt_conf_t *cf, nt_command_t *cmd,
    void *conf);


nt_uint_t nt_stream_script_variables_count(nt_str_t *value);
nt_int_t nt_stream_script_compile(nt_stream_script_compile_t *sc);
u_char *nt_stream_script_run(nt_stream_session_t *s, nt_str_t *value,
    void *code_lengths, size_t reserved, void *code_values);
void nt_stream_script_flush_no_cacheable_variables(nt_stream_session_t *s,
    nt_array_t *indices);

void *nt_stream_script_add_code(nt_array_t *codes, size_t size, void *code);

size_t nt_stream_script_copy_len_code(nt_stream_script_engine_t *e);
void nt_stream_script_copy_code(nt_stream_script_engine_t *e);
size_t nt_stream_script_copy_var_len_code(nt_stream_script_engine_t *e);
void nt_stream_script_copy_var_code(nt_stream_script_engine_t *e);
size_t nt_stream_script_copy_capture_len_code(nt_stream_script_engine_t *e);
void nt_stream_script_copy_capture_code(nt_stream_script_engine_t *e);

#endif /* _NT_STREAM_SCRIPT_H_INCLUDED_ */
