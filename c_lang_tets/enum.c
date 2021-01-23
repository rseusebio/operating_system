#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

enum InstructionType
{
    CPU_BURST = 0, 
    PRINTER,
    MAGNETIC_TAPE
};

struct Code
{
    int time;
    enum InstructionType instruction;
};

int main( )
{
    struct Code arr[ 10 ];

    struct Code* array_pointer = &arr[0];

    srandom( time( NULL ) );

    for( int i = 0; i < 10; i++ )
    {
        struct Code* p = array_pointer + i;

        int r = rand( ) % 3;

        switch( r )
        {
            case 0:
            {
                p->instruction = CPU_BURST;

                break;
            }
            case 1:
            {
                p->instruction = PRINTER;
                
                break;
            }
            case 2:
            default:
            {
                p->instruction = MAGNETIC_TAPE;
                
                break;
            }
        }
    }

    for( int i = 0; i < 10; i++ )
    {
        printf( "%d. instruction: %d\n", i, arr[i].instruction );
    }

    return 0;
}