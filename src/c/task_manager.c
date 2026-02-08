#include <pebble.h>
#include <time.h>
#include <stdio.h>
#include "task_manager.h"
#include "task_list_view.h"
#include "task_detail_view.h"
#include "strings.h"

// Windows
static Window *s_lists_window;
static MenuLayer *s_lists_menu;

// Global state (definitions for variables declared extern in task_manager.h)
AppState current_state = STATE_TASK_LISTS;
TaskList task_lists[20]; //1280 bytes
int task_lists_count = 0;
Task tasks[50];  //9800 bytes
int tasks_count = 0;
int selected_list_index = 0;
int selected_task_index = 0;
bool js_ready = false;  // set when JS signals it's ready
char s_time_buffer[30]; // Large enough for "Day Month DD HH:MM"

//#define TESTING 1
#ifdef TESTING
static const char *task_lists_testing[] = {
  "Personal",
  "Whirligigs and automatons",
  "Tasks",
  "Pebble",
  "Bucket list",
  "Shopping List",
  "To Do",
  "Reminders",
  "To Dos",
  "Shopping",
  "Family",
  "Groceries",
  "Albums/songs",
  "Work Tasks"
};
#endif

// Function prototypes
static void lists_window_load(Window *window);
static void lists_window_unload(Window *window);
#ifdef TESTING
static void fetch_task_lists_testing(void);
static void fetch_tasks_testing(void);
#endif

// Menu callbacks
// Lists menu callbacks
static uint16_t lists_menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "lists_menu_get_num_rows called");
  return task_lists_count;
}

static void lists_menu_draw_row(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "lists_menu_draw_row called for row %d", cell_index->row);
  if (cell_index->row < task_lists_count) {
    menu_cell_basic_draw(ctx, cell_layer, task_lists[cell_index->row].name, NULL, NULL);
  }
}

static void lists_menu_select(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "lists_menu_select called for row %d", cell_index->row);
  selected_list_index = cell_index->row;
  current_state = STATE_TASKS;

  // Reset tasks count before fetching new list
  tasks_count = 0;

  window_stack_push(task_list_view_get_window(), true);

  #ifdef TESTING
    fetch_tasks_testing();
  #else
    fetch_tasks(task_lists[selected_list_index].name);
  #endif
  MenuLayer *tasks_menu = task_list_view_get_menu();
  if (tasks_menu) menu_layer_reload_data(tasks_menu);
}

time_t convert_iso_to_time_t(const char* iso_date_str) {
    if (!iso_date_str || strlen(iso_date_str) < 19) {
        return (time_t)-1;
    }

    struct tm t = {0}; // Initialize to zero

    // Manual parsing to avoid sscanf issues on Pebble
    // Expected format: "2026-02-15T14:30:00Z"
    // Parse year (positions 0-3)
    t.tm_year = (iso_date_str[0] - '0') * 1000 +
                (iso_date_str[1] - '0') * 100 +
                (iso_date_str[2] - '0') * 10 +
                (iso_date_str[3] - '0');

    // Parse month (positions 5-6)
    t.tm_mon = (iso_date_str[5] - '0') * 10 + (iso_date_str[6] - '0');

    // Parse day (positions 8-9)
    t.tm_mday = (iso_date_str[8] - '0') * 10 + (iso_date_str[9] - '0');

    // Parse hour (positions 11-12)
    t.tm_hour = (iso_date_str[11] - '0') * 10 + (iso_date_str[12] - '0');

    // Parse minute (positions 14-15)
    t.tm_min = (iso_date_str[14] - '0') * 10 + (iso_date_str[15] - '0');

    // Parse second (positions 17-18)
    t.tm_sec = (iso_date_str[17] - '0') * 10 + (iso_date_str[18] - '0');

    // Adjust the tm structure fields to expected ranges:
    // tm_year is years since 1900
    t.tm_year -= 1900;
    // tm_mon is 0-based (0-11)
    t.tm_mon -= 1;

    // Set daylight saving time flag
    t.tm_isdst = -1; // Let mktime determine DST

    // Convert to time_t
    time_t timestamp = mktime(&t);

    return timestamp;
}

void convert_iso_to_friendly_date(const char* iso_date_str, char* buffer, size_t buffer_size) {
  // Check if the date string is "No due date"
  if (strcmp(iso_date_str, STR_NO_DUE_DATE) == 0) {
    snprintf(buffer, buffer_size, STR_NO_DUE_DATE);
    return;
  }

  time_t timestamp = convert_iso_to_time_t(iso_date_str);
  if (timestamp == (time_t)-1) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "convert_iso_to_time_t failed for date: %s", iso_date_str);

    snprintf(buffer, buffer_size, STR_INVALID_DATE);
    return;
  }

  struct tm *local_time = localtime(&timestamp);

  // Format the time and date based on user preference
  if (clock_is_24h_style()) {
    // 24-hour format: "Mon Feb 15 14:30"
    strftime(buffer, buffer_size, "%a %b %d %H:%M", local_time);
  } else {
    // 12-hour format with AM/PM: "Mon Feb 15 2:30 PM"
    strftime(buffer, buffer_size, "%a %b %d %I:%M %p", local_time);
  }

}

