/*
 * ui.c - ncurses UI implementation
 */

#include "ui.h"
#include "ui_tabs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/kd.h>

/*
 * =============================================================================
 * Static Helper Functions
 * =============================================================================
 */

/* Initialize color pairs */
static void init_colors(void)
{
    if (has_colors()) {
        start_color();
        use_default_colors();

        /* Define color pairs */
        init_pair(COLOR_PAIR_NORMAL,       COLOR_WHITE,  -1);
        init_pair(COLOR_PAIR_STATUS,       COLOR_BLACK,  COLOR_CYAN);
        init_pair(COLOR_PAIR_TAB_ACTIVE,   COLOR_BLACK,  COLOR_WHITE);
        init_pair(COLOR_PAIR_TAB_INACTIVE, COLOR_WHITE,  COLOR_BLUE);
        init_pair(COLOR_PAIR_HELP,         COLOR_BLACK,  COLOR_CYAN);
        init_pair(COLOR_PAIR_HIGHLIGHT,    COLOR_BLACK,  COLOR_YELLOW);
        init_pair(COLOR_PAIR_SEARCH,       COLOR_BLACK,  COLOR_GREEN);
        init_pair(COLOR_PAIR_ERROR,        COLOR_WHITE,  COLOR_RED);
        init_pair(COLOR_PAIR_HEADER,       COLOR_YELLOW, -1);
        init_pair(COLOR_PAIR_DATA,         COLOR_GREEN,  -1);
    }
}

/* Create/recreate windows based on current terminal size */
static void create_windows(UIContext *ctx)
{
    /* Delete existing windows */
    if (ctx->win_status)  { delwin(ctx->win_status);  ctx->win_status = NULL; }
    if (ctx->win_tabs)    { delwin(ctx->win_tabs);    ctx->win_tabs = NULL; }
    if (ctx->win_content) { delwin(ctx->win_content); ctx->win_content = NULL; }
    if (ctx->win_help)    { delwin(ctx->win_help);    ctx->win_help = NULL; }
    if (ctx->win_search)  { delwin(ctx->win_search);  ctx->win_search = NULL; }

    /* Get screen size */
    getmaxyx(stdscr, ctx->screen_height, ctx->screen_width);

    /* Calculate content area size */
    ctx->content_height = ctx->screen_height - UI_STATUS_BAR_HEIGHT
                         - UI_TAB_BAR_HEIGHT - UI_HELP_BAR_HEIGHT;
    ctx->content_width = ctx->screen_width;

    /* Create windows */
    /* Status bar at top */
    ctx->win_status = newwin(UI_STATUS_BAR_HEIGHT, ctx->screen_width, 0, 0);

    /* Tab bar below status */
    ctx->win_tabs = newwin(UI_TAB_BAR_HEIGHT, ctx->screen_width,
                          UI_STATUS_BAR_HEIGHT, 0);

    /* Content area in middle */
    ctx->win_content = newwin(ctx->content_height, ctx->content_width,
                             UI_STATUS_BAR_HEIGHT + UI_TAB_BAR_HEIGHT, 0);

    /* Help bar at bottom */
    ctx->win_help = newwin(UI_HELP_BAR_HEIGHT, ctx->screen_width,
                          ctx->screen_height - UI_HELP_BAR_HEIGHT, 0);

    /* Enable scrolling for content window */
    scrollok(ctx->win_content, TRUE);

    /* Enable keypad for content window */
    keypad(ctx->win_content, TRUE);
}

