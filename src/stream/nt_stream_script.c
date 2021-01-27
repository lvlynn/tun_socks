

#include <nt_core.h>
#include <nt_stream.h>


static nt_int_t nt_stream_script_init_arrays(
    nt_stream_script_compile_t *sc);
static nt_int_t nt_stream_script_done(nt_stream_script_compile_t *sc);
static nt_int_t nt_stream_script_add_copy_code(
    nt_stream_script_compile_t *sc, nt_str_t *value, nt_uint_t last);
static nt_int_t nt_stream_script_add_var_code(
    nt_stream_script_compile_t *sc, nt_str_t *name);
#if (NT_PCRE)
static nt_int_t nt_stream_script_add_capture_code(
    nt_stream_script_compile_t *sc, nt_uint_t n);
#endif
static nt_int_t nt_stream_script_add_full_name_code(
    nt_stream_script_compile_t *sc);
static size_t nt_stream_script_full_name_len_code(
    nt_stream_script_engine_t *e);
static void nt_stream_script_full_name_code(nt_stream_script_engine_t *e);


#define nt_stream_script_exit  (u_char *) &nt_stream_script_exit_code

static uintptr_t nt_stream_script_exit_code = (uintptr_t) NULL;


void
nt_stream_script_flush_complex_value(nt_stream_session_t *s,
    nt_stream_complex_value_t *val)
{
    nt_uint_t *index;

    index = val->flushes;

    if (index) {
        while (*index != (nt_uint_t) -1) {

            if (s->variables[*index].no_cacheable) {
                s->variables[*index].valid = 0;
                s->variables[*index].not_found = 0;
            }

            index++;
        }
    }
}


nt_int_t
nt_stream_complex_value(nt_stream_session_t *s,
    nt_stream_complex_value_t *val, nt_str_t *value)
{
    size_t                         len;
    nt_stream_script_code_pt      code;
    nt_stream_script_engine_t     e;
    nt_stream_script_len_code_pt  lcode;

    if (val->lengths == NULL) {
        *value = val->value;
        return NT_OK;
    }

    nt_stream_script_flush_complex_value(s, val);

    nt_memzero(&e, sizeof(nt_stream_script_engine_t));

    e.ip = val->lengths;
    e.session = s;
    e.flushed = 1;

    len = 0;

    while (*(uintptr_t *) e.ip) {
        lcode = *(nt_stream_script_len_code_pt *) e.ip;
        len += lcode(&e);
    }

    value->len = len;
    value->data = nt_pnalloc(s->connection->pool, len);
    if (value->data == NULL) {
        return NT_ERROR;
    }

    e.ip = val->values;
    e.pos = value->data;
    e.buf = *value;

    while (*(uintptr_t *) e.ip) {
        code = *(nt_stream_script_code_pt *) e.ip;
        code((nt_stream_script_engine_t *) &e);
    }

    *value = e.buf;

    return NT_OK;
}


size_t
nt_stream_complex_value_size(nt_stream_session_t *s,
    nt_stream_complex_value_t *val, size_t default_value)
{
    size_t     size;
    nt_str_t  value;

    if (val == NULL) {
        return default_value;
    }

    if (val->lengths == NULL) {
        return val->u.size;
    }

    if (nt_stream_complex_value(s, val, &value) != NT_OK) {
        return default_value;
    }

    size = nt_parse_size(&value);

    if (size == (size_t) NT_ERROR) {
        nt_log_error(NT_LOG_ERR, s->connection->log, 0,
                      "invalid size \"%V\"", &value);
        return default_value;
    }

    return size;
}