// AppMessage handlers

static const char* app_message_result_to_string(AppMessageResult result) {
  switch(result) {
    case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    default: return "UNKNOWN";
  }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox_received_callback called");
  
  Tuple *type_tuple = dict_find(iterator, KEY_TYPE);
  
  if (type_tuple) {
    int type = type_tuple->value->int32;

    switch(type) {
      case 0: { // JS ready signal
        APP_LOG(APP_LOG_LEVEL_INFO, "JavaScript is ready!");
        js_ready = true;
        // Now fetch task lists if we're on the lists window
        if (current_state == STATE_TASK_LISTS) {
          fetch_task_lists();
        }
        break;
      }
      case 1: { // Task list names
        APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox, received list name");
        Tuple *name_tuple = dict_find(iterator, KEY_NAME);
        if (name_tuple && task_lists_count < 20) {
          snprintf(task_lists[task_lists_count].name, sizeof(task_lists[0].name),
                   "%s", name_tuple->value->cstring);
          APP_LOG(APP_LOG_LEVEL_DEBUG, "added list name: %s", task_lists[task_lists_count].name);
          task_lists_count++;
          if (s_lists_menu) menu_layer_reload_data(s_lists_menu);
        }
        break;
      }
      
      case 2: { // Tasks in a list
        APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox, received task list");
        
        // Parse task data from dictionary
        Tuple *id_tuple = dict_find(iterator, KEY_ID);
        Tuple *name_tuple = dict_find(iterator, KEY_NAME);
        Tuple *due_tuple = dict_find(iterator, KEY_DUE_DATE);
        Tuple *completed_tuple = dict_find(iterator, KEY_COMPLETED);
        Tuple* idx_tuple = dict_find(iterator, KEY_IDX);
        Tuple* priority_tuple = dict_find(iterator, KEY_PRIORITY);

        if (id_tuple && name_tuple && tasks_count < 50) {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox, setting task data");
        
          snprintf(tasks[tasks_count].id, sizeof(tasks[0].id),
                   "%s", id_tuple->value->cstring);
          snprintf(tasks[tasks_count].name, sizeof(tasks[0].name),
                   "%s", name_tuple->value->cstring);
          if (due_tuple) {
            snprintf(tasks[tasks_count].due_date, sizeof(tasks[0].due_date),
                     "%s", due_tuple->value->cstring);
          }
          tasks[tasks_count].completed = completed_tuple ? completed_tuple->value->int16 : 0;
          tasks[tasks_count].priority = priority_tuple ? priority_tuple->value->int16 : 0;
          tasks[tasks_count].idx = idx_tuple ? idx_tuple->value->int16 : 0;

          tasks_count++;
          MenuLayer *tasks_menu = task_list_view_get_menu();
          if (tasks_menu) menu_layer_reload_data(tasks_menu);
        } else {
          APP_LOG(APP_LOG_LEVEL_ERROR, "Missing task data or task limit reached");
          tasks_count = 0;
        } 
        break;
      }
    }
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped! Reason: %s", app_message_result_to_string(reason));
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
  APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending outbox: %s", app_message_result_to_string(reason));

}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

// API functions
#ifdef TESTING
static void fetch_task_lists_testing(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "fetch_task_lists_testing called");
  
  task_lists_count = 0;
  int n = (int)(sizeof(task_lists_testing) / sizeof(task_lists_testing[0]));
  for (int i = 0; i < n && task_lists_count < (int)(sizeof(task_lists) / sizeof(task_lists[0])); i++) {
    snprintf(task_lists[task_lists_count].name, sizeof(task_lists[task_lists_count].name), "%s", task_lists_testing[i]);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "added list name: %s", task_lists[task_lists_count].name);
    task_lists_count++;
  }
  if (s_lists_menu) menu_layer_reload_data(s_lists_menu);
}

