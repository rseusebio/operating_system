#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <libconfig.h>
#include <string.h>

int PROCESSES_LIMIT;
int PROCESS_QUANTITY;
int DISK_TIME;
int MAGNETIC_TAPE_TIME;
int PRINTER_TIME;
int READY_QUEUE_QNT;

pthread_t process_creator;
pthread_t cpu_scheduler;
pthread_t printer_scheduler;
pthread_t magnetic_tape_scheduler;
pthread_t disk_scheduler;

enum Status
{
    NEW = 0, 
    READY, 
    RUNNING, 
    WAITING, 
    TERMINATED
};

enum InstructionType
{
    CPU = 100, 
    DISK,
    MAGNETIC_TAPE,
    PRINTER, 
};

struct Instruction
{
    enum InstructionType type;
    int time;
};

struct PCB
{
    int PID;
    int PC;
    enum Status status;
    struct Instruction* instructions;
    int instruction_qnt;
    int start_after;
};

struct PCB          *processes_list;
pthread_mutex_t     processes_lock;

#pragma region  I/O QUEUES

struct PCB          **printer_queue;
int                 printer_queue_pointer;
pthread_mutex_t     printer_lock;


struct PCB          **magnetic_tape_queue;
int                 magnetic_tape_pointer;
pthread_mutex_t     magnetic_tape_lock;


struct PCB          **disk_queue;
int                 disk_queue_pointer;
pthread_mutex_t     disk_lock;

#pragma endregion

// pointer matrix
struct PCB          ***ready_queues;
int                 *ready_pointers;
pthread_mutex_t     ready_queues_lock;
int                 *quantum_time_lists; 

struct PCB          **terminated_queue;
int                 terminated_pointer;
pthread_mutex_t     terminated_lock;

void print_instructions( struct Instruction *instructions, int qnt )
{
    printf( "\tinstructions: [ " );

    for( int i = 0; i < qnt; i++ )
    {
        struct Instruction *inst = instructions + i;
        printf( "( type: %d, time: %d ), ", inst->type, inst->time );
    }

    printf( " ]\n" );
}

void print_process_control_block( struct PCB *pcb )
{
    printf( "{\n" );
    printf( "\tPID: %d\n", pcb->PID );
    printf( "\tPC: %d\n",  pcb->PC );
    printf( "\tstatus: %d\n",  pcb->status );
    printf( "\tinstruction_qnt: %d\n",  pcb->instruction_qnt );

    print_instructions( pcb->instructions, pcb->instruction_qnt );

    printf( "\tstart_after: %d\n",  pcb->start_after );
    printf( "}\n" );
}

