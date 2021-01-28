#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <libconfig.h>
#include <string.h>

#pragma region TYPES & ENUMS

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

struct ExecRecord
{
    int   PID;
    int   start_time;
    int   end_time;
};

struct RecordContainer 
{
    struct ExecRecord   *records;
    pthread_mutex_t     lock;
    int                 pointer;
    int                 size;
};

#pragma endregion

#pragma region FUNCTIONS 

char * status_name( enum Status status_code );
char * instruction_type( enum InstructionType instruction_type );

void print_instructions( struct Instruction *instructions, int qnt );
void print_process_control_block( struct PCB *pcb );
void print_configuration( );
void print_ready_queue_elements( struct PCB **ready_queue, int length );
void show_records( );

void init_simluator( char *config_file_path );

void * process_creator_thread( void *arg );
void * cpu_scheduler_thread( void *arg );
void * io_thread ( void *arg );

struct PCB * get_next_element( struct PCB **ready_queue, int length );
int * get_snapshot( int * arr, int length );
int any_change( int *curr, int *snapshot, int boundary, int *changed_queue );

#pragma endregion

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

char * status_name( enum Status status_code )
{
    switch( status_code )
    {
        case NEW:
        {
            return "NEW";
        }
        case READY:
        {
            return "READY";
        }
        case RUNNING:
        {
            return "RUNNING";
        }
        case WAITING:
        {
            return "WAITING";
        }
        case TERMINATED:
        {
            return "TERMINATED";
        }
        default:
        {
            printf( "Invalid status_code: %d.\n", status_code );
            exit( -1 );
        }
    }
}

char * instruction_type( enum InstructionType instruction_type )
{
    switch( instruction_type )
    {
        case CPU:
        {
            return "CPU";
        }
        case PRINTER:
        {
            return "PRINTER";
        }
        case DISK:
        {
            return "DISK";
        }
        case MAGNETIC_TAPE:
        {
            return "MAGNETIC_TAPE";
        }
        default:
        {
            printf( "Invalid status_code: %d.\n", instruction_type );
            exit( -1 );
        }
    }
}

void print_instructions( struct Instruction *instructions, int qnt )
{
    printf( "\tinstructions: [ " );

    for( int i = 0; i < qnt; i++ )
    {
        struct Instruction *inst = instructions + i;
        printf( "( %s, %d ), ", instruction_type( inst->type ), inst->time );
    }

    printf( " ]\n" );
}

void print_process_control_block( struct PCB *pcb )
{
    printf( "{\n" );
    printf( "\tAddresses: %p\n", pcb );
    printf( "\tPID: %d\n", pcb->PID );
    printf( "\tPC: %d\n",  pcb->PC );
    printf( "\tstatus: %s\n",  status_name( pcb->status ) );
    printf( "\tinstruction_qnt: %d\n",  pcb->instruction_qnt );

    print_instructions( pcb->instructions, pcb->instruction_qnt );

    printf( "\tstart_after: %d\n",  pcb->start_after );
    printf( "}\n" );
}

