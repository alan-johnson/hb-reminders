#include <pebble.h>
#include <time.h>
#include <stdio.h>

// Windows
static Window *s_lists_window;
static MenuLayer *s_lists_menu;

static Window *s_tasks_window;
static MenuLayer *s_tasks_menu;

// Navigation state
typedef enum {
  STATE_TASK_LISTS,
  STATE_TASKS,
  STATE_TASK_DETAIL
} AppState;

static AppState current_state = STATE_TASK_LISTS;

// Data structures
typedef struct {
  char name[64];
} TaskList;

// structure of apple reminder id field
//          8     -  4  -  4  -  4  - 12
//example: 65A7AD7B-F211-4E3D-A892-E47CE8B038B5
typedef struct {
  char id[33];        // 33 bytes, 32 chars + null terminator
  int8_t idx;         // 1 byte 
  char name[128];     // 128 bytes
  int8_t priority;    // 1 byte
  char due_date[32];  // 32 bytes
  bool completed;     // 1 byte
} Task;               // total: 196 bytes

// Storage
static TaskList task_lists[20]; //1280 bytes
static int task_lists_count = 0;
static Task tasks[50];  //9800 bytes
static int tasks_count = 0;
static int selected_list_index = 0;
static int selected_task_index = 0;
static bool js_ready = false;  // set when JS signals it's ready
// A buffer to hold the time and date string
static char s_time_buffer[30]; // Large enough for "Day Month DD HH:MM"

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

// API callback keys
#define KEY_TYPE 0
#define KEY_NAME 1
#define KEY_ID 2
#define KEY_DUE_DATE 3
#define KEY_COMPLETED 4
#define KEY_LIST_NAME 5
#define KEY_IDX 6
#define KEY_PRIORITY 7

// Function prototypes
static void fetch_task_lists(void);
static void fetch_tasks(const char *list_name);
static void complete_task(uint8_t task_id, const char *list_name);
static void show_task_detail(void);
static void tasks_window_load(Window *window);
static void tasks_window_unload(Window *window);
static void lists_window_load(Window *window);
static void lists_window_unload(Window *window);
static time_t convert_iso_to_time_t(const char* iso_date_str);
static void detail_click_config_provider(void *context);
static void detail_select_click_handler(ClickRecognizerRef recognizer, void *context);
static void detail_up_click_handler(ClickRecognizerRef recognizer, void *context);
static void detail_down_click_handler(ClickRecognizerRef recognizer, void *context);
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

  window_stack_push(s_tasks_window, true);

  #ifdef TESTING
    fetch_tasks_testing();
  #else
    fetch_tasks(task_lists[selected_list_index].name);
  #endif
  if (s_tasks_menu) menu_layer_reload_data(s_tasks_menu);
}

static time_t convert_iso_to_time_t(const char* iso_date_str) {
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

// Tasks menu callbacks
static uint16_t tasks_menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "tasks_menu_get_num_rows called");
  // Return at least 1 row to display "No tasks" message when list is empty
  return tasks_count > 0 ? tasks_count : 1;
}

static void tasks_menu_draw_row(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "tasks_menu_draw_row called for row %d", cell_index->row);
  
  if (tasks_count == 0) {
    menu_cell_basic_draw(ctx, cell_layer, "No tasks", "No tasks in list", NULL);
    return;
  }

  if (cell_index->row < tasks_count) {
    Task *task = &tasks[cell_index->row];
    time_t due_time = convert_iso_to_time_t(task->due_date);
    struct tm *local_time = localtime(&due_time);
    
    // Format the time and date based on user preference
    if (clock_is_24h_style()) {
      // 24-hour format: "Mon Feb 15 14:30"
      strftime(s_time_buffer, sizeof(s_time_buffer), "%a %b %d %H:%M", local_time);
    } else {
      // 12-hour format with AM/PM: "Mon Feb 15 2:30 PM"
      strftime(s_time_buffer, sizeof(s_time_buffer), "%a %b %d %I:%M %p", local_time);
    }

    const char *subtitle = task->completed ? "✓ Completed" : s_time_buffer;
    menu_cell_basic_draw(ctx, cell_layer, task->name, subtitle, NULL);
  }
}

static void tasks_menu_select(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "tasks_menu_select called for row %d", cell_index->row);

  // Don't open detail view if there are no tasks
  if (tasks_count == 0) {
    return;
  }

  selected_task_index = cell_index->row;
  show_task_detail();
}

// Task detail window
static Window *s_detail_window;
static ScrollLayer *s_scroll_layer;
static TextLayer *s_detail_text_layer;
static ActionBarLayer *s_action_bar;
static GBitmap *s_checkmark_bitmap;
static char s_detail_text[256];