void init_simluator( char *config_file_path )
{
    config_t            config;
    config_setting_t    *process_config,
                        *list_config,
                        *process, 
                        *process_instructions,
                        *io_config, 
                        *queue_config,
                        *queues,
                        *queue_elem;

    config_init( &config );

    if( config_read_file( &config, config_file_path ) < 0)
    {   
        printf( "Could not read configuration file: %s\n", config_file_path );
        printf( "Error: %s.\n", config_error_text( &config ) );
        exit( -1 );
    }

    #pragma region PROCESS CONFIGURATION
    
    process_config = config_lookup( &config, "simulator.process_config" );
    if( process_config == NULL )
    {
        printf( "Could not find process configuration.\n" ); 
        exit( -1 );
    }

    if( config_setting_lookup_int( process_config, "limit", &PROCESSES_LIMIT ) < 0)
    {   
        printf( "Could not read process limit.\n" );
        exit( -1 );
    }
    else if( PROCESSES_LIMIT <= 0 )
    {
        printf( "Invalid process limit: %d. It should be a positive integer value.\n", PROCESSES_LIMIT );
        exit( -1 );
    }

    processes_list = malloc( PROCESSES_LIMIT * sizeof( struct PCB ) );

    if( pthread_mutex_init( &processes_lock, NULL ) != 0 )
    {
        printf( "Couldn't create process lock.\n" );
        exit( -1 );
    }

    list_config = config_setting_get_member( process_config, "processes" );
    if( list_config == NULL )
    {
        printf( "Could not read process list.\n" );
        exit( -1 );
    }

    PROCESS_QUANTITY = 0;

    const char *type;
    int time, instruction_qnt, start_after;

    for( int i = 0; i < PROCESSES_LIMIT; i++ )
    {
        struct PCB* pcb = processes_list + i;

        pcb->PID        =   i + 1;
        pcb->PC         =   0;
        pcb->status     =   NEW;

        process = config_setting_get_elem( list_config, i );
        if( process == NULL )
        {
            printf( "Loaded %d processes. Now shutting down...\n", i );
            break;
        }

        PROCESS_QUANTITY++;

        if( config_setting_lookup_int( process, "qnt", &instruction_qnt ) < 0)
        {
            printf( "Could not read quantity of instruction of %d process.\n", i + 1 );
            exit( -1 );
        }
        else if( instruction_qnt <= 0 )
        {
            printf( "Invalid quantity of instructions: %d. Should be greater or equal to 1.", instruction_qnt );
            exit( -1 );
        }

        if( config_setting_lookup_int( process, "start_after", &start_after ) < 0)
        {
            printf( "Could not read start_after of %d process.\n", i + 1 );
            exit( -1 );
        }
        else if( start_after < 0 )
        {
            printf( "Invalid start_after value: %d. Should be greater or equal to 0.", start_after );
            exit( -1 );
        }

        pcb->start_after        =   start_after;
        pcb->instruction_qnt    =   instruction_qnt;
        pcb->instructions       =   malloc( instruction_qnt * sizeof( struct Instruction ) );

        process_instructions = config_setting_get_member( process, "instructions" );
        if( process_instructions == NULL )
        {
            printf( "Could not read instructions member of %d process.\n", i + 1 );
            exit( -1 );
        }
        
        for( int j = 0; j < instruction_qnt; j++ )
        {
            struct Instruction *inst = pcb->instructions + j;

            config_setting_t *instruction = config_setting_get_elem( process_instructions, j );
            if( instruction == NULL )
            {
                printf( "Couldn't find instruction n.%d from process %d.\n", j+1, i+1 );
                exit( -1 );
            }

            if( config_setting_lookup_string( instruction, "type", &type) < 0)
            {
                printf( "Couldn't read field type from process %d.\n", i+1 );
                exit( -1 ); 
            }

            if( strcmp( type, "PRINTER" ) == 0 )
            {
                inst->type = PRINTER;
                inst->time = -1;
            }
            else if( strcmp( type, "MAGNETIC_TAPE" ) == 0 )
            {
                inst->type = MAGNETIC_TAPE;
                inst->time = -1;
            }
            else if( strcmp( type, "DISK" ) == 0 )
            {
                inst->type = DISK;
                inst->time = -1;
            }
            else if( strcmp( type, "CPU" ) == 0 )
            {
                inst->type = CPU;

                if( config_setting_lookup_int( instruction, "time", &time) < 0)
                {
                    printf( "Couldn't read field time from process %d.\n", i+1 );
                    exit( -1 ); 
                }
                else if( time <= 0 )
                {
                    printf( "Invalid time value: %d. Should be greater or equal to 1.", time );
                    exit( -1 );
                }

                inst->time = time;
            }
            else
            {
                printf( "Invalid instruction type at instruction no.%d at process %d.\n", j+1, i+1 ); 
                exit( -1 );
            }
        }

        print_process_control_block( pcb );
    }
    #pragma endregion

    #pragma region I/O CONFIGURATION
    
    io_config = config_lookup( &config, "simulator.io_config" );
    if( io_config == NULL )
    {
        printf( "Couldn't load IO configuration.\n" );
        exit( -1 );
    }

    if( config_setting_lookup_int( io_config, "disk", &DISK_TIME ) < 0 )
    {
        printf( "Couldn't read disk time from configuration.\n" );
        exit( -1 );
    }
    else if( DISK_TIME <= 0 ) 
    {
        printf( "Invalid value of disk time: %d. Should be greater or equal to 1.\n", DISK_TIME );
        exit( -1 );
    }

    if( config_setting_lookup_int( io_config, "printer", &PRINTER_TIME ) < 0 )
    {
        printf( "Couldn't read printer time from configuration.\n" );
        exit( -1 );
    }
    else if( PRINTER_TIME <= 0 ) 
    {
        printf( "Invalid value of printer time: %d. Should be greater or equal to 1.\n", PRINTER_TIME );
        exit( -1 );
    }

    if( config_setting_lookup_int( io_config, "magnetic_tape", &MAGNETIC_TAPE_TIME ) < 0 )
    {
        printf( "Couldn't read magnetic tape time from configuration.\n" );
        exit( -1 );
    }
    else if( MAGNETIC_TAPE_TIME <= 0 ) 
    {
        printf( "Invalid value of magnetic tape time: %d. Should be greater or equal to 1.\n", MAGNETIC_TAPE_TIME );
        exit( -1 );
    }

    #pragma endregion  

    #pragma region CREATING I/O QUEUES

    printer_queue         = malloc( PROCESSES_LIMIT * sizeof( struct PCB * ) );
    printer_queue_pointer = 0;
    if( pthread_mutex_init( &printer_lock, NULL ) != 0 )
    {   
        printf( "Failed to initialize printer's lock.\n");
        exit( -1 );
    }

    magnetic_tape_queue   = malloc( PROCESSES_LIMIT * sizeof( struct PCB *)  );
    magnetic_tape_pointer = 0;
    if( pthread_mutex_init( &magnetic_tape_lock, NULL ) != 0 )
    {   
        printf( "Failed to initialize magnetic tape's lock.\n");
        exit( -1 );
    }

    disk_queue         = malloc( PROCESSES_LIMIT * sizeof( struct PCB * ) );
    disk_queue_pointer = 0;
    if( pthread_mutex_init( &disk_lock, NULL ) != 0 )
    {   
        printf( "Failed to initialize disk's lock.\n");
        exit( -1 );
    }

    #pragma endregion

    #pragma region CREATING READY QUEUES

    queue_config = config_lookup( &config, "simulator.queue_config" );
    if( queue_config == NULL )
    {
        printf( "Couldn't get queue configuration.\n" );
        exit( -1 );
    }

    int quantum_time;

    if( config_setting_lookup_int( queue_config, "quantity", &READY_QUEUE_QNT ) < 0 )
    {
        printf( "Couldn't read queues' quantity.\n" );
        exit( -1 );
    }
    else if( READY_QUEUE_QNT <= 0 ) 
    {
        printf( "Invalid ready queues quantity value: %d. Should be greater or equal to 1.\n", READY_QUEUE_QNT );
        exit( -1 );
    }

    queues = config_setting_get_member( queue_config, "queues" );
    if( queues == NULL )
    {
        printf( "Couldn't read queues member from config file.\n" );
        exit( -1 );
    }

    ready_queues        = malloc( READY_QUEUE_QNT * sizeof( struct PCB ** ) );
    quantum_time_lists  = malloc( READY_QUEUE_QNT * sizeof( int ) );
    ready_pointers      = malloc( READY_QUEUE_QNT * sizeof( int ) );

    for( int i = 0; i < READY_QUEUE_QNT; i++ )
    {
        struct PCB **ready_queue = ( ready_queues + i )[0];

        int *rq_quantum = quantum_time_lists + i;

        ready_queue = malloc( PROCESSES_LIMIT * sizeof( struct PCB * ) );

        ready_pointers[ i ] = 0;

        queue_elem = config_setting_get_elem( queues, i );
        if( queue_elem == NULL )
        {
            printf( "Couldn't retreive element no. %d from queues.\n", i + 1 );
            exit( -1 );
        }

        if( config_setting_lookup_int( queue_elem, "quantum_time", rq_quantum ) < 0 )
        {
            printf( "Couldn't read quantum time of queue no. %d.\n", i + 1 );
            exit( -1 );
        }
        else if( *rq_quantum <= 0 )
        {
            printf( "Invalid quantum time value: %d. Should be greater or equal to 1.\n", *rq_quantum );
            exit( -1 );
        }
    }

    if( pthread_mutex_init( &ready_queues_lock, NULL ) != 0)
    {
        printf( "Couldn't initiate ready queues lock\n" );
        exit( -1 );
    }

    #pragma endregion

    #pragma region  CREATING TERMINATED QUEUE

    terminated_queue    = malloc( PROCESSES_LIMIT * sizeof( struct PCB * ) );
    terminated_pointer  = 0;

    if( pthread_mutex_init( &terminated_lock, NULL ) != 0 )
    {
        printf( "Could not initialize terminated lock.\n" );
        exit( -1 );
    }

    #pragma endregion
    
    printf( "Configuration has ended.\n" );
}

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

        for( int i = 0; i < PROCESS_QUANTITY; i++ )
        {    
            #pragma region READ PROCESS INFORMATION

            pthread_mutex_lock( &processes_lock );

            struct PCB *process = processes_list + i;

            int pid = process->PID;

            if( process->status != NEW )
            {
                pthread_mutex_unlock( &processes_lock );
                continue;
            }

            if( shortest_start_after < 0 || process->start_after < shortest_start_after )
            {
                shortest_start_after = process->start_after;
            }

            int past_time  = time( NULL ) - start_time;
            int should_create = process->start_after <= past_time;

            // printf( "PID: %d, past_time: %d, should_create: %d, start_after: %d\n", pid, past_time, should_create, process->start_after );

            pthread_mutex_unlock( &processes_lock );

            #pragma endregion

            #pragma region PUTTING PROCESS ON READY QUEUE

            if( should_create )
            {
                pthread_mutex_lock( &processes_lock );
                process->status  = READY;
                pthread_mutex_unlock( &processes_lock );

                pthread_mutex_lock( &ready_queues_lock );
                
                struct PCB **ready_queue = ready_queues[ 0 ];

                ready_queue[ ready_pointers[ 0 ]++ ] = process;
                
                processes_created++;

                pthread_mutex_unlock( &ready_queues_lock );

                printf( "Created process PID: %d at %d secs.\n ", pid, past_time );
            }

            #pragma endregion 
        }

        int past_time = time( NULL ) - start_time;

        // printf( "Total processes created: %d, shortest_start_after: %d\n ", processes_created, shortest_start_after );

        // sleep until the next process
        int sleep_time = shortest_start_after - past_time;
        if ( sleep_time > 0 )
        {
            printf( "About to sleep for %d seconds.\n", sleep_time );
            sleep( sleep_time );
        }
    }

    printf( "processes creator has ended\n" );  
}

