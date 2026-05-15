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

typedef void (*LogCallback)(const char *message);
LogCallback log_cb = NULL;

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
void add_log(const char *_get_output_format, ...) {
    char message[500];        // buffer to hold the final message
    va_list args;             // special type for handling "..."
    va_start(args, _get_output_format);   // initialize args to start reading after 'format'
    vsnprintf(message, sizeof(message), _get_output_format, args); 
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
    // GUI code
    if (log_cb) log_cb(message);  
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


                    // GUI CODE

/* GUI version of create_process — called by the GTK dialog callback.
   Takes all values as parameters instead of reading from stdin.          */
void create_process_auto(const char *name, int arrival, int burst,
                         int priority, int memory) {
    if (process_count >= MAX_PROCESSES) {
        add_log("[PROCESS ERROR] Maximum processes reached (%d).", MAX_PROCESSES);
        return;
    }
    if (strlen(name) == 0) {
        add_log("[ERROR] Process name cannot be empty.");
        return;
    }

    PCB *p        = &process_table[process_count];
    p->pid        = next_pid++;
    strncpy(p->name, name, NAME_MAX_LEN - 1);
    p->name[NAME_MAX_LEN - 1] = '\0';
    p->arrival_time   = arrival;
    p->burst_time     = burst;
    p->priority       = priority;
    p->remaining_time = burst;
    p->state          = NEW;
    p->memory_size    = memory;

    int start_addr = first_fit_allocate(memory, p->pid);
    if (start_addr != -1) {
        p->memory_start = start_addr;
        p->state = READY;
        add_log("[CREATED] '%s' | PID %d | Memory at %d KB | State: READY",
                p->name, p->pid, start_addr);
    } else {
        p->memory_start = -1;
        add_log("[WARNING] PID %d (%s) | Insufficient memory", p->pid, p->name);
    }
    process_count++;
}

/* GUI versions of suspend/terminate — accept pid as parameter */
void suspend_process_by_pid(int pid) {
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].pid != pid) continue;
        if (process_table[i].state == TERMINATED) {
            add_log("[ERROR] PID %d is TERMINATED — cannot suspend", pid); return;
        }
        if (process_table[i].state == WAITING) {
            add_log("[INFO] PID %d (%s) already WAITING", pid, process_table[i].name); return;
        }
        process_table[i].state = WAITING;
        add_log("SUSPENDED: PID %d (%s) -> WAITING", pid, process_table[i].name);
        return;
    }
    add_log("[ERROR] PID %d not found", pid);
}

void free_memory(int pid);   /* forward declaration */
void terminate_process_by_pid(int pid) {
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].pid != pid) continue;
        if (process_table[i].state == TERMINATED) {
            add_log("[INFO] PID %d (%s) already TERMINATED", pid, process_table[i].name); return;
        }
        process_table[i].state = TERMINATED;
        if (process_table[i].memory_start != -1) {
            free_memory(pid);
            process_table[i].memory_start = -1;
            add_log("[INFO] Released %d KB from PID %d (%s)",
                    process_table[i].memory_size, pid, process_table[i].name);
        }
        add_log("TERMINATED: PID %d (%s)", pid, process_table[i].name);
        return;
    }
    add_log("[ERROR] PID %d not found", pid);
}

void resume_process_by_pid(int pid) {
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].pid != pid) continue;
        if (process_table[i].state == TERMINATED) {
            add_log("[ERROR] PID %d is TERMINATED — cannot resume", pid); return;
        }
        if (process_table[i].state != WAITING) {
            add_log("[INFO] PID %d (%s) is not suspended (current state: %s)",
                    pid, process_table[i].name,
                    state_to_string(process_table[i].state)); return;
        }
        process_table[i].state = READY;
        add_log("RESUMED: PID %d (%s) -> READY", pid, process_table[i].name);
        return;
    }
    add_log("[ERROR] PID %d not found", pid);
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
    if (index > 0 && mem_blocks[index - 1].pid == -1) { // mem_blocks[index - 1].pid == -1 -> checks if that previous block is free
        mem_blocks[index - 1].size += mem_blocks[index].size;
        for (int j = index; j < count_block - 1; j++)
            mem_blocks[j] = mem_blocks[j + 1];
        count_block--; //Decrement count_block since two blocks became one.
    }
}

