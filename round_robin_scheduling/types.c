#include <pthread.h>

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