struct PCB * get_next_element( struct PCB **ready_queue, int length )
{
    struct PCB *pcb = ready_queue[ 0 ];

    for( int i = 0; i < length - 1; i++ )
    {
        ready_queue[ i ] = ready_queue[ i + 1 ];
    }

    ready_queue[ length - 1] = NULL;

    return pcb;

}

void * cpu_scheduler_thread( void *arg )
{
    while( 1 )
    {
        // get a snapshot of ready_pointers;

        for( int i  = 0; i < READY_QUEUE_QNT; i++ )
        {

            if( i > 0 )
            {
                // should check if there is another process
                // recently arrived in higher priorities queue
            }

            pthread_mutex_lock( &ready_queues_lock );

            if( ready_pointers[ i ] <= 0 )
            {
                pthread_mutex_unlock( &ready_queues_lock );

                continue;
            }

            struct PCB **ready_queue = ready_queues[ i ];

            struct PCB *pcb = get_next_element( ready_queue, ready_pointers[ i ]-- );

            pthread_mutex_unlock( &ready_queues_lock );

            pthread_mutex_lock( &processes_lock );

            if( pcb->PC >= pcb->instruction_qnt )
            {
                pcb->status == TERMINATED;

                continue;
            }

            struct Instruction *inst = pcb->instructions + pcb->PC;

            pthread_mutex_unlock( &processes_lock );

            printf( "CPU_SCHEDULER :: PID: %d, PC: %d, int_qnt: %d, int_type: %d, int_time: %d.\n", pcb->PID, pcb->PC, pcb->instruction_qnt, inst->type, inst->time );

            switch( inst->type )
            {
                case DISK:
                {
                    pthread_mutex_lock( &disk_lock );

                    disk_queue[ disk_queue_pointer++ ] = pcb;

                    pthread_mutex_unlock( &disk_lock );

                    break;
                }
                case MAGNETIC_TAPE:
                {
                    pthread_mutex_lock( &magnetic_tape_lock );

                    magnetic_tape_queue[ magnetic_tape_pointer++ ] = pcb;

                    pthread_mutex_unlock( &magnetic_tape_lock );

                    break;
                }
                case PRINTER:
                {
                    pthread_mutex_lock( &printer_lock );

                    printer_queue[ printer_queue_pointer++ ] = pcb;

                    pthread_mutex_unlock( &printer_lock );

                    break;
                }
                default:
                case CPU:
                {
                    if( inst->time <= quantum_time_lists[ i ] )
                    {
                        sleep( inst->time );
                        
                        pthread_mutex_lock( &processes_lock );

                        inst->time = 0;
                        
                        pcb->PC++;

                        pthread_mutex_unlock( &processes_lock );
                    }
                    else
                    {
                        sleep( quantum_time_lists[ i ] );

                        pthread_mutex_lock( &processes_lock );

                        inst->time -= quantum_time_lists[ i ];

                        pthread_mutex_unlock( &processes_lock );
                    }
                    break;
                }
            }
        }
    }
}

