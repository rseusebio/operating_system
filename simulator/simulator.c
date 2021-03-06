#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <libconfig.h>
#include <string.h>

#include "utilities.c"

void * process_creator_thread( void *arg )
{
    int start_time = *( (int *) arg );

    int processes_created = 0;

    while( 1 )
    {
        int shortest_start_after = -1;

        if( processes_created == PROCESS_QUANTITY )
        {
            break;
        }

        #pragma region LIMIT CHECKING
        
        pthread_mutex_lock( &terminated_lock );

        int terminated_processes = terminated_pointer;

        pthread_mutex_unlock( &terminated_lock );

        if( processes_created - terminated_processes >= PROCESSES_LIMIT )
        {
            printf( "Process Creator :: Waiting until a process is terminated." );

            continue;
        }

        #pragma endregion

        for( int i = 0; i < PROCESS_QUANTITY; i++ )
        {    
            #pragma region READ PROCESS INFORMATION

            pthread_mutex_lock( &processes_lock );

            struct PCB *pcb = processes_list + i;

            int pid = pcb->PID;

            if( pcb->status != NEW )
            {
                pthread_mutex_unlock( &processes_lock );

                continue;
            }

            pthread_mutex_unlock( &processes_lock );

            if( shortest_start_after < 0 || pcb->start_after < shortest_start_after )
            {
                shortest_start_after = pcb->start_after;
            }

            int past_time  = time( NULL ) - start_time;
            int should_create = pcb->start_after <= past_time;

            #pragma endregion

            #pragma region PUTTING PROCESS ON READY QUEUE

            if( should_create )
            {
                pcb->status  = READY;

                pthread_mutex_lock( &ready_queues_lock );
                
                ready_queues[ 0 ][ ready_pointers[ 0 ] ] = pcb;

                ready_pointers[ 0 ] += 1;
                
                processes_created++;

                printf( "Process Creator :: Created a new process { PID: %d } at %d second(s).\n", pid, past_time );

                print_ready_queue_elements( ready_queues[ 0 ], ready_pointers[ 0 ] );

                pthread_mutex_unlock( &ready_queues_lock );
            }

            #pragma endregion 
        }

        int past_time   = time( NULL ) - start_time;

        int sleep_time  = shortest_start_after - past_time;

        if ( sleep_time > 0 )
        {
            printf( "Process Creator :: Sleeping for %d second(s) until next process can be created.\n", sleep_time );

            sleep( sleep_time );
        }
    }

    printf( "Process Creator :: Exiting.\n" );  
}

