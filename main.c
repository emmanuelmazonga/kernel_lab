#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

// CONSTANTS USED IN THE PROGRAM
#define MAX_MEMORY      1024 // 1MB total ram
#define MAX_PROCESSES    20
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
} Process_state;

// PROCESS CONTROL BLOCK (PCB)
// This keeps track of the processes and their details
typedef struct {
    int pid;   // process ID
    char name[NAME_MAX_LEN];
    int arrival_time;
    int burst_time;
    int priority;
    int remaining_time;
    int memory_required;
    Process_state state;
    int memory_size;  // This stores how much memory (in KB) the process when requested 
    int memory_start; // This records the starting address of the memory block
} PCB;

// MEMORY BLOCK
typedef struct {
    int  start; // starting address of the memory block
    int  size; // size of the memory block
    int  is_free;
    int  pid;       // -1 if free
} MemoryBlock; 
                       // GLOBAL STATE
// PROCESS TABLE
PCB process_table[MAX_PROCESSES];
int process_count = 0;
int next_pid = 1; // increasing counter — every new process gets the current value then increments it
                    //Generates unique process IDs (PIDs).

// MEMORY BLOCK ++
MemoryBlock mem_blocks[MAX_MEMORY_BLOCKS];
int count_block = 0;

// DIsplay Helpers
const char* state_to_string(Process_state state_value) {
    switch (state_value) {
        case NEW:
            return "NEW";
        case READY:
            return "READY";
        case RUNNING:
            return "RUNNING";
        case WAITING: 
            return "WAITING";
        case TERMINATED: 
            return "TERMINATED";
        default: 
            return "STATE UNKNOWN";
    }
}

                          // FILE MANAGEMENT
// LOGGING
void add_log(const char *message, ...) {
    char message[500];        // buffer to hold the final message
    va_list args;             // special type for handling "..."
    va_start(args, message);   // initialize args to start reading after 'format'
    vsnprintf(message, sizeof(message), message, args); 
    va_end(args);             // clean up

    // Time handling
    time_t current_real_time = time(NULL);
    struct tm* time_info = localtime(&current_real_time);
    char time_string[60];
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", time_info);
    // Print to console
    printf("[%s] %s\n", time_string, message);
    // Append to log file
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file) {
        fprintf(log_file, "[%s] %s\n", time_string, message);
        fclose(log_file);
    } else {
        printf("[FILE ERROR] Could not write to log file.\n");
    }
    
}


                       // MEMORY MANAGEMENT
void init_memory(void) {
    mem_blocks[0].start = 0;          // block will begin at address 0 KB
    mem_blocks[0].size  = MAX_MEMORY; // to start with, the block covers the entire memory (1024 KB)
    mem_blocks[0].pid   = -1;         // -1 means "free" (no process owns it, yet)
    count_block = 1;               // only one block exists right now, the 1024KB block
}

// Strategy to be used: First-Fit allocation
/* First-Fit: scan for the first block large enough; split if necessary.
Returns the start address of the allocated region, or -1 on failure. */

int first_fit_allocate(int size, int pid) {
    for (int i = 0; i < count_block; i++) {
        // checking if the block is free && if the avalaible space is enough for the the requested space
        if (mem_blocks[i].pid == -1 && mem_blocks[i].size >= size) {
            // Save its starting address (alloc_start)
            int alloc_start = mem_blocks[i].start;
            if (mem_blocks[i].size > size ){
                //split and shift everything right to make room for the remainder
                for (int j = count_block; j > i; j--) {
                    mem_blocks[j] = mem_blocks[j - 1];
                }
                mem_blocks[i + 1].start = alloc_start + size;
                mem_blocks[i + 1].size  = mem_blocks[i].size - size;
                mem_blocks[i + 1].pid   = -1;
                count_block++;
            }
            mem_blocks[i].size = size;
            mem_blocks[i].pid  = pid;
            return alloc_start;
        }
    }
    return -1; // allocation failed
}

// Memory Deallocation
void free_memory(int pid) {
    //Loop through mem_blocks[] until block owned by pid is located
    int index = -1;
    for (int i = 0; i < count_block; i++) {
        if (mem_blocks[i].pid == pid){
            index = i;
            break;
        }
    }
    if ( index == -1){  // PID not found
        return; 
    }
    mem_blocks[index].pid = -1; // mark block as free

    // Merge with NEXT block if free
    if (index + 1 < count_block && mem_blocks[index + 1].pid == -1) {
        mem_blocks[index].size += mem_blocks[index + 1].size;
        for (int j = index + 1; j < count_block - 1; j++){
            mem_blocks[j] = mem_blocks[j + 1];
        }
        count_block--;
    }

    // Merge with PREVIOUS block if free
    if (index > 0 && mem_blocks[index - 1].pid == -1) { // mem_blocks[index - 1].pid == -1 → checks if that previous block is free
        mem_blocks[index - 1].size += mem_blocks[index].size;
        for (int j = index; j < count_block - 1; j++)
            mem_blocks[j] = mem_blocks[j + 1];
        count_block--; //Decrement count_block since two blocks became one.
    }
}

// DISPLAY HELPERS ++
void flush_input(void) {
    int key_char; // discard characters until newline or end-of-file
    while ((key_char = getchar()) != '\n' && key_char != EOF);
}