/* Draw the status bar */
static void draw_status_bar(UIContext *ctx)
{
    werase(ctx->win_status);
    wbkgd(ctx->win_status, COLOR_PAIR(COLOR_PAIR_STATUS));

    /* Left side: emulator type and connection status */
    wmove(ctx->win_status, 0, 1);
    if (ctx->connected) {
        wprintw(ctx->win_status, "[%s] Connected",
                ctx->emu_type[0] ? ctx->emu_type : "---");
    } else {
        wattron(ctx->win_status, A_BOLD);
        wprintw(ctx->win_status, "[---] Disconnected");
        wattroff(ctx->win_status, A_BOLD);
    }

    /* Right side: CAPS LOCK status */
    const char *caps_str = ctx->caps_lock ? "CAPS: ON " : "CAPS: OFF";
    mvwprintw(ctx->win_status, 0, ctx->screen_width - strlen(caps_str) - 1,
              "%s", caps_str);

    wrefresh(ctx->win_status);
}

/* Draw the tab bar */
static void draw_tab_bar(UIContext *ctx)
{
    static const char *tab_names[] = {
        "1:Info", "2:IO", "3:CPU", "4:Memory", "5:Text"
    };

    werase(ctx->win_tabs);
    wbkgd(ctx->win_tabs, COLOR_PAIR(COLOR_PAIR_TAB_INACTIVE));

    /* First line: tab names */
    int x = 1;
    int tab_positions[TAB_COUNT];  /* Store x position for each tab */
    int tab_widths[TAB_COUNT];     /* Store width for each tab */

    for (int i = 0; i < TAB_COUNT; i++) {
        tab_positions[i] = x;
        tab_widths[i] = strlen(tab_names[i]) + 5;

        wmove(ctx->win_tabs, 0, x);

        if (i == (int)ctx->current_tab) {
            wattron(ctx->win_tabs, COLOR_PAIR(COLOR_PAIR_TAB_ACTIVE) | A_BOLD);
            wprintw(ctx->win_tabs, " [%s] ", tab_names[i]);
            wattroff(ctx->win_tabs, COLOR_PAIR(COLOR_PAIR_TAB_ACTIVE) | A_BOLD);
        } else {
            wprintw(ctx->win_tabs, "  %s  ", tab_names[i]);
        }

        x += tab_widths[i];
    }

    /* Second line: message counts for tabs 2-5 (IO, CPU, Memory, Text) */
    unsigned long counts[TAB_COUNT] = {
        0,                              /* Tab 1: Info (no count) */
        ctx->datastore->io_messages,    /* Tab 2: IO */
        ctx->datastore->cpu_messages,   /* Tab 3: CPU */
        ctx->datastore->mem_messages,   /* Tab 4: Memory */
        ctx->datastore->text_messages   /* Tab 5: Text */
    };

    for (int i = 1; i < TAB_COUNT; i++) {  /* Skip tab 0 (Info) */
        char count_str[32];
        snprintf(count_str, sizeof(count_str), "%lu", counts[i]);

        /* Center the count under the tab name */
        int count_len = strlen(count_str);
        int center_x = tab_positions[i] + (tab_widths[i] - count_len) / 2;

        wmove(ctx->win_tabs, 1, center_x);
        wattron(ctx->win_tabs, A_DIM);
        wprintw(ctx->win_tabs, "%s", count_str);
        wattroff(ctx->win_tabs, A_DIM);
    }

    wrefresh(ctx->win_tabs);
}

/* Draw the help bar */
static void draw_help_bar(UIContext *ctx)
{
    werase(ctx->win_help);
    wbkgd(ctx->win_help, COLOR_PAIR(COLOR_PAIR_HELP));

    wmove(ctx->win_help, 0, 1);

    if (ctx->mode == UI_MODE_SEARCH) {
        wprintw(ctx->win_help, "Enter: Search | ESC: Cancel");
    } else if (ctx->current_tab == TAB_MEMORY) {
        wprintw(ctx->win_help,
                "1-5:Tab | f:Search | s:Snap | Up/Dn/PgUp/PgDn/Home/End:Scroll | q:Quit");
    } else {
        wprintw(ctx->win_help,
                "1-5:Tab | f:Search | s:Snapshot | q:Quit");
    }

    /* Show status message if present and not expired (5 seconds) */
    if (ctx->status_message[0] != '\0') {
        time_t now = time(NULL);
        if (now - ctx->status_message_time < 5) {
            wprintw(ctx->win_help, " - %s", ctx->status_message);
        } else {
            ctx->status_message[0] = '\0';  /* Clear expired message */
        }
    }

    wrefresh(ctx->win_help);
}