static void detail_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create action bar first so we can get proper unobstructed bounds
  s_action_bar = action_bar_layer_create();
  action_bar_layer_add_to_window(s_action_bar, window);

  // Load icon
  s_checkmark_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHECKMARK);
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_SELECT, s_checkmark_bitmap);

  // Get bounds accounting for action bar
  GRect scroll_bounds;
  #if defined(PBL_ROUND)
  // For round displays, start at origin and account for action bar
  // Don't use unobstructed origin to avoid positioning issues
  int16_t scroll_width = bounds.size.w - ACTION_BAR_WIDTH - 10; // Extra margin for action bar
  scroll_bounds = GRect(0, 0, scroll_width, bounds.size.h);
  #else
  // For rectangular displays, account for action bar width
  scroll_bounds = GRect(0, 0, bounds.size.w - ACTION_BAR_WIDTH, bounds.size.h);
  #endif

  // Create scroll layer
  s_scroll_layer = scroll_layer_create(scroll_bounds);
  // Don't set click config here - we'll do it manually in the provider to avoid conflicts

  // Enable scroll layer content indicator (shows scroll position)
  #ifdef PBL_COLOR
  scroll_layer_set_shadow_hidden(s_scroll_layer, false);
  #endif

  // Create text layer for task details with padding
  #if defined(PBL_ROUND)
  // For round, use generous padding on all sides to avoid clipping at curved edges
  // Horizontal padding needs to be larger to account for the circular display
  int padding_horizontal = 25; // Larger horizontal padding for round edges
  int padding_vertical = 20;   // Vertical padding for top/bottom
  GRect text_bounds = GRect(padding_horizontal, padding_vertical,
                            scroll_bounds.size.w - (2 * padding_horizontal),
                            2000); // Large height for scrolling
  #else
  // For rectangular, standard padding
  GRect text_bounds = GRect(8, 5, scroll_bounds.size.w - 16, 2000);
  #endif

  s_detail_text_layer = text_layer_create(text_bounds);

  // Set text alignment based on display type
  #if defined(PBL_ROUND)
  text_layer_set_text_alignment(s_detail_text_layer, GTextAlignmentCenter);
  #else
  text_layer_set_text_alignment(s_detail_text_layer, GTextAlignmentLeft);
  #endif

  text_layer_set_overflow_mode(s_detail_text_layer, GTextOverflowModeWordWrap);
  text_layer_set_text(s_detail_text_layer, s_detail_text);

  // Get the actual content size after setting text
  GSize text_size = text_layer_get_content_size(s_detail_text_layer);
  text_layer_set_size(s_detail_text_layer, text_size);

  // Set the scroll layer content size
  // Must account for the text layer's y offset (top padding) plus bottom padding
  #if defined(PBL_ROUND)
  int16_t content_height = padding_vertical + text_size.h + padding_vertical;
  #else
  int16_t content_height = 5 + text_size.h + 10; // top padding + text + bottom padding
  #endif
  scroll_layer_set_content_size(s_scroll_layer,
                                 GSize(scroll_bounds.size.w, content_height));

  // Add text layer to scroll layer
  scroll_layer_add_child(s_scroll_layer, text_layer_get_layer(s_detail_text_layer));

  // Add scroll layer to window
  layer_add_child(window_layer, scroll_layer_get_layer(s_scroll_layer));

  // Set up click config provider with scroll layer as context
  window_set_click_config_provider_with_context(window, detail_click_config_provider, s_scroll_layer);
}

static void detail_window_unload(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "detail_window_unload called");

  text_layer_destroy(s_detail_text_layer);
  scroll_layer_destroy(s_scroll_layer);
  action_bar_layer_destroy(s_action_bar);
  gbitmap_destroy(s_checkmark_bitmap);
  window_destroy(s_detail_window);
  s_detail_window = NULL;
}

static void detail_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  Task *task = &tasks[selected_task_index];
  if (!task->completed) {
    // Mark task as complete
    complete_task(task->idx, task_lists[selected_list_index].name);
    task->completed = true;

    // Update display
    static char detail_text[256];
    snprintf(detail_text, sizeof(detail_text),
             "Task: %s\n\nDue: %s\n\nStatus: Completed ✓",
             task->name, task->due_date);
    text_layer_set_text(s_detail_text_layer, detail_text);

    // Update scroll layer content size
    GSize text_size = text_layer_get_content_size(s_detail_text_layer);
    text_layer_set_size(s_detail_text_layer, text_size);
    GRect scroll_bounds = layer_get_bounds(scroll_layer_get_layer(s_scroll_layer));

    // Account for padding (same calculation as in detail_window_load)
    #if defined(PBL_ROUND)
    int padding_vertical = 20;
    int16_t content_height = padding_vertical + text_size.h + padding_vertical;
    #else
    int16_t content_height = 5 + text_size.h + 10;
    #endif
    scroll_layer_set_content_size(s_scroll_layer,
                                   GSize(scroll_bounds.size.w, content_height));

    // Update the tasks list
    if (s_tasks_menu) menu_layer_reload_data(s_tasks_menu);
  }
}

// Back button handler no longer needed - window stack handles this automatically
// static void detail_back_click_handler(ClickRecognizerRef recognizer, void *context) {
//   window_stack_pop(true);
// }

