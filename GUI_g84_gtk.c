#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include "serc_g84.h"




static GtkWidget    *main_window;      /* top-level application window            */
static GtkListStore *proc_store;       /* data model backing the process TreeView */
static GtkListStore *mem_store;        /* data model backing the memory TreeView  */
static GtkTextView  *log_view;         /* scrollable event log panel              */
static GtkWidget    *cpu_sched_label;  /* scheduler algorithm name (dynamic)      */
static GtkWidget    *cpu_info_label;   /* running PID / queue / util (dynamic)    */
static char          g_sched_name[64] = "Idle"; /* updated by each sched callback */




/* Process table — 10 columns (8 data + 2 colour) */
enum {
    PC_PID,       /* int    — process ID                  */
    PC_NAME,      /* string — process name                */
    PC_ARRIVAL,   /* int    — arrival time                */
    PC_BURST,     /* int    — CPU burst time              */
    PC_PRIORITY,  /* int    — priority level              */
    PC_STATE,     /* string — state label                 */
    PC_MEM,       /* int    — memory size (KB)            */
    PC_START,     /* int    — memory start address        */
    PC_BG,        /* string — row background colour (hex) */
    PC_FG,        /* string — row foreground colour (hex) */
    PC_COUNT      /* total column count — always last     */
};

/* Memory map — 5 columns (4 data + 1 colour) */
enum {
    MC_START,   /* int    — block start address           */
    MC_SIZE,    /* int    — block size (KB)               */
    MC_STATUS,  /* string — "FREE" or "ALLOCATED"         */
    MC_PID,     /* int    — owning PID  (0 = free)        */
    MC_BG,      /* string — row background colour (hex)   */
    MC_COUNT    /* total column count — always last        */
};


/* ╔══════════════════════════════════════════════════════════════════════════╗
 * ║  SECTION 3 — DARK THEME (CSS)                                          ║
 * ╠══════════════════════════════════════════════════════════════════════════╣
 * ║  GTK3 supports CSS styling via GtkCssProvider.  The stylesheet is       ║
 * ║  injected once at startup and applies to every widget on the default    ║
 * ║  screen.  Palette: Catppuccin Mocha (dark purple-grey).                ║
 * ║                                                                          ║
 * ║  Key palette colours                                                     ║
 * ║    #1e1e2e  base      — main window background                          ║
 * ║    #181825  mantle    — panel / TreeView background                     ║
 * ║    #11111b  crust     — log view background (deepest)                   ║
 * ║    #313244  surface0  — buttons, entry fields                           ║
 * ║    #45475a  surface1  — hover state, borders                            ║
 * ║    #cdd6f4  text      — primary text                                    ║
 * ║    #89b4fa  blue      — accent (frame labels, column headers)           ║
 * ║    #a6e3a1  green     — log text                                        ║
 * ╚══════════════════════════════════════════════════════════════════════════╝ */

