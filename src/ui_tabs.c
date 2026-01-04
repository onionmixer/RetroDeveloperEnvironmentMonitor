/*
 * ui_tabs.c - Tab content rendering implementation
 */

#include "ui_tabs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/*
 * =============================================================================
 * Helper Functions
 * =============================================================================
 */

/* Case-insensitive string search */
static const char *strcasestr_local(const char *haystack, const char *needle)
{
    if (!needle[0]) return haystack;

    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;

        while (*h && *n && (tolower((unsigned char)*h) == tolower((unsigned char)*n))) {
            h++;
            n++;
        }

        if (!*n) return haystack;
    }

    return NULL;
}

bool tabs_search_match(const char *text, const char *search_term)
{
    if (!search_term || !search_term[0]) return false;
    if (!text) return false;
    return strcasestr_local(text, search_term) != NULL;
}

void tabs_draw_with_highlight(WINDOW *win, int y, int x,
                              const char *text, const char *search_term)
{
    if (!text) return;

    wmove(win, y, x);

    if (!search_term || !search_term[0]) {
        waddstr(win, text);
        return;
    }

    int term_len = strlen(search_term);
    const char *p = text;
    const char *match;

    while (*p) {
        match = strcasestr_local(p, search_term);
        if (match) {
            /* Print text before match */
            while (p < match) {
                waddch(win, *p++);
            }
            /* Print match with highlight */
            wattron(win, COLOR_PAIR(COLOR_PAIR_SEARCH) | A_REVERSE);
            for (int i = 0; i < term_len && *p; i++) {
                waddch(win, *p++);
            }
            wattroff(win, COLOR_PAIR(COLOR_PAIR_SEARCH) | A_REVERSE);
        } else {
            /* Print remaining text */
            waddstr(win, p);
            break;
        }
    }
}

/* Draw a horizontal separator line */
static void draw_separator(WINDOW *win, int y, int width, const char *title)
{
    wmove(win, y, 0);
    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));

    if (title && title[0]) {
        wprintw(win, "--- %s ", title);
        for (int i = strlen(title) + 5; i < width; i++) {
            waddch(win, '-');
        }
    } else {
        for (int i = 0; i < width; i++) {
            waddch(win, '-');
        }
    }

    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));
}

/*
 * =============================================================================
 * Tab Renderers
 * =============================================================================
 */

void tabs_draw_content(UIContext *ctx)
{
    switch (ctx->current_tab) {
        case TAB_INFO:
            tabs_draw_info(ctx);
            break;
        case TAB_IO:
            tabs_draw_io(ctx);
            break;
        case TAB_CPU:
            tabs_draw_cpu(ctx);
            break;
        case TAB_MEMORY:
            tabs_draw_memory(ctx);
            break;
        case TAB_TEXT:
            tabs_draw_text(ctx);
            break;
        default:
            break;
    }
}

