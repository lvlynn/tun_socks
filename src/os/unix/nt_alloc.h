
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NT_ALLOC_H_INCLUDED_
#define _NT_ALLOC_H_INCLUDED_


#include <nt_def.h>
#include <nt_core.h>


void *nt_alloc(size_t size, nt_log_t *log);
void *nt_calloc(size_t size, nt_log_t *log);
#if (T_DEPRECATED)
void *nt_realloc(void *p, size_t size, nt_log_t *log);
#endif

#define nt_free          free

/*
#if (NT_JEMALLOC)

#define NT_HAVE_POSIX_MEMALIGN 1
#define NT_HAVE_MEMALIGN 1

#endif  (NT_JEMALLOC) 
*/

/*
 * Linux has memalign() or posix_memalign()
 * Solaris has memalign()
 * FreeBSD 7.0 has posix_memalign(), besides, early version's malloc()
 * aligns allocations bigger than page size at the page boundary
 */

#if (NT_HAVE_POSIX_MEMALIGN || NT_HAVE_MEMALIGN)

void *nt_memalign(size_t alignment, size_t size, nt_log_t *log);

#else

#define nt_memalign(alignment, size, log)  nt_alloc(size, log)

#endif


extern nt_uint_t  nt_pagesize;
extern nt_uint_t  nt_pagesize_shift;
extern nt_uint_t  nt_cacheline_size;


#endif /* _NT_ALLOC_H_INCLUDED_ */
