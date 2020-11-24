#ifndef _NT_CYCLE_H_
#define _NT_CYCLE_H_

//用于存储 大循环配置
struct nt_cycle_s {

    void                  ****conf_ctx;  //配置

    ngx_module_t            **modules;
    ngx_uint_t                modules_n;
    ngx_uint_t                modules_used;    /* unsigned  modules_used:1; */
    //   nt_process_event;
};
#endif

