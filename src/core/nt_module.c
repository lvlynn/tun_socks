
#include <nt_core.h>


#define NT_MAX_DYNAMIC_MODULES  128


static nt_uint_t nt_module_index(nt_cycle_t *cycle);
static nt_uint_t nt_module_ctx_index(nt_cycle_t *cycle, nt_uint_t type,
    nt_uint_t index);


nt_uint_t         nt_max_module;
#if (!T_NT_SHOW_INFO)
static
#endif
nt_uint_t  nt_modules_n;


nt_int_t
nt_preinit_modules(void)
{
    // 计算模块个数，并且设置各个模块顺序（索引）
    nt_uint_t  i;

    for (i = 0; nt_modules[i]; i++) {
        nt_modules[i]->index = i;
        nt_modules[i]->name = nt_module_names[i];
    }

    nt_modules_n = i;
    nt_max_module = nt_modules_n + NT_MAX_DYNAMIC_MODULES;

    return NT_OK;
}


nt_int_t
nt_cycle_modules(nt_cycle_t *cycle)
{
    /*
     * create a list of modules to be used for this cycle,
     * copy static modules to it
     */

    cycle->modules = nt_pcalloc(cycle->pool, (nt_max_module + 1)
                                              * sizeof(nt_module_t *));
    if (cycle->modules == NULL) {
        return NT_ERROR;
    }

    nt_memcpy(cycle->modules, nt_modules,
               nt_modules_n * sizeof(nt_module_t *));

    cycle->modules_n = nt_modules_n;

    return NT_OK;
}


nt_int_t
nt_init_modules(nt_cycle_t *cycle)
{
    nt_uint_t  i;

    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->init_module) {
            if (cycle->modules[i]->init_module(cycle) != NT_OK) {
                return NT_ERROR;
            }
        }
    }

    return NT_OK;
}


nt_int_t
nt_count_modules(nt_cycle_t *cycle, nt_uint_t type)
{
    nt_uint_t     i, next, max;
    nt_module_t  *module;

    next = 0;
    max = 0;

    /* count appropriate modules, set up their indices */

    for (i = 0; cycle->modules[i]; i++) {
        module = cycle->modules[i];

        if (module->type != type) {
            continue;
        }

        if (module->ctx_index != NT_MODULE_UNSET_INDEX) {

            /* if ctx_index was assigned, preserve it */

            if (module->ctx_index > max) {
                max = module->ctx_index;
            }

            if (module->ctx_index == next) {
                next++;
            }

            continue;
        }

        /* search for some free index */

        module->ctx_index = nt_module_ctx_index(cycle, type, next);

        if (module->ctx_index > max) {
            max = module->ctx_index;
        }

        next = module->ctx_index + 1;
    }

    /*
     * make sure the number returned is big enough for previous
     * cycle as well, else there will be problems if the number
     * will be stored in a global variable (as it's used to be)
     * and we'll have to roll back to the previous cycle
     */

    if (cycle->old_cycle && cycle->old_cycle->modules) {

        for (i = 0; cycle->old_cycle->modules[i]; i++) {
            module = cycle->old_cycle->modules[i];

            if (module->type != type) {
                continue;
            }

            if (module->ctx_index > max) {
                max = module->ctx_index;
            }
        }
    }

    /* prevent loading of additional modules */

    cycle->modules_used = 1;

    return max + 1;
}