/* Draw search dialog */
static void draw_search_dialog(UIContext *ctx)
{
    if (ctx->mode != UI_MODE_SEARCH) {
        return;
    }

    /* Create search window if not exists */
    if (ctx->win_search == NULL) {
        int dialog_width = 50;
        int dialog_height = 5;
        int start_y = (ctx->screen_height - dialog_height) / 2;
        int start_x = (ctx->screen_width - dialog_width) / 2;

        ctx->win_search = newwin(dialog_height, dialog_width, start_y, start_x);
        keypad(ctx->win_search, TRUE);
    }

    werase(ctx->win_search);
    box(ctx->win_search, 0, 0);

    /* Title */
    wattron(ctx->win_search, A_BOLD);
    mvwprintw(ctx->win_search, 0, 2, " Search ");
    wattroff(ctx->win_search, A_BOLD);

    /* Prompt */
    mvwprintw(ctx->win_search, 2, 2, "Enter search term: ");

    /* Current search input */
    SearchState *search = &ctx->search[ctx->current_tab];
    wprintw(ctx->win_search, "%s", search->term);

    /* Show cursor */
    wmove(ctx->win_search, 2, 21 + search->cursor_pos);
    curs_set(1);

    wrefresh(ctx->win_search);
}

/* Hide search dialog */
static void hide_search_dialog(UIContext *ctx)
{
    if (ctx->win_search != NULL) {
        werase(ctx->win_search);
        wrefresh(ctx->win_search);
        delwin(ctx->win_search);
        ctx->win_search = NULL;
    }
    curs_set(0);
    ctx->mode = UI_MODE_NORMAL;
    ctx->needs_refresh = true;
}

/* Check CAPS LOCK state using sysfs LED interface */
static bool check_caps_lock_sysfs(void)
{
    static char capslock_path[256] = {0};
    static bool path_initialized = false;
    static bool path_found = false;

    /* Find capslock LED path on first call */
    if (!path_initialized) {
        path_initialized = true;
        DIR *dir = opendir("/sys/class/leds");
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strstr(entry->d_name, "capslock") != NULL) {
                    snprintf(capslock_path, sizeof(capslock_path),
                             "/sys/class/leds/%s/brightness", entry->d_name);
                    path_found = true;
                    break;
                }
            }
            closedir(dir);
        }
    }

    if (!path_found) {
        return false;
    }

    /* Read brightness value */
    FILE *fp = fopen(capslock_path, "r");
    if (!fp) {
        return false;
    }

    int value = 0;
    if (fscanf(fp, "%d", &value) != 1) {
        fclose(fp);
        return false;
    }
    fclose(fp);

    return value != 0;
}

/* Check CAPS LOCK state using ioctl (fallback for console) */
static bool check_caps_lock_ioctl(void)
{
    int fd = open("/dev/tty", O_RDONLY);
    if (fd < 0) {
        return false;
    }

    int state = 0;
    if (ioctl(fd, KDGETLED, &state) < 0) {
        close(fd);
        return false;
    }

    close(fd);
    return (state & LED_CAP) != 0;
}

/* Check CAPS LOCK state - tries multiple methods */
static bool check_caps_lock(void)
{
    /* Try sysfs first (works in X11/Wayland) */
    static int method = -1;  /* -1 = unknown, 0 = sysfs, 1 = ioctl, 2 = none */

    if (method == -1) {
        /* Determine which method works */
        DIR *dir = opendir("/sys/class/leds");
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strstr(entry->d_name, "capslock") != NULL) {
                    method = 0;  /* sysfs available */
                    break;
                }
            }
            closedir(dir);
        }
        if (method == -1) {
            /* Try ioctl */
            int fd = open("/dev/tty", O_RDONLY);
            if (fd >= 0) {
                int state = 0;
                if (ioctl(fd, KDGETLED, &state) >= 0) {
                    method = 1;  /* ioctl works */
                }
                close(fd);
            }
        }
        if (method == -1) {
            method = 2;  /* No method available */
        }
    }

    switch (method) {
        case 0:
            return check_caps_lock_sysfs();
        case 1:
            return check_caps_lock_ioctl();
        default:
            return false;
    }
}

