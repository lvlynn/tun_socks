#include <nt_def.h>
#include <nt_core.h>

extern nt_module_t  nt_core_module;
extern nt_module_t  nt_events_module;
extern nt_module_t  nt_select_module;

nt_module_t *nt_modules[] = {
    &nt_select_module,
    NULL
};

char *nt_module_names[] = {
    "nt_select_module",
};


