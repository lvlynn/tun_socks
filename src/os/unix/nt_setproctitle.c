
#include <nt_core.h>


#if (NT_SETPROCTITLE_USES_ENV)

/*
 * To change the process title in Linux and Solaris we have to set argv[1]
 * to NULL and to copy the title to the same place where the argv[0] points to.
 * However, argv[0] may be too small to hold a new title.  Fortunately, Linux
 * and Solaris store argv[] and environ[] one after another.  So we should
 * ensure that is the continuous memory and then we allocate the new memory
 * for environ[] and copy it.  After this we could use the memory starting
 * from argv[0] for our process title.
 *
 * The Solaris's standard /bin/ps does not show the changed process title.
 * You have to use "/usr/ucb/ps -w" instead.  Besides, the UCB ps does not
 * show a new title if its length less than the origin command line length.
 * To avoid it we append to a new title the origin command line in the
 * parenthesis.
 */

extern char **environ;

static char *nt_os_argv_last;



nt_int_t
nt_init_setproctitle( nt_log_t *log )
{
	u_char      *p;
	size_t       size;
	nt_uint_t   i;

	size = 0;

	for ( i = 0; environ[i]; i++ ) {
		size += nt_strlen( environ[i] ) + 1;
	}

	p = nt_alloc( size, log );
	if ( p == NULL ) {
		return NT_ERROR;
	}

	nt_os_argv_last = nt_os_argv[0];

	for ( i = 0; nt_os_argv[i]; i++ ) {
		if ( nt_os_argv_last == nt_os_argv[i] ) {
			nt_os_argv_last = nt_os_argv[i] + nt_strlen( nt_os_argv[i] ) + 1;
		}
	}

	for ( i = 0; environ[i]; i++ ) {
		if ( nt_os_argv_last == environ[i] ) {

			size = nt_strlen( environ[i] ) + 1;
			nt_os_argv_last = environ[i] + size;

			nt_cpystrn( p, ( u_char * ) environ[i], size );
			environ[i] = ( char * ) p;
			p += size;
		}
	}

	nt_os_argv_last--;

	return NT_OK;
}


void
nt_setproctitle( char *title )
{
	u_char     *p;

#if (NT_SOLARIS)

	nt_int_t   i;
	size_t      size;

#endif

	nt_os_argv[1] = NULL;

	p = nt_cpystrn( ( u_char * ) nt_os_argv[0], ( u_char * ) "funjsq_dl: ",
	                 nt_os_argv_last - nt_os_argv[0] );

	p = nt_cpystrn( p, ( u_char * ) title, nt_os_argv_last - ( char * ) p );

#if (NT_SOLARIS)

	size = 0;

	for ( i = 0; i < nt_argc; i++ ) {
		size += nt_strlen( nt_argv[i] ) + 1;
	}

	if ( size > ( size_t )( ( char * ) p - nt_os_argv[0] ) ) {

		/*
		 * nt_setproctitle() is too rare operation so we use
		 * the non-optimized copies
		 */

		p = nt_cpystrn( p, ( u_char * ) " (", nt_os_argv_last - ( char * ) p );

		for ( i = 0; i < nt_argc; i++ ) {
			p = nt_cpystrn( p, ( u_char * ) nt_argv[i],
			                 nt_os_argv_last - ( char * ) p );
			p = nt_cpystrn( p, ( u_char * ) " ", nt_os_argv_last - ( char * ) p );
		}

		if ( *( p - 1 ) == ' ' ) {
			*( p - 1 ) = ')';
		}
	}

#endif

	if ( nt_os_argv_last - ( char * ) p ) {
		nt_memset( p, NT_SETPROCTITLE_PAD, nt_os_argv_last - ( char * ) p );
	}

	nt_log_debug1( NT_LOG_DEBUG_CORE, nt_cycle->log, 0,
	                "setproctitle: \"%s\"", nt_os_argv[0] );
}

#endif /* NT_SETPROCTITLE_USES_ENV */