void * io_thread ( void *arg )
{
    char *io_type = (char *) arg;

    pthread_mutex_t     *io_lock; 
    struct PCB          **io_queue;
    int                 *io_queue_pointer;
    int                 blocked_time;

    if( strcmp( io_type, "DISK" ) == 0 )
    {
        io_queue         =  disk_queue;
        io_lock          =  &disk_lock;
        io_queue_pointer =  &disk_queue_pointer;
        blocked_time     =  DISK_TIME;
    }
    else if( strcmp( io_type, "PRINTER" ) == 0 )
    {
        io_queue         =  printer_queue;
        io_lock          =  &printer_lock;
        io_queue_pointer =  &printer_queue_pointer;
        blocked_time     =  PRINTER_TIME;
    }
    else if( strcmp( io_type, "MAGNETIC_TAPE" ) == 0 )
    {   
        io_queue         =  magnetic_tape_queue;
        io_lock          =  &magnetic_tape_lock;
        io_queue_pointer =  &magnetic_tape_pointer;
        blocked_time     =  MAGNETIC_TAPE_TIME;
    }
    else 
    {
        printf( "Invalid IO type: %s. Exiting system...\n", io_type );
        exit( -1 );
    }

    while( 1 )
    {
        pthread_mutex_lock( io_lock );

        if( *io_queue_pointer <= 0 )
        {
            pthread_mutex_unlock( io_lock );
            
            continue;
        }

        struct PCB *pcb = get_next_element( io_queue, *io_queue_pointer );

        *io_queue_pointer -= 1;

        pthread_mutex_unlock( io_lock );

        sleep( blocked_time );

        pthread_mutex_lock( &processes_lock );

        pcb->PC++;

        pthread_mutex_unlock( &processes_lock );

        // sending process back to the first list
        // instead, should every io device sends a process to 
        // a different place

        pthread_mutex_lock( &ready_queues_lock );

        ready_queues[0][ ready_pointers[0]++ ] = pcb;

        pthread_mutex_unlock( &ready_queues_lock );
    }
}