void tabs_draw_info(UIContext *ctx)
{
    WINDOW *win = ctx->win_content;
    const InfoData *info = datastore_get_info(ctx->datastore);
    const MemFlagsData *memflags = datastore_get_memflags(ctx->datastore);
    const char *search = ui_get_search_term(ctx);
    int width, height;
    ui_get_content_size(ctx, &width, &height);

    werase(win);

    int y = 0;

    /* Header */
    draw_separator(win, y++, width, "Basic Information");
    y++;

    /* Emulator info */
    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    mvwprintw(win, y, 2, "Emulator    : ");
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));

    char buf[256];
    snprintf(buf, sizeof(buf), "%s %s",
             info->emu_type[0] ? info->emu_type : "---",
             info->emu_version[0] ? info->emu_version : "");
    tabs_draw_with_highlight(win, y++, 16, buf, search);

    /* Machine info */
    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    mvwprintw(win, y, 2, "Machine ID  : ");
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    tabs_draw_with_highlight(win, y++, 16,
                             info->machine_id[0] ? info->machine_id : "---", search);

    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    mvwprintw(win, y, 2, "Machine Name: ");
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    tabs_draw_with_highlight(win, y++, 16,
                             info->machine_name[0] ? info->machine_name : "---", search);

    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    mvwprintw(win, y, 2, "Machine Type: ");
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    tabs_draw_with_highlight(win, y++, 16,
                             info->machine_type[0] ? info->machine_type : "---", search);

    y++;

    /* CPU info */
    draw_separator(win, y++, width, "CPU");
    y++;

    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    mvwprintw(win, y, 2, "CPU Type    : ");
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    tabs_draw_with_highlight(win, y++, 16,
                             info->cpu_type[0] ? info->cpu_type : "---", search);

    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    mvwprintw(win, y, 2, "Cycles      : ");
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    tabs_draw_with_highlight(win, y++, 16,
                             info->cpu_cycles[0] ? info->cpu_cycles : "---", search);

    y++;

    /* Status */
    draw_separator(win, y++, width, "Status");
    y++;

    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    mvwprintw(win, y, 2, "Mode        : ");
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    tabs_draw_with_highlight(win, y++, 16,
                             info->status_mode[0] ? info->status_mode : "---", search);

    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    mvwprintw(win, y, 2, "Powered     : ");
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    wprintw(win, "%s", info->status_powered[0] ? info->status_powered : "---");
    y++;

    y++;

    /* Video */
    if (info->video_mode[0]) {
        draw_separator(win, y++, width, "Video");
        y++;

        wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
        mvwprintw(win, y, 2, "Video Mode  : ");
        wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));
        tabs_draw_with_highlight(win, y++, 16, info->video_mode, search);
    }

    y++;

    /* Connection time */
    if (info->connected_time > 0) {
        draw_separator(win, y++, width, "Connection");
        y++;

        wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
        mvwprintw(win, y, 2, "Connected   : ");
        wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));

        char time_buf[64];
        struct tm *tm_info = localtime(&info->connected_time);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        wprintw(win, "%s", time_buf);
        y++;
    }

    /* Statistics */
    y++;
    draw_separator(win, y++, width, "Statistics");
    y++;

    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    mvwprintw(win, y, 2, "Messages    : ");
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    wprintw(win, "%lu", ctx->datastore->total_messages);
    y++;

    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    mvwprintw(win, y, 2, "Parse Errors: ");
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    wprintw(win, "%lu", ctx->datastore->parse_errors);
    y++;

    /* V01.1: Memory Flags (AppleWin) */
    if (memflags->count > 0 && y < height - 4) {
        y++;
        draw_separator(win, y++, width, "Memory Flags");
        y++;

        char line[256] = "";
        int line_len = 0;
        for (int i = 0; i < memflags->count && line_len < 200; i++) {
            int added = snprintf(line + line_len, sizeof(line) - line_len,
                                "%s:%s  ", memflags->flags[i].name, memflags->flags[i].value);
            line_len += added;
        }
        if (line_len > 0) {
            tabs_draw_with_highlight(win, y++, 2, line, search);
        }
    }

    wrefresh(win);
}

void tabs_draw_io(UIContext *ctx)
{
    WINDOW *win = ctx->win_content;
    const IOData *io = datastore_get_io(ctx->datastore);
    const AnnunciatorData *ann = datastore_get_annunciator(ctx->datastore);
    const char *search = ui_get_search_term(ctx);
    int width, height;
    ui_get_content_size(ctx, &width, &height);

    werase(win);

    int y = 0;

    /* V01.1: Annunciator section (AppleWin) */
    if (ann->has_data) {
        draw_separator(win, y++, width, "Annunciators");
        y++;

        char line[64];
        snprintf(line, sizeof(line), "ANN0:%d  ANN1:%d  ANN2:%d  ANN3:%d",
                 ann->state[0], ann->state[1], ann->state[2], ann->state[3]);
        tabs_draw_with_highlight(win, y++, 2, line, search);
        y++;
    }

    /* Header */
    char header[64];
    snprintf(header, sizeof(header), "I/O (%d entries)", io->count);
    draw_separator(win, y++, width, header);
    y++;

    if (io->count == 0 && !ann->has_data) {
        mvwprintw(win, y, 2, "No I/O data received yet.");
        wrefresh(win);
        return;
    }

    if (io->count == 0) {
        wrefresh(win);
        return;
    }

    /* Get sorted entries */
    const IOEntry *sorted[IO_MAX_ENTRIES];
    int count = datastore_io_get_sorted(io, sorted, io->count);

    /* Calculate how many lines we can display */
    int display_lines = height - 3;  /* Header + margins */
    int start_idx = count > display_lines ? count - display_lines : 0;

    /* Draw entries (most recent at bottom) */
    for (int i = start_idx; i < count && y < height - 1; i++) {
        const IOEntry *e = sorted[i];

        char line[256];
        if (e->addr[0]) {
            snprintf(line, sizeof(line), "[%-6s] %-8s @%-6s = %s",
                     e->sec, e->fld, e->addr, e->val);
        } else if (e->idx >= 0) {
            snprintf(line, sizeof(line), "[%-6s] %-8s #%-6d = %s",
                     e->sec, e->fld, e->idx, e->val);
        } else {
            snprintf(line, sizeof(line), "[%-6s] %-8s         = %s",
                     e->sec, e->fld, e->val);
        }

        wmove(win, y, 2);
        tabs_draw_with_highlight(win, y++, 2, line, search);
    }

    wrefresh(win);
}