void * cpu_scheduler_thread( void *arg )
{
    time_t start_time = *( (time_t *) arg );

    int i = 0;

    while( 1 )
    {   
        #pragma region  CHECKING IF THERE IS ANY REMAINING PROCESS

        pthread_mutex_lock( &terminated_lock );

        if( terminated_pointer >= PROCESS_QUANTITY )
        {
            pthread_mutex_unlock( &terminated_lock );

            break;
        }

        pthread_mutex_unlock( &terminated_lock );

        #pragma endregion

        #pragma region  CHECKING IF THERE IS ANY PROCESS IN THIS LIST

        pthread_mutex_lock( &ready_queues_lock );

        if( ready_pointers[ i ] <= 0 )
        {
            pthread_mutex_unlock( &ready_queues_lock );

            i = ( i + 1 ) % READY_QUEUE_QNT;

            continue;
        }

        pthread_mutex_unlock( &ready_queues_lock );

        #pragma endregion

        // taking a snapshot of how the ready queues look like

        pthread_mutex_lock( &ready_queues_lock );

        int *snapshot = get_snapshot( ready_pointers , READY_QUEUE_QNT );

        printf( "CPU Scheduler :: Ready Queue No. %d with %d elements.\n", i, ready_pointers[ i ] );
        fflush( stdout );

        print_ready_queue_elements( ready_queues[ i ], ready_pointers[ i ] );

        struct PCB *pcb = get_next_element( ready_queues[ i ], ready_pointers[ i ] );

        ready_pointers[ i ] -= 1;

        pthread_mutex_unlock( &ready_queues_lock );

        struct Instruction *inst = pcb->instructions + pcb->PC;

        switch( inst->type )
        {
            case DISK:
            {   
                pcb->status = WAITING;

                pthread_mutex_lock( &disk_lock );

                disk_queue[ disk_queue_pointer++ ] = pcb;

                pthread_mutex_unlock( &disk_lock );

                printf( "CPU Scheduler :: process { PID: %d } is waiting for disk.\n", pcb->PID );

                break;
            }
            case MAGNETIC_TAPE:
            {
                pcb->status = WAITING;

                pthread_mutex_lock( &magnetic_tape_lock );

                magnetic_tape_queue[ magnetic_tape_pointer++ ] = pcb;

                pthread_mutex_unlock( &magnetic_tape_lock );

                printf( "CPU Scheduler :: process { PID: %d } is waiting for magnetic tape.\n", pcb->PID );

                break;
            }
            case PRINTER:
            {
                pcb->status = WAITING;

                pthread_mutex_lock( &printer_lock );

                printer_queue[ printer_queue_pointer++ ] = pcb;

                pthread_mutex_unlock( &printer_lock );

                printf( "CPU Scheduler :: process { PID: %d } is waiting for printer.\n", pcb->PID );

                break;
            }
            case CPU:
            {
                pcb->status = RUNNING;

                int running_time = time_quantum_list[ i ];

                if( inst->time <= time_quantum_list[ i ] )
                {
                    running_time = inst->time;
                }

                int ran_time      = 0;
                int changed_queue = -1;

                printf( "CPU Scheduler :: Process { PID: %d } will be executed.\n", pcb->PID );

                while( running_time > 0 )
                {
                    #pragma region Saving Execution Record 
                                
                    struct ExecRecord *record = cpu_container.records + cpu_container.pointer;

                    cpu_container.pointer += 1;

                    record->PID        = pcb->PID;    
                    record->start_time = time( NULL ) - start_time;
                    record->end_time   = record->start_time + 1;
                    
                    #pragma endregion

                    sleep( 1 );

                    ran_time++;
                    running_time--;

                    #pragma region  CHECK HIGHER PRIORITY QUEUES

                    if( i > 0 )
                    {
                        pthread_mutex_lock( &ready_queues_lock );

                        if( any_change( ready_pointers, snapshot, i, &changed_queue ) > 0 )
                        {
                            printf( "CPU Scheduler :: Execution of process { PID: %d } at Queue No. %d was preempted by Queue No. %d.\n",pcb->PID,  i, changed_queue );
                            fflush( stdout );

                            pthread_mutex_unlock( &ready_queues_lock );

                            break;
                        }

                        pthread_mutex_unlock( &ready_queues_lock );
                    }

                    #pragma endregion 
                }

                inst->time -= ran_time;

                pcb->status = READY;

                printf( "CPU Scheduler :: Process { PID: %d } ran for %d second(s).\n", pcb->PID, ran_time );
                fflush( stdout );

                if( inst->time <= 0 )
                {
                    pcb->PC++;

                    #pragma region  CHECKING IF A PROCESS HAS FINISHED
        
                    if( pcb->PC >= pcb->instruction_qnt )
                    {
                        pcb->status = TERMINATED;

                        pthread_mutex_lock( &terminated_lock );

                        terminated_queue[ terminated_pointer++ ] = pcb;

                        pthread_mutex_unlock( &terminated_lock );

                        printf( "CPU Scheduler :: Process { PID: %d } has finished.\n", pcb->PID );
                        fflush( stdout );
                    }

                    #pragma endregion
                }

                #pragma region  SWITCH TO HIGHER PRIORITY QUEUE IF THERE IS ANY CHANGE
                
                if( changed_queue >= 0 )
                {
                    #pragma region  PUSHING PROCESS BACK TO ITS ORIGINAL QUEUE

                    if( pcb->status != TERMINATED )
                    {
                        pthread_mutex_lock( &ready_queues_lock );

                        ready_queues[ i ][ ready_pointers[ i ] ] = pcb;

                        ready_pointers[ i ] += 1;

                        pthread_mutex_unlock( &ready_queues_lock );
                    }

                    #pragma endregion

                    printf( "CPU Scheduler :: Switching from Queue No. %d to Queue No. %d.\n", i, changed_queue );

                    i = changed_queue;
                    
                    continue;
                }

                #pragma endregion

                #pragma region  PUSHING PROCESS BACK TO SOME READY QUEUE

                if( pcb->status == TERMINATED )
                {
                    continue;
                }

                pthread_mutex_lock( &ready_queues_lock );
            
                if( i == READY_QUEUE_QNT - 1 )
                {
                    ready_queues[ i ][ ready_pointers[ i ] ] = pcb;

                    ready_pointers[ i ] += 1;
                }
                else
                {                    
                    ready_queues[ i + 1 ][ ready_pointers[ i + 1 ] ] = pcb;

                    ready_pointers[ i + 1 ] += 1;
                }

                pthread_mutex_unlock( &ready_queues_lock );

                #pragma endregion

                break;
            }
            default:
            {
                printf( "Invalid instruction type: %s of process { PID: %d }.\n", instruction_type( inst->type ), pcb->PID );
                exit( -1 );
            }
        }
    }

    printf( "CPU Scheduler :: No more process to execute.\n" );
}

