
#include <nt_def.h>
#include <nt_core.h>
#include <nt_event.h>


//程序全局变量
volatile nt_cycle_t  *nt_cycle;

nt_uint_t             nt_test_config;
nt_uint_t             nt_dump_config;

 nt_cycle_t *
 nt_init_cycle(nt_cycle_t *old_cycle){

     return old_cycle;
 }
    