static void fetch_tasks_testing(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "fetch_tasks_testing called");
  
  // Sample task names
  const char* sample_names[] = {
    "Complete project report",
    "Buy groceries",
    "Call dentist",
    "Review code changes",
    "Schedule meeting",
    "Fix bug in login",
    "Update documentation",
    "Pay bills",
    "Organize files",
    "Send email to team",
    "Create presentation",
    "Test new feature",
    "Refactor database",
    "Plan vacation",
    "Clean desk",
    "Review design mockups",
    "Deploy to production",
    "Setup development environment",
    "Update dependencies",
    "Write unit tests",
    "Backup important files",
    "Schedule dentist appointment",
    "Renew subscription",
    "Review analytics",
    "Optimize performance",
    "Fix CSS styling",
    "Add error handling",
    "Create user manual",
    "Attend standup meeting",
    "Update status report",
    "Research new tools",
    "Configure CI/CD pipeline",
    "Interview candidates",
    "Approve pull requests",
    "Update project roadmap",
    "Plan sprint",
    "Write blog post",
    "Record tutorial video",
    "Optimize queries",
    "Add logging",
    "Review security",
    "Plan architecture",
    "Migrate data",
    "Setup monitoring",
    "Create dashboard",
    "Document API",
    "Test edge cases",
    "Improve UX",
    "Add caching",
    "Configure webhook"
  };
  
  // Sample due dates
  const char* sample_dates[] = {
    "2026-02-01", "2026-02-05", "2026-02-10", "2026-02-15", "2026-02-20",
    "2026-03-01", "2026-03-05", "2026-03-10", "2026-03-15", "2026-03-20"
  };
  
  tasks_count = 10;
  for (int i = 0; i < tasks_count; i++) {
    // Cycle through list indices
    tasks[i].idx = i % 14;
    
    // Generate unique ID
    snprintf(tasks[i].id, sizeof(tasks[i].id), "task_%d", i);
    
    // Assign sample name
    snprintf(tasks[i].name, sizeof(tasks[i].name), "%s", sample_names[i % 50]);
    
    // Assign priority (0-3)
    tasks[i].priority = i % 4;
    
    // Assign due date
    snprintf(tasks[i].due_date, sizeof(tasks[i].due_date), "%s", sample_dates[i % 10]);
    
    // Roughly 30% completed
    tasks[i].completed = (i % 10 < 3) ? true : false;
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "added task: %s (list idx: %d)", tasks[i].name, tasks[i].idx);
  }
  
  if (s_tasks_menu) menu_layer_reload_data(s_tasks_menu);
}
#endif

void fetch_task_lists(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "fetch_task_lists called");

  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Error beginning outbox: %s", app_message_result_to_string(result));
    return;
  }

  // Send a test message with more data to verify format
  APP_LOG(APP_LOG_LEVEL_ERROR, "Sending KEY_TYPE = 1");
  dict_write_uint8(iter, KEY_TYPE, 1);

  result = app_message_outbox_send();
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending outbox: %s", app_message_result_to_string(result));
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "Message sent successfully");
  }
}

void fetch_tasks(const char *list_name) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "fetch_tasks called for list: %s", list_name);

  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "app_message_outbox_begin failed: %s", app_message_result_to_string(result));
    return;
  }
  dict_write_uint8(iter, KEY_TYPE, 2); // Request tasks
  dict_write_cstring(iter, KEY_ID, list_id);
  dict_write_cstring(iter, KEY_LIST_NAME, list_name);
  app_message_outbox_send();
}

void complete_task(uint8_t task_id, const char *list_name) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "app_message_outbox_begin failed: %s", app_message_result_to_string(result));
    return;
  }
  dict_write_uint8(iter, KEY_TYPE, 3); // Complete task
  dict_write_cstring(iter, KEY_ID, task_id);
  dict_write_cstring(iter, KEY_LIST_NAME, list_name);
  app_message_outbox_send();
}

// Main window
static void lists_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_lists_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_lists_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = lists_menu_get_num_rows,
    .draw_row = lists_menu_draw_row,
    .select_click = lists_menu_select,
  });
  menu_layer_set_click_config_onto_window(s_lists_menu, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_lists_menu));

  // Reset count before fetching task lists
  task_lists_count = 0;

  // Fetch initial data now that menu is ready
#ifdef TESTING
  fetch_task_lists_testing();
#else
  // Only fetch if JS is ready, otherwise wait for ready signal
  if (js_ready) {
    fetch_task_lists();
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "Waiting for JavaScript to be ready...");
  }
#endif
}

static void lists_window_unload(Window *window) {
  if (s_lists_menu) {
    menu_layer_destroy(s_lists_menu);
    s_lists_menu = NULL;
  }
}

static void init(void) {
  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
  // Open AppMessage
  //app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  app_message_open(512, 512);
  
  // Create lists (main) Window
  s_lists_window = window_create();
  window_set_window_handlers(s_lists_window, (WindowHandlers) {
    .load = lists_window_load,
    .unload = lists_window_unload
  });

  // Initialize tasks view
  task_list_view_init();

  // Initialize detail view
  task_detail_view_init();

  window_stack_push(s_lists_window, true);
}

static void deinit(void) {
  if (s_lists_window) window_destroy(s_lists_window);
  task_list_view_deinit();
  task_detail_view_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
