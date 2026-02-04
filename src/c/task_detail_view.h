#ifndef TASK_DETAIL_VIEW_H
#define TASK_DETAIL_VIEW_H

#include <pebble.h>

// Initialize detail window (called from main init)
void task_detail_view_init(void);

// Cleanup detail window (called from deinit)
void task_detail_view_deinit(void);

// Show task detail for the given task
void task_detail_view_show(Task *task);

// Get detail window pointer
Window* task_detail_view_get_window(void);

#endif // TASK_DETAIL_VIEW_H
