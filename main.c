
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>.

// CONSTANTS
#define MAX_MEMORY      1024 // 1GB total ram
#define MAX_PROCESSES    20
#define MAX_RESOURCES     5
#define NAME_MAX_LEN     50
#define MAX_MEMORY_BLOCKS   50  


// STATES OF PROCESSES
typedef enum {
    NEW,
    READY,
    RUNNING,
    WAITING,
    TERMINATED
} ProcessState;

// PROCESS TYPES
typedef enum {
    AMBULANCE,
    FIRE,
    POLICE,
} ProcessTypes;

// PROCESS CONTROL BLOCK (PCB)
// This keeps track of the processes and their details
typedef struct {
    int     pid;
    int     priority;
    char    name[NAME_MAX_LEN];
    ProcessTypes    type;
    ProcessState    state;
    int     arrival_time;
    int     burst_time;
    int     remaining_time;
    int     memory_required;
    int     memory_start;
    int     wainting_time;
    int     turnaround_time;
    int     allocated[MAX_RESOURCES];
    int     max_needed[MAX_RESOURCES];
} PCB;

// MEMORY BLOCK
typedef struct {
    int  start;
    int  size;
    int  is_free;
    int  pid;       // -1 if free
} MemoryBlock; 

// PROCESS TABLE
PCB process_table[MAX_PROCESSES];
int process_count = 0;
int next_pid = 1; // increasing counter — every new process gets the current value then increments it
                    //Generates unique process IDs (PIDs).
// MEMORY BLOCK ++
MemoryBlock memory[MAX_MEMORY_BLOCKS];
int count_block = 0;

void flush_input(void) {
    int key_char;
    while ((key_char = getchar()) != '\n' && c != EOF);
}
// PROCESS MANAGEMENT
// CREATING A PROCESS
void create_process(void){
    PCB* p = &process_table[process_count];
    p->pid = next_pid++;

    // prompt the user for the name of the process
    prinf("\nEnter name of process: ");
    flush_input();
    fgets(p->name, NAME_MAX_LEN, stdin);
    p->name[strcspn(p->name, "\n")] = '\0';





}





//MAIN 
int main(){


    return 0;
}