nt_int_t
nt_stream_compile_complex_value(nt_stream_compile_complex_value_t *ccv)
{
    nt_str_t                    *v;
    nt_uint_t                    i, n, nv, nc;
    nt_array_t                   flushes, lengths, values, *pf, *pl, *pv;
    nt_stream_script_compile_t   sc;

    v = ccv->value;

    nv = 0;
    nc = 0;

    for (i = 0; i < v->len; i++) {
        if (v->data[i] == '$') {
            if (v->data[i + 1] >= '1' && v->data[i + 1] <= '9') {
                nc++;

            } else {
                nv++;
            }
        }
    }

    if ((v->len == 0 || v->data[0] != '$')
        && (ccv->conf_prefix || ccv->root_prefix))
    {
        if (nt_conf_full_name(ccv->cf->cycle, v, ccv->conf_prefix) != NT_OK) {
            return NT_ERROR;
        }

        ccv->conf_prefix = 0;
        ccv->root_prefix = 0;
    }

    ccv->complex_value->value = *v;
    ccv->complex_value->flushes = NULL;
    ccv->complex_value->lengths = NULL;
    ccv->complex_value->values = NULL;

    if (nv == 0 && nc == 0) {
        return NT_OK;
    }

    n = nv + 1;

    if (nt_array_init(&flushes, ccv->cf->pool, n, sizeof(nt_uint_t))
        != NT_OK)
    {
        return NT_ERROR;
    }

    n = nv * (2 * sizeof(nt_stream_script_copy_code_t)
                  + sizeof(nt_stream_script_var_code_t))
        + sizeof(uintptr_t);

    if (nt_array_init(&lengths, ccv->cf->pool, n, 1) != NT_OK) {
        return NT_ERROR;
    }

    n = (nv * (2 * sizeof(nt_stream_script_copy_code_t)
                   + sizeof(nt_stream_script_var_code_t))
                + sizeof(uintptr_t)
                + v->len
                + sizeof(uintptr_t) - 1)
            & ~(sizeof(uintptr_t) - 1);

    if (nt_array_init(&values, ccv->cf->pool, n, 1) != NT_OK) {
        return NT_ERROR;
    }

    pf = &flushes;
    pl = &lengths;
    pv = &values;

    nt_memzero(&sc, sizeof(nt_stream_script_compile_t));

    sc.cf = ccv->cf;
    sc.source = v;
    sc.flushes = &pf;
    sc.lengths = &pl;
    sc.values = &pv;
    sc.complete_lengths = 1;
    sc.complete_values = 1;
    sc.zero = ccv->zero;
    sc.conf_prefix = ccv->conf_prefix;
    sc.root_prefix = ccv->root_prefix;

    if (nt_stream_script_compile(&sc) != NT_OK) {
        return NT_ERROR;
    }

    if (flushes.nelts) {
        ccv->complex_value->flushes = flushes.elts;
        ccv->complex_value->flushes[flushes.nelts] = (nt_uint_t) -1;
    }

    ccv->complex_value->lengths = lengths.elts;
    ccv->complex_value->values = values.elts;

    return NT_OK;
}