static void apply_dark_theme(void) {
    const char *css =
        /* ── Global reset ── */
        "* { "
        "  background-color: #1e1e2e; "
        "  color: #cdd6f4; "
        "} "

        /* ── Main window ── */
        "window { background-color: #1e1e2e; } "

        /* ── Frames (panel borders + titles) ── */
                "frame { "
        "  border: 1px solid #45475a; "
        "  border-radius: 12px; "
        "  background-color: #181825; "
        "  padding: 8px; "
        "} "
        "frame border { border-color: #45475a; } "

        /* ── TreeView ── */
        "treeview { "
        "  background-color: #181825; "
        "  color: #cdd6f4; "
        "} "
        "treeview:selected { "
        "  background-color: #45475a; "
        "  color: #89b4fa; "
        "} "

        /* ── UPDATED: TreeView Column Headers ── */
        "treeview header button { "
        "  background-color: #313244; " /* Matches action buttons */
        "  color: #89b4fa; "            /* Keeps the blue accent text */
        "  font-weight: bold; "
        "  border: 3px solid #45475a; "
        "  border-radius: 0; "          /* Squared edges look better for table headers */
        "  padding: 5px 8px; "
        "} "
        "treeview header button:hover { "
        "  background-color: #45475a; " /* Matches action button hover */
        "  color: #89b4fa; "
        "} "

        /* ── Action buttons (bottom bar) ── */
       /* ── Modern Dashboard Buttons ── */
        "#action-btn { "
        "  background-color: #89b4fa; "
        "  color: #313244; "
        "  border: 1px solid #585b70; "
        "  border-radius: 8px; "
        "  padding: 8px 14px; "
        "  font-weight: bold; "
        "} "

        "#action-btn label { "
        "  color: #313244; "
        "  background-color: transparent; "
        "} "

        "#action-btn:hover { "
        "  background-color: #45475a; "
        "  border-color: #89b4fa; "
        "  color: #89b4fa; "
        "} "

        "#action-btn:active { "
        "  background-color: #585b70; "
        "} "

        /* ── Entry fields ── */
        "entry { "
        "  background-color: #313244; "
        "  color: #cdd6f4; "
        "  border-color: #45475a; "
        "  border-radius: 4px; "
        "  caret-color: #89b4fa; "
        "  padding: 4px 8px; "
        "} "
        "entry:focus { border-color: #89b4fa; } "

        /* ── Log TextView ── */
        "textview { "
        "  background-color: #11111b; "
        "  color: #a6e3a1; "
        "} "
        "textview text { "
        "  background-color: #11111b; "
        "  color: #a6e3a1; "
        "} "

      /* ── Scrollbars ── */
    "scrollbar { background-color: #1e1e2e; border: none; } "
    "scrollbar slider { "
    "  background-color: #45475a; "
    "  border-radius: 4px; "
    "  min-width: 6px; min-height: 6px; "
    "} "
    "scrollbar slider:hover { background-color: #585b70; } "

    /* ── Dividers ── */
    "separator { background-color: #45475a; padding: 8px; } "
    "paned > separator { background-color: #313244; } "

    /* ── Dialogs ── */
    "dialog { background-color: #1e1e2e; } "
    "dialog > box { background-color: #1e1e2e; } "
    ".dialog-action-area { "
    "  background-color: #1e1e2e; "
    "  color: #313244; "
    "  border-top: 1px solid #45475a; "
    "  padding: 8px; "
    "} "
    "#action-btn label { "
    "  color: #313244; "
    "  background-color: transparent; "
    "} "

    /* ── Labels ── */
    "label { color: #cdd6f4; background-color: transparent; } ";

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref(provider);
}

/* ╔══════════════════════════════════════════════════════════════════════════╗
 * ║  SECTION 4 — STATE COLOUR MAPPING                                      ║
 * ╠══════════════════════════════════════════════════════════════════════════╣
 * ║  Each process row in the TreeView is coloured based on its current      ║
 * ║  state.  Colours are chosen to be readable against the dark TreeView    ║
 * ║  background (#181825).                                                  ║
 * ╚══════════════════════════════════════════════════════════════════════════╝ */

/* Returns the background hex colour for a given process state */
static const char *state_bg(Process_state s) {
    switch (s) {
        case NEW:        return "#1a3a5f"; /* muted blue   — process just created  */
        case READY:      return "#1a3d1a"; /* muted green  — waiting for CPU       */
        case RUNNING:    return "#0d5c28"; /* stronger green — actively on CPU     */
        case WAITING:    return "#5f4a00"; /* amber        — blocked / suspended   */
        case TERMINATED: return "#5c0f0f"; /* dark red     — process has ended     */
        default:         return "#313244"; /* surface grey — fallback              */
    }
}

/* Returns the foreground (text) colour for a given process state.
   All dark backgrounds use light text for contrast.                 */
static const char *state_fg(Process_state s) {
    switch (s) {
        case NEW:        return "#89b4fa"; /* blue text    */
        case READY:      return "#a6e3a1"; /* green text   */
        case RUNNING:    return "#a6e3a1"; /* green text   */
        case WAITING:    return "#f9e2af"; /* yellow text  */
        case TERMINATED: return "#f38ba8"; /* pink/red text*/
        default:         return "#cdd6f4"; /* default text */
    }
}


static void gui_log_callback(const char *message) {

    /* Get the text buffer that backs the log TextView */
    GtkTextBuffer *buf = gtk_text_view_get_buffer(log_view);

    /* Move the insertion iterator to the end of existing text */
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buf, &end);

    /* Append the message and a newline */
    gtk_text_buffer_insert(buf, &end, message, -1);
    gtk_text_buffer_insert(buf, &end, "\n",    -1);

    /* Auto-scroll: move the cursor mark to the new end, then scroll to it */
    GtkTextMark *mark = gtk_text_buffer_get_insert(buf);
    gtk_text_view_scroll_to_mark(log_view, mark, 0.0, FALSE, 0.0, 1.0);
}