nt_int_t
nt_add_module(nt_conf_t *cf, nt_str_t *file, nt_module_t *module,
    char **order)
{
    void               *rv;
    nt_uint_t          i, m, before;
    nt_core_module_t  *core_module;

    if (cf->cycle->modules_n >= nt_max_module) {
        nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                           "too many modules loaded");
        return NT_ERROR;
    }

    if (module->version != nginx_version) {
        nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                           "module \"%V\" version %ui instead of %ui",
                           file, module->version, (nt_uint_t) nginx_version);
        return NT_ERROR;
    }

    if (nt_strcmp(module->signature, NT_MODULE_SIGNATURE) != 0) {
        nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                           "module \"%V\" is not binary compatible",
                           file);
        return NT_ERROR;
    }

    for (m = 0; cf->cycle->modules[m]; m++) {
        if (nt_strcmp(cf->cycle->modules[m]->name, module->name) == 0) {
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "module \"%s\" is already loaded",
                               module->name);
            return NT_ERROR;
        }
    }

    /*
     * if the module wasn't previously loaded, assign an index
     */

    if (module->index == NT_MODULE_UNSET_INDEX) {
        module->index = nt_module_index(cf->cycle);

        if (module->index >= nt_max_module) {
            nt_conf_log_error(NT_LOG_EMERG, cf, 0,
                               "too many modules loaded");
            return NT_ERROR;
        }
    }

    /*
     * put the module into the cycle->modules array
     */

    before = cf->cycle->modules_n;

    if (order) {
        for (i = 0; order[i]; i++) {
            if (nt_strcmp(order[i], module->name) == 0) {
                i++;
                break;
            }
        }

        for ( /* void */ ; order[i]; i++) {

#if 0
            nt_log_debug2(NT_LOG_DEBUG_CORE, cf->log, 0,
                           "module: %s before %s",
                           module->name, order[i]);
#endif

            for (m = 0; m < before; m++) {
                if (nt_strcmp(cf->cycle->modules[m]->name, order[i]) == 0) {

                    nt_log_debug3(NT_LOG_DEBUG_CORE, cf->log, 0,
                                   "module: %s before %s:%i",
                                   module->name, order[i], m);

                    before = m;
                    break;
                }
            }
        }
    }

    /* put the module before modules[before] */

    if (before != cf->cycle->modules_n) {
        nt_memmove(&cf->cycle->modules[before + 1],
                    &cf->cycle->modules[before],
                    (cf->cycle->modules_n - before) * sizeof(nt_module_t *));
    }

    cf->cycle->modules[before] = module;
    cf->cycle->modules_n++;

    if (module->type == NT_CORE_MODULE) {

        /*
         * we are smart enough to initialize core modules;
         * other modules are expected to be loaded before
         * initialization - e.g., http modules must be loaded
         * before http{} block
         */

        core_module = module->ctx;

        if (core_module->create_conf) {
            rv = core_module->create_conf(cf->cycle);
            if (rv == NULL) {
                return NT_ERROR;
            }

            cf->cycle->conf_ctx[module->index] = rv;
        }
    }

    return NT_OK;
}


static nt_uint_t
nt_module_index(nt_cycle_t *cycle)
{
    nt_uint_t     i, index;
    nt_module_t  *module;

    index = 0;

again:

    /* find an unused index */

    for (i = 0; cycle->modules[i]; i++) {
        module = cycle->modules[i];

        if (module->index == index) {
            index++;
            goto again;
        }
    }

    /* check previous cycle */

    if (cycle->old_cycle && cycle->old_cycle->modules) {

        for (i = 0; cycle->old_cycle->modules[i]; i++) {
            module = cycle->old_cycle->modules[i];

            if (module->index == index) {
                index++;
                goto again;
            }
        }
    }

    return index;
}


static nt_uint_t
nt_module_ctx_index(nt_cycle_t *cycle, nt_uint_t type, nt_uint_t index)
{
    nt_uint_t     i;
    nt_module_t  *module;

again:

    /* find an unused ctx_index */

    for (i = 0; cycle->modules[i]; i++) {
        module = cycle->modules[i];

        if (module->type != type) {
            continue;
        }

        if (module->ctx_index == index) {
            index++;
            goto again;
        }
    }

    /* check previous cycle */

    if (cycle->old_cycle && cycle->old_cycle->modules) {

        for (i = 0; cycle->old_cycle->modules[i]; i++) {
            module = cycle->old_cycle->modules[i];

            if (module->type != type) {
                continue;
            }

            if (module->ctx_index == index) {
                index++;
                goto again;
            }
        }
    }

    return index;
}
