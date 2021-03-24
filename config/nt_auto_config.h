#ifndef _NT_AUTO_CONFIG_H_
#define _NT_AUTO_CONFIG_H_

#ifndef NT_DEBUG
   #define NT_DEBUG 1
// debug函数中的localtime函数 会触发一次malloc 跟free， 因此正式版需要关闭，否则影响性能
#endif

//功能开启
#ifndef NT_HAVE_SELECT
    #define NT_HAVE_SELECT  1
#endif

#ifndef NT_HAVE_POLL
    //    #define NT_HAVE_POLL  1
#endif

#ifndef NT_HAVE_EPOLL
    //    #define NT_HAVE_EPOLL  1
#endif

//定义参数长度
#ifndef NT_PTR_SIZE
    #define NT_PTR_SIZE  4
#endif

#ifndef NT_SIG_ATOMIC_T_SIZE
    #define NT_SIG_ATOMIC_T_SIZE  4
#endif

#ifndef NT_MAX_SIZE_T_VALUE
    #define NT_MAX_SIZE_T_VALUE  2147483647
#endif


#ifndef NT_SIZE_T_LEN
    #define NT_SIZE_T_LEN  (sizeof("-2147483648") - 1)
#endif


#ifndef NT_MAX_OFF_T_VALUE
    #define NT_MAX_OFF_T_VALUE  2147483647
#endif


#ifndef NT_OFF_T_LEN
    #define NT_OFF_T_LEN  (sizeof("-2147483648") - 1)
#endif


#ifndef NT_TIME_T_SIZE
    #define NT_TIME_T_SIZE  4
#endif


#ifndef NT_TIME_T_LEN
    #define NT_TIME_T_LEN  (sizeof("-2147483648") - 1)
#endif

#ifndef NT_MAX_TIME_T_VALUE
    #define NT_MAX_TIME_T_VALUE  2147483647
#endif

#ifndef NT_SYS_NERR
  #define NT_SYS_NERR  1
  #endif


#ifndef NT_HAVE_SYSVSHM
#define NT_HAVE_SYSVSHM  1
#endif

//在 nt_channel.c中需要用，否则编译不过
#ifndef NT_HAVE_MSGHDR_MSG_CONTROL
#define NT_HAVE_MSGHDR_MSG_CONTROL  1                                                                                           
#endif

//必须启用， 否则nt_pipe.c 中找不到nt_pipe.h的两个宏定义
#ifndef T_PIPES
#define T_PIPES  1                                                                                                               
#endif


#define plu_tmp_config_path "/tmp"

#ifndef NT_ERROR_LOG_PATH
#define NT_ERROR_LOG_PATH  plu_tmp_config_path"/dl/.error.log"
#endif

#ifndef NT_CONF_PATH
#define NT_CONF_PATH  plu_tmp_config_path"/dl/index.conf"
#endif



//process
  #ifndef NT_USER
  #define NT_USER  "nobody"
  #endif
  
  
  #ifndef NT_GROUP
  #define NT_GROUP  "nogroup"
  #endif

  #ifndef NT_SUPPRESS_WARN
  #define NT_SUPPRESS_WARN  1
  #endif

  #define NT_OLDPID_EXT     ".oldbin"

  #ifndef NT_PID_PATH
  #define NT_PID_PATH  ".pid"
  #endif
  
  
  #ifndef NT_LOCK_PATH
  #define NT_LOCK_PATH  ".lock"
  #endif

/*
 * pentium NGX_CPU_CACHE_LINE=32
 * pentiumpro | pentium3  NGX_CPU_CACHE_LINE=32
 * pentium4 NGX_CPU_CACHE_LINE=128 
 * athlon  NGX_CPU_CACHE_LINE=64
 * opteron NGX_CPU_CACHE_LINE=64
 * sparc32 NGX_CPU_CACHE_LINE=64
 * sparc64 NGX_CPU_CACHE_LINE=64
 * ppc64 NGX_CPU_CACHE_LINE=128
 * */  
  #ifndef NT_CPU_CACHE_LINE
  #define NT_CPU_CACHE_LINE  64                                                                                   
  #endif

// net socket
// 可以listen unix域
#ifndef NT_HAVE_UNIX_DOMAIN
    #define NT_HAVE_UNIX_DOMAIN 1
#endif

//允许socket 复用
#ifndef NT_HAVE_REUSEPORT
    #define NT_HAVE_REUSEPORT 1
#endif

// 拥有加速器 接收器功能
#ifndef NT_HAVE_ACC_RCV
    #define NT_HAVE_ACC_RCV 1
#endif

//允许设置socket 阻塞，非阻塞
/* #ifndef NT_HAVE_FIONBIO
    #define NT_HAVE_FIONBIO 1
#endif
 */


#endif
