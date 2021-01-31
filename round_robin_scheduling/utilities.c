#include <stdio.h>
#include <stdlib.h>

#include "config_parser.c"

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

void print_ready_queue_elements( struct PCB **ready_queue, int length )
{
    for( int i = 0; i < length; i++ )
    {
        struct PCB *pcb = ready_queue[ i ];

        printf( "{ PID: %d, status: %s }\n", pcb->PID, status_name( pcb->status ) );
    }

    printf( "==========================\n" );
}

int * get_snapshot( int *arr, int length )
{
    int *new_arr = malloc( length * sizeof( int ) );
    
    for( int i = 0; i < length; i++ )
    {
        new_arr[ i ] = arr[ i ];
    }

    return new_arr;
}

int any_change( int *curr, int *snapshot, int curr_queue, int *changed_queue )
{
    int true = 1, false = 0;

    for( int i = 0; i < curr_queue; i++ )
    {
        if ( curr_queue == i )
        {
            continue;
        }

        if( curr[ i ] > snapshot[ i ] )
        {
            *changed_queue = i;

            return true;
        }
    }

    return false;
}

struct PCB *get_next_element( struct PCB **ready_queue, int length )
{
    struct PCB *pcb = ready_queue[ 0 ];

    for( int i = 0; i < length - 1; i++ )
    {
        ready_queue[ i ] = ready_queue[ i + 1 ];
    }

    return pcb;
}

void show_records( struct RecordContainer *cpu_container )
{
    printf( "CPU Records: \n" );

    for( int i = 0; i < cpu_container->pointer; i++ )
    {
        struct ExecRecord *record = cpu_container->records + i;

        printf( "[ \"%d\", getDate( %d ), getDate( %d ) ]\n", record->PID, record->start_time, record->end_time );
    }

    printf( "============================\n" );
}

void print_configuration( struct PCB *processes_list, int PROCESS_QUANTITY, int PROCESSES_LIMIT, int READY_QUEUE_QNT, int DISK_TIME, int PRINTER_TIME, int MAGNETIC_TAPE_TIME ) 
{
    for( int i = 0; i < PROCESS_QUANTITY; i++ )
    {
        struct PCB *pcb = processes_list + i;

        print_process_control_block( pcb );
    }

    printf( "Setting :: Process Limit: %d," 
            "Ready Queue Quantity: %d seconds," 
            "Disk Time: %d seconds," 
            "Printer Time: %d seconds," 
            "Magnetic Tape Time: %d seconds \n\n", 
            PROCESSES_LIMIT, 
            READY_QUEUE_QNT,
            DISK_TIME, 
            PRINTER_TIME, 
            MAGNETIC_TAPE_TIME );
}

