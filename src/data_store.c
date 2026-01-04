/*
 * data_store.c - Data storage implementation
 */

#include "data_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * =============================================================================
 * Helper Functions
 * =============================================================================
 */

/* Safe string copy using snprintf to avoid truncation warnings */
static void safe_strcpy(char *dest, size_t dest_size, const char *src)
{
    if (dest_size == 0) return;
    if (src == NULL) {
        dest[0] = '\0';
        return;
    }
    snprintf(dest, dest_size, "%s", src);
}

/* Parse hex string to uint32_t */
static uint32_t parse_hex(const char *hex)
{
    if (hex == NULL || hex[0] == '\0') return 0;
    return (uint32_t)strtoul(hex, NULL, 16);
}

/* Parse hex data string to bytes */
static int parse_hex_data(const char *hex_str, uint8_t *out, int max_bytes)
{
    if (hex_str == NULL) return 0;

    int len = strlen(hex_str);
    int bytes = 0;

    for (int i = 0; i < len && bytes < max_bytes; i += 2) {
        if (!isxdigit(hex_str[i])) break;
        if (i + 1 >= len || !isxdigit(hex_str[i + 1])) break;

        char byte_str[3] = { hex_str[i], hex_str[i + 1], '\0' };
        out[bytes++] = (uint8_t)strtoul(byte_str, NULL, 16);
    }

    return bytes;
}

/*
 * =============================================================================
 * Info Data Functions
 * =============================================================================
 */

void datastore_info_clear(InfoData *info)
{
    memset(info, 0, sizeof(InfoData));
    info->is_connected = false;
    info->has_data = false;
}

static void process_info_sys(InfoData *info, const ParsedData *data)
{
    /* sys.conn - connection messages */
    if (strcmp(data->sec, "conn") == 0) {
        if (strcmp(data->fld, "hello") == 0) {
            safe_strcpy(info->emu_version, sizeof(info->emu_version), data->val);
            if (data->has_ver) {
                safe_strcpy(info->protocol_ver, sizeof(info->protocol_ver), data->ver);
            }
            info->connected_time = time(NULL);
            info->is_connected = true;
            info->has_data = true;
        }
        else if (strcmp(data->fld, "goodbye") == 0) {
            info->is_connected = false;
        }
    }
}

static void process_info_mach(InfoData *info, const ParsedData *data)
{
    /* mach.info - machine information */
    if (strcmp(data->sec, "info") == 0) {
        if (strcmp(data->fld, "id") == 0) {
            safe_strcpy(info->machine_id, sizeof(info->machine_id), data->val);
        }
        else if (strcmp(data->fld, "name") == 0) {
            safe_strcpy(info->machine_name, sizeof(info->machine_name), data->val);
        }
        else if (strcmp(data->fld, "type") == 0) {
            safe_strcpy(info->machine_type, sizeof(info->machine_type), data->val);
        }
        info->has_data = true;
    }
    /* mach.status - machine status */
    else if (strcmp(data->sec, "status") == 0) {
        if (strcmp(data->fld, "mode") == 0) {
            safe_strcpy(info->status_mode, sizeof(info->status_mode), data->val);
        }
        else if (strcmp(data->fld, "powered") == 0) {
            safe_strcpy(info->status_powered, sizeof(info->status_powered), data->val);
        }
        info->has_data = true;
    }
    /* mach.video - video mode */
    else if (strcmp(data->sec, "video") == 0) {
        if (strcmp(data->fld, "mode") == 0) {
            safe_strcpy(info->video_mode, sizeof(info->video_mode), data->val);
        }
        else if (strcmp(data->fld, "page") == 0) {
            safe_strcpy(info->video_page, sizeof(info->video_page), data->val);
        }
        else if (strcmp(data->fld, "mixed") == 0) {
            safe_strcpy(info->video_mixed, sizeof(info->video_mixed), data->val);
        }
        info->has_data = true;
    }
}

static void process_info_cpu_state(InfoData *info, const ParsedData *data)
{
    /* cpu.state - CPU state info (for Info tab) */
    if (strcmp(data->sec, "state") == 0) {
        if (strcmp(data->fld, "type") == 0) {
            safe_strcpy(info->cpu_type, sizeof(info->cpu_type), data->val);
        }
        else if (strcmp(data->fld, "cycles") == 0) {
            safe_strcpy(info->cpu_cycles, sizeof(info->cpu_cycles), data->val);
        }
        info->has_data = true;
    }
}

