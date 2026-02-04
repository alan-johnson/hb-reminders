#ifndef TASK_LIST_VIEW_H
#define TASK_LIST_VIEW_H

#include <pebble.h>

// Initialize tasks window (called from main init)
void task_list_view_init(void);

// Cleanup tasks window (called from deinit)
void task_list_view_deinit(void);

// Get tasks window pointer
Window* task_list_view_get_window(void);

// Get tasks menu pointer (for reload after completion)
MenuLayer* task_list_view_get_menu(void);

#endif // TASK_LIST_VIEW_H