void init_simluator( char *config_file_path )
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

    if( config_setting_lookup_int( process_config, "quantity", &PROCESS_QUANTITY ) < 0)
    {   
        printf( "Could not read process limit.\n" );
        exit( -1 );
    }
    else if( PROCESSES_LIMIT <= 0 )
    {
        printf( "Invalid process quantity: %d. It should be a positive integer value.\n", PROCESS_QUANTITY );
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
    
    for( int i = 0; i < PROCESS_QUANTITY; i++ )
    {
        struct PCB* pcb = processes_list + i;

        pcb->PID        =   i + 1;
        pcb->PC         =   0;
        pcb->status     =   NEW;

        process = config_setting_get_elem( list_config, i );
        if( process == NULL )
        {
            printf( "Could not loaded process no. %d.\n", i + 1 );
            exit( -1 );
        }

        if( config_setting_lookup_int( process, "qnt", &( pcb->instruction_qnt ) ) < 0 )
        {
            printf( "Could not read quantity of instruction of %d process.\n", i + 1 );
            exit( -1 );
        }
        else if( pcb->instruction_qnt <= 0 )
        {
            printf( "Invalid quantity of instructions: %d. Should be greater or equal to 1.", pcb->instruction_qnt );
            exit( -1 );
        }

        if( config_setting_lookup_int( process, "start_after", &( pcb->start_after ) ) < 0 )
        {
            printf( "Could not read start_after of %d process.\n", i + 1 );
            exit( -1 );
        }
        else if( pcb->start_after < 0 )
        {
            printf( "Invalid start_after value: %d. Should be greater or equal to 0.", pcb->start_after );
            exit( -1 );
        }

        pcb->instructions = malloc( pcb->instruction_qnt * sizeof( struct Instruction ) );

        process_instructions = config_setting_get_member( process, "instructions" );
        if( process_instructions == NULL )
        {
            printf( "Could not read instructions member of %d process.\n", i + 1 );
            exit( -1 );
        }
        
        for( int j = 0; j < pcb->instruction_qnt; j++ )
        {
            const char *type;
 
            struct Instruction *inst = pcb->instructions + j;

            config_setting_t *instruction = config_setting_get_elem( process_instructions, j );
            if( instruction == NULL )
            {
                printf( "Couldn't find instruction n.%d of process %d.\n", j + 1, i + 1 );
                exit( -1 );
            }

            if( config_setting_lookup_string( instruction, "type", &type ) < 0)
            {
                printf( "Couldn't read field type from process %d.\n", i+1 );
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
                    printf( "Couldn't read field time from process %d.\n", i+1 );
                    exit( -1 ); 
                }
                else if( inst->time <= 0 )
                {
                    printf( "Invalid time value: %d. Should be greater or equal to 1.", inst->time );
                    exit( -1 );
                }

                if( greatest_cpu_time == 0 || inst->time > greatest_cpu_time )
                {
                    greatest_cpu_time = inst->time;
                }
            }
            else
            {
                printf( "Invalid instruction type at instruction no.%d at process %d.\n", j+1, i+1 ); 
                exit( -1 );
            }
        }
    }
    
    printf( "THERE WILL BE %d CPU executions.\n", CPU_execs );

    cpu_container.records = malloc( CPU_execs * greatest_cpu_time * sizeof( struct ExecRecord ) );
    cpu_container.pointer = 0;
    cpu_container.size    = 0;
    if( pthread_mutex_init( &cpu_container.lock, NULL ) != 0 )
    {   
        printf( "Couldn't init cpu_container's lock.\n" );
        exit( -1 );
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

    printer_queue         = malloc( PROCESS_QUANTITY * sizeof( struct PCB * ) );
    printer_queue_pointer = 0;
    if( pthread_mutex_init( &printer_lock, NULL ) != 0 )
    {   
        printf( "Failed to initialize printer's lock.\n");
        exit( -1 );
    }

    magnetic_tape_queue   = malloc( PROCESS_QUANTITY * sizeof( struct PCB *) );
    magnetic_tape_pointer = 0;
    if( pthread_mutex_init( &magnetic_tape_lock, NULL ) != 0 )
    {   
        printf( "Failed to initialize magnetic tape's lock.\n");
        exit( -1 );
    }

    disk_queue         = malloc( PROCESS_QUANTITY * sizeof( struct PCB * ) );
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
        // struct PCB **ready_queue = ready_queues[ i ][ 0 ];

        int *rq_quantum = quantum_time_lists + i;

        ready_queues[ i ] = malloc( PROCESS_QUANTITY * sizeof( struct PCB * ) );

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

    terminated_queue    = malloc( PROCESS_QUANTITY * sizeof( struct PCB * ) );
    terminated_pointer  = 0;

    if( pthread_mutex_init( &terminated_lock, NULL ) != 0 )
    {
        printf( "Could not initialize terminated lock.\n" );
        exit( -1 );
    }

    #pragma endregion
    
    printf( "Configuration has ended.\n" );
}

