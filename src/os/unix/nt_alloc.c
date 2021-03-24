

#include <nt_def.h>
#include <nt_core.h>


nt_uint_t  nt_pagesize;
nt_uint_t  nt_pagesize_shift;
nt_uint_t  nt_cacheline_size;


#if (NT_JEMALLOC)
	#include <jemalloc.h>
#endif // (NT_JEMALLOC) 

void *
nt_alloc(size_t size, nt_log_t *log)
{
    void  *p;

    p = malloc(size);
    if (p == NULL) {
        nt_log_error(NT_LOG_EMERG, log, nt_errno,
                      "malloc(%uz) failed", size);
    }

    debug( "malloc size=%d" , size);
    nt_log_debug2(NT_LOG_DEBUG_ALLOC, log, 0, "malloc: %p:%uz", p, size);

    return p;
}


void *
nt_calloc(size_t size, nt_log_t *log)
{
    void  *p;

    p = nt_alloc(size, log);

    if (p) {
        nt_memzero(p, size);
    }

    return p;
}


#if (T_DEPRECATED)
void *
nt_realloc(void *p, size_t size, nt_log_t *log)
{
    void *new;

    new = realloc(p, size);
    if (new == NULL) {
        nt_log_error(NT_LOG_EMERG, log, nt_errno,
                      "realloc(%p:%uz) failed", p, size);
    }

    nt_log_debug2(NT_LOG_DEBUG_ALLOC, log, 0, "realloc: %p:%uz", new, size);

    return new;
}
#endif


#if (NT_HAVE_POSIX_MEMALIGN)

void *
nt_memalign(size_t alignment, size_t size, nt_log_t *log)
{
    void  *p;
    int    err;

    err = posix_memalign(&p, alignment, size);

    if (err) {
        nt_log_error(NT_LOG_EMERG, log, err,
                      "posix_memalign(%uz, %uz) failed", alignment, size);
        p = NULL;
    }

    nt_log_debug3(NT_LOG_DEBUG_ALLOC, log, 0,
                   "posix_memalign: %p:%uz @%uz", p, size, alignment);

    return p;
}

#elif (NT_HAVE_MEMALIGN)

void *
nt_memalign(size_t alignment, size_t size, nt_log_t *log)
{
    void  *p;

    p = memalign(alignment, size);
    if (p == NULL) {
        nt_log_error(NT_LOG_EMERG, log, nt_errno,
                      "memalign(%uz, %uz) failed", alignment, size);
    }

    nt_log_debug3(NT_LOG_DEBUG_ALLOC, log, 0,
                   "memalign: %p:%uz @%uz", p, size, alignment);

    return p;
}

#endif
