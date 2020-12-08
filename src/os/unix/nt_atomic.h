

#ifndef _NT_ATOMIC_H_INCLUDED_
#define _NT_ATOMIC_H_INCLUDED_


#include <nt_core.h>


#if (NT_HAVE_LIBATOMIC)

#define AO_REQUIRE_CAS
#include <atomic_ops.h>

#define NT_HAVE_ATOMIC_OPS  1

typedef long                        nt_atomic_int_t;
typedef AO_t                        nt_atomic_uint_t;
typedef volatile nt_atomic_uint_t  nt_atomic_t;

#if (NT_PTR_SIZE == 8)
#define NT_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)
#else
#define NT_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)
#endif

#define nt_atomic_cmp_set(lock, old, new)                                    \
    AO_compare_and_swap(lock, old, new)
#define nt_atomic_fetch_add(value, add)                                      \
    AO_fetch_and_add(value, add)
#define nt_memory_barrier()        AO_nop()
#define nt_cpu_pause()


#elif (NT_DARWIN_ATOMIC)

/*
 * use Darwin 8 atomic(3) and barrier(3) operations
 * optimized at run-time for UP and SMP
 */

#include <libkern/OSAtomic.h>

/* "bool" conflicts with perl's CORE/handy.h */
#if 0
#undef bool
#endif


#define NT_HAVE_ATOMIC_OPS  1

#if (NT_PTR_SIZE == 8)

typedef int64_t                     nt_atomic_int_t;
typedef uint64_t                    nt_atomic_uint_t;
#define NT_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)

#define nt_atomic_cmp_set(lock, old, new)                                    \
    OSAtomicCompareAndSwap64Barrier(old, new, (int64_t *) lock)

#define nt_atomic_fetch_add(value, add)                                      \
    (OSAtomicAdd64(add, (int64_t *) value) - add)

#else

typedef int32_t                     nt_atomic_int_t;
typedef uint32_t                    nt_atomic_uint_t;
#define NT_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)

#define nt_atomic_cmp_set(lock, old, new)                                    \
    OSAtomicCompareAndSwap32Barrier(old, new, (int32_t *) lock)

#define nt_atomic_fetch_add(value, add)                                      \
    (OSAtomicAdd32(add, (int32_t *) value) - add)

#endif

#define nt_memory_barrier()        OSMemoryBarrier()

#define nt_cpu_pause()

typedef volatile nt_atomic_uint_t  nt_atomic_t;


#elif (NT_HAVE_GCC_ATOMIC)

/* GCC 4.1 builtin atomic operations */

#define NT_HAVE_ATOMIC_OPS  1

typedef long                        nt_atomic_int_t;
typedef unsigned long               nt_atomic_uint_t;

#if (NT_PTR_SIZE == 8)
#define NT_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)
#else
#define NT_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)
#endif

typedef volatile nt_atomic_uint_t  nt_atomic_t;


#define nt_atomic_cmp_set(lock, old, set)                                    \
    __sync_bool_compare_and_swap(lock, old, set)

#define nt_atomic_fetch_add(value, add)                                      \
    __sync_fetch_and_add(value, add)

#define nt_memory_barrier()        __sync_synchronize()

#if ( __i386__ || __i386 || __amd64__ || __amd64 )
#define nt_cpu_pause()             __asm__ ("pause")
#else
#define nt_cpu_pause()
#endif


#elif ( __i386__ || __i386 )

typedef int32_t                     nt_atomic_int_t;
typedef uint32_t                    nt_atomic_uint_t;
typedef volatile nt_atomic_uint_t  nt_atomic_t;
#define NT_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)


#if ( __SUNPRO_C )

#define NT_HAVE_ATOMIC_OPS  1

nt_atomic_uint_t
nt_atomic_cmp_set(nt_atomic_t *lock, nt_atomic_uint_t old,
    nt_atomic_uint_t set);

nt_atomic_int_t
nt_atomic_fetch_add(nt_atomic_t *value, nt_atomic_int_t add);

/*
 * Sun Studio 12 exits with segmentation fault on '__asm ("pause")',
 * so nt_cpu_pause is declared in src/os/unix/nt_sunpro_x86.il
 */

void
nt_cpu_pause(void);