void print_configuration( ) 
{
    for( int i = 0; i < PROCESS_QUANTITY; i++ )
    {
        struct PCB *pcb = processes_list + i;
        print_process_control_block( pcb );
    }

    printf( "CONFIGURATION :: PROCESSES_LIMIT: %d, READY_QUEUE_QNT: %d, DISK_TIME: %d, PRINTER_TIME: %d, MAGNETIC_TAPE_TIME: %d \n\n", 
           PROCESSES_LIMIT, 
           READY_QUEUE_QNT,
           DISK_TIME, 
           PRINTER_TIME, 
           MAGNETIC_TAPE_TIME );
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

            struct PCB *pcb = processes_list + i;

            int pid = pcb->PID;

            if( pcb->status != NEW )
            {
                continue;
            }

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

                printf( "Process Creator :: Created a new process { PID: %d, Address: %p } at %d secs.\n", pid, pcb, past_time );

                print_ready_queue_elements( ready_queues[ 0 ],  ready_pointers[ 0 ]  );

                pthread_mutex_unlock( &ready_queues_lock );

            }

            #pragma endregion 
        }

        int past_time = time( NULL ) - start_time;

        // sleep until the next process
        int sleep_time = shortest_start_after - past_time;
        if ( sleep_time > 0 )
        {
            printf( "Process Creator :: Waiting for %d secs until next process can be created.\n", sleep_time );
            sleep( sleep_time );
        }
    }

    printf( "Process Creator :: Exiting.\n" );  
}

struct PCB * get_next_element( struct PCB **ready_queue, int length )
{
    struct PCB *pcb = ready_queue[ 0 ];

    for( int i = 0; i < length - 1; i++ )
    {
        ready_queue[ i ] = ready_queue[ i + 1 ];
    }

    return pcb;
}

void print_ready_queue_elements( struct PCB **ready_queue, int length )
{
    for( int i = 0; i < length; i++ )
    {
        struct PCB *pcb = ready_queue[ i ];

        printf( "{ PID: %d, status: %s }\n", pcb->PID, status_name( pcb->status ) );
    }

    printf( "==========================\n" );
}

int * get_snapshot( int * arr, int length )
{
    int *new_arr = malloc( length * sizeof( int ) );
    
    pthread_mutex_lock( &ready_queues_lock );

    for( int i = 0; i < length; i++ )
    {
        new_arr[ i ] = arr[ i ];
    }

    pthread_mutex_unlock( &ready_queues_lock );

    return new_arr;
}

