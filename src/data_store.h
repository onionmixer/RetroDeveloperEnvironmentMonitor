/*
 * data_store.h - Data storage for rdemonitor
 *
 * Manages data structures for all four tabs:
 * - Tab 1 (Info): Basic machine/emulator information
 * - Tab 2 (IO): I/O port and switch information (2048 line buffer)
 * - Tab 3 (CPU): CPU registers, flags, state (replace by key)
 * - Tab 4 (Memory): Memory dump in hex format (dynamic, replace by address)
 */

#ifndef DATA_STORE_H
#define DATA_STORE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "parser.h"

/*
 * =============================================================================
 * Tab 1: Info Data (mach, sys categories)
 * =============================================================================
 */

typedef struct {
    /* Emulator info */
    char emu_type[16];          /* "apple" or "msx" */
    char emu_version[64];       /* e.g., "AppleWin 1.30.0" */
    char protocol_ver[16];      /* Protocol version from hello */

    /* Machine info (mach.info) */
    char machine_id[64];        /* e.g., "Panasonic_A1F" */
    char machine_name[128];     /* e.g., "Panasonic FS-A1F" */
    char machine_type[32];      /* e.g., "MSXturboR", "Apple2e" */

    /* CPU info (cpu.state) */
    char cpu_type[32];          /* e.g., "Z80", "65C02", "R800" */
    char cpu_cycles[32];        /* Total cycles */

    /* Status (mach.status) */
    char status_mode[32];       /* "running", "paused" */
    char status_powered[8];     /* "0" or "1" */

    /* Video (mach.video) */
    char video_mode[32];
    char video_page[8];
    char video_mixed[8];

    /* Connection info */
    time_t connected_time;
    bool   is_connected;

    /* Update tracking */
    bool   has_data;
} InfoData;

/*
 * =============================================================================
 * Tab 2: IO Data (io category)
 * =============================================================================
 */

#define IO_MAX_ENTRIES      2048
#define IO_KEY_MAX_LEN      96

typedef struct {
    char key[IO_KEY_MAX_LEN];   /* Unique key: "sec:fld:addr" or "sec:fld:idx" */
    char sec[32];               /* Section: port, switch, slot */
    char fld[32];               /* Field: read, write, type, etc. */
    char addr[16];              /* Address (hex) */
    char val[256];              /* Value */
    int  idx;                   /* Index (-1 if not used) */
    long ts;                    /* Timestamp */
    int  order;                 /* Insertion order for display */
} IOEntry;

typedef struct {
    IOEntry entries[IO_MAX_ENTRIES];
    int     count;              /* Current number of unique entries */
    int     next_order;         /* Next order number */
} IOData;

/*
 * =============================================================================
 * Tab 3: CPU Data (cpu category)
 * =============================================================================
 */

#define CPU_MAX_ENTRIES     64
#define CPU_KEY_MAX_LEN     64

typedef struct {
    char key[CPU_KEY_MAX_LEN];  /* Unique key: "sec:fld" */
    char sec[32];               /* Section: reg, flag, int, state */
    char fld[32];               /* Field name */
    char val[64];               /* Value */
} CPUEntry;

typedef struct {
    CPUEntry entries[CPU_MAX_ENTRIES];
    int      count;

    /* Quick access to common registers (indices into entries, -1 if not set) */
    int idx_pc;
    int idx_sp;
    int idx_a;      /* 6502: A, Z80: AF */
    int idx_x;      /* 6502: X, Z80: BC */
    int idx_y;      /* 6502: Y, Z80: DE */
                    /* Z80: HL, IX, IY stored normally */
} CPUData;

/*
 * =============================================================================
 * Tab 4: Memory Data (mem category)
 * =============================================================================
 */

#define MEM_BYTES_PER_LINE  16
#define MEM_INITIAL_CAPACITY 256

