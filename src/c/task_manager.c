#include <pebble.h>

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

typedef struct {
  char id[32];
  int8_t idx;
  char name[128];
  int8_t priority;
  char due_date[32];
  bool completed;
} Task;

// Storage
static TaskList task_lists[20];
static int task_lists_count = 0;
static Task tasks[50];
static int tasks_count = 0;
static int selected_list_index = 0;
static int selected_task_index = 0;
static bool js_ready = false;  // set when JS signals it's ready


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
static void complete_task(const char *task_id, const char *list_name);
static void show_task_detail(void);
static void tasks_window_load(Window *window);
static void tasks_window_unload(Window *window);
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

  window_stack_push(s_tasks_window, true);

  #ifdef TESTING
    fetch_tasks_testing();
  #else
    fetch_tasks(task_lists[selected_list_index].name);
  #endif
  if (s_tasks_menu) menu_layer_reload_data(s_tasks_menu);
}

// Tasks menu callbacks
static uint16_t tasks_menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "tasks_menu_get_num_rows called");
  return tasks_count;
}

static void tasks_menu_draw_row(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "tasks_menu_draw_row called for row %d", cell_index->row);
  if (cell_index->row < tasks_count) {
    Task *task = &tasks[cell_index->row];
    const char *subtitle = task->completed ? "✓ Completed" : task->due_date;
    menu_cell_basic_draw(ctx, cell_layer, task->name, subtitle, NULL);
  }
}

static void tasks_menu_select(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "tasks_menu_select called for row %d", cell_index->row);
  selected_task_index = cell_index->row;
  show_task_detail();
}

// Task detail window
static Window *s_detail_window;
static TextLayer *s_detail_text_layer;
static ActionBarLayer *s_action_bar;
static GBitmap *s_checkmark_bitmap;
static char s_detail_text[256];

static void detail_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Create text layer for task details
  s_detail_text_layer = text_layer_create(GRect(0, 10, bounds.size.w - ACTION_BAR_WIDTH, bounds.size.h - 20));
  text_layer_set_text_alignment(s_detail_text_layer, GTextAlignmentLeft);
  text_layer_set_overflow_mode(s_detail_text_layer, GTextOverflowModeWordWrap);
  text_layer_set_text(s_detail_text_layer, s_detail_text);
  layer_add_child(window_layer, text_layer_get_layer(s_detail_text_layer));
  
  // Create action bar
  s_action_bar = action_bar_layer_create();
  action_bar_layer_add_to_window(s_action_bar, window);
  
  // Load icon
  s_checkmark_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHECKMARK);
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_SELECT, s_checkmark_bitmap);
}

static void detail_window_unload(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "detail_window_unload called");
  
  text_layer_destroy(s_detail_text_layer);
  action_bar_layer_destroy(s_action_bar);
  gbitmap_destroy(s_checkmark_bitmap);
  window_destroy(s_detail_window);
  s_detail_window = NULL;
}

static void detail_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  Task *task = &tasks[selected_task_index];
  if (!task->completed) {
    // Mark task as complete
    complete_task(task->id, task_lists[selected_list_index].name);
    task->completed = true;
    
    // Update display
    static char detail_text[256];
    snprintf(detail_text, sizeof(detail_text), 
             "Task: %s\n\nDue: %s\n\nStatus: Completed ✓", 
             task->name, task->due_date);
    text_layer_set_text(s_detail_text_layer, detail_text);
    
    // Update the tasks list
    if (s_tasks_menu) menu_layer_reload_data(s_tasks_menu);
  }
}

static void detail_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  window_stack_pop(true);
}

static void detail_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, detail_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, detail_back_click_handler);
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
  snprintf(s_detail_text, sizeof(s_detail_text), 
           "Task: %s\n\nDue: %s\n\nStatus: %s\n\nSelect to mark complete", 
           task->name, 
           task->due_date,
           task->completed ? "Completed ✓" : "Pending");
  
  // Push window to stack (this will trigger the load callback which sets the text)
  window_stack_push(s_detail_window, true);
  
  // Set click config provider
  if (s_action_bar) {
    action_bar_layer_set_click_config_provider(s_action_bar, detail_click_config_provider);
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
        // Parse task data from dictionary
        Tuple *id_tuple = dict_find(iterator, KEY_ID);
        Tuple *name_tuple = dict_find(iterator, KEY_NAME);
        Tuple *due_tuple = dict_find(iterator, KEY_DUE_DATE);
        Tuple *completed_tuple = dict_find(iterator, KEY_COMPLETED);

        if (id_tuple && name_tuple && tasks_count < 50) {
          snprintf(tasks[tasks_count].id, sizeof(tasks[0].id),
                   "%s", id_tuple->value->cstring);
          snprintf(tasks[tasks_count].name, sizeof(tasks[0].name),
                   "%s", name_tuple->value->cstring);
          if (due_tuple) {
            snprintf(tasks[tasks_count].due_date, sizeof(tasks[0].due_date),
                     "%s", due_tuple->value->cstring);
          }
          tasks[tasks_count].completed = completed_tuple ? completed_tuple->value->int32 : 0;

          tasks_count++;
          if (s_tasks_menu) menu_layer_reload_data(s_tasks_menu);
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

static void complete_task(const char *task_id, const char *list_name) {
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

  /*
  // Fetch initial data now that menu is ready
  #ifdef TESTING
    fetch_task_lists_testing();
  #else
    fetch_task_lists();
  #endif
  */
  
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