int any_change( int *curr, int *snapshot, int boundary, int *changed_queue )
{
    int true = 1, false = 0;

    pthread_mutex_lock( &ready_queues_lock );

    for( int i = 0; i < boundary; i++ )
    {
        if( curr[ i ] > snapshot[ i ] )
        {
            *changed_queue = i;

            return true;
        }
    }

    pthread_mutex_unlock( &ready_queues_lock );


    return false;
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

        int *snapshot = get_snapshot( ready_pointers , READY_QUEUE_QNT );

        #pragma region  CHECK HIGHER PRIORITY QUEUES

        if( i > 0 )
        {
            int changed_queue;

            if( any_change( ready_pointers, snapshot, i, &changed_queue) )
            {
                printf( "CPU Scheduler :: changing queue from %d to %d.\n", i, changed_queue );
                
                i = changed_queue - 1;

                continue;
            }
        }
        
        #pragma endregion

        pthread_mutex_lock( &ready_queues_lock );

        if( ready_pointers[ i ] <= 0 )
        {
            pthread_mutex_unlock( &ready_queues_lock );

            i = ( i + 1 ) % READY_QUEUE_QNT;

            continue;
        }

        printf( "CPU Scheduler :: processing ready queue no. %d with %d elements.\n", i, ready_pointers[ i ] );
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

                int running_time = quantum_time_lists[ i ];

                if( inst->time <= quantum_time_lists[ i ] )
                {
                    running_time = inst->time;
                }

                #pragma region Saving Execution Record 
                
                pthread_mutex_lock( &cpu_container.lock );

                // if( cpu_container.pointer >= cpu_container.size )
                // {
                //     increase_size( &cpu_container );
                // }
                
                struct ExecRecord *record = cpu_container.records + cpu_container.pointer;

                cpu_container.pointer += 1;

                record->PID        = pcb->PID;    
                record->start_time = time( NULL ) - start_time;
                record->end_time   = record->start_time + running_time;
                
                pthread_mutex_unlock( &cpu_container.lock );

                #pragma endregion

                printf( "CPU Scheduler :: process { PID: %d } will run for %d secs.\n", pcb->PID, running_time );

                sleep( inst->time );

                printf( "CPU Scheduler :: process { PID: %d } ran for %d secs.\n", pcb->PID, running_time );
                
                inst->time -= running_time;

                if( inst->time <= 0 )
                {
                    pcb->PC++;

                    #pragma region  CHECKING IF A PROCESS HAS FINISHED
        
                    if( pcb->PC >= pcb->instruction_qnt )
                    {
                        pcb->status == TERMINATED;

                        pthread_mutex_lock( &terminated_lock );

                        terminated_queue[ terminated_pointer++ ] = pcb;

                        pthread_mutex_unlock( &terminated_lock );

                        printf( "CPU Scheduler :: Process { PID: %d, PC: %d, inst: %d } has terminated.\n", pcb->PID, pcb->PC, pcb->instruction_qnt );
                        fflush( stdout );

                        break;
                    }

                    #pragma endregion
                }

                pcb->status = READY;

                #pragma region  MOVE TO THE NEXT READY QUEUE

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

void * io_thread ( void *arg )
{
    time_t start_time = time( NULL );

    char *io_type = (char *) arg;

    pthread_mutex_t     *io_lock; 
    struct PCB          **io_queue;
    int                 *io_queue_pointer;
    int                 blocked_time;

    #pragma region SETTING IO TYPE

    if( strcmp( io_type, "Disk" ) == 0 )
    {
        io_queue         =  disk_queue;
        io_lock          =  &disk_lock;
        io_queue_pointer =  &disk_queue_pointer;
        blocked_time     =  DISK_TIME;
    }
    else if( strcmp( io_type, "Printer" ) == 0 )
    {
        io_queue         =  printer_queue;
        io_lock          =  &printer_lock;
        io_queue_pointer =  &printer_queue_pointer;
        blocked_time     =  PRINTER_TIME;
    }
    else if( strcmp( io_type, "Magnetic Tape" ) == 0 )
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

        printf( "%s :: blocking for process { PID: %d } for %d secs.\n", io_type, pcb->PID, blocked_time );
        fflush( stdout );

        sleep( blocked_time );

        pcb->PC++;

        if( pcb->PC >= pcb->instruction_qnt ) 
        {
            pcb->status = TERMINATED;

            printf( "%s :: process { PID: %d } has blocked for %d secs and has terminated.\n", io_type, pcb->PID, blocked_time );
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

            printf( "%s :: process { PID: %d } blocked for %d secs and is now ready at queue no. %d.\n", io_type, pcb->PID, blocked_time, ready_queue_index );
            fflush( stdout );

            pthread_mutex_lock( &ready_queues_lock );

            ready_queues[ ready_queue_index ][ ready_pointers[ ready_queue_index ]++ ] = pcb;

            pthread_mutex_unlock( &ready_queues_lock );
        }
    }

    printf( "%s :: No more processes to execute.\n", io_type );
    fflush( stdout );
}

void show_records( )
{
    printf( "CPU Records: \n" );

    for( int i = 0; i < cpu_container.pointer; i++ )
    {
        struct ExecRecord *record = cpu_container.records + i;

        printf( "[ \"%d\", getDate( %d ), getDate( %d ) ]\n", record->PID, record->start_time, record->end_time );
    }

    printf( "============================\n" );
}

int main( int argc, char *argv[] )
{
   char *file_name = "./configfile_2.txt";

   init_simluator( file_name );

   print_configuration( );

   int start_time = time( NULL );

   char *printer         =     "Printer", 
        *disk            =     "Disk", 
        *magnetic_tape   =     "Magnetic Tape";

   pthread_create( &process_creator, NULL, &process_creator_thread, ( void * ) &start_time );

   pthread_create( &cpu_scheduler, NULL, &cpu_scheduler_thread, ( void * ) &start_time );

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
    
   show_records( );

   printf( "Simulator is over.\n" );

   return 0;
}