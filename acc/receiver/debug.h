#ifndef _REV_DEBUG_H_
#define _REV_DEBUG_H_

#include <stdio.h>
#include <time.h>
#include <stdarg.h>

#define IP4_8(var,n) (var >> n) & 0x000000ff
#define IP4_STR(var) IP4_8(var,0),IP4_8(var,8),IP4_8(var,16),IP4_8(var,24)

static void _debug( const char *f,   char *fmt, ... )
{

    va_list ap; 
    char ts[128];
    time_t t = time( NULL );
    struct tm *tm = localtime( &t );

    FILE *filp;

    filp = stderr;

    strftime( ts, sizeof( ts ), "[%Y-%m-%d %H:%M:%S] ", tm );
    fprintf( filp, "%s", ts );

    //    fprintf(filp, " [%s] ", level_msg_map(level) ); //打印当前函数信息
    fprintf( filp, " %s  ",  f ); //打印当前函数信息


    va_start( ap, fmt );
    vfprintf( filp, fmt, ap );
    fputs( "\n", filp );
    va_end( ap );

}

#define debug( arg... ) _debug( __func__ ,  arg)


#endif