void * io_thread( void *arg )
{
    time_t start_time = time( NULL );

    char *io_type = (char *) arg;

    pthread_mutex_t         *io_lock; 
    struct PCB              **io_queue;
    int                     *io_queue_pointer;
    int                     blocked_time;
    struct RecordContainer  *record_container;

    #pragma region SETTING IO TYPE

    if( strcmp( io_type, "Disk" ) == 0 )
    {
        io_queue         =  disk_queue;
        io_lock          =  &disk_lock;
        io_queue_pointer =  &disk_queue_pointer;
        blocked_time     =  DISK_TIME;
        record_container =  &disk_container;
    }
    else if( strcmp( io_type, "Printer" ) == 0 )
    {
        io_queue         =  printer_queue;
        io_lock          =  &printer_lock;
        io_queue_pointer =  &printer_queue_pointer;
        blocked_time     =  PRINTER_TIME;
        record_container =  &printer_container;
    }
    else if( strcmp( io_type, "Magnetic Tape" ) == 0 )
    {   
        io_queue         =  magnetic_tape_queue;
        io_lock          =  &magnetic_tape_lock;
        io_queue_pointer =  &magnetic_tape_pointer;
        blocked_time     =  MAGNETIC_TAPE_TIME;
        record_container =  &magnetic_tape_container;
    }
    else 
    {
        printf( "Invalid IO type: %s. Exiting system...\n", io_type );

        exit( -1 );
    }

    #pragma endregion

    while( 1 )
    {  
        #pragma region  CHECKING IF THERE IS ANY PROCESSES LEFT
        
        pthread_mutex_lock( &terminated_lock );

        if( terminated_pointer >= PROCESS_QUANTITY )
        {
            pthread_mutex_unlock( &terminated_lock );

            break;
        }

        pthread_mutex_unlock( &terminated_lock );
        
        #pragma endregion

        pthread_mutex_lock( io_lock );

        if( *io_queue_pointer <= 0 )
        {
            pthread_mutex_unlock( io_lock );
            
            continue;
        }

        struct PCB *pcb = get_next_element( io_queue, *io_queue_pointer );

        *io_queue_pointer -= 1;

        pthread_mutex_unlock( io_lock );

        printf( "%s :: Blocking process { PID: %d } for %d second(s).\n", io_type, pcb->PID, blocked_time );
        fflush( stdout );

        #pragma region Saving Execution Record 
                    
        struct ExecRecord *record = record_container->records + record_container->pointer;

        record_container->pointer += 1;

        record->PID        = pcb->PID;    
        record->start_time = time( NULL ) - start_time;
        record->end_time   = record->start_time + blocked_time;
        
        #pragma endregion

        sleep( blocked_time );

        pcb->PC++;

        if( pcb->PC >= pcb->instruction_qnt ) 
        {
            pcb->status = TERMINATED;

            printf( "%s :: process { PID: %d } has blocked for %d second(s) and has terminated.\n", io_type, pcb->PID, blocked_time );
            fflush( stdout );

            pthread_mutex_lock( &terminated_lock );

            terminated_queue[ terminated_pointer++ ] = pcb;

            pthread_mutex_unlock( &terminated_lock );
        }
        else 
        {
            int ready_queue_index = 0;

            if( strcmp( io_type, "Disk" ) == 0 )
            {
                ready_queue_index = READY_QUEUE_QNT  - 1;
            }

            pcb->status = READY;

            printf( "%s :: process { PID: %d } blocked for %d second(s) and is now ready at queue no. %d.\n", io_type, pcb->PID, blocked_time, ready_queue_index );
            fflush( stdout );

            pthread_mutex_lock( &ready_queues_lock );

            ready_queues[ ready_queue_index ][ ready_pointers[ ready_queue_index ]++ ] = pcb;

            pthread_mutex_unlock( &ready_queues_lock );
        }
    }

    printf( "%s :: No more processes to execute.\n", io_type );
    fflush( stdout );
}

int main( int argc, char *argv[] )
{
   parse_configfile( "./ConfigFile2.txt" );

   print_configuration( processes_list, PROCESS_QUANTITY, PROCESSES_LIMIT, READY_QUEUE_QNT, DISK_TIME, PRINTER_TIME, MAGNETIC_TAPE_TIME );

   int start_time = time( NULL );

   char *printer         =     "Printer", 
        *disk            =     "Disk", 
        *magnetic_tape   =     "Magnetic Tape";

   pthread_create( &process_creator, NULL, &process_creator_thread, ( void * ) &start_time );
   pthread_create( &cpu_scheduler,   NULL, &cpu_scheduler_thread,   ( void * ) &start_time );
   pthread_create( &printer_scheduler,       NULL, &io_thread, ( void * ) printer );
   pthread_create( &disk_scheduler,          NULL, &io_thread, ( void * ) disk );
   pthread_create( &magnetic_tape_scheduler, NULL, &io_thread, ( void * ) magnetic_tape );

   pthread_join( process_creator,         NULL );
   pthread_join( cpu_scheduler,           NULL );
   pthread_join( printer_scheduler,       NULL );
   pthread_join( disk_scheduler,          NULL );
   pthread_join( magnetic_tape_scheduler, NULL );

   printf( "All threads have finished.\n" );

   pthread_mutex_destroy( &ready_queues_lock );
   pthread_mutex_destroy( &processes_lock );
   pthread_mutex_destroy( &disk_lock );
   pthread_mutex_destroy( &printer_lock );
   pthread_mutex_destroy( &magnetic_tape_lock );

   free( ready_queues );
   free( ready_pointers );
   free( disk_queue );
   free( printer_queue );
   free( magnetic_tape_queue );
   free( terminated_queue );
   free( processes_list );

   printf( "Saving records...\n" );
    
   save_records( "../charts/src/simulation_records.json" );

   printf( "\nSIMULATOR IS OVER.\n" );

   system( "cd ../charts && npm run start " );

   return 0;
}