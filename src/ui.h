/*
 * ui.h - ncurses UI for rdemonitor
 *
 * Main UI controller handling window layout, input, and rendering.
 */

#ifndef UI_H
#define UI_H

#include <ncurses.h>
#include <stdbool.h>
#include <time.h>
#include "data_store.h"

/*
 * =============================================================================
 * Constants
 * =============================================================================
 */

#define UI_STATUS_BAR_HEIGHT    1
#define UI_TAB_BAR_HEIGHT       1
#define UI_HELP_BAR_HEIGHT      1
#define UI_MIN_WIDTH            80
#define UI_MIN_HEIGHT           24

/* Color pairs */
#define COLOR_PAIR_NORMAL       1
#define COLOR_PAIR_STATUS       2
#define COLOR_PAIR_TAB_ACTIVE   3
#define COLOR_PAIR_TAB_INACTIVE 4
#define COLOR_PAIR_HELP         5
#define COLOR_PAIR_HIGHLIGHT    6
#define COLOR_PAIR_SEARCH       7
#define COLOR_PAIR_ERROR        8
#define COLOR_PAIR_HEADER       9
#define COLOR_PAIR_DATA         10

/*
 * =============================================================================
 * Types
 * =============================================================================
 */

typedef enum {
    TAB_INFO = 0,
    TAB_IO,
    TAB_CPU,
    TAB_MEMORY,
    TAB_COUNT
} TabType;

typedef enum {
    UI_MODE_NORMAL = 0,
    UI_MODE_SEARCH,
    UI_MODE_DIALOG
} UIMode;

typedef struct {
    char term[256];
    bool active;
    int  cursor_pos;
} SearchState;

typedef struct {
    /* Windows */
    WINDOW *win_status;     /* Top status bar */
    WINDOW *win_tabs;       /* Tab bar */
    WINDOW *win_content;    /* Main content area */
    WINDOW *win_help;       /* Bottom help bar */
    WINDOW *win_search;     /* Search dialog (popup) */

    /* State */
    TabType     current_tab;
    UIMode      mode;
    SearchState search[TAB_COUNT];  /* Per-tab search state */

    /* Connection info */
    bool        connected;
    char        emu_type[16];
    bool        caps_lock;

    /* Status message (displayed in help bar) */
    char        status_message[128];
    time_t      status_message_time;

    /* Screen dimensions */
    int         screen_width;
    int         screen_height;
    int         content_height;
    int         content_width;

    /* Data reference */
    DataStore  *datastore;

    /* Flags */
    bool        needs_refresh;
    bool        running;
} UIContext;

/*
 * =============================================================================
 * Function Declarations
 * =============================================================================
 */

/*
 * Initialize the UI system
 * Returns: 0 on success, -1 on error
 */
int ui_init(UIContext *ctx, DataStore *datastore);

/*
 * Cleanup and shutdown UI
 */
void ui_cleanup(UIContext *ctx);

/*
 * Main UI update - call this in the main loop
 * Redraws all windows if needed
 */
void ui_update(UIContext *ctx);

/*
 * Force a full redraw
 */
void ui_refresh(UIContext *ctx);

/*
 * Handle window resize (SIGWINCH)
 */
void ui_resize(UIContext *ctx);

/*
 * Handle keyboard input
 * Returns: 0 to continue, 1 to quit
 */
int ui_handle_input(UIContext *ctx, int ch);

/*
 * Set connection status
 */
void ui_set_connected(UIContext *ctx, bool connected, const char *emu_type);

/*
 * Update caps lock status
 */
void ui_update_caps_lock(UIContext *ctx);

/*
 * Switch to a specific tab
 */
void ui_switch_tab(UIContext *ctx, TabType tab);

/*
 * Show search dialog
 */
void ui_show_search(UIContext *ctx);

/*
 * Hide search dialog
 */
void ui_hide_search(UIContext *ctx);

/*
 * Get current search term for active tab
 */
const char *ui_get_search_term(UIContext *ctx);

/*
 * Check if UI is still running
 */
bool ui_is_running(const UIContext *ctx);

/*
 * Mark that data has changed and UI needs refresh
 */
void ui_mark_dirty(UIContext *ctx);

/*
 * Get content window dimensions (for tab renderers)
 */
void ui_get_content_size(const UIContext *ctx, int *width, int *height);

/*
 * Get the content window (for direct drawing by tabs)
 */
WINDOW *ui_get_content_window(UIContext *ctx);

/*
 * Set a status message to display in the help bar
 * Message will auto-clear after a few seconds
 */
void ui_set_status_message(UIContext *ctx, const char *fmt, ...);

#endif /* UI_H */
