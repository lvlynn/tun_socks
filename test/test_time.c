#include <nt_core.h>

int main()
{
    nt_time_init();
    printf("%ld\n", nt_current_msec);
    return 0;
}