char *
nt_stream_set_complex_value_slot(nt_conf_t *cf, nt_command_t *cmd,
    void *conf)
{
    char  *p = conf;

    nt_str_t                            *value;
    nt_stream_complex_value_t          **cv;
    nt_stream_compile_complex_value_t    ccv;

    cv = (nt_stream_complex_value_t **) (p + cmd->offset);

    if (*cv != NULL) {
        return "is duplicate";
    }

    *cv = nt_palloc(cf->pool, sizeof(nt_stream_complex_value_t));
    if (*cv == NULL) {
        return NT_CONF_ERROR;
    }

    value = cf->args->elts;

    nt_memzero(&ccv, sizeof(nt_stream_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = *cv;

    if (nt_stream_compile_complex_value(&ccv) != NT_OK) {
        return NT_CONF_ERROR;
    }

    return NT_CONF_OK;
}


char *
nt_stream_set_complex_value_size_slot(nt_conf_t *cf, nt_command_t *cmd,
    void *conf)
{
    char  *p = conf;

    char                        *rv;
    nt_stream_complex_value_t  *cv;

    rv = nt_stream_set_complex_value_slot(cf, cmd, conf);

    if (rv != NT_CONF_OK) {
        return rv;
    }

    cv = *(nt_stream_complex_value_t **) (p + cmd->offset);

    if (cv->lengths) {
        return NT_CONF_OK;
    }

    cv->u.size = nt_parse_size(&cv->value);
    if (cv->u.size == (size_t) NT_ERROR) {
        return "invalid value";
    }

    return NT_CONF_OK;
}


nt_uint_t
nt_stream_script_variables_count(nt_str_t *value)
{
    nt_uint_t  i, n;

    for (n = 0, i = 0; i < value->len; i++) {
        if (value->data[i] == '$') {
            n++;
        }
    }

    return n;
}


nt_int_t
nt_stream_script_compile(nt_stream_script_compile_t *sc)
{
    u_char       ch;
    nt_str_t    name;
    nt_uint_t   i, bracket;

    if (nt_stream_script_init_arrays(sc) != NT_OK) {
        return NT_ERROR;
    }

    for (i = 0; i < sc->source->len; /* void */ ) {

        name.len = 0;

        if (sc->source->data[i] == '$') {

            if (++i == sc->source->len) {
                goto invalid_variable;
            }

            if (sc->source->data[i] >= '1' && sc->source->data[i] <= '9') {
#if (NT_PCRE)
                nt_uint_t  n;

                n = sc->source->data[i] - '0';

                if (nt_stream_script_add_capture_code(sc, n) != NT_OK) {
                    return NT_ERROR;
                }

                i++;

                continue;
#else
                nt_conf_log_error(NT_LOG_EMERG, sc->cf, 0,
                                   "using variable \"$%c\" requires "
                                   "PCRE library", sc->source->data[i]);
                return NT_ERROR;
#endif
            }

            if (sc->source->data[i] == '{') {
                bracket = 1;

                if (++i == sc->source->len) {
                    goto invalid_variable;
                }

                name.data = &sc->source->data[i];

            } else {
                bracket = 0;
                name.data = &sc->source->data[i];
            }

            for ( /* void */ ; i < sc->source->len; i++, name.len++) {
                ch = sc->source->data[i];

                if (ch == '}' && bracket) {
                    i++;
                    bracket = 0;
                    break;
                }

                if ((ch >= 'A' && ch <= 'Z')
                    || (ch >= 'a' && ch <= 'z')
                    || (ch >= '0' && ch <= '9')
                    || ch == '_')
                {
                    continue;
                }

                break;
            }

            if (bracket) {
                nt_conf_log_error(NT_LOG_EMERG, sc->cf, 0,
                                   "the closing bracket in \"%V\" "
                                   "variable is missing", &name);
                return NT_ERROR;
            }

            if (name.len == 0) {
                goto invalid_variable;
            }

            sc->variables++;

            if (nt_stream_script_add_var_code(sc, &name) != NT_OK) {
                return NT_ERROR;
            }

            continue;
        }

        name.data = &sc->source->data[i];

        while (i < sc->source->len) {

            if (sc->source->data[i] == '$') {
                break;
            }

            i++;
            name.len++;
        }

        sc->size += name.len;

        if (nt_stream_script_add_copy_code(sc, &name, (i == sc->source->len))
            != NT_OK)
        {
            return NT_ERROR;
        }
    }

    return nt_stream_script_done(sc);

invalid_variable:

    nt_conf_log_error(NT_LOG_EMERG, sc->cf, 0, "invalid variable name");

    return NT_ERROR;
}


u_char *
nt_stream_script_run(nt_stream_session_t *s, nt_str_t *value,
    void *code_lengths, size_t len, void *code_values)
{
    nt_uint_t                      i;
    nt_stream_script_code_pt       code;
    nt_stream_script_engine_t      e;
    nt_stream_core_main_conf_t    *cmcf;
    nt_stream_script_len_code_pt   lcode;

    cmcf = nt_stream_get_module_main_conf(s, nt_stream_core_module);

    for (i = 0; i < cmcf->variables.nelts; i++) {
        if (s->variables[i].no_cacheable) {
            s->variables[i].valid = 0;
            s->variables[i].not_found = 0;
        }
    }

    nt_memzero(&e, sizeof(nt_stream_script_engine_t));

    e.ip = code_lengths;
    e.session = s;
    e.flushed = 1;

    while (*(uintptr_t *) e.ip) {
        lcode = *(nt_stream_script_len_code_pt *) e.ip;
        len += lcode(&e);
    }


    value->len = len;
    value->data = nt_pnalloc(s->connection->pool, len);
    if (value->data == NULL) {
        return NULL;
    }

    e.ip = code_values;
    e.pos = value->data;

    while (*(uintptr_t *) e.ip) {
        code = *(nt_stream_script_code_pt *) e.ip;
        code((nt_stream_script_engine_t *) &e);
    }

    return e.pos;
}


void
nt_stream_script_flush_no_cacheable_variables(nt_stream_session_t *s,
    nt_array_t *indices)
{
    nt_uint_t  n, *index;

    if (indices) {
        index = indices->elts;
        for (n = 0; n < indices->nelts; n++) {
            if (s->variables[index[n]].no_cacheable) {
                s->variables[index[n]].valid = 0;
                s->variables[index[n]].not_found = 0;
            }
        }
    }
}


static nt_int_t
nt_stream_script_init_arrays(nt_stream_script_compile_t *sc)
{
    nt_uint_t   n;

    if (sc->flushes && *sc->flushes == NULL) {
        n = sc->variables ? sc->variables : 1;
        *sc->flushes = nt_array_create(sc->cf->pool, n, sizeof(nt_uint_t));
        if (*sc->flushes == NULL) {
            return NT_ERROR;
        }
    }

    if (*sc->lengths == NULL) {
        n = sc->variables * (2 * sizeof(nt_stream_script_copy_code_t)
                             + sizeof(nt_stream_script_var_code_t))
            + sizeof(uintptr_t);

        *sc->lengths = nt_array_create(sc->cf->pool, n, 1);
        if (*sc->lengths == NULL) {
            return NT_ERROR;
        }
    }

    if (*sc->values == NULL) {
        n = (sc->variables * (2 * sizeof(nt_stream_script_copy_code_t)
                              + sizeof(nt_stream_script_var_code_t))
                + sizeof(uintptr_t)
                + sc->source->len
                + sizeof(uintptr_t) - 1)
            & ~(sizeof(uintptr_t) - 1);

        *sc->values = nt_array_create(sc->cf->pool, n, 1);
        if (*sc->values == NULL) {
            return NT_ERROR;
        }
    }

    sc->variables = 0;

    return NT_OK;
}


static nt_int_t
nt_stream_script_done(nt_stream_script_compile_t *sc)
{
    nt_str_t    zero;
    uintptr_t   *code;

    if (sc->zero) {

        zero.len = 1;
        zero.data = (u_char *) "\0";

        if (nt_stream_script_add_copy_code(sc, &zero, 0) != NT_OK) {
            return NT_ERROR;
        }
    }

    if (sc->conf_prefix || sc->root_prefix) {
        if (nt_stream_script_add_full_name_code(sc) != NT_OK) {
            return NT_ERROR;
        }
    }

    if (sc->complete_lengths) {
        code = nt_stream_script_add_code(*sc->lengths, sizeof(uintptr_t),
                                          NULL);
        if (code == NULL) {
            return NT_ERROR;
        }

        *code = (uintptr_t) NULL;
    }

    if (sc->complete_values) {
        code = nt_stream_script_add_code(*sc->values, sizeof(uintptr_t),
                                          &sc->main);
        if (code == NULL) {
            return NT_ERROR;
        }

        *code = (uintptr_t) NULL;
    }

    return NT_OK;
}


void *
nt_stream_script_add_code(nt_array_t *codes, size_t size, void *code)
{
    u_char  *elts, **p;
    void    *new;

    elts = codes->elts;

    new = nt_array_push_n(codes, size);
    if (new == NULL) {
        return NULL;
    }

    if (code) {
        if (elts != codes->elts) {
            p = code;
            *p += (u_char *) codes->elts - elts;
        }
    }

    return new;
}


static nt_int_t
nt_stream_script_add_copy_code(nt_stream_script_compile_t *sc,
    nt_str_t *value, nt_uint_t last)
{
    u_char                         *p;
    size_t                          size, len, zero;
    nt_stream_script_copy_code_t  *code;

    zero = (sc->zero && last);
    len = value->len + zero;

    code = nt_stream_script_add_code(*sc->lengths,
                                      sizeof(nt_stream_script_copy_code_t),
                                      NULL);
    if (code == NULL) {
        return NT_ERROR;
    }

    code->code = (nt_stream_script_code_pt) (void *)
                                               nt_stream_script_copy_len_code;
    code->len = len;

    size = (sizeof(nt_stream_script_copy_code_t) + len + sizeof(uintptr_t) - 1)
            & ~(sizeof(uintptr_t) - 1);

    code = nt_stream_script_add_code(*sc->values, size, &sc->main);
    if (code == NULL) {
        return NT_ERROR;
    }

    code->code = nt_stream_script_copy_code;
    code->len = len;

    p = nt_cpymem((u_char *) code + sizeof(nt_stream_script_copy_code_t),
                   value->data, value->len);

    if (zero) {
        *p = '\0';
        sc->zero = 0;
    }

    return NT_OK;
}


size_t
nt_stream_script_copy_len_code(nt_stream_script_engine_t *e)
{
    nt_stream_script_copy_code_t  *code;

    code = (nt_stream_script_copy_code_t *) e->ip;

    e->ip += sizeof(nt_stream_script_copy_code_t);

    return code->len;
}


void
nt_stream_script_copy_code(nt_stream_script_engine_t *e)
{
    u_char                         *p;
    nt_stream_script_copy_code_t  *code;

    code = (nt_stream_script_copy_code_t *) e->ip;

    p = e->pos;

    if (!e->skip) {
        e->pos = nt_copy(p, e->ip + sizeof(nt_stream_script_copy_code_t),
                          code->len);
    }

    e->ip += sizeof(nt_stream_script_copy_code_t)
          + ((code->len + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1));

    nt_log_debug2(NT_LOG_DEBUG_STREAM, e->session->connection->log, 0,
                   "stream script copy: \"%*s\"", e->pos - p, p);
}


static nt_int_t
nt_stream_script_add_var_code(nt_stream_script_compile_t *sc, nt_str_t *name)
{
    nt_int_t                      index, *p;
    nt_stream_script_var_code_t  *code;

    index = nt_stream_get_variable_index(sc->cf, name);

    if (index == NT_ERROR) {
        return NT_ERROR;
    }

    if (sc->flushes) {
        p = nt_array_push(*sc->flushes);
        if (p == NULL) {
            return NT_ERROR;
        }

        *p = index;
    }

    code = nt_stream_script_add_code(*sc->lengths,
                                      sizeof(nt_stream_script_var_code_t),
                                      NULL);
    if (code == NULL) {
        return NT_ERROR;
    }

    code->code = (nt_stream_script_code_pt) (void *)
                                           nt_stream_script_copy_var_len_code;
    code->index = (uintptr_t) index;

    code = nt_stream_script_add_code(*sc->values,
                                      sizeof(nt_stream_script_var_code_t),
                                      &sc->main);
    if (code == NULL) {
        return NT_ERROR;
    }

    code->code = nt_stream_script_copy_var_code;
    code->index = (uintptr_t) index;

    return NT_OK;
}


size_t
nt_stream_script_copy_var_len_code(nt_stream_script_engine_t *e)
{
    nt_stream_variable_value_t   *value;
    nt_stream_script_var_code_t  *code;

    code = (nt_stream_script_var_code_t *) e->ip;

    e->ip += sizeof(nt_stream_script_var_code_t);

    if (e->flushed) {
        value = nt_stream_get_indexed_variable(e->session, code->index);

    } else {
        value = nt_stream_get_flushed_variable(e->session, code->index);
    }

    if (value && !value->not_found) {
        return value->len;
    }

    return 0;
}


void
nt_stream_script_copy_var_code(nt_stream_script_engine_t *e)
{
    u_char                        *p;
    nt_stream_variable_value_t   *value;
    nt_stream_script_var_code_t  *code;

    code = (nt_stream_script_var_code_t *) e->ip;

    e->ip += sizeof(nt_stream_script_var_code_t);

    if (!e->skip) {

        if (e->flushed) {
            value = nt_stream_get_indexed_variable(e->session, code->index);

        } else {
            value = nt_stream_get_flushed_variable(e->session, code->index);
        }

        if (value && !value->not_found) {
            p = e->pos;
            e->pos = nt_copy(p, value->data, value->len);

            nt_log_debug2(NT_LOG_DEBUG_STREAM,
                           e->session->connection->log, 0,
                           "stream script var: \"%*s\"", e->pos - p, p);
        }
    }
}


#if (NT_PCRE)

static nt_int_t
nt_stream_script_add_capture_code(nt_stream_script_compile_t *sc,
    nt_uint_t n)
{
    nt_stream_script_copy_capture_code_t  *code;

    code = nt_stream_script_add_code(*sc->lengths,
                                  sizeof(nt_stream_script_copy_capture_code_t),
                                  NULL);
    if (code == NULL) {
        return NT_ERROR;
    }

    code->code = (nt_stream_script_code_pt) (void *)
                                       nt_stream_script_copy_capture_len_code;
    code->n = 2 * n;


    code = nt_stream_script_add_code(*sc->values,
                                  sizeof(nt_stream_script_copy_capture_code_t),
                                  &sc->main);
    if (code == NULL) {
        return NT_ERROR;
    }

    code->code = nt_stream_script_copy_capture_code;
    code->n = 2 * n;

    if (sc->ncaptures < n) {
        sc->ncaptures = n;
    }

    return NT_OK;
}


size_t
nt_stream_script_copy_capture_len_code(nt_stream_script_engine_t *e)
{
    int                                    *cap;
    nt_uint_t                              n;
    nt_stream_session_t                   *s;
    nt_stream_script_copy_capture_code_t  *code;

    s = e->session;

    code = (nt_stream_script_copy_capture_code_t *) e->ip;

    e->ip += sizeof(nt_stream_script_copy_capture_code_t);

    n = code->n;

    if (n < s->ncaptures) {
        cap = s->captures;
        return cap[n + 1] - cap[n];
    }

    return 0;
}


void
nt_stream_script_copy_capture_code(nt_stream_script_engine_t *e)
{
    int                                    *cap;
    u_char                                 *p, *pos;
    nt_uint_t                              n;
    nt_stream_session_t                   *s;
    nt_stream_script_copy_capture_code_t  *code;

    s = e->session;

    code = (nt_stream_script_copy_capture_code_t *) e->ip;

    e->ip += sizeof(nt_stream_script_copy_capture_code_t);

    n = code->n;

    pos = e->pos;

    if (n < s->ncaptures) {
        cap = s->captures;
        p = s->captures_data;
        e->pos = nt_copy(pos, &p[cap[n]], cap[n + 1] - cap[n]);
    }

    nt_log_debug2(NT_LOG_DEBUG_STREAM, e->session->connection->log, 0,
                   "stream script capture: \"%*s\"", e->pos - pos, pos);
}

#endif


static nt_int_t
nt_stream_script_add_full_name_code(nt_stream_script_compile_t *sc)
{
    nt_stream_script_full_name_code_t  *code;

    code = nt_stream_script_add_code(*sc->lengths,
                                    sizeof(nt_stream_script_full_name_code_t),
                                    NULL);
    if (code == NULL) {
        return NT_ERROR;
    }

    code->code = (nt_stream_script_code_pt) (void *)
                                          nt_stream_script_full_name_len_code;
    code->conf_prefix = sc->conf_prefix;

    code = nt_stream_script_add_code(*sc->values,
                        sizeof(nt_stream_script_full_name_code_t), &sc->main);
    if (code == NULL) {
        return NT_ERROR;
    }

    code->code = nt_stream_script_full_name_code;
    code->conf_prefix = sc->conf_prefix;

    return NT_OK;
}


static size_t
nt_stream_script_full_name_len_code(nt_stream_script_engine_t *e)
{
    nt_stream_script_full_name_code_t  *code;

    code = (nt_stream_script_full_name_code_t *) e->ip;

    e->ip += sizeof(nt_stream_script_full_name_code_t);

    return code->conf_prefix ? nt_cycle->conf_prefix.len:
                               nt_cycle->prefix.len;
}


static void
nt_stream_script_full_name_code(nt_stream_script_engine_t *e)
{
    nt_stream_script_full_name_code_t  *code;

    nt_str_t  value, *prefix;

    code = (nt_stream_script_full_name_code_t *) e->ip;

    value.data = e->buf.data;
    value.len = e->pos - e->buf.data;

    prefix = code->conf_prefix ? (nt_str_t *) &nt_cycle->conf_prefix:
                                 (nt_str_t *) &nt_cycle->prefix;

    if (nt_get_full_name(e->session->connection->pool, prefix, &value)
        != NT_OK)
    {
        e->ip = nt_stream_script_exit;
        return;
    }

    e->buf = value;

    nt_log_debug1(NT_LOG_DEBUG_STREAM, e->session->connection->log, 0,
                   "stream script fullname: \"%V\"", &value);

    e->ip += sizeof(nt_stream_script_full_name_code_t);
}