/*
 * =============================================================================
 * Public Functions
 * =============================================================================
 */

int ui_init(UIContext *ctx, DataStore *datastore)
{
    memset(ctx, 0, sizeof(UIContext));
    ctx->datastore = datastore;
    ctx->running = true;
    ctx->needs_refresh = true;

    /* Initialize ncurses */
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    /* Set up non-blocking input with timeout */
    timeout(0);  /* Non-blocking */

    /* Initialize colors */
    init_colors();

    /* Create windows */
    create_windows(ctx);

    /* Initial caps lock check */
    ctx->caps_lock = check_caps_lock();

    /* Draw initial UI */
    ui_update(ctx);

    return 0;
}

void ui_cleanup(UIContext *ctx)
{
    /* Delete windows */
    if (ctx->win_status)  delwin(ctx->win_status);
    if (ctx->win_tabs)    delwin(ctx->win_tabs);
    if (ctx->win_content) delwin(ctx->win_content);
    if (ctx->win_help)    delwin(ctx->win_help);
    if (ctx->win_search)  delwin(ctx->win_search);

    /* End ncurses */
    endwin();
}

void ui_update(UIContext *ctx)
{
    if (!ctx->needs_refresh) {
        return;
    }

    draw_status_bar(ctx);
    draw_tab_bar(ctx);
    draw_help_bar(ctx);

    /* Draw current tab content */
    tabs_draw_content(ctx);

    /* Draw search dialog if active */
    if (ctx->mode == UI_MODE_SEARCH) {
        draw_search_dialog(ctx);
    }

    ctx->needs_refresh = false;
}

void ui_refresh(UIContext *ctx)
{
    ctx->needs_refresh = true;
    touchwin(stdscr);
    ui_update(ctx);
}

void ui_resize(UIContext *ctx)
{
    endwin();
    refresh();
    create_windows(ctx);
    ctx->needs_refresh = true;
    ui_update(ctx);
}