static void refresh_cpu_panel(void);
/* Rebuild the process table from process_table[] */
static void refresh_proc_table(void) {
    gtk_list_store_clear(proc_store); /* remove all existing rows */

    for (int i = 0; i < process_count; i++) {
        PCB *p = &process_table[i];
        GtkTreeIter it;

        /* Append a blank row, then fill every column */
        gtk_list_store_append(proc_store, &it);
        gtk_list_store_set(proc_store, &it,
            PC_PID,      p->pid,
            PC_NAME,     p->name,
            PC_ARRIVAL,  p->arrival_time,
            PC_BURST,    p->burst_time,
            PC_PRIORITY, p->priority,
            PC_STATE,    state_to_string(p->state),
            PC_MEM,      p->memory_size,
            PC_START,    p->memory_start == -1 ? 0 : p->memory_start,
            PC_BG,       state_bg(p->state),  /* row background colour */
            PC_FG,       state_fg(p->state),  /* row text colour       */
            -1);                               /* sentinel — end of args */
    }
}

/* Rebuild the memory map from mem_blocks[] */
static void refresh_mem_table(void) {
    gtk_list_store_clear(mem_store); /* remove all existing rows */

    for (int i = 0; i < count_block; i++) {
        MemoryBlock *b = &mem_blocks[i];
        GtkTreeIter  it;

        gtk_list_store_append(mem_store, &it);
        gtk_list_store_set(mem_store, &it,
            MC_START,  b->start,
            MC_SIZE,   b->size,
            MC_STATUS, b->pid == -1 ? "FREE" : "ALLOCATED",
            MC_PID,    b->pid == -1 ? 0 : b->pid,
            /* Dark green for free blocks, dark red for allocated */
            MC_BG,     b->pid == -1 ? "#0d2a0d" : "#2a0d0d",
            -1);
    }
}

/* Convenience wrapper — always refresh both tables together */
static void refresh_all(void) {
    refresh_proc_table();
    refresh_mem_table();
    refresh_cpu_panel();   /* update the CPU sidebar panel */
}

/* Rebuild the CPU panel from live process and memory data.
   Called automatically by refresh_all() after every operation.         */
static void refresh_cpu_panel(void) {
    /* Guard: labels may not exist yet during early init */
    if (!cpu_info_label || !cpu_sched_label) return;

    /* ── Scan the process table ───────────────────────────────────── */
    int  running_pid  = -1;
    int  ready_count  = 0;
    int  terminated   = 0;
    char queue_str[256] = "";

    for (int i = 0; i < process_count; i++) {
        PCB *p = &process_table[i];
        if (p->state == RUNNING) {
            running_pid = p->pid;
        } else if (p->state == READY) {
            char tmp[32];
            snprintf(tmp, sizeof(tmp),
                     ready_count ? ", PID %d" : "PID %d", p->pid);
            strncat(queue_str, tmp, sizeof(queue_str) - strlen(queue_str) - 1);
            ready_count++;
        } else if (p->state == TERMINATED) {
            terminated++;
        }
    }

    /* ── Count free memory blocks (fragmentation indicator) ────────── */
    int free_blocks = 0;
    for (int i = 0; i < count_block; i++) {
        if (mem_blocks[i].pid == -1) free_blocks++;
    }

    /* ── CPU utilisation: fraction of processes that have finished ─── */
    int util = (process_count > 0) ? (terminated * 100 / process_count) : 0;

    /* ── Format running PID string ────────────────────────────────── */
    char running_str[32];
    if (running_pid >= 0)
        snprintf(running_str, sizeof(running_str), "%d", running_pid);
    else
        strncpy(running_str, "None", sizeof(running_str));

    /* ── Build pango markup for the info block ─────────────────────── */
    char info_markup[768];
    snprintf(info_markup, sizeof(info_markup),
        "<b>Running PID  :</b> %s\n"
        "<b>Ready Queue  :</b> %s\n"
        "<b>CPU Util     :</b> %d%%\n"
        "<b>Free Blocks  :</b> %d",
        running_str,
        ready_count > 0 ? queue_str : "\xe2\x80\x94", /* em-dash when empty */
        util,
        free_blocks);

    gtk_label_set_markup(GTK_LABEL(cpu_info_label), info_markup);

    /* ── Update the scheduler name label ─────────────────────────── */
    char sched_markup[128];
    snprintf(sched_markup, sizeof(sched_markup),
        "<span foreground='#89b4fa' weight='bold'>%s</span>",
        g_sched_name);
    gtk_label_set_markup(GTK_LABEL(cpu_sched_label), sched_markup);
}


/* ╔══════════════════════════════════════════════════════════════════════════╗
 * ║  SECTION 7 — WIDGET BUILDER HELPERS                                    ║
 * ╠══════════════════════════════════════════════════════════════════════════╣
 * ║  Small utility functions that reduce repeated boilerplate when          ║
 * ║  constructing the UI.                                                   ║
 * ╚══════════════════════════════════════════════════════════════════════════╝ */

