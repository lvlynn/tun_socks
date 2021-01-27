

#include <nt_core.h>
#include <nt_stream.h>
//#include <nginx.h>

static nt_stream_variable_t *nt_stream_add_prefix_variable(nt_conf_t *cf,
    nt_str_t *name, nt_uint_t flags);

static nt_int_t nt_stream_variable_binary_remote_addr(
    nt_stream_session_t *s, nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_remote_addr(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_remote_port(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_proxy_protocol_addr(
    nt_stream_session_t *s, nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_proxy_protocol_port(
    nt_stream_session_t *s, nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_server_addr(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_server_port(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_bytes(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_session_time(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_status(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_connection(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);

static nt_int_t nt_stream_variable_nginx_version(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_hostname(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_pid(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_msec(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_time_iso8601(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_time_local(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);
static nt_int_t nt_stream_variable_protocol(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data);


static nt_stream_variable_t  nt_stream_core_variables[] = {

    { nt_string("binary_remote_addr"), NULL,
      nt_stream_variable_binary_remote_addr, 0, 0, 0 },

    { nt_string("remote_addr"), NULL,
      nt_stream_variable_remote_addr, 0, 0, 0 },

    { nt_string("remote_port"), NULL,
      nt_stream_variable_remote_port, 0, 0, 0 },

    { nt_string("proxy_protocol_addr"), NULL,
      nt_stream_variable_proxy_protocol_addr, 0, 0, 0 },

    { nt_string("proxy_protocol_port"), NULL,
      nt_stream_variable_proxy_protocol_port, 0, 0, 0 },

    { nt_string("server_addr"), NULL,
      nt_stream_variable_server_addr, 0, 0, 0 },

    { nt_string("server_port"), NULL,
      nt_stream_variable_server_port, 0, 0, 0 },

    { nt_string("bytes_sent"), NULL, nt_stream_variable_bytes,
      0, 0, 0 },

    { nt_string("bytes_received"), NULL, nt_stream_variable_bytes,
      1, 0, 0 },

    { nt_string("session_time"), NULL, nt_stream_variable_session_time,
      0, NT_STREAM_VAR_NOCACHEABLE, 0 },

    { nt_string("status"), NULL, nt_stream_variable_status,
      0, NT_STREAM_VAR_NOCACHEABLE, 0 },

    { nt_string("connection"), NULL,
      nt_stream_variable_connection, 0, 0, 0 },

    { nt_string("nginx_version"), NULL, nt_stream_variable_nginx_version,
      0, 0, 0 },

    { nt_string("hostname"), NULL, nt_stream_variable_hostname,
      0, 0, 0 },

    { nt_string("pid"), NULL, nt_stream_variable_pid,
      0, 0, 0 },

    { nt_string("msec"), NULL, nt_stream_variable_msec,
      0, NT_STREAM_VAR_NOCACHEABLE, 0 },

    { nt_string("time_iso8601"), NULL, nt_stream_variable_time_iso8601,
      0, NT_STREAM_VAR_NOCACHEABLE, 0 },

    { nt_string("time_local"), NULL, nt_stream_variable_time_local,
      0, NT_STREAM_VAR_NOCACHEABLE, 0 },

    { nt_string("protocol"), NULL,
      nt_stream_variable_protocol, 0, 0, 0 },

      nt_stream_null_variable
};


nt_stream_variable_value_t  nt_stream_variable_null_value =
    nt_stream_variable("");
nt_stream_variable_value_t  nt_stream_variable_true_value =
    nt_stream_variable("1");


static nt_uint_t  nt_stream_variable_depth = 100;


nt_stream_variable_t *
nt_stream_add_variable(nt_conf_t *cf, nt_str_t *name, nt_uint_t flags)
{
    nt_int_t                     rc;
    nt_uint_t                    i;
    nt_hash_key_t               *key;
    nt_stream_variable_t        *v;
    nt_stream_core_main_conf_t  *cmcf;

    if (name->len == 0) {
        nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                           "invalid variable name \"$\"");
        return NULL;
    }

    if (flags & NT_STREAM_VAR_PREFIX) {
        return nt_stream_add_prefix_variable(cf, name, flags);
    }

    cmcf = nt_stream_conf_get_module_main_conf(cf, nt_stream_core_module);

    key = cmcf->variables_keys->keys.elts;
    for (i = 0; i < cmcf->variables_keys->keys.nelts; i++) {
        if (name->len != key[i].key.len
            || nt_strncasecmp(name->data, key[i].key.data, name->len) != 0)
        {
            continue;
        }

        v = key[i].value;

        if (!(v->flags & NT_STREAM_VAR_CHANGEABLE)) {
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "the duplicate \"%V\" variable", name);
            return NULL;
        }

        if (!(flags & NT_STREAM_VAR_WEAK)) {
            v->flags &= ~NT_STREAM_VAR_WEAK;
        }

        return v;
    }

    v = nt_palloc(cf->pool, sizeof(nt_stream_variable_t));
    if (v == NULL) {
        return NULL;
    }

    v->name.len = name->len;
    v->name.data = nt_pnalloc(cf->pool, name->len);
    if (v->name.data == NULL) {
        return NULL;
    }

    nt_strlow(v->name.data, name->data, name->len);

    v->set_handler = NULL;
    v->get_handler = NULL;
    v->data = 0;
    v->flags = flags;
    v->index = 0;

    rc = nt_hash_add_key(cmcf->variables_keys, &v->name, v, 0);

    if (rc == NT_ERROR) {
        return NULL;
    }

    if (rc == NT_BUSY) {
        nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                           "conflicting variable name \"%V\"", name);
        return NULL;
    }

    return v;
}


static nt_stream_variable_t *
nt_stream_add_prefix_variable(nt_conf_t *cf, nt_str_t *name,
    nt_uint_t flags)
{
    nt_uint_t                    i;
    nt_stream_variable_t        *v;
    nt_stream_core_main_conf_t  *cmcf;

    cmcf = nt_stream_conf_get_module_main_conf(cf, nt_stream_core_module);

    v = cmcf->prefix_variables.elts;
    for (i = 0; i < cmcf->prefix_variables.nelts; i++) {
        if (name->len != v[i].name.len
            || nt_strncasecmp(name->data, v[i].name.data, name->len) != 0)
        {
            continue;
        }

        v = &v[i];

        if (!(v->flags & NT_STREAM_VAR_CHANGEABLE)) {
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "the duplicate \"%V\" variable", name);
            return NULL;
        }

        if (!(flags & NT_STREAM_VAR_WEAK)) {
            v->flags &= ~NT_STREAM_VAR_WEAK;
        }

        return v;
    }

    v = nt_array_push(&cmcf->prefix_variables);
    if (v == NULL) {
        return NULL;
    }

    v->name.len = name->len;
    v->name.data = nt_pnalloc(cf->pool, name->len);
    if (v->name.data == NULL) {
        return NULL;
    }

    nt_strlow(v->name.data, name->data, name->len);

    v->set_handler = NULL;
    v->get_handler = NULL;
    v->data = 0;
    v->flags = flags;
    v->index = 0;

    return v;
}


nt_int_t
nt_stream_get_variable_index(nt_conf_t *cf, nt_str_t *name)
{
    nt_uint_t                    i;
    nt_stream_variable_t        *v;
    nt_stream_core_main_conf_t  *cmcf;

    if (name->len == 0) {
        nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                           "invalid variable name \"$\"");
        return NT_ERROR;
    }

    cmcf = nt_stream_conf_get_module_main_conf(cf, nt_stream_core_module);

    v = cmcf->variables.elts;

    if (v == NULL) {
        if (nt_array_init(&cmcf->variables, cf->pool, 4,
                           sizeof(nt_stream_variable_t))
            != NT_OK)
        {
            return NT_ERROR;
        }

    } else {
        for (i = 0; i < cmcf->variables.nelts; i++) {
            if (name->len != v[i].name.len
                || nt_strncasecmp(name->data, v[i].name.data, name->len) != 0)
            {
                continue;
            }

            return i;
        }
    }

    v = nt_array_push(&cmcf->variables);
    if (v == NULL) {
        return NT_ERROR;
    }

    v->name.len = name->len;
    v->name.data = nt_pnalloc(cf->pool, name->len);
    if (v->name.data == NULL) {
        return NT_ERROR;
    }

    nt_strlow(v->name.data, name->data, name->len);

    v->set_handler = NULL;
    v->get_handler = NULL;
    v->data = 0;
    v->flags = 0;
    v->index = cmcf->variables.nelts - 1;

    return v->index;
}


nt_stream_variable_value_t *
nt_stream_get_indexed_variable(nt_stream_session_t *s, nt_uint_t index)
{
    nt_stream_variable_t        *v;
    nt_stream_core_main_conf_t  *cmcf;

    cmcf = nt_stream_get_module_main_conf(s, nt_stream_core_module);

    if (cmcf->variables.nelts <= index) {
        nt_log_error(NT_LOG_ALERT, s->connection->log, 0,
                      "unknown variable index: %ui", index);
        return NULL;
    }

    if (s->variables[index].not_found || s->variables[index].valid) {
        return &s->variables[index];
    }

    v = cmcf->variables.elts;

    if (nt_stream_variable_depth == 0) {
        nt_log_error(NT_LOG_ERR, s->connection->log, 0,
                      "cycle while evaluating variable \"%V\"",
                      &v[index].name);
        return NULL;
    }

    nt_stream_variable_depth--;

    if (v[index].get_handler(s, &s->variables[index], v[index].data)
        == NT_OK)
    {
        nt_stream_variable_depth++;

        if (v[index].flags & NT_STREAM_VAR_NOCACHEABLE) {
            s->variables[index].no_cacheable = 1;
        }

        return &s->variables[index];
    }

    nt_stream_variable_depth++;

    s->variables[index].valid = 0;
    s->variables[index].not_found = 1;

    return NULL;
}


nt_stream_variable_value_t *
nt_stream_get_flushed_variable(nt_stream_session_t *s, nt_uint_t index)
{
    nt_stream_variable_value_t  *v;

    v = &s->variables[index];

    if (v->valid || v->not_found) {
        if (!v->no_cacheable) {
            return v;
        }

        v->valid = 0;
        v->not_found = 0;
    }

    return nt_stream_get_indexed_variable(s, index);
}


nt_stream_variable_value_t *
nt_stream_get_variable(nt_stream_session_t *s, nt_str_t *name,
    nt_uint_t key)
{
    size_t                        len;
    nt_uint_t                    i, n;
    nt_stream_variable_t        *v;
    nt_stream_variable_value_t  *vv;
    nt_stream_core_main_conf_t  *cmcf;

    cmcf = nt_stream_get_module_main_conf(s, nt_stream_core_module);

    v = nt_hash_find(&cmcf->variables_hash, key, name->data, name->len);

    if (v) {
        if (v->flags & NT_STREAM_VAR_INDEXED) {
            return nt_stream_get_flushed_variable(s, v->index);
        }

        if (nt_stream_variable_depth == 0) {
            nt_log_error(NT_LOG_ERR, s->connection->log, 0,
                          "cycle while evaluating variable \"%V\"", name);
            return NULL;
        }

        nt_stream_variable_depth--;

        vv = nt_palloc(s->connection->pool,
                        sizeof(nt_stream_variable_value_t));

        if (vv && v->get_handler(s, vv, v->data) == NT_OK) {
            nt_stream_variable_depth++;
            return vv;
        }

        nt_stream_variable_depth++;
        return NULL;
    }

    vv = nt_palloc(s->connection->pool, sizeof(nt_stream_variable_value_t));
    if (vv == NULL) {
        return NULL;
    }

    len = 0;

    v = cmcf->prefix_variables.elts;
    n = cmcf->prefix_variables.nelts;

    for (i = 0; i < cmcf->prefix_variables.nelts; i++) {
        if (name->len >= v[i].name.len && name->len > len
            && nt_strncmp(name->data, v[i].name.data, v[i].name.len) == 0)
        {
            len = v[i].name.len;
            n = i;
        }
    }

    if (n != cmcf->prefix_variables.nelts) {
        if (v[n].get_handler(s, vv, (uintptr_t) name) == NT_OK) {
            return vv;
        }

        return NULL;
    }

    vv->not_found = 1;

    return vv;
}


static nt_int_t
nt_stream_variable_binary_remote_addr(nt_stream_session_t *s,
     nt_stream_variable_value_t *v, uintptr_t data)
{
    struct sockaddr_in   *sin;
#if (NT_HAVE_INET6)
    struct sockaddr_in6  *sin6;
#endif

    switch (s->connection->sockaddr->sa_family) {

#if (NT_HAVE_INET6)
    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) s->connection->sockaddr;

        v->len = sizeof(struct in6_addr);
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = sin6->sin6_addr.s6_addr;

        break;
#endif

#if (NT_HAVE_UNIX_DOMAIN)
    case AF_UNIX:

        v->len = s->connection->addr_text.len;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = s->connection->addr_text.data;

        break;
#endif

    default: /* AF_INET */
        sin = (struct sockaddr_in *) s->connection->sockaddr;

        v->len = sizeof(in_addr_t);
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = (u_char *) &sin->sin_addr;

        break;
    }

    return NT_OK;
}


static nt_int_t
nt_stream_variable_remote_addr(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    v->len = s->connection->addr_text.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = s->connection->addr_text.data;

    return NT_OK;
}


static nt_int_t
nt_stream_variable_remote_port(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    nt_uint_t  port;

    v->len = 0;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    v->data = nt_pnalloc(s->connection->pool, sizeof("65535") - 1);
    if (v->data == NULL) {
        return NT_ERROR;
    }

    port = nt_inet_get_port(s->connection->sockaddr);

    if (port > 0 && port < 65536) {
        v->len = nt_sprintf(v->data, "%ui", port) - v->data;
    }

    return NT_OK;
}


static nt_int_t
nt_stream_variable_proxy_protocol_addr(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    v->len = s->connection->proxy_protocol_addr.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = s->connection->proxy_protocol_addr.data;

    return NT_OK;
}


static nt_int_t
nt_stream_variable_proxy_protocol_port(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    nt_uint_t  port;

    v->len = 0;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    v->data = nt_pnalloc(s->connection->pool, sizeof("65535") - 1);
    if (v->data == NULL) {
        return NT_ERROR;
    }

    port = s->connection->proxy_protocol_port;

    if (port > 0 && port < 65536) {
        v->len = nt_sprintf(v->data, "%ui", port) - v->data;
    }

    return NT_OK;
}


static nt_int_t
nt_stream_variable_server_addr(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    nt_str_t  str;
    u_char     addr[NT_SOCKADDR_STRLEN];

    str.len = NT_SOCKADDR_STRLEN;
    str.data = addr;

    if (nt_connection_local_sockaddr(s->connection, &str, 0) != NT_OK) {
        return NT_ERROR;
    }

    str.data = nt_pnalloc(s->connection->pool, str.len);
    if (str.data == NULL) {
        return NT_ERROR;
    }

    nt_memcpy(str.data, addr, str.len);

    v->len = str.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = str.data;

    return NT_OK;
}


static nt_int_t
nt_stream_variable_server_port(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    nt_uint_t  port;

    v->len = 0;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    if (nt_connection_local_sockaddr(s->connection, NULL, 0) != NT_OK) {
        return NT_ERROR;
    }

    v->data = nt_pnalloc(s->connection->pool, sizeof("65535") - 1);
    if (v->data == NULL) {
        return NT_ERROR;
    }

    port = nt_inet_get_port(s->connection->local_sockaddr);

    if (port > 0 && port < 65536) {
        v->len = nt_sprintf(v->data, "%ui", port) - v->data;
    }

    return NT_OK;
}


static nt_int_t
nt_stream_variable_bytes(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    p = nt_pnalloc(s->connection->pool, NT_OFF_T_LEN);
    if (p == NULL) {
        return NT_ERROR;
    }

    if (data == 1) {
        v->len = nt_sprintf(p, "%O", s->received) - p;

    } else {
        v->len = nt_sprintf(p, "%O", s->connection->sent) - p;
    }

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NT_OK;
}


static nt_int_t
nt_stream_variable_session_time(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    u_char          *p;
    nt_time_t      *tp;
    nt_msec_int_t   ms;

    p = nt_pnalloc(s->connection->pool, NT_TIME_T_LEN + 4);
    if (p == NULL) {
        return NT_ERROR;
    }

    tp = nt_timeofday();

    ms = (nt_msec_int_t)
             ((tp->sec - s->start_sec) * 1000 + (tp->msec - s->start_msec));
    ms = nt_max(ms, 0);

    v->len = nt_sprintf(p, "%T.%03M", (time_t) ms / 1000, ms % 1000) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NT_OK;
}


static nt_int_t
nt_stream_variable_status(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    v->data = nt_pnalloc(s->connection->pool, NT_INT_T_LEN);
    if (v->data == NULL) {
        return NT_ERROR;
    }

    v->len = nt_sprintf(v->data, "%03ui", s->status) - v->data;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NT_OK;
}


static nt_int_t
nt_stream_variable_connection(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    p = nt_pnalloc(s->connection->pool, NT_ATOMIC_T_LEN);
    if (p == NULL) {
        return NT_ERROR;
    }

    v->len = nt_sprintf(p, "%uA", s->connection->number) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NT_OK;
}


static nt_int_t
nt_stream_variable_nginx_version(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    v->len = sizeof(NT_VERSION) - 1;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = (u_char *) NT_VERSION;

    return NT_OK;
}


static nt_int_t
nt_stream_variable_hostname(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    v->len = nt_cycle->hostname.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = nt_cycle->hostname.data;

    return NT_OK;
}


static nt_int_t
nt_stream_variable_pid(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    p = nt_pnalloc(s->connection->pool, NT_INT64_LEN);
    if (p == NULL) {
        return NT_ERROR;
    }

    v->len = nt_sprintf(p, "%P", nt_pid) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NT_OK;
}


static nt_int_t
nt_stream_variable_msec(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    u_char      *p;
    nt_time_t  *tp;

    p = nt_pnalloc(s->connection->pool, NT_TIME_T_LEN + 4);
    if (p == NULL) {
        return NT_ERROR;
    }

    tp = nt_timeofday();

    v->len = nt_sprintf(p, "%T.%03M", tp->sec, tp->msec) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NT_OK;
}


static nt_int_t
nt_stream_variable_time_iso8601(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    p = nt_pnalloc(s->connection->pool, nt_cached_http_log_iso8601.len);
    if (p == NULL) {
        return NT_ERROR;
    }

    nt_memcpy(p, nt_cached_http_log_iso8601.data,
               nt_cached_http_log_iso8601.len);

    v->len = nt_cached_http_log_iso8601.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NT_OK;
}


static nt_int_t
nt_stream_variable_time_local(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    p = nt_pnalloc(s->connection->pool, nt_cached_http_log_time.len);
    if (p == NULL) {
        return NT_ERROR;
    }

    nt_memcpy(p, nt_cached_http_log_time.data, nt_cached_http_log_time.len);

    v->len = nt_cached_http_log_time.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NT_OK;
}


static nt_int_t
nt_stream_variable_protocol(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    v->len = 3;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = (u_char *) (s->connection->type == SOCK_DGRAM ? "UDP" : "TCP");

    return NT_OK;
}


void *
nt_stream_map_find(nt_stream_session_t *s, nt_stream_map_t *map,
    nt_str_t *match)
{
    void        *value;
    u_char      *low;
    size_t       len;
    nt_uint_t   key;

    len = match->len;

    if (len) {
        low = nt_pnalloc(s->connection->pool, len);
        if (low == NULL) {
            return NULL;
        }

    } else {
        low = NULL;
    }

    key = nt_hash_strlow(low, match->data, len);

    value = nt_hash_find_combined(&map->hash, key, low, len);
    if (value) {
        return value;
    }

#if (NT_PCRE)

    if (len && map->nregex) {
        nt_int_t                n;
        nt_uint_t               i;
        nt_stream_map_regex_t  *reg;

        reg = map->regex;

        for (i = 0; i < map->nregex; i++) {

            n = nt_stream_regex_exec(s, reg[i].regex, match);

            if (n == NT_OK) {
                return reg[i].value;
            }

            if (n == NT_DECLINED) {
                continue;
            }

            /* NT_ERROR */

            return NULL;
        }
    }

#endif

    return NULL;
}


#if (NT_PCRE)

static nt_int_t
nt_stream_variable_not_found(nt_stream_session_t *s,
    nt_stream_variable_value_t *v, uintptr_t data)
{
    v->not_found = 1;
    return NT_OK;
}


nt_stream_regex_t *
nt_stream_regex_compile(nt_conf_t *cf, nt_regex_compile_t *rc)
{
    u_char                       *p;
    size_t                        size;
    nt_str_t                     name;
    nt_uint_t                    i, n;
    nt_stream_variable_t        *v;
    nt_stream_regex_t           *re;
    nt_stream_regex_variable_t  *rv;
    nt_stream_core_main_conf_t  *cmcf;

    rc->pool = cf->pool;

    if (nt_regex_compile(rc) != NT_OK) {
        nt_conf_log_error(NT_LOG_EMERG, cf, 0, "%V", &rc->err);
        return NULL;
    }

    re = nt_pcalloc(cf->pool, sizeof(nt_stream_regex_t));
    if (re == NULL) {
        return NULL;
    }

    re->regex = rc->regex;
    re->ncaptures = rc->captures;
    re->name = rc->pattern;

    cmcf = nt_stream_conf_get_module_main_conf(cf, nt_stream_core_module);
    cmcf->ncaptures = nt_max(cmcf->ncaptures, re->ncaptures);

    n = (nt_uint_t) rc->named_captures;

    if (n == 0) {
        return re;
    }

    rv = nt_palloc(rc->pool, n * sizeof(nt_stream_regex_variable_t));
    if (rv == NULL) {
        return NULL;
    }

    re->variables = rv;
    re->nvariables = n;

    size = rc->name_size;
    p = rc->names;

    for (i = 0; i < n; i++) {
        rv[i].capture = 2 * ((p[0] << 8) + p[1]);

        name.data = &p[2];
        name.len = nt_strlen(name.data);

        v = nt_stream_add_variable(cf, &name, NT_STREAM_VAR_CHANGEABLE);
        if (v == NULL) {
            return NULL;
        }

        rv[i].index = nt_stream_get_variable_index(cf, &name);
        if (rv[i].index == NT_ERROR) {
            return NULL;
        }

        v->get_handler = nt_stream_variable_not_found;

        p += size;
    }

    return re;
}


nt_int_t
nt_stream_regex_exec(nt_stream_session_t *s, nt_stream_regex_t *re,
    nt_str_t *str)
{
    nt_int_t                     rc, index;
    nt_uint_t                    i, n, len;
    nt_stream_variable_value_t  *vv;
    nt_stream_core_main_conf_t  *cmcf;

    cmcf = nt_stream_get_module_main_conf(s, nt_stream_core_module);

    if (re->ncaptures) {
        len = cmcf->ncaptures;

        if (s->captures == NULL) {
            s->captures = nt_palloc(s->connection->pool, len * sizeof(int));
            if (s->captures == NULL) {
                return NT_ERROR;
            }
        }

    } else {
        len = 0;
    }

    rc = nt_regex_exec(re->regex, str, s->captures, len);

    if (rc == NT_REGEX_NO_MATCHED) {
        return NT_DECLINED;
    }

    if (rc < 0) {
        nt_log_error(NT_LOG_ALERT, s->connection->log, 0,
                      nt_regex_exec_n " failed: %i on \"%V\" using \"%V\"",
                      rc, str, &re->name);
        return NT_ERROR;
    }

    for (i = 0; i < re->nvariables; i++) {

        n = re->variables[i].capture;
        index = re->variables[i].index;
        vv = &s->variables[index];

        vv->len = s->captures[n + 1] - s->captures[n];
        vv->valid = 1;
        vv->no_cacheable = 0;
        vv->not_found = 0;
        vv->data = &str->data[s->captures[n]];

#if (NT_DEBUG)
        {
        nt_stream_variable_t  *v;

        v = cmcf->variables.elts;

        nt_log_debug2(NT_LOG_DEBUG_STREAM, s->connection->log, 0,
                       "stream regex set $%V to \"%v\"", &v[index].name, vv);
        }
#endif
    }

    s->ncaptures = rc * 2;
    s->captures_data = str->data;

    return NT_OK;
}

#endif


nt_int_t
nt_stream_variables_add_core_vars(nt_conf_t *cf)
{
    nt_stream_variable_t        *cv, *v;
    nt_stream_core_main_conf_t  *cmcf;

    cmcf = nt_stream_conf_get_module_main_conf(cf, nt_stream_core_module);

    cmcf->variables_keys = nt_pcalloc(cf->temp_pool,
                                       sizeof(nt_hash_keys_arrays_t));
    if (cmcf->variables_keys == NULL) {
        return NT_ERROR;
    }

    cmcf->variables_keys->pool = cf->pool;
    cmcf->variables_keys->temp_pool = cf->pool;

    if (nt_hash_keys_array_init(cmcf->variables_keys, NT_HASH_SMALL)
        != NT_OK)
    {
        return NT_ERROR;
    }

    if (nt_array_init(&cmcf->prefix_variables, cf->pool, 8,
                       sizeof(nt_stream_variable_t))
        != NT_OK)
    {
        return NT_ERROR;
    }

    for (cv = nt_stream_core_variables; cv->name.len; cv++) {
        v = nt_stream_add_variable(cf, &cv->name, cv->flags);
        if (v == NULL) {
            return NT_ERROR;
        }

        *v = *cv;
    }

    return NT_OK;
}


nt_int_t
nt_stream_variables_init_vars(nt_conf_t *cf)
{
    size_t                        len;
    nt_uint_t                    i, n;
    nt_hash_key_t               *key;
    nt_hash_init_t               hash;
    nt_stream_variable_t        *v, *av, *pv;
    nt_stream_core_main_conf_t  *cmcf;

    /* set the handlers for the indexed stream variables */

    cmcf = nt_stream_conf_get_module_main_conf(cf, nt_stream_core_module);

    v = cmcf->variables.elts;
    pv = cmcf->prefix_variables.elts;
    key = cmcf->variables_keys->keys.elts;

    for (i = 0; i < cmcf->variables.nelts; i++) {

        for (n = 0; n < cmcf->variables_keys->keys.nelts; n++) {

            av = key[n].value;

            if (v[i].name.len == key[n].key.len
                && nt_strncmp(v[i].name.data, key[n].key.data, v[i].name.len)
                   == 0)
            {
                v[i].get_handler = av->get_handler;
                v[i].data = av->data;

                av->flags |= NT_STREAM_VAR_INDEXED;
                v[i].flags = av->flags;

                av->index = i;

                if (av->get_handler == NULL
                    || (av->flags & NT_STREAM_VAR_WEAK))
                {
                    break;
                }

                goto next;
            }
        }

        len = 0;
        av = NULL;

        for (n = 0; n < cmcf->prefix_variables.nelts; n++) {
            if (v[i].name.len >= pv[n].name.len && v[i].name.len > len
                && nt_strncmp(v[i].name.data, pv[n].name.data, pv[n].name.len)
                   == 0)
            {
                av = &pv[n];
                len = pv[n].name.len;
            }
        }

        if (av) {
            v[i].get_handler = av->get_handler;
            v[i].data = (uintptr_t) &v[i].name;
            v[i].flags = av->flags;

            goto next;
         }

        if (v[i].get_handler == NULL) {
            nt_log_error(NT_LOG_EMERG, cf->log, 0,
                          "unknown \"%V\" variable", &v[i].name);
            return NT_ERROR;
        }

    next:
        continue;
    }


    for (n = 0; n < cmcf->variables_keys->keys.nelts; n++) {
        av = key[n].value;

        if (av->flags & NT_STREAM_VAR_NOHASH) {
            key[n].key.data = NULL;
        }
    }


    hash.hash = &cmcf->variables_hash;
    hash.key = nt_hash_key;
    hash.max_size = cmcf->variables_hash_max_size;
    hash.bucket_size = cmcf->variables_hash_bucket_size;
    hash.name = "variables_hash";
    hash.pool = cf->pool;
    hash.temp_pool = NULL;

    if (nt_hash_init(&hash, cmcf->variables_keys->keys.elts,
                      cmcf->variables_keys->keys.nelts)
        != NT_OK)
    {
        return NT_ERROR;
    }

    cmcf->variables_keys = NULL;

    return NT_OK;
}