// DISPLAY HELPERS ++

void display_process_table(void) {
    printf("\n%-5s %-20s %-8s %-6s %-9s %-12s %-8s %-6s\n",
           "PID", "Name", "Arrival", "Burst", "Priority", "State", "Mem(KB)", "Start");
    printf("%-5s %-20s %-8s %-6s %-9s %-12s %-8s %-6s\n",
           "---", "----", "-------", "-----", "--------", "-----", "-------", "-----");
    for (int i = 0; i < process_count; i++) {
        PCB *p = &process_table[i];
        printf("%-5d %-20s %-8d %-6d %-9d %-12s %-8d %-6d\n",
               p->pid, p->name, p->arrival_time, p->burst_time,
               p->priority, state_to_string(p->state),
               p->memory_size,
               p->memory_start == -1 ? 0 : p->memory_start);
    }
    printf("\n");
}

void display_memory_map(void) {
    int used = 0;
    printf("\n%-12s %-10s %-12s %-5s\n", "Start (KB)", "Size (KB)", "Status", "PID");
    printf("%-12s %-10s %-12s %-5s\n",   "----------", "---------", "------", "---");
    for (int i = 0; i < count_block; i++) {
        printf("%-12d %-10d %-12s %-5d\n",
               mem_blocks[i].start,
               mem_blocks[i].size,
               mem_blocks[i].pid == -1 ? "FREE" : "ALLOCATED",
               mem_blocks[i].pid == -1 ? 0 : mem_blocks[i].pid);
        if (mem_blocks[i].pid != -1) used += mem_blocks[i].size;
    }
    printf("\nProcesses: %d | Used Memory: %d KB | Free Memory: %d KB\n\n",
           process_count, used, MAX_MEMORY - used);
}

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
        add_log("\n[SUCCESSFUL] '%s' created | PID '%d' | Memory at %d KB | State: READY\n",
            p->name, p->pid, start_addr);
    } else {
        p->memory_start = -1;
        add_log("WARNING: PID %d (%s) | Insufficient memory", p->pid, p->name);
    }
    process_count++;
    display_process_table();
    display_memory_map();

} 

void suspend_process(void) {
    int pid;
    printf("Enter PID to suspend: ");
    if (scanf("%d", &pid) != 1) { // If the user types a valid integer, scanf returns 1
        printf("Invalid Input.\n");
        return;
    }
    
    for (int i = 0; i < process_count; i++) {
         if (process_table[i].pid != pid)  // if not the entered pid, skip that process
            continue;
        if (process_table[i].state == TERMINATED) {
            add_log("[ERROR] PID %d is already TERMINATED - cannot suspend", pid);
            return;
        }

        if (process_table[i].state == WAITING) {
            add_log("[INFO] PID %d (%S) is already WAITING", pid, process_table[i].name);
            return;
        }
        process_table[i].state = WAITING; // changes process state from running or ready to waiting
        add_log("SUSPENDED: PID %d (%s) -> WAITING", pid, process_table[i].name);
        // display_process_table
        display_process_table();
        return;
    }
    add_log("[ERROR] PID %d not found", pid);
}

