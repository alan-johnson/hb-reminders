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

#define TESTING 1
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

// data for testing without javascript connection
#define TESTING 1

#if TESTING
const char* task_lists_str[] = {
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
static void fetch_task_lists_testing(void);
static void fetch_task_lists(void);
static void fetch_task_lists_testing(void);
static void fetch_tasks(const char *list_name);
static void complete_task(const char *task_id, const char *list_name);
static void show_task_detail(void);

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
  // Push tasks window first so its menu exists
  if (!s_tasks_window) {
    s_tasks_window = window_create();
    window_set_window_handlers(s_tasks_window, (WindowHandlers) {
      .load = tasks_window_load,
      .unload = tasks_window_unload,
    });
  }
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
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox_received_callback called");
  
  Tuple *type_tuple = dict_find(iterator, KEY_TYPE);
  
  if (type_tuple) {
    int type = type_tuple->value->int32;
    
    switch(type) {
      case 1: { // Task list names
        APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox, received list names");
        task_lists_count = 0;
        Tuple *name_tuple = dict_find(iterator, KEY_NAME);
        while (name_tuple && task_lists_count < 20) {
          snprintf(task_lists[task_lists_count].name, sizeof(task_lists[0].name), 
                   "%s", name_tuple->value->cstring);
          APP_LOG(APP_LOG_LEVEL_DEBUG, "added list name: %s", task_lists[task_lists_count].name);
          task_lists_count++;
          name_tuple = dict_read_next(iterator);
        }
        if (s_lists_menu) menu_layer_reload_data(s_lists_menu);
        break;
      }
      
      case 2: { // Tasks in a list
        tasks_count = 0;
        // Parse task data from dictionary
        Tuple *id_tuple = dict_find(iterator, KEY_ID);
        Tuple *name_tuple = dict_find(iterator, KEY_NAME);
        Tuple *due_tuple = dict_find(iterator, KEY_DUE_DATE);
        Tuple *completed_tuple = dict_find(iterator, KEY_COMPLETED);
        
        while (id_tuple && name_tuple && tasks_count < 50) {
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
          id_tuple = dict_read_next(iterator);
          name_tuple = dict_read_next(iterator);
          due_tuple = dict_read_next(iterator);
          completed_tuple = dict_read_next(iterator);
        }
        if (s_tasks_menu) menu_layer_reload_data(s_tasks_menu);
        break;
      }
    }
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

// API functions
static void fetch_task_lists(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "fetch_task_lists called");

  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, KEY_TYPE, 1); // Request task lists
  app_message_outbox_send();
}

static void fetch_tasks(const char *list_name) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, KEY_TYPE, 2); // Request tasks
  dict_write_cstring(iter, KEY_LIST_NAME, list_name);
  app_message_outbox_send();
}

static void complete_task(const char *task_id, const char *list_name) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, KEY_TYPE, 3); // Complete task
  dict_write_cstring(iter, KEY_ID, task_id);
  dict_write_cstring(iter, KEY_LIST_NAME, list_name);
  app_message_outbox_send();
}

// Main window
static void tasks_window_unload(Window *window) {
  // When tasks window is closed we should return to lists state
  current_state = STATE_TASK_LISTS;
  // Ensure lists menu is refreshed
  if (s_lists_menu) {
    menu_layer_reload_data(s_lists_menu);
    menu_layer_set_selected_index(s_lists_menu, (MenuIndex){.section = 0, .row = selected_list_index}, MenuRowAlignCenter, false);
  }
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
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  
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
  
  // Fetch initial data
  fetch_task_lists();
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