/*
 * add_col — append a text column to a GtkTreeView.
 *
 *   tv       : the TreeView to add the column to
 *   title    : column header label
 *   data_col : model column index that holds the display string/int
 *   bg_col   : model column index for the "background" CSS colour
 *              (-1 = no background binding)
 *   fg_col   : model column index for the "foreground" CSS colour
 *              (-1 = no foreground binding)
 */
static void add_col(GtkTreeView *tv, const char *title,
                    int data_col, int bg_col, int fg_col) {

    GtkCellRenderer   *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col      = gtk_tree_view_column_new();

    gtk_tree_view_column_set_title(col, title);
    gtk_tree_view_column_pack_start(col, renderer, TRUE);

    /* Bind the display text */
    gtk_tree_view_column_add_attribute(col, renderer, "text", data_col);

    /* Optionally bind row background and foreground colours */
    if (bg_col >= 0)
        gtk_tree_view_column_add_attribute(col, renderer, "background", bg_col);
    if (fg_col >= 0)
        gtk_tree_view_column_add_attribute(col, renderer, "foreground", fg_col);

    gtk_tree_view_column_set_resizable(col, TRUE); /* user can resize columns */
    gtk_tree_view_column_set_min_width(col, 55);
    gtk_tree_view_append_column(tv, col);
}

/*
 * scrolled — wrap a widget in a GtkScrolledWindow.
 *
 *   child : widget to embed (typically a TreeView or TextView)
 *   min_h : minimum height in pixels for the scroll area
 */
static GtkWidget *scrolled(GtkWidget *child, int min_h) {
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

    /* Show scrollbars only when content overflows */
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_container_add(GTK_CONTAINER(sw), child);
    gtk_widget_set_size_request(sw, -1, min_h);
    return sw;
}

/*
 * grid_entry — add a label + text entry pair to a GtkGrid row.
 *
 *   grid        : parent grid widget
 *   label       : descriptive text shown to the left of the entry
 *   placeholder : grey hint text shown inside the empty entry
 *   row         : which grid row to attach to (0-based)
 *
 *   Returns the GtkEntry widget so the caller can read its value later.
 */
static GtkWidget *grid_entry(GtkWidget *grid, const char *label,
                              const char *placeholder, int row) {
    /* Label — right-aligned so it sits flush against the entry */
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_halign(lbl, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), lbl, 0, row, 1, 1);

    /* Entry field — expands horizontally to fill available space */
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), placeholder);
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), entry, 1, row, 1, 1);

    return entry;
}

/*
 * show_error — display a modal error dialog.
 *
 *   msg : the error message to display
 */