typedef struct {
    uint32_t address;                   /* Start address of this line */
    uint8_t  data[MEM_BYTES_PER_LINE];  /* 16 bytes per line */
    uint8_t  valid[MEM_BYTES_PER_LINE]; /* Which bytes are valid */
    int      valid_count;               /* Number of valid bytes */
} MemLine;

typedef struct {
    MemLine *lines;             /* Dynamic array of memory lines */
    int      count;             /* Number of lines */
    int      capacity;          /* Allocated capacity */
    int      scroll_pos;        /* Current scroll position (line index) */
    uint32_t min_addr;          /* Minimum address seen */
    uint32_t max_addr;          /* Maximum address seen */
} MemoryData;

/*
 * =============================================================================
 * AppleWin Extended Data (V01.1 - 65501-65504 compatibility)
 * =============================================================================
 */

/* Zero Page data (mem.zp) - $0000-$00FF */
#define ZP_SIZE 256
typedef struct {
    uint8_t  data[ZP_SIZE];
    uint8_t  valid[ZP_SIZE];
    bool     has_data;
} ZeroPageData;

/* Stack Page data (mem.stackpage) - $0100-$01FF */
#define STACK_PAGE_SIZE 256
typedef struct {
    uint8_t  data[STACK_PAGE_SIZE];
    uint8_t  valid[STACK_PAGE_SIZE];
    bool     has_data;
} StackPageData;

/* Memory flags (mem.flag) - AppleWin soft switches state */
#define MEM_FLAG_MAX 16
typedef struct {
    char name[32];
    char value[8];
} MemFlagEntry;

typedef struct {
    MemFlagEntry flags[MEM_FLAG_MAX];
    int count;
} MemFlagsData;

/* Text screen data (mem.text) - 24 rows x 40 columns */
#define TEXT_ROWS 24
#define TEXT_COLS 40
typedef struct {
    char rows[TEXT_ROWS][TEXT_COLS + 1];  /* +1 for null terminator */
    uint16_t row_addr[TEXT_ROWS];          /* Address of each row */
    uint8_t row_valid[TEXT_ROWS];          /* Which rows are valid */
    int current_page;                       /* Text page (1 or 2) */
    bool has_data;
} TextScreenData;

/* Annunciator data (io.ann) - 4 annunciators */
#define ANN_COUNT 4
typedef struct {
    uint8_t state[ANN_COUNT];   /* 0 or 1 for each annunciator */
    bool has_data;
} AnnunciatorData;

/* Disassembly data (dbg.disasm) */
#define DISASM_MAX_LINES 64
typedef struct {
    char instruction[64];
    char address[8];
    int idx;
} DisasmLine;

typedef struct {
    DisasmLine lines[DISASM_MAX_LINES];
    int count;
    int scroll_pos;
} DisasmData;

/* CPU Stack data (cpu.stack) */
#define CPU_STACK_MAX_ENTRIES 32
typedef struct {
    char sp[8];                              /* Stack pointer value */
    char depth[8];                           /* Stack depth */
    struct {
        char val[8];
        char addr[8];
    } entries[CPU_STACK_MAX_ENTRIES];
    int entry_count;
    bool has_data;
} CPUStackData;

/*
 * =============================================================================
 * Global Data Store
 * =============================================================================
 */

typedef struct {
    InfoData   info;
    IOData     io;
    CPUData    cpu;
    MemoryData memory;

    /* AppleWin Extended Data (V01.1) */
    ZeroPageData   zeropage;
    StackPageData  stackpage;
    MemFlagsData   memflags;
    TextScreenData textscreen;
    AnnunciatorData annunciator;
    DisasmData     disasm;
    CPUStackData   cpustack;

    /* Statistics */
    unsigned long total_messages;
    unsigned long parse_errors;
} DataStore;

/*
 * =============================================================================
 * Function Declarations
 * =============================================================================
 */

/*
 * Initialize the data store
 */
void datastore_init(DataStore *ds);