int main( int argc, char *argv[] )
{
   char *file_name = "./configfile.txt";

   init_simluator( file_name );

   int start_time = time( NULL );

   printf( "PROCESSES_LIMIT: %d, READY_QUEUE_QNT: %d, DISK_TIME: %d, PRINTER_TIME: %d, MAGNETIC_TAPE_TIME: %d \n", 
           PROCESSES_LIMIT, 
           READY_QUEUE_QNT,
           DISK_TIME, 
           PRINTER_TIME, 
           MAGNETIC_TAPE_TIME );

    char *printer        = "PRINTER", 
        *disk           = "DISK", 
        *magnetic_tape  = "MAGNETIC_TAPE";

   pthread_create( &process_creator, NULL, &process_creator_thread, ( void * ) ( &start_time ) );

   pthread_create( &cpu_scheduler, NULL, &cpu_scheduler_thread, NULL );

   pthread_create( &printer_scheduler,       NULL, &io_thread, ( void * ) printer );
   pthread_create( &disk_scheduler,          NULL, &io_thread, ( void * ) disk );
   pthread_create( &magnetic_tape_scheduler, NULL, &io_thread, ( void * ) magnetic_tape );

   pthread_join( process_creator,         NULL );
   pthread_join( cpu_scheduler,           NULL );
   pthread_join( printer_scheduler,       NULL );
   pthread_join( disk_scheduler,          NULL );
   pthread_join( magnetic_tape_scheduler, NULL );

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

   return 0;
}