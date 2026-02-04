#include <pebble.h>
#include "task_manager.h"
#include "strings.h"
#include "task_detail_view.h"

// Static variables
static Window *s_tasks_window;
static MenuLayer *s_tasks_menu;

// Menu callbacks
static uint16_t tasks_menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "tasks_menu_get_num_rows called");
  // Return at least 1 row to display "No tasks" message when list is empty
  return tasks_count > 0 ? tasks_count : 1;
}

static void tasks_menu_draw_row(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "tasks_menu_draw_row called for row %d", cell_index->row);

  if (tasks_count == 0) {
    menu_cell_basic_draw(ctx, cell_layer, STR_NO_TASKS, STR_NO_TASKS_IN_LIST, NULL);
    return;
  }

  if (cell_index->row < tasks_count) {
    Task *task = &tasks[cell_index->row];
    convert_iso_to_friendly_date(task->due_date, s_time_buffer, sizeof(s_time_buffer));

    const char *subtitle = task->completed ? STR_COMPLETED : s_time_buffer;
    menu_cell_basic_draw(ctx, cell_layer, task->name, subtitle, NULL);
  }
}

static void tasks_menu_select(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "tasks_menu_select called for row %d", cell_index->row);

  // Don't open detail view if there are no tasks
  if (tasks_count == 0) {
    return;
  }

  // Validate the selected index
  if (cell_index->row >= tasks_count) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid task index: %d", cell_index->row);
    return;
  }

  selected_task_index = cell_index->row;
  task_detail_view_show(&tasks[selected_task_index]);
}

// Window callbacks
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

// Public interface
void task_list_view_init(void) {
  s_tasks_window = window_create();
  window_set_window_handlers(s_tasks_window, (WindowHandlers) {
    .load = tasks_window_load,
    .unload = tasks_window_unload
  });
}

void task_list_view_deinit(void) {
  if (s_tasks_window) {
    window_destroy(s_tasks_window);
    s_tasks_window = NULL;
  }
}

Window* task_list_view_get_window(void) {
  return s_tasks_window;
}

MenuLayer* task_list_view_get_menu(void) {
  return s_tasks_menu;
}
