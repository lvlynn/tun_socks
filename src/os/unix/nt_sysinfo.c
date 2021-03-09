
#include <nt_core.h>

#if (NT_HAVE_SYSINFO)
#include <sys/sysinfo.h>
#endif


nt_int_t
nt_getloadavg(nt_int_t avg[], nt_int_t nelem, nt_log_t *log)
{
#if (NT_HAVE_GETLOADAVG)
    double      loadavg[3];
    nt_int_t   i;

    if (getloadavg(loadavg, nelem) == -1) {
        return NT_ERROR;
    }

    for (i = 0; i < nelem; i ++) {
        avg[i] = loadavg[i] * 1000;
    }

    return NT_OK;

#elif (NT_HAVE_SYSINFO)

    struct sysinfo s;
    nt_int_t   i;

    if (sysinfo(&s)) {
        return NT_ERROR;
    }

    for (i = 0; i < nelem; i ++) {
        avg[i] = s.loads[i] * 1000 / 65536;
    }

    return NT_OK;

#else

    nt_log_error(NT_LOG_EMERG, log, 0,
                  "getloadavg is unsupported under current os");

    return NT_ERROR;
#endif
}

#if (NT_HAVE_PROC_MEMINFO)

static nt_file_t                   nt_meminfo_file;

#define NT_MEMINFO_FILE            "/proc/meminfo"
#define NT_MEMINFO_MAX_NAME_LEN    16


nt_int_t
nt_getmeminfo(nt_meminfo_t *meminfo, nt_log_t *log)
{
    u_char              buf[2048];
    u_char             *p, *start, *last;
    size_t             *sz = NULL;
    ssize_t             n, len;
    nt_fd_t            fd;
    enum {
        sw_name = 0,
        sw_value_start,
        sw_value,
        sw_skipline,
        sw_newline,
    } state;

    nt_memzero(meminfo, sizeof(nt_meminfo_t));

    if (nt_meminfo_file.fd == 0) {

        fd = nt_open_file(NT_MEMINFO_FILE, NT_FILE_RDONLY,
                           NT_FILE_OPEN,
                           NT_FILE_DEFAULT_ACCESS);

        if (fd == NT_INVALID_FILE) {
            nt_log_error(NT_LOG_EMERG, log, nt_errno,
                          nt_open_file_n " \"%s\" failed",
                          NT_MEMINFO_FILE);

            return NT_ERROR;
        }

        nt_meminfo_file.name.data = (u_char *) NT_MEMINFO_FILE;
        nt_meminfo_file.name.len = nt_strlen(NT_MEMINFO_FILE);

        nt_meminfo_file.fd = fd;
    }

    nt_meminfo_file.log = log;
    n = nt_read_file(&nt_meminfo_file, buf, sizeof(buf) - 1, 0);
    if (n == NT_ERROR) {
        nt_log_error(NT_LOG_ALERT, log, nt_errno,
                      nt_read_file_n " \"%s\" failed",
                      NT_MEMINFO_FILE);

        return NT_ERROR;
    }

    p = buf;
    start = buf;
    last = buf + n;
    state = sw_name;

    for (; p < last; p++) {

        if (*p == '\n') {
            state = sw_newline;
        }

        switch (state) {

        case sw_name:
            if (*p != ':') {
                continue;
            }

            len = p - start;
            sz = NULL;

            switch (len) {
            case 6:
                /* Cached */
                if (meminfo->cachedram == 0 &&
                    nt_strncmp(start, "Cached", len) == 0)
                {
                    sz = &meminfo->cachedram;
                }
                break;
            case 7:
                /* Buffers MemFree */
                if (meminfo->bufferram == 0 &&
                    nt_strncmp(start, "Buffers", len) == 0)
                {
                    sz = &meminfo->bufferram;
                } else if (meminfo->freeram == 0 &&
                           nt_strncmp(start, "MemFree", len) == 0)
                {
                    sz = &meminfo->freeram;
                }
                break;
            case 8:
                /* MemTotal SwapFree */
                if (meminfo->totalram == 0 &&
                    nt_strncmp(start, "MemTotal", len) == 0)
                {
                    sz = &meminfo->totalram;
                } else if (meminfo->freeswap == 0 &&
                           nt_strncmp(start, "SwapFree", len) == 0)
                {
                    sz = &meminfo->freeswap;
                }
                break;
            case 9:
                /* SwapTotal */
                if (meminfo->totalswap == 0 &&
                    nt_strncmp(start, "SwapTotal", len) == 0)
                {
                    sz = &meminfo->totalswap;
                }
                break;
            }

            if (sz == NULL) {
                state = sw_skipline;
                continue;
            }

            state = sw_value_start;

            continue;

        case sw_value_start:

            if (*p == ' ') {
                continue;
            }

            start = p;
            state = sw_value;

            continue;

        case sw_value:

            if (*p >= '0' && *p <= '9') {
                continue;
            }

            *(sz) =  nt_atosz(start, p - start) * 1024;

            state = sw_skipline;

            continue;

        case sw_skipline:

            continue;

        case sw_newline:

            state = sw_name;
            start = p + 1;

            continue;
        }
    }

    return NT_OK;
}