int ui_handle_input(UIContext *ctx, int ch)
{
    if (ch == ERR) {
        return 0;  /* No input */
    }

    /* Handle search mode input */
    if (ctx->mode == UI_MODE_SEARCH) {
        SearchState *search = &ctx->search[ctx->current_tab];

        switch (ch) {
            case 27:  /* ESC */
                hide_search_dialog(ctx);
                break;

            case '\n':
            case KEY_ENTER:
                /* Execute search */
                search->active = (search->term[0] != '\0');
                hide_search_dialog(ctx);
                break;

            case KEY_BACKSPACE:
            case 127:
            case 8:
                if (search->cursor_pos > 0) {
                    search->cursor_pos--;
                    search->term[search->cursor_pos] = '\0';
                    draw_search_dialog(ctx);
                }
                break;

            default:
                if (ch >= 32 && ch < 127 && search->cursor_pos < 255) {
                    search->term[search->cursor_pos++] = ch;
                    search->term[search->cursor_pos] = '\0';
                    draw_search_dialog(ctx);
                }
                break;
        }
        return 0;
    }

    /* Normal mode input */
    switch (ch) {
        case 'q':
        case 'Q':
            ctx->running = false;
            return 1;

        /* Tab switching: Shift+1,2,3,4 -> !, @, #, $ on US keyboard */
        case '!':
            ui_switch_tab(ctx, TAB_INFO);
            break;
        case '@':
            ui_switch_tab(ctx, TAB_IO);
            break;
        case '#':
            ui_switch_tab(ctx, TAB_CPU);
            break;
        case '$':
            ui_switch_tab(ctx, TAB_MEMORY);
            break;
        case '%':
            ui_switch_tab(ctx, TAB_TEXT);
            break;

        /* Also support direct number keys for convenience */
        case '1':
            ui_switch_tab(ctx, TAB_INFO);
            break;
        case '2':
            ui_switch_tab(ctx, TAB_IO);
            break;
        case '3':
            ui_switch_tab(ctx, TAB_CPU);
            break;
        case '4':
            ui_switch_tab(ctx, TAB_MEMORY);
            break;
        case '5':
            ui_switch_tab(ctx, TAB_TEXT);
            break;

        case 'f':
        case 'F':
            ui_show_search(ctx);
            break;

        case 's':
        case 'S':
            /* Snapshot - will be handled by main */
            return 's';

        /* Memory tab navigation */
        case KEY_UP:
            if (ctx->current_tab == TAB_MEMORY) {
                tabs_memory_scroll(ctx, -1);
            }
            break;

        case KEY_DOWN:
            if (ctx->current_tab == TAB_MEMORY) {
                tabs_memory_scroll(ctx, 1);
            }
            break;

        case KEY_PPAGE:
            if (ctx->current_tab == TAB_MEMORY) {
                tabs_memory_scroll(ctx, -(ctx->content_height - 2));
            }
            break;

        case KEY_NPAGE:
            if (ctx->current_tab == TAB_MEMORY) {
                tabs_memory_scroll(ctx, ctx->content_height - 2);
            }
            break;

        case KEY_HOME:
            if (ctx->current_tab == TAB_MEMORY) {
                tabs_memory_scroll_home(ctx);
            }
            break;

        case KEY_END:
            if (ctx->current_tab == TAB_MEMORY) {
                tabs_memory_scroll_end(ctx);
            }
            break;

        case KEY_RESIZE:
            ui_resize(ctx);
            break;

        default:
            break;
    }

    return 0;
}

void ui_set_connected(UIContext *ctx, bool connected, const char *emu_type)
{
    ctx->connected = connected;
    if (emu_type) {
        snprintf(ctx->emu_type, sizeof(ctx->emu_type), "%s", emu_type);
    } else {
        ctx->emu_type[0] = '\0';
    }
    ctx->needs_refresh = true;
}

void ui_update_caps_lock(UIContext *ctx)
{
    bool new_state = check_caps_lock();
    if (new_state != ctx->caps_lock) {
        ctx->caps_lock = new_state;
        ctx->needs_refresh = true;
    }
}

void ui_switch_tab(UIContext *ctx, TabType tab)
{
    if (tab >= 0 && tab < TAB_COUNT && tab != ctx->current_tab) {
        ctx->current_tab = tab;
        ctx->needs_refresh = true;
    }
}

void ui_show_search(UIContext *ctx)
{
    ctx->mode = UI_MODE_SEARCH;
    SearchState *search = &ctx->search[ctx->current_tab];
    search->term[0] = '\0';
    search->cursor_pos = 0;
    draw_search_dialog(ctx);
}

void ui_hide_search(UIContext *ctx)
{
    hide_search_dialog(ctx);
}

const char *ui_get_search_term(UIContext *ctx)
{
    SearchState *search = &ctx->search[ctx->current_tab];
    return search->active ? search->term : NULL;
}

bool ui_is_running(const UIContext *ctx)
{
    return ctx->running;
}

void ui_mark_dirty(UIContext *ctx)
{
    ctx->needs_refresh = true;
}

void ui_get_content_size(const UIContext *ctx, int *width, int *height)
{
    *width = ctx->content_width;
    *height = ctx->content_height;
}

WINDOW *ui_get_content_window(UIContext *ctx)
{
    return ctx->win_content;
}

void ui_set_status_message(UIContext *ctx, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->status_message, sizeof(ctx->status_message), fmt, args);
    va_end(args);
    ctx->status_message_time = time(NULL);
    ctx->needs_refresh = true;
}
