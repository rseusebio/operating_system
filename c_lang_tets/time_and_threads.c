#include <pthread.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <math.h>
#include <time.h>
  
pthread_t t1;
pthread_t t2;

pthread_mutex_t lock; 
pthread_mutex_t lock2; 

int counter; 

int arr[10];

struct ThreadConfig
{
    time_t start_time;
    char name[100];
    int seconds_waiting;
};


void* trythis( void* arg ) 
{ 
    int* tid = (int*) arg;

    printf( "hello, i'm thread %d.\n", *tid );

    pthread_mutex_lock( &lock) ; 

    srand( time( NULL ) );
      
    counter += round( rand( ) % 10 ) ; 

    printf( "\nJob %d has started :: counter: %d\n", *tid, counter ); 
    
    pthread_mutex_unlock( &lock ); 

    for( int i = 0; i < 10; i++ )
    {
        int r = rand() % 3;

        sleep( r );

        pthread_mutex_lock( &lock2 );

        if ( !arr[i] )
        {
            arr[i] = *tid;
        }

        pthread_mutex_unlock( &lock2 );
    }

    printf( "\n Job %d has finished\n", *tid ); 
  
    return NULL; 
} 


void* thread_func( void* arg )
{
    struct ThreadConfig* thread_config =  (struct ThreadConfig*) arg;

    int wasted_seconds = time( NULL ) - thread_config->start_time;

    printf( "[ %s ] :: wasted seconds: %d.\n", thread_config->name, wasted_seconds );

    while( time( NULL ) - thread_config->start_time + wasted_seconds <= thread_config->seconds_waiting )
    {

    }

    printf( "[ %s ] :: waited %d seconds.\n", thread_config->name, thread_config->seconds_waiting );

}

int main( ) 
{   
    int time_waiting = 5;

    time_t start_time = time( NULL );

    struct ThreadConfig th1;
    struct ThreadConfig th2;

    th1.start_time = start_time;
    th2.start_time = start_time;
    th1.seconds_waiting = 10;
    th2.seconds_waiting = 5;

    strcpy( th1.name, "First Thread" );
    strcpy( th2.name, "Second Thread" );
    
    int id = 1;

    int res = pthread_create( &t1,  NULL, &thread_func, (void*) (&th1) ); 

    if (res != 0) 
    {
        printf( "\nThread couldn't be created :[%s]", strerror( res ) ); 

        exit( -1 );
    }

    printf( "created thread: %ld.\n", t1 );

    int id2 = 2;

    res = pthread_create( &t2,  NULL, &thread_func, (void*) (&th2) ); 

    if (res != 0) 
    {
        printf( "\nThread couldn't be created :[%s]", strerror( res ) ); 

        exit( -1 );
    }

    printf( "created thread: %ld.\n", t2 );
            
    pthread_join( t1, NULL ); 
    pthread_join( t2, NULL ); 

    pthread_mutex_destroy( &lock ); 
    pthread_mutex_destroy( &lock2 ); 

    for( int i = 0; i < 10; i++ )
    {
        printf( "%d. %d\n", i, arr[i] );
    }
  
    return 0; 
} 