void tabs_draw_cpu(UIContext *ctx)
{
    WINDOW *win = ctx->win_content;
    const CPUData *cpu = datastore_get_cpu(ctx->datastore);
    const InfoData *info = datastore_get_info(ctx->datastore);
    const CPUStackData *cpustack = datastore_get_cpustack(ctx->datastore);
    const DisasmData *disasm = datastore_get_disasm(ctx->datastore);
    const char *search = ui_get_search_term(ctx);
    int width, height;
    ui_get_content_size(ctx, &width, &height);

    werase(win);

    int y = 0;
    bool is_z80 = (strcmp(info->emu_type, "msx") == 0);

    /* Registers section */
    draw_separator(win, y++, width, "Registers");
    y++;

    if (cpu->count == 0) {
        mvwprintw(win, y, 2, "No CPU data received yet.");
        wrefresh(win);
        return;
    }

    /* Draw registers in a formatted layout */
    if (is_z80) {
        /* Z80 layout */
        char line[256];

        /* Main registers */
        const char *af = datastore_cpu_get_reg(cpu, "af");
        const char *bc = datastore_cpu_get_reg(cpu, "bc");
        const char *de = datastore_cpu_get_reg(cpu, "de");
        const char *hl = datastore_cpu_get_reg(cpu, "hl");

        snprintf(line, sizeof(line), "AF: %-6s  BC: %-6s  DE: %-6s  HL: %-6s",
                 af ? af : "----", bc ? bc : "----",
                 de ? de : "----", hl ? hl : "----");
        tabs_draw_with_highlight(win, y++, 2, line, search);

        /* Alternate registers */
        const char *af2 = datastore_cpu_get_reg(cpu, "af2");
        const char *bc2 = datastore_cpu_get_reg(cpu, "bc2");
        const char *de2 = datastore_cpu_get_reg(cpu, "de2");
        const char *hl2 = datastore_cpu_get_reg(cpu, "hl2");

        snprintf(line, sizeof(line), "AF':%-6s  BC':%-6s  DE':%-6s  HL':%-6s",
                 af2 ? af2 : "----", bc2 ? bc2 : "----",
                 de2 ? de2 : "----", hl2 ? hl2 : "----");
        tabs_draw_with_highlight(win, y++, 2, line, search);

        /* Index and pointer registers */
        const char *ix = datastore_cpu_get_reg(cpu, "ix");
        const char *iy = datastore_cpu_get_reg(cpu, "iy");
        const char *sp = datastore_cpu_get_reg(cpu, "sp");
        const char *pc = datastore_cpu_get_reg(cpu, "pc");

        snprintf(line, sizeof(line), "IX: %-6s  IY: %-6s  SP: %-6s  PC: %-6s",
                 ix ? ix : "----", iy ? iy : "----",
                 sp ? sp : "----", pc ? pc : "----");
        tabs_draw_with_highlight(win, y++, 2, line, search);

        /* I and R registers */
        const char *i = datastore_cpu_get_reg(cpu, "i");
        const char *r = datastore_cpu_get_reg(cpu, "r");

        snprintf(line, sizeof(line), "I:  %-6s  R:  %-6s",
                 i ? i : "--", r ? r : "--");
        tabs_draw_with_highlight(win, y++, 2, line, search);

    } else {
        /* 6502 layout */
        char line[256];

        const char *a = datastore_cpu_get_reg(cpu, "a");
        const char *x = datastore_cpu_get_reg(cpu, "x");
        const char *y_reg = datastore_cpu_get_reg(cpu, "y");

        snprintf(line, sizeof(line), "A: %-4s  X: %-4s  Y: %-4s",
                 a ? a : "--", x ? x : "--", y_reg ? y_reg : "--");
        tabs_draw_with_highlight(win, y++, 2, line, search);

        const char *sp = datastore_cpu_get_reg(cpu, "sp");
        const char *pc = datastore_cpu_get_reg(cpu, "pc");
        const char *p = datastore_cpu_get_reg(cpu, "p");

        snprintf(line, sizeof(line), "SP: %-4s  PC: %-6s  P: %-4s",
                 sp ? sp : "--", pc ? pc : "----", p ? p : "--");
        tabs_draw_with_highlight(win, y++, 2, line, search);
    }

    y++;

    /* Flags section */
    draw_separator(win, y++, width, "Flags");
    y++;

    char flags_line[256] = "";
    int flags_len = 0;

    for (int i = 0; i < cpu->count; i++) {
        if (strcmp(cpu->entries[i].sec, "flag") == 0) {
            flags_len += snprintf(flags_line + flags_len, sizeof(flags_line) - flags_len,
                                 "%s:%s  ", cpu->entries[i].fld, cpu->entries[i].val);
        }
    }

    if (flags_len > 0) {
        tabs_draw_with_highlight(win, y++, 2, flags_line, search);
    } else {
        mvwprintw(win, y++, 2, "(no flags)");
    }

    y++;

    /* Interrupt section */
    draw_separator(win, y++, width, "Interrupt");
    y++;

    bool has_int_data = false;
    for (int i = 0; i < cpu->count; i++) {
        if (strcmp(cpu->entries[i].sec, "int") == 0) {
            char line[128];
            snprintf(line, sizeof(line), "%s: %s",
                     cpu->entries[i].fld, cpu->entries[i].val);
            tabs_draw_with_highlight(win, y++, 2, line, search);
            has_int_data = true;
        }
    }
    if (!has_int_data) {
        mvwprintw(win, y++, 2, "(no interrupt data)");
    }

    y++;

    /* State section */
    draw_separator(win, y++, width, "State");
    y++;

    bool has_state_data = false;
    for (int i = 0; i < cpu->count; i++) {
        if (strcmp(cpu->entries[i].sec, "state") == 0) {
            char line[128];
            snprintf(line, sizeof(line), "%s: %s",
                     cpu->entries[i].fld, cpu->entries[i].val);
            tabs_draw_with_highlight(win, y++, 2, line, search);
            has_state_data = true;
        }
    }
    if (!has_state_data) {
        mvwprintw(win, y++, 2, "(no state data)");
    }

    /* V01.1: CPU Stack section (AppleWin only) */
    if (cpustack->has_data && y < height - 5) {
        y++;
        draw_separator(win, y++, width, "Stack");
        y++;

        char line[128];
        snprintf(line, sizeof(line), "SP: %s  Depth: %s",
                 cpustack->sp[0] ? cpustack->sp : "--",
                 cpustack->depth[0] ? cpustack->depth : "--");
        tabs_draw_with_highlight(win, y++, 2, line, search);

        /* Show stack entries (up to visible space) */
        int max_entries = height - y - 1;
        if (max_entries > cpustack->entry_count) {
            max_entries = cpustack->entry_count;
        }
        if (max_entries > 8) {
            max_entries = 8;  /* Limit display to 8 entries */
        }

        for (int i = 0; i < max_entries; i++) {
            snprintf(line, sizeof(line), "  [%s]: %s",
                     cpustack->entries[i].addr[0] ? cpustack->entries[i].addr : "----",
                     cpustack->entries[i].val[0] ? cpustack->entries[i].val : "--");
            tabs_draw_with_highlight(win, y++, 2, line, search);
        }

        if (cpustack->entry_count > max_entries) {
            mvwprintw(win, y++, 2, "  ... (%d more)", cpustack->entry_count - max_entries);
        }
    }

    /* V01.1: Disassembly section (AppleWin) */
    if (disasm->count > 0 && y < height - 4) {
        y++;
        draw_separator(win, y++, width, "Disassembly");
        y++;

        int max_lines = height - y - 1;
        if (max_lines > disasm->count) {
            max_lines = disasm->count;
        }
        if (max_lines > 8) {
            max_lines = 8;  /* Limit display */
        }

        for (int i = 0; i < max_lines; i++) {
            char line[128];
            snprintf(line, sizeof(line), "%s: %s",
                     disasm->lines[i].address[0] ? disasm->lines[i].address : "----",
                     disasm->lines[i].instruction);
            tabs_draw_with_highlight(win, y++, 2, line, search);
        }

        if (disasm->count > max_lines) {
            mvwprintw(win, y++, 2, "... (%d more)", disasm->count - max_lines);
        }
    }

    wrefresh(win);
}