static void show_error(const char *msg) {
    GtkWidget *dlg = gtk_message_dialog_new(
        GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
        "%s", msg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

/*
 * make_btn — create a uniformly sized button and connect its callback.
 *
 *   label : button text
 *   cb    : GCallback function to invoke on "clicked" signal
 */
static GtkWidget *make_btn(const char *label, GCallback cb) {
    GtkWidget *btn = gtk_button_new_with_label(label);
    gtk_widget_set_name(btn, "action-btn"); /* CSS ID for targeted styling */
    gtk_widget_set_size_request(btn, 145, 38); /* uniform button size */
    g_signal_connect(btn, "clicked", cb, NULL);
    return btn;
}


/* ╔══════════════════════════════════════════════════════════════════════════╗
 * ║  SECTION 8 — TREEVIEW BUILDERS                                         ║
 * ╠══════════════════════════════════════════════════════════════════════════╣
 * ║  Each builder creates the GtkListStore data model and the GtkTreeView   ║
 * ║  display widget, attaches all columns, and returns the TreeView.        ║
 * ╚══════════════════════════════════════════════════════════════════════════╝ */

/* Build the process table TreeView.
   Column order must match the PC_* enum exactly.                      */
static GtkWidget *build_proc_view(void) {

    /* Create the data model — one row per process, 10 columns */
    proc_store = gtk_list_store_new(PC_COUNT,
        G_TYPE_INT,    /* PC_PID      */
        G_TYPE_STRING, /* PC_NAME     */
        G_TYPE_INT,    /* PC_ARRIVAL  */
        G_TYPE_INT,    /* PC_BURST    */
        G_TYPE_INT,    /* PC_PRIORITY */
        G_TYPE_STRING, /* PC_STATE    */
        G_TYPE_INT,    /* PC_MEM      */
        G_TYPE_INT,    /* PC_START    */
        G_TYPE_STRING, /* PC_BG       */
        G_TYPE_STRING  /* PC_FG       */
    );

    /* Create the view and hand it the model */
    GtkWidget *tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(proc_store));

    /* The TreeView now holds its own reference to the store,
       so we release ours to avoid a memory leak.               */
    g_object_unref(proc_store);

    /* Attach all visible columns — colour columns (PC_BG, PC_FG)
       are bound as attributes but not shown as standalone columns  */
    add_col(GTK_TREE_VIEW(tv), "PID",      PC_PID,      PC_BG, PC_FG);
    add_col(GTK_TREE_VIEW(tv), "Name",     PC_NAME,     PC_BG, PC_FG);
    add_col(GTK_TREE_VIEW(tv), "Arrival",  PC_ARRIVAL,  PC_BG, PC_FG);
    add_col(GTK_TREE_VIEW(tv), "Burst",    PC_BURST,    PC_BG, PC_FG);
    add_col(GTK_TREE_VIEW(tv), "Priority", PC_PRIORITY, PC_BG, PC_FG);
    add_col(GTK_TREE_VIEW(tv), "State",    PC_STATE,    PC_BG, PC_FG);
    add_col(GTK_TREE_VIEW(tv), "Mem(KB)",  PC_MEM,      PC_BG, PC_FG);
    add_col(GTK_TREE_VIEW(tv), "Start",    PC_START,    PC_BG, PC_FG);

    /* Subtle horizontal grid lines between rows */
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(tv),
                                 GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
    return tv;
}

/* Build the memory map TreeView.
   Column order must match the MC_* enum exactly.                      */
static GtkWidget *build_mem_view(void) {

    /* Create the data model — one row per memory block, 5 columns */
    mem_store = gtk_list_store_new(MC_COUNT,
        G_TYPE_INT,    /* MC_START  */
        G_TYPE_INT,    /* MC_SIZE   */
        G_TYPE_STRING, /* MC_STATUS */
        G_TYPE_INT,    /* MC_PID    */
        G_TYPE_STRING  /* MC_BG     */
    );

    GtkWidget *tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(mem_store));
    g_object_unref(mem_store);

    /* Memory map has no per-row foreground colour (-1 = use CSS default) */
    add_col(GTK_TREE_VIEW(tv), "Start(KB)", MC_START,  MC_BG, -1);
    add_col(GTK_TREE_VIEW(tv), "Size(KB)",  MC_SIZE,   MC_BG, -1);
    add_col(GTK_TREE_VIEW(tv), "Status",    MC_STATUS, MC_BG, -1);
    add_col(GTK_TREE_VIEW(tv), "PID",       MC_PID,    MC_BG, -1);

    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(tv),
                                 GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
    return tv;
}


/* ── Create Process ─────────────────────────────────────────────────────── */
/*
 * Opens a modal dialog with five input fields.
 * On confirmation, calls create_process_auto() with the entered values
 * and refreshes both tables.
 */
