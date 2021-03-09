#include <nt_def.h>
#include <nt_core.h>

extern nt_module_t  nt_core_module;
extern nt_module_t  nt_errlog_module;
extern nt_module_t  nt_conf_module;
extern nt_module_t  nt_events_module;
extern nt_module_t  nt_event_core_module;
extern nt_module_t  nt_select_module;
extern nt_module_t  nt_stream_module;
extern nt_module_t  nt_stream_core_module;
extern nt_module_t  nt_stream_log_module;
extern nt_module_t  nt_stream_proxy_module;
extern nt_module_t  nt_stream_socks5_module;
extern nt_module_t  nt_stream_upstream_module;

nt_module_t *nt_modules[] = {
    &nt_core_module,
    &nt_errlog_module,
    &nt_conf_module,
    &nt_events_module,
    &nt_event_core_module,
    &nt_select_module,
    &nt_stream_module,
    &nt_stream_core_module,
    &nt_stream_log_module,
    &nt_stream_proxy_module,
    &nt_stream_socks5_module,
    &nt_stream_upstream_module,
    NULL
};

char *nt_module_names[] = {
    "nt_core_module",
    "nt_errlog_module",
    "nt_conf_module",
    "nt_events_module",
    "nt_event_core_module",
    "nt_select_module",
    "nt_stream_module",
    "nt_stream_core_module",
    "nt_stream_log_module",
    "nt_stream_proxy_module",
    "nt_stream_socks5_module",
    "nt_stream_upstream_module",
};