void tabs_draw_memory(UIContext *ctx)
{
    WINDOW *win = ctx->win_content;
    MemoryData *mem = (MemoryData *)datastore_get_memory(ctx->datastore);
    const ZeroPageData *zp = datastore_get_zeropage(ctx->datastore);
    const StackPageData *sp = datastore_get_stackpage(ctx->datastore);
    const MemFlagsData *mf = datastore_get_memflags(ctx->datastore);
    const char *search = ui_get_search_term(ctx);
    int width, height;
    ui_get_content_size(ctx, &width, &height);

    werase(win);

    int y = 0;

    /* V01.1: Memory Flags (AppleWin soft switches) */
    if (mf->count > 0) {
        draw_separator(win, y++, width, "Memory Flags");

        /* Display flags in compact format: name=val name=val ... */
        char flags_line[256];
        int flags_len = 0;
        for (int i = 0; i < mf->count; i++) {
            int need = snprintf(NULL, 0, "%s=%s ", mf->flags[i].name, mf->flags[i].value);
            if (flags_len + need >= (int)sizeof(flags_line) - 1) break;
            flags_len += snprintf(flags_line + flags_len, sizeof(flags_line) - flags_len,
                                 "%s=%s ", mf->flags[i].name, mf->flags[i].value);
        }
        if (flags_len > 0) {
            flags_line[flags_len - 1] = '\0';  /* Remove trailing space */
        }
        tabs_draw_with_highlight(win, y++, 2, flags_line, search);
        y++;
    }

    /* V01.1: Zero Page full dump (256 bytes = 16 lines) */
    if (zp->has_data) {
        draw_separator(win, y++, width, "Zero Page ($0000-$00FF)");

        /* Column header */
        wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER) | A_DIM);
        mvwprintw(win, y, 2, "     ");
        for (int i = 0; i < 16; i++) {
            wprintw(win, "%02X ", i);
        }
        wprintw(win, " ASCII");
        wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER) | A_DIM);
        y++;

        /* 16 lines x 16 bytes = 256 bytes */
        for (int line = 0; line < 16 && y < height - 2; line++) {
            /* Address */
            wattron(win, COLOR_PAIR(COLOR_PAIR_DATA));
            mvwprintw(win, y, 2, "$%02X: ", line * 16);
            wattroff(win, COLOR_PAIR(COLOR_PAIR_DATA));

            /* Hex data */
            char hex_line[64];
            char ascii_part[20];
            int hex_len = 0;
            for (int i = 0; i < 16; i++) {
                int idx = line * 16 + i;
                if (zp->valid[idx]) {
                    hex_len += snprintf(hex_line + hex_len, sizeof(hex_line) - hex_len,
                                       "%02X ", zp->data[idx]);
                    unsigned char c = zp->data[idx];
                    ascii_part[i] = (c >= 32 && c < 127) ? c : '.';
                } else {
                    hex_len += snprintf(hex_line + hex_len, sizeof(hex_line) - hex_len, "-- ");
                    ascii_part[i] = ' ';
                }
            }
            ascii_part[16] = '\0';
            tabs_draw_with_highlight(win, y, 7, hex_line, search);

            /* Draw ASCII */
            wattron(win, A_DIM);
            mvwprintw(win, y, 7 + 48 + 1, "%s", ascii_part);
            wattroff(win, A_DIM);

            y++;
        }
        y++;
    }

    /* V01.1: Stack Page full dump (256 bytes = 16 lines) */
    if (sp->has_data) {
        draw_separator(win, y++, width, "Stack Page ($0100-$01FF)");

        /* Column header */
        wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER) | A_DIM);
        mvwprintw(win, y, 2, "      ");
        for (int i = 0; i < 16; i++) {
            wprintw(win, "%02X ", i);
        }
        wprintw(win, " ASCII");
        wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER) | A_DIM);
        y++;

        /* 16 lines x 16 bytes = 256 bytes */
        for (int line = 0; line < 16 && y < height - 2; line++) {
            /* Address */
            wattron(win, COLOR_PAIR(COLOR_PAIR_DATA));
            mvwprintw(win, y, 2, "$%03X: ", 0x100 + line * 16);
            wattroff(win, COLOR_PAIR(COLOR_PAIR_DATA));

            /* Hex data */
            char hex_line[64];
            char ascii_part[20];
            int hex_len = 0;
            for (int i = 0; i < 16; i++) {
                int idx = line * 16 + i;
                if (sp->valid[idx]) {
                    hex_len += snprintf(hex_line + hex_len, sizeof(hex_line) - hex_len,
                                       "%02X ", sp->data[idx]);
                    unsigned char c = sp->data[idx];
                    ascii_part[i] = (c >= 32 && c < 127) ? c : '.';
                } else {
                    hex_len += snprintf(hex_line + hex_len, sizeof(hex_line) - hex_len, "-- ");
                    ascii_part[i] = ' ';
                }
            }
            ascii_part[16] = '\0';
            tabs_draw_with_highlight(win, y, 8, hex_line, search);

            /* Draw ASCII */
            wattron(win, A_DIM);
            mvwprintw(win, y, 8 + 48 + 1, "%s", ascii_part);
            wattroff(win, A_DIM);

            y++;
        }
        y++;
    }

    /* Header with address range */
    char header[64];
    if (mem->count > 0) {
        snprintf(header, sizeof(header), "Memory Dump (%04X-%04X, %d lines)",
                 mem->min_addr, mem->max_addr, mem->count);
    } else {
        snprintf(header, sizeof(header), "Memory Dump");
    }
    draw_separator(win, y++, width, header);

    if (mem->count == 0 && !zp->has_data && !sp->has_data) {
        mvwprintw(win, y + 1, 2, "No memory data received yet.");
        wrefresh(win);
        return;
    }

    if (mem->count == 0) {
        wrefresh(win);
        return;
    }

    /* Column header */
    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER) | A_BOLD);
    mvwprintw(win, y, 0, "Address  ");
    for (int i = 0; i < 16; i++) {
        wprintw(win, "%02X ", i);
    }
    wprintw(win, " ASCII");
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER) | A_BOLD);
    y++;

    /* Draw memory lines */
    int visible_lines = height - 3;
    int scroll = mem->scroll_pos;

    for (int i = 0; i < visible_lines && (scroll + i) < mem->count; i++) {
        const MemLine *line = datastore_memory_get_line(mem, scroll + i);
        if (!line) continue;

        char hex_part[64];
        char ascii_part[20];
        int hex_len = 0;

        /* Build hex string */
        for (int j = 0; j < MEM_BYTES_PER_LINE; j++) {
            if (line->valid[j]) {
                hex_len += snprintf(hex_part + hex_len, sizeof(hex_part) - hex_len,
                                   "%02X ", line->data[j]);
            } else {
                hex_len += snprintf(hex_part + hex_len, sizeof(hex_part) - hex_len,
                                   "-- ");
            }
        }

        /* Build ASCII string */
        for (int j = 0; j < MEM_BYTES_PER_LINE; j++) {
            if (line->valid[j]) {
                unsigned char c = line->data[j];
                ascii_part[j] = (c >= 32 && c < 127) ? c : '.';
            } else {
                ascii_part[j] = ' ';
            }
        }
        ascii_part[16] = '\0';

        /* Draw address */
        wattron(win, COLOR_PAIR(COLOR_PAIR_DATA));
        mvwprintw(win, y, 0, "%04X:    ", line->address);
        wattroff(win, COLOR_PAIR(COLOR_PAIR_DATA));

        /* Draw hex data with search highlight */
        tabs_draw_with_highlight(win, y, 9, hex_part, search);

        /* Draw ASCII */
        wattron(win, A_DIM);
        mvwprintw(win, y, 9 + 48 + 1, "%s", ascii_part);
        wattroff(win, A_DIM);

        y++;
    }

    /* Scroll indicator */
    if (mem->count > visible_lines) {
        int percent = (scroll * 100) / (mem->count - visible_lines);
        mvwprintw(win, height - 1, width - 10, "[%3d%%]", percent);
    }

    wrefresh(win);
}