static void detail_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Scroll up by moving content offset down (more content visible at top)
  ScrollLayer *scroll_layer = (ScrollLayer *)context;
  GPoint offset = scroll_layer_get_content_offset(scroll_layer);
  offset.y += 20; // Scroll up 20 pixels
  if (offset.y > 0) offset.y = 0; // Don't scroll past the top
  scroll_layer_set_content_offset(scroll_layer, offset, true);
}

static void detail_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Scroll down by moving content offset up (more content visible at bottom)
  ScrollLayer *scroll_layer = (ScrollLayer *)context;
  GPoint offset = scroll_layer_get_content_offset(scroll_layer);
  GSize content_size = scroll_layer_get_content_size(scroll_layer);
  GRect frame = layer_get_frame(scroll_layer_get_layer(scroll_layer));

  offset.y -= 20; // Scroll down 20 pixels
  int16_t min_offset = frame.size.h - content_size.h;
  if (min_offset > 0) min_offset = 0; // If content fits, don't scroll
  if (offset.y < min_offset) offset.y = min_offset; // Don't scroll past the bottom
  scroll_layer_set_content_offset(scroll_layer, offset, true);
}

static void detail_click_config_provider(void *context) {
  // Set up SELECT button for action bar
  window_single_click_subscribe(BUTTON_ID_SELECT, detail_select_click_handler);

  // Set up UP and DOWN buttons for scrolling
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, detail_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, detail_down_click_handler);

  // BACK button is handled automatically by window stack
  // window_single_click_subscribe(BUTTON_ID_BACK, detail_back_click_handler);
}

static void show_task_detail(void) {
 
  APP_LOG(APP_LOG_LEVEL_DEBUG, "show_task_detail called");
  
  if (!s_detail_window) {
    s_detail_window = window_create();
    window_set_window_handlers(s_detail_window, (WindowHandlers) {
      .load = detail_window_load,
      .unload = detail_window_unload,
    });
  }
  
  // Prepare the task details text
  Task *task = &tasks[selected_task_index];
  time_t due_time = convert_iso_to_time_t(task->due_date);
  struct tm *local_time = localtime(&due_time);
  
  // Format the time and date based on user preference
  if (clock_is_24h_style()) {
    // 24-hour format: "Mon Feb 15 14:30"
    strftime(s_time_buffer, sizeof(s_time_buffer), "%a %b %d %H:%M", local_time);
  } else {
    // 12-hour format with AM/PM: "Mon Feb 15 2:30 PM"
    strftime(s_time_buffer, sizeof(s_time_buffer), "%a %b %d %I:%M %p", local_time);
  }

  snprintf(s_detail_text, sizeof(s_detail_text), 
           "Task: %s\n\nDue: %s\n\nStatus: %s\n\nSelect to mark complete", 
           task->name, 
           s_time_buffer,
           task->completed ? "Completed ✓" : "Pending");
  
  // Push window to stack (this will trigger the load callback which sets the text and click config)
  window_stack_push(s_detail_window, true);
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
          if (s_tasks_menu) menu_layer_reload_data(s_tasks_menu);
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

static void fetch_task_lists(void) {
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

static void fetch_tasks(const char *list_name) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "fetch_tasks called for list: %s", list_name);

  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "app_message_outbox_begin failed: %s", app_message_result_to_string(result));
    return;
  }
  dict_write_uint8(iter, KEY_TYPE, 2); // Request tasks
  dict_write_cstring(iter, KEY_LIST_NAME, list_name);
  app_message_outbox_send();
}

static void complete_task(uint8_t task_id, const char *list_name) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "app_message_outbox_begin failed: %s", app_message_result_to_string(result));
    return;
  }
  dict_write_uint8(iter, KEY_TYPE, 3); // Complete task
  dict_write_uint8(iter, KEY_ID, task_id);
  dict_write_cstring(iter, KEY_LIST_NAME, list_name);
  app_message_outbox_send();
}

// Main window
static void tasks_window_unload(Window *window) {
  // When tasks window is closed we should return to lists state
  current_state = STATE_TASK_LISTS;
  if (s_tasks_menu) {
    menu_layer_destroy(s_tasks_menu);
    s_tasks_menu = NULL;
  }
}

static void tasks_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_tasks_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_tasks_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = tasks_menu_get_num_rows,
    .draw_row = tasks_menu_draw_row,
    .select_click = tasks_menu_select,
  });
  menu_layer_set_click_config_onto_window(s_tasks_menu, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_tasks_menu));
}

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
  // Create tasks window and set handlers
  s_tasks_window = window_create();
  window_set_window_handlers(s_tasks_window, (WindowHandlers) {
    .load = tasks_window_load,
    .unload = tasks_window_unload
  });

  window_stack_push(s_lists_window, true);
}

static void deinit(void) {
  if (s_lists_window) window_destroy(s_lists_window);
  if (s_tasks_window) window_destroy(s_tasks_window);
  if (s_detail_window) window_destroy(s_detail_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