static void on_create_clicked(GtkWidget *btn, gpointer data) {
    (void)btn; (void)data;

    /* Build the dialog */
    GtkWidget *dlg = gtk_dialog_new_with_buttons(
        "Create Emergency Process",
        GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Create", GTK_RESPONSE_OK,
        "Cancel", GTK_RESPONSE_CANCEL, NULL);
        gtk_window_set_default_size(GTK_WINDOW(dlg), 400, -1);

        /* Get the content area and add padding */
        GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
        gtk_container_set_border_width(GTK_CONTAINER(content), 16);

        /* Form grid — 2 columns (label | entry), 5 rows */
        GtkWidget *grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid),    12);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 16);

        /* Create one labeled entry per field */
        GtkWidget *e_name     = grid_entry(grid, "Process Name:",       "e.g. Ambulance Dispatch", 0);
        GtkWidget *e_arrival  = grid_entry(grid, "Arrival Time:",       "0",                       1);
        GtkWidget *e_burst    = grid_entry(grid, "Burst Time:",         "5",                       2);
        GtkWidget *e_priority = grid_entry(grid, "Priority (1=High):",  "1",                       3);
        GtkWidget *e_memory   = grid_entry(grid, "Memory (KB):",        "128",                     4);

        gtk_container_add(GTK_CONTAINER(content), grid);
        gtk_widget_show_all(dlg);

        /* Run the dialog — blocks until user clicks Create or Cancel */
        if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
            const char *name = gtk_entry_get_text(GTK_ENTRY(e_name));

            if (strlen(name) == 0) {
                /* Reject empty name — show inline error */
                show_error("Process name cannot be empty.");
            } else {
                /* Pass all values to the logic layer */
                create_process_auto(
                    name,
                    atoi(gtk_entry_get_text(GTK_ENTRY(e_arrival))),
                    atoi(gtk_entry_get_text(GTK_ENTRY(e_burst))),
                    atoi(gtk_entry_get_text(GTK_ENTRY(e_priority))),
                    atoi(gtk_entry_get_text(GTK_ENTRY(e_memory)))
                );
                refresh_all(); /* rebuild tables to show the new process */
            }
        }
        gtk_widget_destroy(dlg);
    }

    /* ── Suspend Process ────────────────────────────────────────────────────── */
    /*
    * Asks for a PID and calls suspend_process_by_pid().
    * State transition: READY / RUNNING → WAITING.  Memory is retained.
    */
    static void on_suspend_clicked(GtkWidget *btn, gpointer data) {
        (void)btn; (void)data;

        GtkWidget *dlg = gtk_dialog_new_with_buttons(
            "Suspend Process",
            GTK_WINDOW(main_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "Suspend", GTK_RESPONSE_OK,
            "Cancel",  GTK_RESPONSE_CANCEL, NULL);

        GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
        gtk_container_set_border_width(GTK_CONTAINER(content), 16);

        GtkWidget *grid  = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid),    12);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
        GtkWidget *e_pid = grid_entry(grid, "PID to suspend:", "e.g. 1", 0);
        gtk_container_add(GTK_CONTAINER(content), grid);
        gtk_widget_show_all(dlg);

        if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
            const char *txt = gtk_entry_get_text(GTK_ENTRY(e_pid));
            if (strlen(txt) == 0) {
                show_error("Please enter a PID.");
            } else {
                suspend_process_by_pid(atoi(txt));
                refresh_all();
            }
        }
        gtk_widget_destroy(dlg);
    }

    /* ── Resume Process ─────────────────────────────────────────────────────── */
    /*
    * Asks for a PID and calls resume_process_by_pid().
    * State transition: WAITING → READY.
    * Only WAITING (suspended) processes can be resumed.
    */
    static void on_resume_clicked(GtkWidget *btn, gpointer data) {
        (void)btn; (void)data;

        GtkWidget *dlg = gtk_dialog_new_with_buttons(
            "Resume Process",
            GTK_WINDOW(main_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "Resume", GTK_RESPONSE_OK,
            "Cancel",  GTK_RESPONSE_CANCEL, NULL);

        GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
        gtk_container_set_border_width(GTK_CONTAINER(content), 16);

        GtkWidget *grid  = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid),    12);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
        GtkWidget *e_pid = grid_entry(grid, "PID to resume:", "e.g. 1", 0);
        gtk_container_add(GTK_CONTAINER(content), grid);
        gtk_widget_show_all(dlg);

        if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
            const char *txt = gtk_entry_get_text(GTK_ENTRY(e_pid));
            if (strlen(txt) == 0) {
                show_error("Please enter a PID.");
            } else {
                resume_process_by_pid(atoi(txt));
                refresh_all();
            }
        }
        gtk_widget_destroy(dlg);
    }

    /* ── Terminate Process ──────────────────────────────────────────────────── */
    /*
    * Asks for a PID and calls terminate_process_by_pid().
    * State transition: any live state → TERMINATED.
    * Memory is released and adjacent free blocks are coalesced.
    */
    static void on_terminate_clicked(GtkWidget *btn, gpointer data) {
        (void)btn; (void)data;

        GtkWidget *dlg = gtk_dialog_new_with_buttons(
            "Terminate Process",
            GTK_WINDOW(main_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "Terminate", GTK_RESPONSE_OK,
            "Cancel",    GTK_RESPONSE_CANCEL, NULL);

        GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
        gtk_container_set_border_width(GTK_CONTAINER(content), 16);

        GtkWidget *grid  = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid),    12);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
        GtkWidget *e_pid = grid_entry(grid, "PID to terminate:", "e.g. 1", 0);
        gtk_container_add(GTK_CONTAINER(content), grid);
        gtk_widget_show_all(dlg);

        if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
            const char *txt = gtk_entry_get_text(GTK_ENTRY(e_pid));
            if (strlen(txt) == 0) {
                show_error("Please enter a PID.");
            } else {
                terminate_process_by_pid(atoi(txt));
                refresh_all();
            }
        }
        gtk_widget_destroy(dlg);
    }

    /* ── FCFS Scheduling ────────────────────────────────────────────────────── */
    /*
    * Runs First-Come First-Served scheduling on all non-terminated processes.
    * Processes are sorted by arrival_time; state transitions are
    * READY → RUNNING → TERMINATED.  Results logged via add_log().
    */
    static void on_fcfs_clicked(GtkWidget *btn, gpointer data) {
        (void)btn; (void)data;
        strncpy(g_sched_name, "FCFS Scheduler", sizeof(g_sched_name) - 1);
        fcfs_algo();   /* defined in OS_SERC_G84.c */
        refresh_all(); /* update tables to show TERMINATED states */
    }

    /* ── Priority Scheduling ────────────────────────────────────────────────── */
    /*
    * Runs non-preemptive Priority Scheduling.  At each step the
    * highest-priority arrived process is selected.  If no process has
    * arrived yet, time advances to the next arrival.
    */
    static void on_priority_clicked(GtkWidget *btn, gpointer data) {
        (void)btn; (void)data;
        strncpy(g_sched_name, "Priority-Based Scheduler", sizeof(g_sched_name) - 1);
        priority_algo(); /* defined in OS_SERC_G84.c */
        refresh_all();
    }

    /* ── Clear Log ──────────────────────────────────────────────────────────── */
    /*
    * Truncates the log file (logs\os.log) to zero bytes and clears the
    * log TextView so both the file and the on-screen view are reset.
    */
    static void on_clear_log_clicked(GtkWidget *btn, gpointer data) {
        (void)btn; (void)data;

        /* Truncate the file by opening in write mode and closing immediately */
        FILE *lf = fopen(LOG_FILE, "w");
        if (lf) fclose(lf);

        /* Clear the TextView buffer */
        GtkTextBuffer *buf = gtk_text_view_get_buffer(log_view);
        gtk_text_buffer_set_text(buf, "", -1);

        /* Confirm in the log itself */
        gui_log_callback("[INFO] Log cleared.");
    }



    int main(int argc, char **argv) {

        /* Initialise the GTK library — must be called before any GTK function */
        gtk_init(&argc, &argv);

        /* Create the logs\ folder if it doesn't already exist.
        "2>NUL" suppresses the Windows error if the folder is already there. */
        system("mkdir logs 2>NUL");

        /* Apply dark theme CSS before any widgets are created so every
        widget that is built afterwards inherits the dark styles.      */
        apply_dark_theme();

        /* Register the GUI log callback BEFORE init_memory() so that the
        startup messages reach the log view from the very first call.  */
        log_cb = gui_log_callback;
        init_memory(); /* defined in OS_SERC_G84.c — sets up the memory map */

        /* ── Main window ──────────────────────────────────────────────────── */
    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title(GTK_WINDOW(main_window),
        "Smart Emergency Response System | Group 84");

    gtk_window_set_default_size(GTK_WINDOW(main_window), 1500, 900);

    gtk_container_set_border_width(GTK_CONTAINER(main_window), 10);

    g_signal_connect(main_window,
                    "destroy",
                    G_CALLBACK(gtk_main_quit),
                    NULL);

    /* ================================================================
    * PROCESS TABLE
    * ================================================================ */
    GtkWidget *proc_tv = build_proc_view();

    GtkWidget *proc_frame = gtk_frame_new("  Process Management  ");

    gtk_container_add(GTK_CONTAINER(proc_frame),
                    scrolled(proc_tv, 400));

    /* ================================================================
    * MEMORY PANEL
    * ================================================================ */
    GtkWidget *mem_tv = build_mem_view();

    GtkWidget *mem_frame = gtk_frame_new("  Memory Map  ");

    gtk_container_add(GTK_CONTAINER(mem_frame),
                    scrolled(mem_tv, 300));

    /* ================================================================
    * EVENT LOG
    * ================================================================ */
    GtkWidget *log_tv = gtk_text_view_new();

    log_view = GTK_TEXT_VIEW(log_tv);

    gtk_text_view_set_editable(GTK_TEXT_VIEW(log_tv), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(log_tv), TRUE);

    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(log_tv), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(log_tv), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(log_tv), 6);

    gtk_widget_set_size_request(log_tv, -1, 180);

    GtkWidget *log_frame = gtk_frame_new("  Event Log  ");

    gtk_container_add(GTK_CONTAINER(log_frame),
                    scrolled(log_tv, 200));

    /* ================================================================
    * TOP TOOLBAR
    * ================================================================ */
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

    gtk_widget_set_margin_top(toolbar, 4);
    gtk_widget_set_margin_bottom(toolbar, 10);
    gtk_widget_set_margin_start(toolbar, 4);
    gtk_widget_set_margin_end(toolbar, 4);

    GtkWidget *title = gtk_label_new(NULL);

    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='18000' weight='bold' foreground='#89b4fa'>"
        " Smart Emergency Response System"
        "</span>");

    gtk_widget_set_hexpand(title, TRUE);
    gtk_widget_set_halign(title, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(toolbar),
                    title,
                    TRUE,
                    TRUE,
                    0);

    /* RIGHT SIDE BUTTONS */
    GtkWidget *btn_create = make_btn("+ Create Process",
                                    G_CALLBACK(on_create_clicked));

    GtkWidget *btn_fcfs = make_btn("Run FCFS",
                                G_CALLBACK(on_fcfs_clicked));

    GtkWidget *btn_priority = make_btn("Priority",
                                    G_CALLBACK(on_priority_clicked));

    GtkWidget *btn_suspend = make_btn("Suspend",
                                    G_CALLBACK(on_suspend_clicked));

    GtkWidget *btn_resume = make_btn("Resume",
                                    G_CALLBACK(on_resume_clicked));

    GtkWidget *btn_terminate = make_btn("Terminate",
                                        G_CALLBACK(on_terminate_clicked));

    GtkWidget *btn_log = make_btn("Clear Log",
                                G_CALLBACK(on_clear_log_clicked));

    /* smaller toolbar buttons */
    gtk_widget_set_size_request(btn_create, 150, 40);
    gtk_widget_set_size_request(btn_fcfs, 120, 40);
    gtk_widget_set_size_request(btn_priority, 120, 40);
    gtk_widget_set_size_request(btn_suspend, 120, 40);
    gtk_widget_set_size_request(btn_resume, 120, 40);
    gtk_widget_set_size_request(btn_terminate, 120, 40);
    gtk_widget_set_size_request(btn_log, 120, 40);

    /* add buttons */
    gtk_box_pack_end(GTK_BOX(toolbar), btn_log,
                    FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(toolbar), btn_terminate,
                    FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(toolbar), btn_resume,
                    FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(toolbar), btn_suspend,
                    FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(toolbar), btn_priority,
                    FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(toolbar), btn_fcfs,
                    FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(toolbar), btn_create,
                    FALSE, FALSE, 0);

    /* ================================================================
    * CPU PANEL
    * ================================================================ */
    GtkWidget *cpu_frame = gtk_frame_new("  CPU Scheduler  ");

    GtkWidget *cpu_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    gtk_container_set_border_width(GTK_CONTAINER(cpu_box), 12);

    /* Scheduler name — updated by refresh_cpu_panel() each time a
       scheduling algorithm runs.  Starts as "Idle" until first run. */
    cpu_sched_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(cpu_sched_label),
        "<span foreground='#89b4fa' weight='bold'>Idle</span>");

    /* Live stats block — updated by refresh_cpu_panel() after every
       create / suspend / terminate / schedule operation.             */
    cpu_info_label = gtk_label_new("No processes yet.");
    gtk_label_set_xalign(GTK_LABEL(cpu_info_label), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(cpu_info_label), FALSE);

    gtk_box_pack_start(GTK_BOX(cpu_box), cpu_sched_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cpu_box), cpu_info_label,  FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(cpu_frame), cpu_box);

    /* ================================================================
    * RIGHT SIDEBAR
    * ================================================================ */
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    gtk_widget_set_size_request(sidebar, 340, -1);

    gtk_box_pack_start(GTK_BOX(sidebar),
                    mem_frame,
                    TRUE,
                    TRUE,
                    0);

    gtk_box_pack_start(GTK_BOX(sidebar),
                    cpu_frame,
                    FALSE,
                    FALSE,
                    0);

    /* ================================================================
    * MAIN DASHBOARD AREA
    * ================================================================ */
    GtkWidget *dashboard = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

    /* process table left */
    gtk_box_pack_start(GTK_BOX(dashboard),
                    proc_frame,
                    TRUE,
                    TRUE,
                    0);

    /* sidebar right */
    gtk_box_pack_start(GTK_BOX(dashboard),
                    sidebar,
                    FALSE,
                    FALSE,
                    0);

    /* ================================================================
    * ROOT LAYOUT
    * ================================================================ */
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    /* toolbar */
    gtk_box_pack_start(GTK_BOX(root),
                    toolbar,
                    FALSE,
                    FALSE,
                    0);

    /* dashboard */
    gtk_box_pack_start(GTK_BOX(root),
                    dashboard,
                    TRUE,
                    TRUE,
                    0);

    /* event log */
    gtk_box_pack_start(GTK_BOX(root),
                    log_frame,
                    FALSE,
                    TRUE,
                    0);

    /* attach root */
    gtk_container_add(GTK_CONTAINER(main_window), root);

    /* show all widgets */
    gtk_widget_show_all(main_window);
    /* Populate tables with the initial (empty) state */
    refresh_all();

    /* Print startup messages — they route through gui_log_callback() */
    add_log("===== Emergency Response System G84 Started =====");
    add_log("Total Available Memory: %d KB", MAX_MEMORY);
    add_log("Use the buttons below to manage emergency processes.");

    /* Hand control to the GTK event loop — returns only on exit */
    gtk_main();
    return 0;
}