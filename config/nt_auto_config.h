#ifndef _NT_AUTO_CONFIG_H_
    #define _NT_AUTO_CONFIG_H_

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

    #ifndef NT_TIME_T_SIZE
        #define NT_TIME_T_SIZE  4
    #endif


    #ifndef NT_TIME_T_LEN
        #define NT_TIME_T_LEN  (sizeof("-2147483648") - 1)
    #endif

#endif
