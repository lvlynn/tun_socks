#include <nt_def.h>
#include <nt_core.h>

int main()
{
    
    

    nt_cycle_t *cycle;
    cycle =( nt_cycle_t *) malloc( sizeof( nt_cycle_t )  );

    nt_event_init( cycle );
    
    for(;;){

        nt_event_process( cycle );
    }

    free( cycle );

    return 0;
}

