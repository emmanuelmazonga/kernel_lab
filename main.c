
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// CONSTANTS
#define MAX_MEMORY      1024 // 1GB total ram
#define MAX_PROCESSES    20
#define MAX_RESOURCES     5
#define NAME_MAX_LEN     50
#define MAX_MEMORY_BLOCKS   50 
#define MESSAGE_SIZE      256
#define LOG_FILE "logs\\os.log" 


// STATES OF PROCESSES
typedef enum {
    NEW,
    READY,
    RUNNING,
    WAITING,
    TERMINATED
} ProcessState;

// PROCESS CONTROL BLOCK (PCB)
// This keeps track of the processes and their details
typedef struct {
    int     pid;
    int     priority;
    char    name[NAME_MAX_LEN];
    ProcessState    state;
    int     arrival_time;
    int     burst_time;
    int     remaining_time;
    int     memory_required;
    int     memory_start;
    int     waiting_time;
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

void log_event(const char* message);

const char* state_to_string(ProcessState state_value) {
    switch (state_value) {
        case NEW: return "NEW";
        case READY: return "READY";
        case RUNNING: return "RUNNING";
        case WAITING: return "WAITING";
        case TERMINATED: return "TERMINATED";
        default: return "STATE UNKNOWN";
    }
}

void flush_input(void) {
    int key_char;
    while ((key_char = getchar()) != '\n' && key_char != EOF);
}
// PROCESS MANAGEMENT
// CREATING A PROCESS
void create_process(void){
    if (process_count >= MAX_PROCESSES) {
        printf("\n[PROCESS] ERROR: Process Table FULL.\n");
        return;
    }
    PCB* p = &process_table[process_count];
    p->pid = next_pid++;

    // prompt the user for the name of the process
    printf("\nEnter name of process: ");
    flush_input();
    fgets(p->name, NAME_MAX_LEN, stdin);
    p->name[strcspn(p->name, "\n")] = '\0';

    printf("\nEnter arrival_time: ");
    scanf("%d" , &p->arrival_time);

    // prompting for burst time
    printf("\nEnter burst time: ");
    scanf("%d" , &p->burst_time);

    printf("\nEnter the Priority value. (1. Highest, 2. Medium, 3. Low): ");
    scanf("%d" , &p->priority);

    printf("\nEnter the memory required: ");
    scanf("%d" , &p->memory_required);

    p->state    = READY;
    p->waiting_time    = 0;
    p->turnaround_time  = 0;
    p->memory_start = -1; 

    // increment process count
    process_count++;

    // add log message to log file 
    char log_message[MESSAGE_SIZE];
    snprintf(log_message, sizeof(log_message),
    // snprintf → formats text into a buffer (msg) safely,
    // ensuring it doesn't overflow.
        "Process Created: PID = %d | %s | priority = %d | Busrt_t = %d | Mem = %d |",
        p->pid,
        p->name,
        p->priority,
        p->burst_time,
        p->memory_required
    );

    //logging the message
    log_event(log_message);
    printf("\n[SUCCESSFUL] '%s' created — PID '%d' | State: READY\n", p->name, p->pid);
}


void display_process_table(void) {
    if (process_count == 0) {
        printf("\n[PROCESS] No processes in the system.\n");
        return;
    }

    printf("\n+---+------+------------------+-----+----------+---------+---------+\n");
    printf("|  #| PID  | Name             | Pri | State    |   Burst | Mem(MB) |\n");
    printf("+---+------+------------------+-----+----------+---------+---------+\n");

    for (int i = 0; i < process_count; i++) {
        PCB* p = &process_table[i];  
        printf("| %d | %4d | %-16s | %3d | %-8s | %2d | %7d |\n",
            i + 1,
            p->pid,
            p->name,
            p->priority,
            state_to_string(p->state),
            p->burst_time,
            p->memory_required);           
    }
    printf("+---+------+------------------+-----+----------+---------+---------+\n");
}

void log_event(const char* message) {
    FILE* log_file = fopen(LOG_FILE, "a");
    if (!log_file) return;

    time_t current_real_time = time(NULL);
    struct tm* time_info = localtime(&current_real_time);
    char time_string[60];
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", time_info);

    fprintf(log_file, "[%s] %s\n", time_string, message);
    fclose(log_file);
}
 // LOGGING FUNCTIONS
void view_logs(void) {
    FILE* log_file = fopen(LOG_FILE, "r");
    if (!log_file) {
        printf("[FILE ERROR] No log filr found.\n");
        return;
    }

    printf("\n==================================================\n");
    printf("|                   MINI-OS  EVENT LOG             |\n");
    printf("\n==================================================\n");

    char message_line[512];
    int line_count = 0;
    while (fgets(message_line, sizeof(message_line), log_file)){
        printf(" %s", message_line);
        line_count++;
    }
    if (line_count == 0 ) {
        printf("[FILE] Log file Empty.");
    }
    else {
        printf("\n  Total events: %d\n", line_count);
        fclose(log_file);
    }
}

void clear_logs(void) {
    FILE* log_file = fopen(LOG_FILE, "w");
    if (!log_file) { printf("[FILE] Could not clear logs.\n"); return; }
    fclose(log_file);
    printf("[FILE] Log cleared.\n");
}




//MAIN 
int main(){


    return 0;
}



// INITIALIZING THE MEMORY
void init_memory(void){
    memory[0].start     = 0;
    memory[0].size      = MAX_MEMORY;
    memory[0].is_free   = 1;
    memory[0].pid       = -1;
    count_block         = 1;
}
 

static void split_block(int index, int memory_required, int pid) {
    int leftover_Memory = memory[index].size - memory_required;

    memory[index].size    = memory_required;
    memory[index].is_free = 0;
    memory[index].pid     = pid;

    
}