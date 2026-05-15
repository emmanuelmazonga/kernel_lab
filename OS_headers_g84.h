#ifndef SERC_G84_H
#define SERC_G84_H

#include <stdarg.h>

/* ── Constants (must match OS_SERC_G84.c) ─────────────────────────────── */
#define MAX_PROCESSES    20
#define MAX_MEMORY       1024
#define MAX_MEMORY_BLOCKS 50
#define NAME_MAX_LEN     50
#define LOG_FILE         "logs\\os.log"

/* ── Types ────────────────────────────────────────────────────────────── */
typedef enum {
    NEW, READY, RUNNING, WAITING, TERMINATED
} Process_state;

typedef struct {
    int pid;
    char name[NAME_MAX_LEN];
    int arrival_time;
    int burst_time;
    int priority;
    int remaining_time;
    int memory_required;
    Process_state state;
    int memory_size;
    int memory_start;
} PCB;

typedef struct {
    int start;
    int size;
    int is_free;
    int pid;
} MemoryBlock;

/* ── Global state (defined in OS_SERC_G84.c) ─────────────────────────── */
extern PCB         process_table[MAX_PROCESSES];
extern int         process_count;
extern int         next_pid;
extern MemoryBlock mem_blocks[MAX_MEMORY_BLOCKS];
extern int         count_block;

/* ── Log callback ─────────────────────────────────────────────────────── */
typedef void (*LogCallback)(const char *message);
extern LogCallback log_cb;

/* ── Function prototypes ──────────────────────────────────────────────── */
void        add_log(const char *format, ...);
void        init_memory(void);
void        free_memory(int pid);
const char *state_to_string(Process_state state);

void create_process_auto(const char *name, int arrival, int burst,
                         int priority, int memory);
void suspend_process_by_pid(int pid);
void resume_process_by_pid(int pid);
void terminate_process_by_pid(int pid);
void fcfs_algo(void);
void priority_algo(void);

#endif /* SERC_G84_H */