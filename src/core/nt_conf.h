#ifndef _NT_CONF_H_
#define _NT_CONF_H_

#include <nt_def.h>
#include <nt_core.h>

//得到module对应的配置
#define nt_get_conf(conf_ctx, module)  conf_ctx[module.index]

#define NT_MAX_CONF_ERRSTR  1024

struct nt_open_file_s {
    nt_fd_t              fd;
    nt_str_t             name;

    void                (*flush)(nt_open_file_t *file, nt_log_t *log);
    void                 *data;                                                                                         

};

#endif

