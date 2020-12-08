#ifndef _NT_AUTO_CONFIG_H_
#define _NT_AUTO_CONFIG_H_

#ifndef NT_DEBUG
    #define NT_DEBUG 1
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


#define plu_tmp_config_path "/tmp"

#ifndef NT_ERROR_LOG_PATH
#define NT_ERROR_LOG_PATH  plu_tmp_config_path"/dl/.error.log"
#endif

#endif