// PROCESS MANAGEMENT
// Create Process with logging
void create_process(void) {
    if (process_count >= MAX_PROCESSES) {
        add_log("[PROCESS ERROR] Maximum number of processes reached (%d).\n", MAX_PROCESSES);
        return;
    }     
    PCB* p = &process_table[process_count];
    int  arrival, burst, priority, memory;
    
    printf("Process Name: ");
    flush_input();
    fgets(p->name, NAME_MAX_LEN, stdin);
    p->name[strcspn(p->name, "\n")] = '\0';   // remove newline

    if (strlen(p->name) == 0) {
        add_log("ERROR: Process name cannot be empty");
        return;
    }

    
    printf("Arrival Time: ");                                 
    scanf("%d", &arrival);
    printf("Burst Time: ");                                   
    scanf("%d", &burst);
    printf("Priority (1=Highest, 2=Emergency, 3=Lowest): "); 
    scanf("%d", &priority);
    printf("Memory (KB): ");                                  
    scanf("%d", &memory);

    p->pid        = next_pid++;
    p->arrival_time   = arrival;
    p->burst_time     = burst;
    p->priority       = priority;
    p->remaining_time = burst;
    p->state          = NEW;
    p->memory_size    = memory;

    int start_addr = first_fit_allocate(memory, p->pid);
    if (start_addr != -1){ // if it's not free
        p->memory_start = start_addr;
        p->state = READY;
        add_log("\n[SUCCESSFUL] '%s' created — PID '%d' |— Memory at %d KB | State: READY\n",
            p->name, p->pid, start_addr);
    } else {
        p->memory_start = -1;
        add_log("WARNING: PID %d (%s) — Insufficient memory", p->pid, p->name);
    }
    process_count++;
    //display processt table and memory map

} 

void suspend_process(void) {
    int pid;
    printf("Enter PID to suspend: ");
    if (scanf("%d", &pid) != 1) { // If the user types a valid integer, scanf returns 1
        printf("Invalid Input.\n");
        return;
    }
    
    for (int i = 0; i < process_count; i++) {
         if (process_table[i].pid != pid)
            continue;
        if (process_table[i].state == TERMINATED) {
            add_log("[ERROR] PID %d is already TERMINATED - cannot suspend", pid);
            return;
        }

        if (process_table[i].state == WAITING) {
            add_log("[INFO] PID %d (%S) is already WAITING", pid, process_table[i].name);
            return;
        }
        process_table[i].state == WAITING;
        add_log("SUSPENDED: PID %d (%s) -> WAITING", pid, process_table[i].name);
        // display_process_table
        return;
    }
    add_log("[ERROR] PID %d not found", pid);
}

                             // FILE MANAGEMENT
// reads and display the log file   
void view_logs(void) {
    FILE* log_file = fopen(LOG_FILE, 'r');  // open log file in read mode'r'
    if (!log_file) {
        printf("[FILE ERROR] No Log file Found");
        return;
    }
    printf("\n==================================================\n");
    printf("|            MINI-OS  EVENT LOG                  |\n");
    printf("==================================================\n");
    char message_line[MESSAGE_SIZE];
    int line_count = 0; // tracks how many lines are read
    // Read each line from the file into message_line
    while (fgets(message_line, sizeof(message_line), log_file)) {
        printf("%S", message_line);
        line_count++;
    }
    // if empty or show total events
    if (line_count == 0){
        printf("[FILE] Log file empty.\n");
    }
    else {
        printf("\n Total events: %d\n", line_count);
    }
    // close log file
    fclose(log_file);
}
// CLEAR LOG FILE
void clear_logs(void) {
    FILE* log_file = fopen(LOG_FILE, "w"); // open log file in write mode'w'
    if (!log_file) {
        printf("[FILE] Could not clear logs.\n");
        return;
    }
    fclose(log_file);
    printf("[FILE] Log cleared.\n");
}

                                // MENU
void display_menu(void) {
    printf("\n======================================================\n");
    printf("              Emergency Response System G84              ");
    printf("======================================================\n");
    printf("  [Process Management]\n");
    printf("  1. Create Emergency Task\n");
    printf("  2. Show Process Table\n");
    printf("  6. Suspend Task\n");
    printf("  7. Terminate Task\n");
    printf("\n  [CPU Scheduling]\n");
    printf("  4. Run FCFS Scheduling\n");
    printf("  5. Run Priority Scheduling\n");
    printf("\n  [Memory Management]\n");
    printf("  3. Show Memory Map\n");
    printf("\n  [File Management]\n");
    printf("  8. View Logs\n");
    printf("  9. Clear Logs\n");
    printf("\n 10. Exit\n");
    printf("--------------------------------------------------------\n");
    printf("Option: ");
}


int main() {

    init_memory();
    add_log("===== Emergency Response System Started G84 ====");
    add_log("TOtal Available Memory: % KB", MAX_MEMORY);

    int option;
    do {
        display_menu();
        if (scanf("%d", &option != 1))
            break;
        switch (option) {
            case 1:  create_task();         
                break;
            case 2:  print_process_table(); 
                break;
            case 3:  print_memory_map();    
                break;
            case 4:  run_fcfs();            
                break;
            case 5:  run_priority();        
                break;
            case 6:  suspend_task();        
                break;
            case 7:  terminate_task();      
                break;
            case 8:  view_logs();           
                break;
            case 9:  clear_logs();          
                break;
            case 10: add_log("Exiting SERC. Goodbye."); 
                break;
            default: printf("Invalid choice. Try again.\n");
        }
    } while(option != 10);
    return 0;
}
