#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <pebble.h>

// API callback keys
#define KEY_TYPE 0
#define KEY_NAME 1
#define KEY_ID 2
#define KEY_DUE_DATE 3
#define KEY_COMPLETED 4
#define KEY_LIST_NAME 5
#define KEY_IDX 6
#define KEY_PRIORITY 7

// Navigation state
typedef enum {
  STATE_TASK_LISTS,
  STATE_TASKS,
  STATE_TASK_DETAIL
} AppState;

// Data structures
typedef struct {
  char name[64];
} TaskList;

typedef struct {
  char id[33];        // 33 bytes, 32 chars + null terminator
  int8_t idx;         // 1 byte
  char name[128];     // 128 bytes
  int8_t priority;    // 1 byte
  char due_date[32];  // 32 bytes
  bool completed;     // 1 byte
} Task;               // total: 196 bytes

// Global state (defined in task_manager.c)
extern TaskList task_lists[20];
extern int task_lists_count;
extern Task tasks[50];
extern int tasks_count;
extern int selected_list_index;
extern int selected_task_index;
extern AppState current_state;
extern bool js_ready;
extern char s_time_buffer[30];

// Shared utility functions
time_t convert_iso_to_time_t(const char* iso_date_str);
void convert_iso_to_friendly_date(const char* iso_date_str, char* buffer, size_t buffer_size);

// AppMessage functions
void fetch_tasks(const char *list_name);
void complete_task(uint8_t task_id, const char *list_name);
void fetch_task_lists(void);

#endif // TASK_MANAGER_H