void tabs_memory_scroll(UIContext *ctx, int lines)
{
    MemoryData *mem = (MemoryData *)datastore_get_memory(ctx->datastore);
    int visible_lines = ctx->content_height - 3;

    if (lines < 0) {
        datastore_memory_scroll_up(mem, -lines);
    } else {
        datastore_memory_scroll_down(mem, lines, visible_lines);
    }
    ctx->needs_refresh = true;
}

void tabs_memory_scroll_home(UIContext *ctx)
{
    MemoryData *mem = (MemoryData *)datastore_get_memory(ctx->datastore);
    datastore_memory_scroll_home(mem);
    ctx->needs_refresh = true;
}

void tabs_memory_scroll_end(UIContext *ctx)
{
    MemoryData *mem = (MemoryData *)datastore_get_memory(ctx->datastore);
    int visible_lines = ctx->content_height - 3;
    datastore_memory_scroll_end(mem, visible_lines);
    ctx->needs_refresh = true;
}

/*
 * =============================================================================
 * Tab 5: Text Screen
 * =============================================================================
 */

void tabs_draw_text(UIContext *ctx)
{
    WINDOW *win = ctx->win_content;
    const TextScreenData *ts = datastore_get_textscreen(ctx->datastore);
    const char *search = ui_get_search_term(ctx);
    int width, height;
    ui_get_content_size(ctx, &width, &height);

    werase(win);

    int y = 0;

    /* Header */
    char header[64];
    if (ts->has_data) {
        snprintf(header, sizeof(header), "Apple II Text Screen (Page %d)", ts->current_page);
    } else {
        snprintf(header, sizeof(header), "Apple II Text Screen");
    }
    draw_separator(win, y++, width, header);

    if (!ts->has_data) {
        mvwprintw(win, y + 1, 2, "No text screen data received yet.");
        wrefresh(win);
        return;
    }

    y++;

    /* Draw column ruler */
    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER) | A_DIM);
    mvwprintw(win, y, 2, "    ");
    for (int col = 0; col < TEXT_COLS; col += 10) {
        wprintw(win, "%-10d", col);
    }
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER) | A_DIM);
    y++;

    /* Draw border top */
    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    mvwprintw(win, y, 2, "   +");
    for (int i = 0; i < TEXT_COLS; i++) {
        waddch(win, '-');
    }
    waddch(win, '+');
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    y++;

    /* Draw text screen rows */
    for (int row = 0; row < TEXT_ROWS && y < height - 2; row++) {
        /* Row number */
        wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER) | A_DIM);
        mvwprintw(win, y, 2, "%2d |", row);
        wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER) | A_DIM);

        if (ts->row_valid[row]) {
            /* Draw text with search highlighting */
            tabs_draw_with_highlight(win, y, 6, ts->rows[row], search);

            /* Draw right border */
            wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
            mvwprintw(win, y, 6 + TEXT_COLS, "|");
            wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));

            /* Show row address */
            wattron(win, A_DIM);
            mvwprintw(win, y, 6 + TEXT_COLS + 2, "$%04X", ts->row_addr[row]);
            wattroff(win, A_DIM);
        } else {
            /* Empty row */
            wattron(win, A_DIM);
            for (int i = 0; i < TEXT_COLS; i++) {
                mvwaddch(win, y, 6 + i, '.');
            }
            wattroff(win, A_DIM);

            wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
            mvwprintw(win, y, 6 + TEXT_COLS, "|");
            wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));
        }

        y++;
    }

    /* Draw border bottom */
    wattron(win, COLOR_PAIR(COLOR_PAIR_HEADER));
    mvwprintw(win, y, 2, "   +");
    for (int i = 0; i < TEXT_COLS; i++) {
        waddch(win, '-');
    }
    waddch(win, '+');
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HEADER));

    wrefresh(win);
}
