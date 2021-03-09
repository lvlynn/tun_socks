
#include <nt_core.h>


#if (NT_HAVE_CPUSET_SETAFFINITY)

void
nt_setaffinity(nt_cpuset_t *cpu_affinity, nt_log_t *log)
{
    nt_uint_t  i;

    for (i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, cpu_affinity)) {
            nt_log_error(NT_LOG_NOTICE, log, 0,
                          "cpuset_setaffinity(): using cpu #%ui", i);
        }
    }

    if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, -1,
                           sizeof(cpuset_t), cpu_affinity) == -1)
    {
        nt_log_error(NT_LOG_ALERT, log, nt_errno,
                      "cpuset_setaffinity() failed");
    }
}

#elif (NT_HAVE_SCHED_SETAFFINITY)

void
nt_setaffinity(nt_cpuset_t *cpu_affinity, nt_log_t *log)
{
    nt_uint_t  i;

    for (i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, cpu_affinity)) {
            nt_log_error(NT_LOG_NOTICE, log, 0,
                          "sched_setaffinity(): using cpu #%ui", i);
        }
    }

    if (sched_setaffinity(0, sizeof(cpu_set_t), cpu_affinity) == -1) {
        nt_log_error(NT_LOG_ALERT, log, nt_errno,
                      "sched_setaffinity() failed");
    }
}

#endif