/*
 * Free all allocated memory in the data store
 */
void datastore_free(DataStore *ds);

/*
 * Process a parsed data message and store it in the appropriate tab
 */
void datastore_process(DataStore *ds, const ParsedData *data);

/*
 * Clear all data (but keep structure)
 */
void datastore_clear(DataStore *ds);

/*
 * -----------------------------------------------------------------------------
 * Info Tab Functions
 * -----------------------------------------------------------------------------
 */
void datastore_info_clear(InfoData *info);
const InfoData *datastore_get_info(const DataStore *ds);

/*
 * -----------------------------------------------------------------------------
 * IO Tab Functions
 * -----------------------------------------------------------------------------
 */
void datastore_io_clear(IOData *io);
const IOData *datastore_get_io(const DataStore *ds);

/* Get entries sorted by order (most recent last) */
int datastore_io_get_sorted(const IOData *io, const IOEntry **out_entries, int max_entries);

/*
 * -----------------------------------------------------------------------------
 * CPU Tab Functions
 * -----------------------------------------------------------------------------
 */
void datastore_cpu_clear(CPUData *cpu);
const CPUData *datastore_get_cpu(const DataStore *ds);

/* Get entry by section and field */
const CPUEntry *datastore_cpu_get_entry(const CPUData *cpu, const char *sec, const char *fld);

/* Get register value by name (returns NULL if not found) */
const char *datastore_cpu_get_reg(const CPUData *cpu, const char *reg_name);

/* Get flag value by name (returns NULL if not found) */
const char *datastore_cpu_get_flag(const CPUData *cpu, const char *flag_name);

/*
 * -----------------------------------------------------------------------------
 * Memory Tab Functions
 * -----------------------------------------------------------------------------
 */
void datastore_memory_clear(MemoryData *mem);
const MemoryData *datastore_get_memory(const DataStore *ds);

/* Memory navigation */
void datastore_memory_scroll_up(MemoryData *mem, int lines);
void datastore_memory_scroll_down(MemoryData *mem, int lines, int visible_lines);
void datastore_memory_scroll_page_up(MemoryData *mem, int page_size);
void datastore_memory_scroll_page_down(MemoryData *mem, int page_size, int visible_lines);
void datastore_memory_scroll_home(MemoryData *mem);
void datastore_memory_scroll_end(MemoryData *mem, int visible_lines);

/* Get memory line at specific index */
const MemLine *datastore_memory_get_line(const MemoryData *mem, int index);

/* Find line index for a specific address (returns -1 if not found) */
int datastore_memory_find_addr(const MemoryData *mem, uint32_t addr);

/*
 * -----------------------------------------------------------------------------
 * AppleWin Extended Data Functions (V01.1)
 * -----------------------------------------------------------------------------
 */

/* Zero Page */
void datastore_zeropage_clear(ZeroPageData *zp);
const ZeroPageData *datastore_get_zeropage(const DataStore *ds);

/* Stack Page */
void datastore_stackpage_clear(StackPageData *sp);
const StackPageData *datastore_get_stackpage(const DataStore *ds);

/* Memory Flags */
void datastore_memflags_clear(MemFlagsData *mf);
const MemFlagsData *datastore_get_memflags(const DataStore *ds);

/* Text Screen */
void datastore_textscreen_clear(TextScreenData *ts);
const TextScreenData *datastore_get_textscreen(const DataStore *ds);

/* Annunciator */
void datastore_annunciator_clear(AnnunciatorData *ann);
const AnnunciatorData *datastore_get_annunciator(const DataStore *ds);

/* Disassembly */
void datastore_disasm_clear(DisasmData *dis);
const DisasmData *datastore_get_disasm(const DataStore *ds);

/* CPU Stack */
void datastore_cpustack_clear(CPUStackData *cs);
const CPUStackData *datastore_get_cpustack(const DataStore *ds);

#endif /* DATA_STORE_H */