#else

nt_int_t
nt_getmeminfo(nt_meminfo_t *meminfo, nt_log_t *log)
{
    nt_log_error(NT_LOG_EMERG, log, 0,
                  "getmeminfo is unsupported under current os");

    return NT_ERROR;
}

#endif

#if (NT_HAVE_PROC_STAT)

static nt_file_t                   nt_cpuinfo_file;

#define NT_CPUINFO_FILE            "/proc/stat"


nt_int_t
nt_getcpuinfo(nt_str_t *cpunumber, nt_cpuinfo_t *cpuinfo, nt_log_t *log)
{
    u_char              buf[1024 * 1024];
    u_char             *p, *q, *last;
    ssize_t             n;
    nt_fd_t            fd;
    time_t              cputime;
    enum {
        sw_user = 0,
        sw_nice,
        sw_sys,
        sw_idle,
        sw_iowait,
        sw_irq,
        sw_softirq ,        
    } state;

    nt_memzero(cpuinfo, sizeof(nt_cpuinfo_t));

    if (nt_cpuinfo_file.fd == 0) {

        fd = nt_open_file(NT_CPUINFO_FILE, NT_FILE_RDONLY,
                           NT_FILE_OPEN,
                           NT_FILE_DEFAULT_ACCESS);

        if (fd == NT_INVALID_FILE) {
            nt_log_error(NT_LOG_EMERG, log, nt_errno,
                          nt_open_file_n " \"%s\" failed",
                          NT_MEMINFO_FILE);

            return NT_ERROR;
        }

        nt_cpuinfo_file.name.data = (u_char *) NT_CPUINFO_FILE;
        nt_cpuinfo_file.name.len = nt_strlen(NT_CPUINFO_FILE);

        nt_cpuinfo_file.fd = fd;
    }

    nt_cpuinfo_file.log = log;
    n = nt_read_file(&nt_cpuinfo_file, buf, sizeof(buf) - 1, 0);
    if (n == NT_ERROR) {
        nt_log_error(NT_LOG_ALERT, log, nt_errno,
                      nt_read_file_n " \"%s\" failed",
                      NT_MEMINFO_FILE);

        return NT_ERROR;
    }

    p = buf;
    last = buf + n;
    
    for (; p < last; p++) {
        while(*p == ' ' || *p == '\n') {
            p++;
        }
        
        if (nt_strncasecmp((u_char *) cpunumber->data,
                            (u_char *) p, cpunumber->len) == 0) 
        {
            
            for (state = 0, p += cpunumber->len, 
                 q = (u_char *) strtok((char *) p, " "); q; state++) 
            {
                cputime = nt_atotm(q, strlen((char *) q));
                        
                switch (state) {
                case sw_user:
                    cpuinfo->usr = cputime;
                    break;
                case sw_nice:
                    cpuinfo->nice = cputime;
                    break;
                case sw_sys:
                    cpuinfo->sys = cputime;
                    break;
                case sw_idle:
                    cpuinfo->idle = cputime;
                    break;
                case sw_iowait:
                    cpuinfo->iowait = cputime;
                    break;
                case sw_irq:
                    cpuinfo->irq = cputime;
                    break;  
                case sw_softirq:
                    cpuinfo->softirq = cputime;
                    break;                    
                }    
                
                q = (u_char *) strtok(NULL, " ");
            }
        }
        
        break;
        
    }

    return NT_OK;
}

#else

nt_int_t
nt_getcpuinfo(nt_str_t *cpunumber, nt_cpuinfo_t *cpuinfo, nt_log_t *log)
{
    nt_log_error(NT_LOG_EMERG, log, 0,
                  "nt_getcpuinfo is unsupported under current os");

    return NT_ERROR;
}

#endif