/*
 * =============================================================================
 * IO Data Functions
 * =============================================================================
 */

void datastore_io_clear(IOData *io)
{
    memset(io, 0, sizeof(IOData));
    io->count = 0;
    io->next_order = 0;
}

static void make_io_key(char *key, size_t key_size, const ParsedData *data)
{
    if (data->has_addr) {
        snprintf(key, key_size, "%s:%s:%s", data->sec, data->fld, data->addr);
    }
    else if (data->has_idx) {
        snprintf(key, key_size, "%s:%s:%d", data->sec, data->fld, data->idx);
    }
    else {
        snprintf(key, key_size, "%s:%s", data->sec, data->fld);
    }
}

static int find_io_entry(const IOData *io, const char *key)
{
    for (int i = 0; i < io->count; i++) {
        if (strcmp(io->entries[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

static void process_io(IOData *io, const ParsedData *data)
{
    char key[IO_KEY_MAX_LEN];
    make_io_key(key, sizeof(key), data);

    int idx = find_io_entry(io, key);

    if (idx >= 0) {
        /* Update existing entry */
        IOEntry *entry = &io->entries[idx];
        safe_strcpy(entry->val, sizeof(entry->val), data->val);
        entry->ts = data->ts;
        entry->order = io->next_order++;
    }
    else {
        /* Add new entry */
        if (io->count < IO_MAX_ENTRIES) {
            IOEntry *entry = &io->entries[io->count];
            safe_strcpy(entry->key, sizeof(entry->key), key);
            safe_strcpy(entry->sec, sizeof(entry->sec), data->sec);
            safe_strcpy(entry->fld, sizeof(entry->fld), data->fld);
            safe_strcpy(entry->addr, sizeof(entry->addr), data->addr);
            safe_strcpy(entry->val, sizeof(entry->val), data->val);
            entry->idx = data->has_idx ? data->idx : -1;
            entry->ts = data->ts;
            entry->order = io->next_order++;
            io->count++;
        }
        else {
            /* Buffer full - find and replace oldest entry */
            int oldest_idx = 0;
            int oldest_order = io->entries[0].order;
            for (int i = 1; i < io->count; i++) {
                if (io->entries[i].order < oldest_order) {
                    oldest_order = io->entries[i].order;
                    oldest_idx = i;
                }
            }
            IOEntry *entry = &io->entries[oldest_idx];
            safe_strcpy(entry->key, sizeof(entry->key), key);
            safe_strcpy(entry->sec, sizeof(entry->sec), data->sec);
            safe_strcpy(entry->fld, sizeof(entry->fld), data->fld);
            safe_strcpy(entry->addr, sizeof(entry->addr), data->addr);
            safe_strcpy(entry->val, sizeof(entry->val), data->val);
            entry->idx = data->has_idx ? data->idx : -1;
            entry->ts = data->ts;
            entry->order = io->next_order++;
        }
    }
}

/* Comparison function for sorting by order */
static int compare_io_by_order(const void *a, const void *b)
{
    const IOEntry *ea = *(const IOEntry **)a;
    const IOEntry *eb = *(const IOEntry **)b;
    return ea->order - eb->order;
}

int datastore_io_get_sorted(const IOData *io, const IOEntry **out_entries, int max_entries)
{
    int count = io->count < max_entries ? io->count : max_entries;

    /* Create array of pointers */
    for (int i = 0; i < count; i++) {
        out_entries[i] = &io->entries[i];
    }

    /* Sort by order */
    qsort((void *)out_entries, count, sizeof(IOEntry *), compare_io_by_order);

    return count;
}

/*
 * =============================================================================
 * CPU Data Functions
 * =============================================================================
 */

void datastore_cpu_clear(CPUData *cpu)
{
    memset(cpu, 0, sizeof(CPUData));
    cpu->count = 0;
    cpu->idx_pc = -1;
    cpu->idx_sp = -1;
    cpu->idx_a = -1;
    cpu->idx_x = -1;
    cpu->idx_y = -1;
}

static void make_cpu_key(char *key, size_t key_size, const char *sec, const char *fld)
{
    snprintf(key, key_size, "%s:%s", sec, fld);
}

static int find_cpu_entry(const CPUData *cpu, const char *key)
{
    for (int i = 0; i < cpu->count; i++) {
        if (strcmp(cpu->entries[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

static void update_cpu_quick_access(CPUData *cpu, int idx, const char *fld)
{
    /* Update quick access indices for common registers */
    if (strcmp(fld, "pc") == 0) cpu->idx_pc = idx;
    else if (strcmp(fld, "sp") == 0) cpu->idx_sp = idx;
    else if (strcmp(fld, "a") == 0 || strcmp(fld, "af") == 0) cpu->idx_a = idx;
    else if (strcmp(fld, "x") == 0 || strcmp(fld, "bc") == 0) cpu->idx_x = idx;
    else if (strcmp(fld, "y") == 0 || strcmp(fld, "de") == 0) cpu->idx_y = idx;
}

static void process_cpu(CPUData *cpu, const ParsedData *data)
{
    char key[CPU_KEY_MAX_LEN];
    make_cpu_key(key, sizeof(key), data->sec, data->fld);

    int idx = find_cpu_entry(cpu, key);

    if (idx >= 0) {
        /* Update existing entry */
        safe_strcpy(cpu->entries[idx].val, sizeof(cpu->entries[idx].val), data->val);
    }
    else if (cpu->count < CPU_MAX_ENTRIES) {
        /* Add new entry */
        idx = cpu->count;
        CPUEntry *entry = &cpu->entries[idx];
        safe_strcpy(entry->key, sizeof(entry->key), key);
        safe_strcpy(entry->sec, sizeof(entry->sec), data->sec);
        safe_strcpy(entry->fld, sizeof(entry->fld), data->fld);
        safe_strcpy(entry->val, sizeof(entry->val), data->val);
        cpu->count++;
    }

    if (idx >= 0) {
        update_cpu_quick_access(cpu, idx, data->fld);
    }
}

const CPUEntry *datastore_cpu_get_entry(const CPUData *cpu, const char *sec, const char *fld)
{
    char key[CPU_KEY_MAX_LEN];
    make_cpu_key(key, sizeof(key), sec, fld);

    int idx = find_cpu_entry(cpu, key);
    if (idx >= 0) {
        return &cpu->entries[idx];
    }
    return NULL;
}

const char *datastore_cpu_get_reg(const CPUData *cpu, const char *reg_name)
{
    const CPUEntry *entry = datastore_cpu_get_entry(cpu, "reg", reg_name);
    return entry ? entry->val : NULL;
}

const char *datastore_cpu_get_flag(const CPUData *cpu, const char *flag_name)
{
    const CPUEntry *entry = datastore_cpu_get_entry(cpu, "flag", flag_name);
    return entry ? entry->val : NULL;
}

/*
 * =============================================================================
 * Memory Data Functions
 * =============================================================================
 */

void datastore_memory_clear(MemoryData *mem)
{
    if (mem->lines != NULL) {
        free(mem->lines);
        mem->lines = NULL;
    }
    mem->count = 0;
    mem->capacity = 0;
    mem->scroll_pos = 0;
    mem->min_addr = 0xFFFFFFFF;
    mem->max_addr = 0;
}

static int ensure_memory_capacity(MemoryData *mem, int needed)
{
    if (needed <= mem->capacity) {
        return 0;
    }

    int new_capacity = mem->capacity == 0 ? MEM_INITIAL_CAPACITY : mem->capacity * 2;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }

    MemLine *new_lines = realloc(mem->lines, new_capacity * sizeof(MemLine));
    if (new_lines == NULL) {
        return -1;
    }

    mem->lines = new_lines;
    mem->capacity = new_capacity;
    return 0;
}

/* Find or create a memory line for the given address */
static int find_or_create_mem_line(MemoryData *mem, uint32_t line_addr)
{
    /* Binary search for existing line */
    int low = 0, high = mem->count - 1;
    while (low <= high) {
        int mid = (low + high) / 2;
        if (mem->lines[mid].address == line_addr) {
            return mid;
        }
        if (mem->lines[mid].address < line_addr) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    /* Not found, insert at position 'low' */
    if (ensure_memory_capacity(mem, mem->count + 1) < 0) {
        return -1;
    }

    /* Shift elements to make room */
    if (low < mem->count) {
        memmove(&mem->lines[low + 1], &mem->lines[low],
                (mem->count - low) * sizeof(MemLine));
    }

    /* Initialize new line */
    MemLine *line = &mem->lines[low];
    memset(line, 0, sizeof(MemLine));
    line->address = line_addr;
    mem->count++;

    return low;
}

static void process_memory_dump(MemoryData *mem, const ParsedData *data)
{
    if (!data->has_addr) return;

    uint32_t addr = parse_hex(data->addr);
    uint8_t bytes[256];
    int byte_count = parse_hex_data(data->val, bytes, sizeof(bytes));

    if (byte_count == 0) return;

    /* Update address range */
    if (addr < mem->min_addr) mem->min_addr = addr;
    if (addr + byte_count - 1 > mem->max_addr) mem->max_addr = addr + byte_count - 1;

    /* Process each byte */
    for (int i = 0; i < byte_count; i++) {
        uint32_t byte_addr = addr + i;
        uint32_t line_addr = (byte_addr / MEM_BYTES_PER_LINE) * MEM_BYTES_PER_LINE;
        int offset = byte_addr % MEM_BYTES_PER_LINE;

        int line_idx = find_or_create_mem_line(mem, line_addr);
        if (line_idx < 0) continue;

        MemLine *line = &mem->lines[line_idx];
        line->data[offset] = bytes[i];
        if (!line->valid[offset]) {
            line->valid[offset] = 1;
            line->valid_count++;
        }
    }
}

static void process_memory_byte(MemoryData *mem, const ParsedData *data)
{
    if (!data->has_addr) return;

    uint32_t addr = parse_hex(data->addr);
    uint8_t byte_val = (uint8_t)parse_hex(data->val);

    /* Update address range */
    if (addr < mem->min_addr) mem->min_addr = addr;
    if (addr > mem->max_addr) mem->max_addr = addr;

    uint32_t line_addr = (addr / MEM_BYTES_PER_LINE) * MEM_BYTES_PER_LINE;
    int offset = addr % MEM_BYTES_PER_LINE;

    int line_idx = find_or_create_mem_line(mem, line_addr);
    if (line_idx < 0) return;

    MemLine *line = &mem->lines[line_idx];
    line->data[offset] = byte_val;
    if (!line->valid[offset]) {
        line->valid[offset] = 1;
        line->valid_count++;
    }
}

/* Helper function to parse space-separated hex data (e.g., "00 00 C6 00") */
static int parse_hex_data_spaced(const char *hex_str, uint8_t *out, int max_bytes)
{
    if (hex_str == NULL) return 0;

    int bytes = 0;
    const char *p = hex_str;

    while (*p && bytes < max_bytes) {
        /* Skip whitespace */
        while (*p == ' ') p++;
        if (!*p) break;

        /* Read two hex digits */
        if (isxdigit(p[0]) && isxdigit(p[1])) {
            char byte_str[3] = { p[0], p[1], '\0' };
            out[bytes++] = (uint8_t)strtoul(byte_str, NULL, 16);
            p += 2;
        } else {
            break;
        }
    }

    return bytes;
}

/* Process Zero Page data (mem.zp) */
static void process_zeropage(ZeroPageData *zp, const ParsedData *data)
{
    if (strcmp(data->fld, "data") != 0) return;
    if (!data->has_addr) return;

    uint32_t addr = parse_hex(data->addr);
    if (addr >= ZP_SIZE) return;

    uint8_t bytes[32];
    int byte_count = parse_hex_data_spaced(data->val, bytes, sizeof(bytes));

    for (int i = 0; i < byte_count && (addr + i) < ZP_SIZE; i++) {
        zp->data[addr + i] = bytes[i];
        zp->valid[addr + i] = 1;
    }
    zp->has_data = true;
}

/* Process Stack Page data (mem.stackpage) */
static void process_stackpage(StackPageData *sp, const ParsedData *data)
{
    if (strcmp(data->fld, "data") != 0) return;
    if (!data->has_addr) return;

    uint32_t addr = parse_hex(data->addr);
    /* Stack page is at $0100-$01FF, convert to 0-255 offset */
    if (addr >= 0x0100 && addr < 0x0200) {
        addr -= 0x0100;
    } else if (addr >= STACK_PAGE_SIZE) {
        return;
    }

    uint8_t bytes[32];
    int byte_count = parse_hex_data_spaced(data->val, bytes, sizeof(bytes));

    for (int i = 0; i < byte_count && (addr + i) < STACK_PAGE_SIZE; i++) {
        sp->data[addr + i] = bytes[i];
        sp->valid[addr + i] = 1;
    }
    sp->has_data = true;
}

/* Process Memory Flags (mem.flag) */
static void process_memflags(MemFlagsData *mf, const ParsedData *data)
{
    /* Find existing flag or add new one */
    int idx = -1;
    for (int i = 0; i < mf->count; i++) {
        if (strcmp(mf->flags[i].name, data->fld) == 0) {
            idx = i;
            break;
        }
    }

    if (idx < 0 && mf->count < MEM_FLAG_MAX) {
        idx = mf->count++;
        safe_strcpy(mf->flags[idx].name, sizeof(mf->flags[idx].name), data->fld);
    }

    if (idx >= 0) {
        safe_strcpy(mf->flags[idx].value, sizeof(mf->flags[idx].value), data->val);
    }
}

/* Process Text Screen data (mem.text) */
static void process_textscreen(TextScreenData *ts, const ParsedData *data)
{
    if (strcmp(data->fld, "page") == 0) {
        ts->current_page = atoi(data->val);
        ts->has_data = true;
    }
    else if (strcmp(data->fld, "row") == 0) {
        if (!data->has_idx) return;

        int row_idx = data->idx;
        if (row_idx >= 0 && row_idx < TEXT_ROWS) {
            /* Copy the row text, up to TEXT_COLS characters */
            safe_strcpy(ts->rows[row_idx], TEXT_COLS + 1, data->val);

            if (data->has_addr) {
                ts->row_addr[row_idx] = (uint16_t)parse_hex(data->addr);
            }
            ts->row_valid[row_idx] = 1;
            ts->has_data = true;
        }
    }
}

/* Process Annunciator data (io.ann) */
static void process_annunciator(AnnunciatorData *ann, const ParsedData *data)
{
    if (strcmp(data->fld, "state") != 0) return;
    if (!data->has_idx) return;

    int idx = data->idx;
    if (idx >= 0 && idx < ANN_COUNT) {
        ann->state[idx] = (uint8_t)atoi(data->val);
        ann->has_data = true;
    }
}

/* Process Disassembly data (dbg.disasm) */
static void process_disasm(DisasmData *dis, const ParsedData *data)
{
    if (strcmp(data->fld, "line") != 0) return;
    if (!data->has_idx) return;

    int idx = data->idx;
    if (idx >= 0 && idx < DISASM_MAX_LINES) {
        safe_strcpy(dis->lines[idx].instruction, sizeof(dis->lines[idx].instruction), data->val);

        if (data->has_addr) {
            safe_strcpy(dis->lines[idx].address, sizeof(dis->lines[idx].address), data->addr);
        }
        dis->lines[idx].idx = idx;

        if (idx >= dis->count) {
            dis->count = idx + 1;
        }
    }
}

/* Process CPU Stack data (cpu.stack) */
static void process_cpustack(CPUStackData *cs, const ParsedData *data)
{
    if (strcmp(data->fld, "sp") == 0) {
        safe_strcpy(cs->sp, sizeof(cs->sp), data->val);
        cs->has_data = true;
    }
    else if (strcmp(data->fld, "depth") == 0) {
        safe_strcpy(cs->depth, sizeof(cs->depth), data->val);
        cs->has_data = true;
    }
    else if (strcmp(data->fld, "val") == 0) {
        if (!data->has_idx) return;

        int idx = data->idx;
        if (idx >= 0 && idx < CPU_STACK_MAX_ENTRIES) {
            safe_strcpy(cs->entries[idx].val, sizeof(cs->entries[idx].val), data->val);

            if (data->has_addr) {
                safe_strcpy(cs->entries[idx].addr, sizeof(cs->entries[idx].addr), data->addr);
            }

            if (idx >= cs->entry_count) {
                cs->entry_count = idx + 1;
            }
            cs->has_data = true;
        }
    }
}

static void process_memory(MemoryData *mem, const ParsedData *data)
{
    if (strcmp(data->sec, "dump") == 0) {
        process_memory_dump(mem, data);
    }
    else if (strcmp(data->sec, "read") == 0 || strcmp(data->sec, "write") == 0) {
        if (strcmp(data->fld, "byte") == 0) {
            process_memory_byte(mem, data);
        }
        /* word reads could be handled similarly */
    }
    /* Note: zp, stackpage, text, flag are handled in datastore_process */
}

/* Memory navigation functions */
void datastore_memory_scroll_up(MemoryData *mem, int lines)
{
    mem->scroll_pos -= lines;
    if (mem->scroll_pos < 0) {
        mem->scroll_pos = 0;
    }
}

void datastore_memory_scroll_down(MemoryData *mem, int lines, int visible_lines)
{
    mem->scroll_pos += lines;
    int max_pos = mem->count - visible_lines;
    if (max_pos < 0) max_pos = 0;
    if (mem->scroll_pos > max_pos) {
        mem->scroll_pos = max_pos;
    }
}

void datastore_memory_scroll_page_up(MemoryData *mem, int page_size)
{
    datastore_memory_scroll_up(mem, page_size);
}

void datastore_memory_scroll_page_down(MemoryData *mem, int page_size, int visible_lines)
{
    datastore_memory_scroll_down(mem, page_size, visible_lines);
}

void datastore_memory_scroll_home(MemoryData *mem)
{
    mem->scroll_pos = 0;
}

void datastore_memory_scroll_end(MemoryData *mem, int visible_lines)
{
    mem->scroll_pos = mem->count - visible_lines;
    if (mem->scroll_pos < 0) {
        mem->scroll_pos = 0;
    }
}

const MemLine *datastore_memory_get_line(const MemoryData *mem, int index)
{
    if (index < 0 || index >= mem->count) {
        return NULL;
    }
    return &mem->lines[index];
}

int datastore_memory_find_addr(const MemoryData *mem, uint32_t addr)
{
    uint32_t line_addr = (addr / MEM_BYTES_PER_LINE) * MEM_BYTES_PER_LINE;

    /* Binary search */
    int low = 0, high = mem->count - 1;
    while (low <= high) {
        int mid = (low + high) / 2;
        if (mem->lines[mid].address == line_addr) {
            return mid;
        }
        if (mem->lines[mid].address < line_addr) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return -1;
}

/*
 * =============================================================================
 * Main DataStore Functions
 * =============================================================================
 */

void datastore_init(DataStore *ds)
{
    memset(ds, 0, sizeof(DataStore));
    datastore_info_clear(&ds->info);
    datastore_io_clear(&ds->io);
    datastore_cpu_clear(&ds->cpu);
    /* Memory is already zeroed */

    /* AppleWin Extended Data (V01.1) */
    datastore_zeropage_clear(&ds->zeropage);
    datastore_stackpage_clear(&ds->stackpage);
    datastore_memflags_clear(&ds->memflags);
    datastore_textscreen_clear(&ds->textscreen);
    datastore_annunciator_clear(&ds->annunciator);
    datastore_disasm_clear(&ds->disasm);
    datastore_cpustack_clear(&ds->cpustack);
}

void datastore_free(DataStore *ds)
{
    datastore_memory_clear(&ds->memory);
}

void datastore_clear(DataStore *ds)
{
    datastore_info_clear(&ds->info);
    datastore_io_clear(&ds->io);
    datastore_cpu_clear(&ds->cpu);
    datastore_memory_clear(&ds->memory);

    /* AppleWin Extended Data (V01.1) */
    datastore_zeropage_clear(&ds->zeropage);
    datastore_stackpage_clear(&ds->stackpage);
    datastore_memflags_clear(&ds->memflags);
    datastore_textscreen_clear(&ds->textscreen);
    datastore_annunciator_clear(&ds->annunciator);
    datastore_disasm_clear(&ds->disasm);
    datastore_cpustack_clear(&ds->cpustack);

    ds->total_messages = 0;
    ds->parse_errors = 0;
    ds->io_messages = 0;
    ds->cpu_messages = 0;
    ds->mem_messages = 0;
    ds->text_messages = 0;
}

void datastore_process(DataStore *ds, const ParsedData *data)
{
    ds->total_messages++;

    /* Store emulator type in info */
    safe_strcpy(ds->info.emu_type, sizeof(ds->info.emu_type), data->emu);

    /* Route to appropriate handler based on category */
    if (strcmp(data->cat, "sys") == 0) {
        process_info_sys(&ds->info, data);
    }
    else if (strcmp(data->cat, "mach") == 0) {
        process_info_mach(&ds->info, data);
    }
    else if (strcmp(data->cat, "cpu") == 0) {
        /* CPU state goes to both info and cpu tabs */
        process_info_cpu_state(&ds->info, data);
        process_cpu(&ds->cpu, data);
        ds->cpu_messages++;

        /* V01.1: Handle cpu.stack section */
        if (strcmp(data->sec, "stack") == 0) {
            process_cpustack(&ds->cpustack, data);
        }
    }
    else if (strcmp(data->cat, "io") == 0) {
        process_io(&ds->io, data);
        ds->io_messages++;

        /* V01.1: Handle io.ann section (annunciators) */
        if (strcmp(data->sec, "ann") == 0) {
            process_annunciator(&ds->annunciator, data);
        }
    }
    else if (strcmp(data->cat, "mem") == 0) {
        /* V01.1: Route to extended memory handlers */
        if (strcmp(data->sec, "zp") == 0) {
            process_zeropage(&ds->zeropage, data);
            ds->mem_messages++;
        }
        else if (strcmp(data->sec, "stackpage") == 0) {
            process_stackpage(&ds->stackpage, data);
            ds->mem_messages++;
        }
        else if (strcmp(data->sec, "flag") == 0) {
            process_memflags(&ds->memflags, data);
            ds->mem_messages++;
        }
        else if (strcmp(data->sec, "text") == 0) {
            process_textscreen(&ds->textscreen, data);
            ds->text_messages++;
        }
        else {
            /* dump, read, write sections go to memory tab */
            process_memory(&ds->memory, data);
            ds->mem_messages++;
        }
    }
    else if (strcmp(data->cat, "dbg") == 0) {
        /* V01.1: Handle dbg.disasm section */
        if (strcmp(data->sec, "disasm") == 0) {
            process_disasm(&ds->disasm, data);
        }
        /* Other dbg sections (bp, watch, trace) can be added here */
    }
}

const InfoData *datastore_get_info(const DataStore *ds)
{
    return &ds->info;
}

const IOData *datastore_get_io(const DataStore *ds)
{
    return &ds->io;
}

const CPUData *datastore_get_cpu(const DataStore *ds)
{
    return &ds->cpu;
}

const MemoryData *datastore_get_memory(const DataStore *ds)
{
    return &ds->memory;
}

/*
 * =============================================================================
 * AppleWin Extended Data Functions (V01.1)
 * =============================================================================
 */

/* Zero Page Functions */
void datastore_zeropage_clear(ZeroPageData *zp)
{
    memset(zp, 0, sizeof(ZeroPageData));
}

const ZeroPageData *datastore_get_zeropage(const DataStore *ds)
{
    return &ds->zeropage;
}

/* Stack Page Functions */
void datastore_stackpage_clear(StackPageData *sp)
{
    memset(sp, 0, sizeof(StackPageData));
}

const StackPageData *datastore_get_stackpage(const DataStore *ds)
{
    return &ds->stackpage;
}

/* Memory Flags Functions */
void datastore_memflags_clear(MemFlagsData *mf)
{
    memset(mf, 0, sizeof(MemFlagsData));
}

const MemFlagsData *datastore_get_memflags(const DataStore *ds)
{
    return &ds->memflags;
}

/* Text Screen Functions */
void datastore_textscreen_clear(TextScreenData *ts)
{
    memset(ts, 0, sizeof(TextScreenData));
}

const TextScreenData *datastore_get_textscreen(const DataStore *ds)
{
    return &ds->textscreen;
}

/* Annunciator Functions */
void datastore_annunciator_clear(AnnunciatorData *ann)
{
    memset(ann, 0, sizeof(AnnunciatorData));
}

const AnnunciatorData *datastore_get_annunciator(const DataStore *ds)
{
    return &ds->annunciator;
}

/* Disassembly Functions */
void datastore_disasm_clear(DisasmData *dis)
{
    memset(dis, 0, sizeof(DisasmData));
}

const DisasmData *datastore_get_disasm(const DataStore *ds)
{
    return &ds->disasm;
}

/* CPU Stack Functions */
void datastore_cpustack_clear(CPUStackData *cs)
{
    memset(cs, 0, sizeof(CPUStackData));
}

const CPUStackData *datastore_get_cpustack(const DataStore *ds)
{
    return &ds->cpustack;
}
