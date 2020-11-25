#ifndef _NT_FILE_H_
#define _NT_FILE_H_


#include <nt_def.h>
#include <nt_core.h>


struct nt_file_s {
    nt_fd_t                   fd;
    nt_str_t                  name;
    nt_file_info_t            info;

    off_t                      offset;
    off_t                      sys_offset;

    nt_log_t                 *log;

    #if (NT_THREADS || NT_COMPAT)
    nt_int_t ( *thread_handler )( nt_thread_task_t *task,
                                   nt_file_t *file );
    void                      *thread_ctx;
    nt_thread_task_t         *thread_task;
    #endif

    #if (NT_HAVE_FILE_AIO || NT_COMPAT)
    nt_event_aio_t           *aio;
    #endif

    unsigned                   valid_info: 1;
    unsigned                   directio: 1;
};


#define NT_MAX_PATH_LEVEL  3


//typedef nt_msec_t ( *nt_path_manager_pt )( void *data );
//ypedef nt_msec_t ( *nt_path_purger_pt )( void *data );
typedef void ( *nt_path_loader_pt )( void *data );



#endif
