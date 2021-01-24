#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

void time_test( )
{
    time_t start_time = time( NULL );

    while( 1 )
    {
        time_t now = time( NULL );

        int delta = now - start_time;

        if( delta > 0 && ( delta % 10 == 0 ) )
        {
            printf( "10 seconds has past.\n" );
            fflush( stdout );
            sleep( 1 );
        }
    }
}

int main( )
{
    time_test( );

    return 0;
}