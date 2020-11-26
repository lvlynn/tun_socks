#ifndef _NT_CYCLE_H_
#define _NT_CYCLE_H_

//用于存储 大循环配置
struct nt_cycle_s {

    void                  ****conf_ctx;  //配置

    nt_module_t            **modules;
    nt_uint_t                modules_n;
    nt_uint_t                modules_used;    /* unsigned  modules_used:1; */

    nt_cycle_t *log;
    //   nt_process_event;
};

extern volatile nt_cycle_t  *nt_cycle;
#endif

