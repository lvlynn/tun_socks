

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

/* TODO: auto_conf: nt_inline   inline __inline __inline__ */
#ifndef nt_inline
#define nt_inline      inline
#endif

#endif
