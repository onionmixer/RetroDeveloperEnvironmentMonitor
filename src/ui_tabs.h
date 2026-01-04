/*
 * ui_tabs.h - Tab content rendering for rdemonitor
 *
 * Each tab has its own rendering logic for displaying data.
 */

#ifndef UI_TABS_H
#define UI_TABS_H

#include "ui.h"

/*
 * Draw the content for the current tab
 */
void tabs_draw_content(UIContext *ctx);

/*
 * Tab 1: Info - Draw basic information
 */
void tabs_draw_info(UIContext *ctx);

/*
 * Tab 2: IO - Draw I/O information
 */
void tabs_draw_io(UIContext *ctx);

/*
 * Tab 3: CPU - Draw CPU registers and flags
 */
void tabs_draw_cpu(UIContext *ctx);

/*
 * Tab 4: Memory - Draw hex memory dump
 */
void tabs_draw_memory(UIContext *ctx);

/*
 * Tab 5: Text - Draw Apple II text screen
 */
void tabs_draw_text(UIContext *ctx);

/*
 * Memory tab scrolling
 */
void tabs_memory_scroll(UIContext *ctx, int lines);
void tabs_memory_scroll_home(UIContext *ctx);
void tabs_memory_scroll_end(UIContext *ctx);

/*
 * Check if text matches current search term (case-insensitive)
 */
bool tabs_search_match(const char *text, const char *search_term);

/*
 * Draw text with search highlighting
 */
void tabs_draw_with_highlight(WINDOW *win, int y, int x,
                              const char *text, const char *search_term);

#endif /* UI_TABS_H */
