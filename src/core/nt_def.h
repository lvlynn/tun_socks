

#ifndef _NT_CONFIG_H_
#define _NT_CONFIG_H_

//引入linux头文件
#include <nt_linux_config.h>

#if !(NT_WIN32)
    #define nt_cdecl
    #define nt_libc_cdecl
#endif

//参考nginx ， intptr_t是为了跨平台，其长度总是所在平台的位数，所以用来存放地址。
typedef intptr_t        nt_int_t;
typedef uintptr_t       nt_uint_t;
typedef intptr_t        nt_flag_t;



#define NT_INT32_LEN   (sizeof("-2147483648") - 1)
#define NT_INT64_LEN   (sizeof("-9223372036854775808") - 1)

#if (NT_PTR_SIZE == 4)
    #define NT_INT_T_LEN   NT_INT32_LEN
    #define NT_MAX_INT_T_VALUE  2147483647

#else
    #define NT_INT_T_LEN   NT_INT64_LEN
    #define NT_MAX_INT_T_VALUE  9223372036854775807
#endif


#ifndef NT_ALIGNMENT
    #define NT_ALIGNMENT   sizeof(unsigned long)    /* platform word */
#endif

//f(a,b)是b的倍数，且f(a,b)是a的一个最小弱上界。
//实际上就是得到一个对齐的地址，或者得到一个对齐的一段由pool分配的数据)
//d 5   8  ->  8
//d 9   8  ->  16
#define nt_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define nt_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))


#define nt_abort       abort


/* TODO: platform specific: array[NT_INVALID_ARRAY_INDEX] must cause SIGSEGV */
#define NT_INVALID_ARRAY_INDEX 0x80000000


/* TODO: auto_conf: nt_inline   inline __inline __inline__ */
#ifndef nt_inline

    #define nt_inline      inline
#endif


#ifndef INADDR_NONE  /* Solaris */
    #define INADDR_NONE  ((unsigned int) -1)
#endif

#ifdef MAXHOSTNAMELEN
    #define NT_MAXHOSTNAMELEN  MAXHOSTNAMELEN
#else
    #define NT_MAXHOSTNAMELEN  256
#endif


#define NT_MAX_UINT32_VALUE  (uint32_t) 0xffffffff
#define NT_MAX_INT32_VALUE   (uint32_t) 0x7fffffff


#if (NT_COMPAT)

    #define NT_COMPAT_BEGIN(slots)  uint64_t spare[slots];
    #define NT_COMPAT_END

#else

    #define NT_COMPAT_BEGIN(slots)
    #define NT_COMPAT_END

#endif


#endif
