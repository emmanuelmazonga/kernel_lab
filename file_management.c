#include <stdio.h>
#define LOG_FILE "logs\\os.log" 

void log_event(const char* message) {
    FILE* log_file = fopen(LOG_FILE, "a`");
    if (!log_file) return;

    time_t current_real_time = time(NULL);
    struct tm* time_info = localtime(&current_real_time);
    char time_string[60];
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", time_info);

    fprintF(log_file, "[%s] %s\n", time_string, message);
    fclose(log_file);
}

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