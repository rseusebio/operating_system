#include <stdio.h>
#include <stdlib.h>
#include <libconfig.h>
#include <pthread.h>

#include "structs_&_enums.c"

int         PROCESSES_LIMIT;
int         PROCESS_QUANTITY;
int         DISK_TIME;
int         MAGNETIC_TAPE_TIME;
int         PRINTER_TIME;
int         READY_QUEUE_QNT;

pthread_t       process_creator;
pthread_t       cpu_scheduler;
pthread_t       printer_scheduler;
pthread_t       magnetic_tape_scheduler;
pthread_t       disk_scheduler;

struct PCB          *processes_list;
pthread_mutex_t     processes_lock;

struct RecordContainer cpu_container;

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


void parse_configfile( char *config_file_path )
{
    int  CPU_execs         =   0,
         printer_execs     =   0,
         disk_execs        =   0,
         magnetic_execs    =   0,
         greatest_cpu_time =   0,
         smallest_qt       =   0;
    
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
        printf( "Parser :: Could not read configuration file: %s\n", config_file_path );
        printf( "Parser :: Error: %s.\n", config_error_text( &config ) );
        exit( -1 );
    }

    #pragma region PROCESS CONFIGURATION
    
    process_config = config_lookup( &config, "simulator.process_config" );
    if( process_config == NULL )
    {
        printf( "Parser :: Could not find process configuration.\n" ); 
        exit( -1 );
    }

    if( config_setting_lookup_int( process_config, "limit", &PROCESSES_LIMIT ) < 0)
    {   
        printf( "Parser :: Could not read process limit.\n" );
        exit( -1 );
    }
    else if( PROCESSES_LIMIT <= 0 )
    {
        printf( "Parser :: Invalid process limit: %d. It should be a positive integer value.\n", PROCESSES_LIMIT );
        exit( -1 );
    }

    if( config_setting_lookup_int( process_config, "quantity", &PROCESS_QUANTITY ) < 0)
    {   
        printf( "Parser :: Could not read process limit.\n" );
        exit( -1 );
    }
    else if( PROCESSES_LIMIT <= 0 )
    {
        printf( "Parser :: Invalid process quantity: %d. It should be a positive integer value.\n", PROCESS_QUANTITY );
        exit( -1 );
    }

    processes_list = malloc( PROCESSES_LIMIT * sizeof( struct PCB ) );

    if( pthread_mutex_init( &processes_lock, NULL ) != 0 )
    {
        printf( "Parser :: Couldn't create process lock.\n" );
        exit( -1 );
    }

    list_config = config_setting_get_member( process_config, "processes" );
    if( list_config == NULL )
    {
        printf( "Parser :: Could not read process list.\n" );
        exit( -1 );
    }
    
    for( int i = 0; i < PROCESS_QUANTITY; i++ )
    {
        struct PCB* pcb = processes_list + i;

        pcb->PID        =   i + 1;
        pcb->PC         =   0;
        pcb->status     =   NEW;

        process = config_setting_get_elem( list_config, i );
        if( process == NULL )
        {
            printf( "Parser :: Could not loaded process no. %d.\n", i + 1 );
            exit( -1 );
        }

        if( config_setting_lookup_int( process, "qnt", &( pcb->instruction_qnt ) ) < 0 )
        {
            printf( "Parser :: Could not read quantity of instruction of %d process.\n", i + 1 );
            exit( -1 );
        }
        else if( pcb->instruction_qnt <= 0 )
        {
            printf( "Parser :: Invalid quantity of instructions: %d. Should be greater or equal to 1.", pcb->instruction_qnt );
            exit( -1 );
        }

        if( config_setting_lookup_int( process, "start_after", &( pcb->start_after ) ) < 0 )
        {
            printf( "Parser :: Could not read start_after of %d process.\n", i + 1 );
            exit( -1 );
        }
        else if( pcb->start_after < 0 )
        {
            printf( "Parser :: Invalid start_after value: %d. Should be greater or equal to 0.", pcb->start_after );
            exit( -1 );
        }

        pcb->instructions = malloc( pcb->instruction_qnt * sizeof( struct Instruction ) );

        process_instructions = config_setting_get_member( process, "instructions" );
        if( process_instructions == NULL )
        {
            printf( "Parser :: Could not read instructions member of %d process.\n", i + 1 );
            exit( -1 );
        }
        
        for( int j = 0; j < pcb->instruction_qnt; j++ )
        {
            const char *type;
 
            struct Instruction *inst = pcb->instructions + j;

            config_setting_t *instruction = config_setting_get_elem( process_instructions, j );
            if( instruction == NULL )
            {
                printf( "Parser :: Couldn't find instruction n.%d of process %d.\n", j + 1, i + 1 );
                exit( -1 );
            }

            if( config_setting_lookup_string( instruction, "type", &type ) < 0)
            {
                printf( "Parser :: Couldn't read field type from process %d.\n", i+1 );
                exit( -1 ); 
            }

            if( strcmp( type, "PRINTER" ) == 0 )
            {
                printer_execs += 1;

                inst->type = PRINTER;
                inst->time = -1;
            }
            else if( strcmp( type, "MAGNETIC_TAPE" ) == 0 )
            {
                magnetic_execs += 1;
                
                inst->type = MAGNETIC_TAPE;
                inst->time = -1;
            }
            else if( strcmp( type, "DISK" ) == 0 )
            {   
                disk_execs += 1;

                inst->type = DISK;
                inst->time = -1;
            }
            else if( strcmp( type, "CPU" ) == 0 )
            {
                CPU_execs += 1;

                inst->type = CPU;

                if( config_setting_lookup_int( instruction, "time", &inst->time) < 0)
                {
                    printf( "Parser :: Couldn't read field time from process %d.\n", i+1 );
                    exit( -1 ); 
                }
                else if( inst->time <= 0 )
                {
                    printf( "Parser :: Invalid time value: %d. Should be greater or equal to 1.", inst->time );
                    exit( -1 );
                }

                if( greatest_cpu_time == 0 || inst->time > greatest_cpu_time )
                {
                    greatest_cpu_time = inst->time;
                }
            }
            else
            {
                printf( "Parser :: Invalid instruction type at instruction no.%d at process %d.\n", j+1, i+1 ); 
                exit( -1 );
            }
        }
    }
    
    printf( "Parser :: CPU executions: %d; Greatest CPU time: %d seconds.\n", CPU_execs, greatest_cpu_time );

    cpu_container.records = malloc( CPU_execs * greatest_cpu_time * sizeof( struct ExecRecord ) );
    cpu_container.pointer = 0;
    cpu_container.size    = 0;
    if( pthread_mutex_init( &cpu_container.lock, NULL ) != 0 )
    {   
        printf( "Parser :: Couldn't init cpu_container's lock.\n" );
        exit( -1 );
    }    
    
    #pragma endregion

    #pragma region I/O CONFIGURATION
    
    io_config = config_lookup( &config, "simulator.io_config" );
    if( io_config == NULL )
    {
        printf( "Parser :: Couldn't load IO configuration.\n" );
        exit( -1 );
    }

    if( config_setting_lookup_int( io_config, "disk", &DISK_TIME ) < 0 )
    {
        printf( "Parser :: Couldn't read disk time from configuration.\n" );
        exit( -1 );
    }
    else if( DISK_TIME <= 0 ) 
    {
        printf( "Parser :: Invalid value of disk time: %d. Should be greater or equal to 1.\n", DISK_TIME );
        exit( -1 );
    }

    if( config_setting_lookup_int( io_config, "printer", &PRINTER_TIME ) < 0 )
    {
        printf( "Parser :: Couldn't read printer time from configuration.\n" );
        exit( -1 );
    }
    else if( PRINTER_TIME <= 0 ) 
    {
        printf( "Parser :: Invalid value of printer time: %d. Should be greater or equal to 1.\n", PRINTER_TIME );
        exit( -1 );
    }

    if( config_setting_lookup_int( io_config, "magnetic_tape", &MAGNETIC_TAPE_TIME ) < 0 )
    {
        printf( "Parser :: Couldn't read magnetic tape time from configuration.\n" );
        exit( -1 );
    }
    else if( MAGNETIC_TAPE_TIME <= 0 ) 
    {
        printf( "Parser :: Invalid value of magnetic tape time: %d. Should be greater or equal to 1.\n", MAGNETIC_TAPE_TIME );
        exit( -1 );
    }

    #pragma endregion  

    #pragma region CREATING I/O QUEUES

    printer_queue         = malloc( PROCESS_QUANTITY * sizeof( struct PCB * ) );
    printer_queue_pointer = 0;
    if( pthread_mutex_init( &printer_lock, NULL ) != 0 )
    {   
        printf( "Parser :: Failed to initialize printer's lock.\n");
        exit( -1 );
    }

    magnetic_tape_queue   = malloc( PROCESS_QUANTITY * sizeof( struct PCB *) );
    magnetic_tape_pointer = 0;
    if( pthread_mutex_init( &magnetic_tape_lock, NULL ) != 0 )
    {   
        printf( "Parser :: Failed to initialize magnetic tape's lock.\n");
        exit( -1 );
    }

    disk_queue         = malloc( PROCESS_QUANTITY * sizeof( struct PCB * ) );
    disk_queue_pointer = 0;
    if( pthread_mutex_init( &disk_lock, NULL ) != 0 )
    {   
        printf( "Parser :: Failed to initialize disk's lock.\n");
        exit( -1 );
    }

    #pragma endregion

    #pragma region CREATING READY QUEUES

    queue_config = config_lookup( &config, "simulator.queue_config" );
    if( queue_config == NULL )
    {
        printf( "Parser :: Couldn't get queue configuration.\n" );
        exit( -1 );
    }

    if( config_setting_lookup_int( queue_config, "quantity", &READY_QUEUE_QNT ) < 0 )
    {
        printf( "Parser :: Couldn't read queues' quantity.\n" );
        exit( -1 );
    }
    else if( READY_QUEUE_QNT <= 0 ) 
    {
        printf( "Parser :: Invalid ready queues quantity value: %d. Should be greater or equal to 1.\n", READY_QUEUE_QNT );
        exit( -1 );
    }

    queues = config_setting_get_member( queue_config, "queues" );
    if( queues == NULL )
    {
        printf( "Parser :: Couldn't read queues member from config file.\n" );
        exit( -1 );
    }

    ready_queues        = malloc( READY_QUEUE_QNT * sizeof( struct PCB ** ) );
    quantum_time_lists  = malloc( READY_QUEUE_QNT * sizeof( int ) );
    ready_pointers      = malloc( READY_QUEUE_QNT * sizeof( int ) );

    for( int i = 0; i < READY_QUEUE_QNT; i++ )
    {
        // struct PCB **ready_queue = ready_queues[ i ][ 0 ];

        int *rq_quantum = quantum_time_lists + i;

        ready_queues[ i ] = malloc( PROCESS_QUANTITY * sizeof( struct PCB * ) );

        ready_pointers[ i ] = 0;

        queue_elem = config_setting_get_elem( queues, i );
        if( queue_elem == NULL )
        {
            printf( "Parser :: Couldn't retreive element no. %d from queues.\n", i + 1 );
            exit( -1 );
        }

        if( config_setting_lookup_int( queue_elem, "quantum_time", rq_quantum ) < 0 )
        {
            printf( "Parser :: Couldn't read quantum time of queue no. %d.\n", i + 1 );
            exit( -1 );
        }
        else if( *rq_quantum <= 0 )
        {
            printf( "Parser :: Invalid quantum time value: %d. Should be greater or equal to 1.\n", *rq_quantum );
            exit( -1 );
        }
    }

    if( pthread_mutex_init( &ready_queues_lock, NULL ) != 0)
    {
        printf( "Parser :: Couldn't initiate ready queues lock\n" );
        exit( -1 );
    }

    #pragma endregion

    #pragma region  CREATING TERMINATED QUEUE

    terminated_queue    = malloc( PROCESS_QUANTITY * sizeof( struct PCB * ) );
    terminated_pointer  = 0;

    if( pthread_mutex_init( &terminated_lock, NULL ) != 0 )
    {
        printf( "Parser :: Could not initialize terminated lock.\n" );
        exit( -1 );
    }

    #pragma endregion
    
    printf( "Parser :: Parsing has finished.\n" );
};

