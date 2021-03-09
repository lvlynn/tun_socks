

#ifndef _NT_SYSINFO_H_INCLUDED_
#define _NT_SYSINFO_H_INCLUDED_


#include <nt_core.h>


/* in bytes */
// 内存信息
typedef struct {
    size_t totalram;
    size_t freeram;
    size_t bufferram;
    size_t cachedram;
    size_t totalswap;
    size_t freeswap;
} nt_meminfo_t;

// cpu 信息
typedef struct {
    time_t usr;
    time_t nice;
    time_t sys;
    time_t idle;
    time_t iowait;
    time_t irq;
    time_t softirq;    
}nt_cpuinfo_t;


nt_int_t nt_getloadavg(nt_int_t avg[], nt_int_t nelem, nt_log_t *log);
nt_int_t nt_getmeminfo(nt_meminfo_t *meminfo, nt_log_t *log);
nt_int_t nt_getcpuinfo(nt_str_t *cpunumber, nt_cpuinfo_t *cpuinfo,
    nt_log_t *log);

#endif /* _NT_SYSINFO_H_INCLUDED_ */

