#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int** queue_pointer;

void init_queue_pointer( int queue_qnt, int queue_size )
{
    queue_pointer = (int**) malloc( queue_qnt * sizeof( int** ) );

    int* queue = *queue_pointer;

    for(int i = 0; i < queue_qnt; i ++ )
    {
        int* pointer = queue + i;

        pointer = (int*) malloc( queue_size * sizeof( int * ) );

        for( int j = 0; j < queue_size; j++)
        {   
            if( j > 0 )
            {
                printf( ", " );
            }

            *pointer = rand( ) % 10;

            printf( "%d",*pointer );

            pointer++;
        }

        printf("\n");
    }
}

int main( )
{
    srandom( time( NULL ) );

    init_queue_pointer( 5, 10 );
}