#include <pthread.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <math.h>
  
pthread_t t1;
pthread_t t2;

pthread_mutex_t lock; 
pthread_mutex_t lock2; 

int counter; 

int arr[10];

  
void* trythis( void* arg ) 
{ 
    int* tid = (int*) arg;

    printf( "hello, i'm thread %d.\n", *tid );

    pthread_mutex_lock( &lock) ; 

    srand( time( NULL ) );
      
    counter += round( rand( ) % 10 ) ; 

    printf( "\n Job %d has started :: counter: %d\n", *tid, counter ); 
    
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
  
int main( ) 
{ 
    int i = 0; 

    int error; 
  
    if( pthread_mutex_init( &lock, NULL ) != 0 ) 
    { 
        printf("\n mutex init has failed\n"); 

        return 1; 
    } 

    if( pthread_mutex_init( &lock2, NULL ) != 0 ) 
    { 
        printf("\n mutex init has failed\n"); 

        return 1; 
    } 
    
    int id = 1;

    int res = pthread_create(  &t1,  NULL, &trythis, (void*) (&id) ); 

    if (res != 0) 
    {
        printf( "\nThread can't be created :[%s]", strerror(error) ); 

        exit( -1 );
    }

    printf( "created thread: %ld.\n", t1 );

    int id2 = 2;

    res = pthread_create(  &t2,  NULL, &trythis, (void*) (&id2) ); 

    if (res != 0) 
    {
        printf( "\nThread can't be created :[%s]", strerror(error) ); 

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