/* the code in src/os/unix/nt_sunpro_x86.il */

#define nt_memory_barrier()        __asm (".volatile"); __asm (".nonvolatile")


#else /* ( __GNUC__ || __INTEL_COMPILER ) */

#define NT_HAVE_ATOMIC_OPS  1

#include "nt_gcc_atomic_x86.h"

#endif


#elif ( __amd64__ || __amd64 )

typedef int64_t                     nt_atomic_int_t;
typedef uint64_t                    nt_atomic_uint_t;
typedef volatile nt_atomic_uint_t  nt_atomic_t;
#define NT_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)


#if ( __SUNPRO_C )

#define NT_HAVE_ATOMIC_OPS  1

nt_atomic_uint_t
nt_atomic_cmp_set(nt_atomic_t *lock, nt_atomic_uint_t old,
    nt_atomic_uint_t set);

nt_atomic_int_t
nt_atomic_fetch_add(nt_atomic_t *value, nt_atomic_int_t add);

/*
 * Sun Studio 12 exits with segmentation fault on '__asm ("pause")',
 * so nt_cpu_pause is declared in src/os/unix/nt_sunpro_amd64.il
 */

void
nt_cpu_pause(void);

/* the code in src/os/unix/nt_sunpro_amd64.il */

#define nt_memory_barrier()        __asm (".volatile"); __asm (".nonvolatile")


#else /* ( __GNUC__ || __INTEL_COMPILER ) */

#define NT_HAVE_ATOMIC_OPS  1

#include "nt_gcc_atomic_amd64.h"

#endif


#elif ( __sparc__ || __sparc || __sparcv9 )

#if (NT_PTR_SIZE == 8)

typedef int64_t                     nt_atomic_int_t;
typedef uint64_t                    nt_atomic_uint_t;
#define NT_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)

#else

typedef int32_t                     nt_atomic_int_t;
typedef uint32_t                    nt_atomic_uint_t;
#define NT_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)

#endif

typedef volatile nt_atomic_uint_t  nt_atomic_t;


#if ( __SUNPRO_C )

#define NT_HAVE_ATOMIC_OPS  1

#include "nt_sunpro_atomic_sparc64.h"


#else /* ( __GNUC__ || __INTEL_COMPILER ) */

#define NT_HAVE_ATOMIC_OPS  1

#include "nt_gcc_atomic_sparc64.h"

#endif


#elif ( __powerpc__ || __POWERPC__ )

#define NT_HAVE_ATOMIC_OPS  1

#if (NT_PTR_SIZE == 8)

typedef int64_t                     nt_atomic_int_t;
typedef uint64_t                    nt_atomic_uint_t;
#define NT_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)

#else

typedef int32_t                     nt_atomic_int_t;
typedef uint32_t                    nt_atomic_uint_t;
#define NT_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)

#endif

typedef volatile nt_atomic_uint_t  nt_atomic_t;


#include "nt_gcc_atomic_ppc.h"

#endif


#if !(NT_HAVE_ATOMIC_OPS)

#define NT_HAVE_ATOMIC_OPS  0

typedef int32_t                     nt_atomic_int_t;
typedef uint32_t                    nt_atomic_uint_t;
typedef volatile nt_atomic_uint_t  nt_atomic_t;
#define NT_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)


static nt_inline nt_atomic_uint_t
nt_atomic_cmp_set(nt_atomic_t *lock, nt_atomic_uint_t old,
    nt_atomic_uint_t set)
{
    if (*lock == old) {
        *lock = set;
        return 1;
    }

    return 0;
}


static nt_inline nt_atomic_int_t
nt_atomic_fetch_add(nt_atomic_t *value, nt_atomic_int_t add)
{
    nt_atomic_int_t  old;

    old = *value;
    *value += add;

    return old;
}

#define nt_memory_barrier()
#define nt_cpu_pause()

#endif


void nt_spinlock(nt_atomic_t *lock, nt_atomic_int_t value, nt_uint_t spin);

#define nt_trylock(lock)  (*(lock) == 0 && nt_atomic_cmp_set(lock, 0, 1))
#define nt_unlock(lock)    *(lock) = 0


#endif /* _NT_ATOMIC_H_INCLUDED_ */
