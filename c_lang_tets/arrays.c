#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

int *arr;

struct Test
{   
    int id;
    int counter;
};

struct Test *tests;

void init_tests( int size )
{
    tests = malloc( size * sizeof( struct Test ) );
    
    for( int i = 0; i < size; i++ )
    {
        tests[i].id = i + 1;
        tests[i].counter = rand( ) % 10;
    }
}

void init_arr( int size )
{
    arr = malloc( size * sizeof( int ) );
    
    for( int i = 0; i < size; i++ )
    {   
        arr[ i ] = ( rand( ) % 10 ) + 1;

        printf( "arr[ %d ] = %d.\n", i, *( arr + i ) );
    }
}

void test_persistency( int size )
{
    for( int i = 0; i < size; i++ )
    {
        struct Test test = tests[i];

        printf( "( id: %d, counter: %d )\n", test.id, test.counter );
    }
}

void arr_persistency( int size )
{
     for( int i = 0; i < size; i++ )
    {   
        printf( "arr[ %d ] = %d.\n", i, arr[ i ] );
    }
}

int main( )
{
    srandom( time( NULL ) );

    int size = 10;

    init_tests( size );

    test_persistency( size );

    init_arr( size );

    arr_persistency( size );
}