// terminate a process
void terminate_process(void) {
    int pid;
    printf("Enter PID to terminate: ");
    if (scanf("%d", &pid) != 1) { // If the user types a valid integer, scanf returns 1
        printf("Invalid Input.\n");
        return;
    }

    for (int i = 0; i < process_count; i++) {
         if (process_table[i].pid != pid)  // if not the entered pid, skip that process
            continue;
        
        if (process_table[i].state == TERMINATED) {
            add_log("[INFO] PID %d (%S) is already TERMINATED", pid, process_table[i].name);
            return;
        }

        process_table[i].state = TERMINATED; // change state of process to terminate
        
        // release memory after termination
        if (process_table[i].memory_start != -1) { // check if memory is allocated
            free_memory(pid); // release the block associated with this PID.
            process_table[i].memory_start = -1; //  marks the process as having no memory.
            add_log("[INFO] MRMORY: RELEASED %d KB from PID %d (%S)",
            process_table[i].memory_size, pid, process_table[i].name);
        }

        add_log("TERMINATED: PID %d (%s) removed from scheduler",
             pid, process_table[i].name);
        // display_process_table
        display_process_table();
        //display_memory_map
        display_memory_map();
    }
}
                             // CPU SCHEDULING
void fcfs_algo(void) {
    if (process_count == 0) {
        add_log("ERROR: No processes to schedule");
        printf("Invalid input. Please create at least one process.\n");
        return;
    }
    add_log("========= FCFS SCHEDULING STARTED =========");

    // making a scratch copy of your process table
    PCB temp[MAX_PROCESSES];
    // memcpy(destination, source, size_in_bytes);
    memcpy(temp, process_table, sizeof(PCB) * process_count);

    /* Sort by arrival time (bubble sort — small n, clarity preferred) */
    for (int i = 0; i < process_count - 1; i++)
        for (int j = 0; j < process_count - i - 1; j++)
            if (temp[j].arrival_time > temp[j + 1].arrival_time) {
                PCB sw = temp[j]; temp[j] = temp[j + 1]; temp[j + 1] = sw;
            }

    int current_t = 0, total_wt = 0, total_tat = 0, total_burst = 0;
    int scheduled_count = 0;

    for (int i = 0; i < process_count; i++) {
        if (temp[i].state == TERMINATED) continue;   /* skip manual terminations */
        if (temp[i].state == WAITING)    continue;   /* skip suspended processes */
        if (current_t < temp[i].arrival_time)
            current_t = temp[i].arrival_time;

        /* READY -> RUNNING */
        for (int k = 0; k < process_count; k++)
            if (process_table[k].pid == temp[i].pid)
                process_table[k].state = RUNNING;
        add_log("PID %d (%s) -> RUNNING at t=%d", temp[i].pid, temp[i].name, current_t);

        int wt  = current_t - temp[i].arrival_time;
        current_t += temp[i].burst_time;
        int tat = current_t - temp[i].arrival_time;
        total_wt    += wt;
        total_tat   += tat;
        total_burst += temp[i].burst_time;
        scheduled_count++;

        /* RUNNING -> TERMINATED */
        // AFTER (fixed)
        for (int k = 0; k < process_count; k++) {
            if (process_table[k].pid == temp[i].pid) {
                process_table[k].state = TERMINATED;
                if (process_table[k].memory_start != -1) {
                    free_memory(process_table[k].pid);
                    process_table[k].memory_start = -1;
                }
            }
        }

    if (scheduled_count > 0) {
        add_log("Average Waiting Time:    %.2f", (float)total_wt  / scheduled_count);
        add_log("Average Turnaround Time: %.2f", (float)total_tat / scheduled_count);
        add_log("CPU Utilization:         %.2f%%", (total_burst * 100.0) / current_t);
    }
    add_log("=== FCFS SCHEDULING COMPLETED ===");
    display_process_table();
    }
}
void priority_algo(void) {
    if (process_count == 0) {
        add_log("ERROR: No processes to schedule");
        return;
    }
    add_log("=== PRIORITY SCHEDULING STARTED ===");

    PCB temp[MAX_PROCESSES];
    memcpy(temp, process_table, sizeof(PCB) * process_count);

    int to_schedule = 0;
    for (int i = 0; i < process_count; i++)
        if (temp[i].state != TERMINATED) to_schedule++;

    int done[MAX_PROCESSES] = {0};
    int current_t = 0, total_wt = 0, total_tat = 0, total_burst = 0;
    int scheduled_count = 0;

    while (scheduled_count < to_schedule) {

        /* Step 1: find the highest-priority process that has arrived */
        int best = -1;
        for (int i = 0; i < process_count; i++) {
            if (done[i] || temp[i].state == TERMINATED || temp[i].state == WAITING) continue;
            if (temp[i].arrival_time <= current_t)
                if (best == -1 || temp[i].priority < temp[best].priority)
                    best = i;
        }

        /* Step 2: nothing arrived yet — advance to the earliest arrival */
        if (best == -1) {
            int earliest = -1;
            for (int i = 0; i < process_count; i++) {
                 if (done[i] || temp[i].state == TERMINATED || temp[i].state == WAITING) continue;
                if (earliest == -1 ||
                    temp[i].arrival_time < temp[earliest].arrival_time)
                    earliest = i;
            }
            if (earliest == -1) break;
            current_t = temp[earliest].arrival_time;
            for (int i = 0; i < process_count; i++) {
                if (done[i] || temp[i].state == TERMINATED || temp[i].state == WAITING) continue;
                if (temp[i].arrival_time <= current_t)
                    if (best == -1 || temp[i].priority < temp[best].priority)
                        best = i;
            }
        }
        if (best == -1) break;
        done[best] = 1;

        /* READY -> RUNNING */
        for (int k = 0; k < process_count; k++)
            if (process_table[k].pid == temp[best].pid)
                process_table[k].state = RUNNING;
        add_log("PID %d (%s) Priority=%d -> RUNNING at t=%d",
                temp[best].pid, temp[best].name, temp[best].priority, current_t);

        int wt  = current_t - temp[best].arrival_time;
        current_t += temp[best].burst_time;
        int tat = current_t - temp[best].arrival_time;
        total_wt    += wt;
        total_tat   += tat;
        total_burst += temp[best].burst_time;
        scheduled_count++;

        /* RUNNING -> TERMINATED */

        for (int k = 0; k < process_count; k++) {
            if (process_table[k].pid == temp[best].pid) {
                process_table[k].state = TERMINATED;
                if (process_table[k].memory_start != -1) {
                    free_memory(process_table[k].pid);
                    process_table[k].memory_start = -1;
                }
            }
        }
    }

    if (scheduled_count > 0) {
        add_log("Average Waiting Time:    %.2f", (float)total_wt  / scheduled_count);
        add_log("Average Turnaround Time: %.2f", (float)total_tat / scheduled_count);
        add_log("CPU Utilization:         %.2f%%", (total_burst * 100.0) / current_t);
    }
    add_log("=== PRIORITY SCHEDULING COMPLETED ===");
    display_process_table();
}

                             // FILE MANAGEMENT
// reads and display the log file   
void view_logs(void) {
    FILE* log_file = fopen(LOG_FILE, "r");  // open log file in read mode'r'
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
        printf("%s", message_line);
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
    printf("             Emergency Response System G84               \n");
    printf("======================================================\n");
    printf("  [Process Management]\n");
    printf("  1. Create Emergency Process\n");
    printf("  2. Show Process Table\n");
    printf("  6. Suspend Process\n");
    printf("  7. Terminate Process\n");
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


/* int main() {

    init_memory();
    add_log("===== Emergency Response System Started G84 ====");
    add_log("Total Available Memory: %d KB", MAX_MEMORY);

    int option;
    do {
        display_menu();
        if (scanf("%d", &option) != 1) {
            // Clear invalid input from buffer
            flush_input();
            printf("Invalid input. Please enter a number.\n");
            continue;
        }
        switch (option) {
            case 1:  create_process();         
                break;
            case 2:  display_process_table(); 
                break;
            case 3:  display_memory_map();    
                break;
            case 4:  fcfs_algo();            
                break;
            case 5:  priority_algo();        
                break;
            case 6:  suspend_process();        
                break;
            case 7:  terminate_process();      
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
*/