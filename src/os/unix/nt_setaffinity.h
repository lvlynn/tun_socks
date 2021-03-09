
#ifndef _NT_SETAFFINITY_H_INCLUDED_
#define _NT_SETAFFINITY_H_INCLUDED_


#if (NT_HAVE_SCHED_SETAFFINITY || NT_HAVE_CPUSET_SETAFFINITY)

    #define NT_HAVE_CPU_AFFINITY 1

    #if (NT_HAVE_SCHED_SETAFFINITY)

        typedef cpu_set_t  nt_cpuset_t;

    #elif (NT_HAVE_CPUSET_SETAFFINITY)

        #include <sys/cpuset.h>

        typedef cpuset_t  nt_cpuset_t;

    #endif

    void nt_setaffinity( nt_cpuset_t *cpu_affinity, nt_log_t *log );

#else

    #define nt_setaffinity(cpu_affinity, log)

    typedef uint64_t  nt_cpuset_t;

#endif


#endif /* _NT_SETAFFINITY_H_INCLUDED_ */
