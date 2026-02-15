#include <pebble.h>
#include "task_manager.h"
#include "strings.h"
#include "task_list_view.h"

// Static variables
static Window *s_detail_window;
static StatusBarLayer *s_detail_status_bar;
static ScrollLayer *s_scroll_layer;
static TextLayer *s_detail_text_layer;
static ActionBarLayer *s_action_bar;
static GBitmap *s_checkmark_bitmap;
static char s_detail_text[256];

// Forward declarations
static void detail_window_load(Window *window);
static void detail_window_unload(Window *window);
static void detail_select_click_handler(ClickRecognizerRef recognizer, void *context);
static void detail_up_click_handler(ClickRecognizerRef recognizer, void *context);
static void detail_down_click_handler(ClickRecognizerRef recognizer, void *context);
static void detail_click_config_provider(void *context);

// Window callbacks
static void detail_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Add status bar
  s_detail_status_bar = status_bar_layer_create();
  layer_add_child(window_layer, status_bar_layer_get_layer(s_detail_status_bar));

  // Create action bar first so we can get proper unobstructed bounds
  s_action_bar = action_bar_layer_create();
  action_bar_layer_add_to_window(s_action_bar, window);

  // Load icon
  s_checkmark_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHECKMARK);
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_SELECT, s_checkmark_bitmap);

  // Get bounds accounting for action bar and status bar
  int16_t content_y = STATUS_BAR_LAYER_HEIGHT;
  int16_t content_h = bounds.size.h - STATUS_BAR_LAYER_HEIGHT;
  GRect scroll_bounds;
  #if defined(PBL_ROUND)
  int16_t scroll_width = bounds.size.w - ACTION_BAR_WIDTH - 10;
  scroll_bounds = GRect(0, content_y, scroll_width, content_h);
  #else
  scroll_bounds = GRect(0, content_y, bounds.size.w - ACTION_BAR_WIDTH, content_h);
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

  if (s_detail_status_bar) {
    status_bar_layer_destroy(s_detail_status_bar);
    s_detail_status_bar = NULL;
  }
  text_layer_destroy(s_detail_text_layer);
  scroll_layer_destroy(s_scroll_layer);
  action_bar_layer_destroy(s_action_bar);
  gbitmap_destroy(s_checkmark_bitmap);
  // Note: Window itself is destroyed in task_detail_view_deinit(), not here
}

// Click handlers
static void detail_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  Task *task = &tasks[selected_task_index];
  if (!task->completed) {
    // Mark task as complete
    complete_task(task->id, task_lists[selected_list_index].name);
    task->completed = true;

    // Update display
    static char detail_text[256];
    convert_iso_to_friendly_date(task->due_date, s_time_buffer, sizeof(s_time_buffer));
    snprintf(detail_text, sizeof(detail_text),
             "%s%s\n\n%s%s\n\n%s%s",
             STR_TASK_LABEL, task->name,
             STR_DUE_LABEL, s_time_buffer,
             STR_STATUS_LABEL, STR_COMPLETED);
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
    MenuLayer *tasks_menu = task_list_view_get_menu();
    if (tasks_menu) menu_layer_reload_data(tasks_menu);
  }
}

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
}

// Public interface
void task_detail_view_init(void) {
  s_detail_window = window_create();
  window_set_window_handlers(s_detail_window, (WindowHandlers) {
    .load = detail_window_load,
    .unload = detail_window_unload,
  });
}

void task_detail_view_deinit(void) {
  if (s_detail_window) {
    window_destroy(s_detail_window);
    s_detail_window = NULL;
  }
}

void task_detail_view_show(Task *task) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "task_detail_view_show called");

  // Validate task pointer
  if (!task) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "task_detail_view_show: invalid task pointer");
    return;
  }

  // Convert due date to friendly format (handles "No due date" case)
  convert_iso_to_friendly_date(task->due_date, s_time_buffer, sizeof(s_time_buffer));

  snprintf(s_detail_text, sizeof(s_detail_text),
           "%s%s\n\n%s%s\n\n%s%s\n\n%s",
           STR_TASK_LABEL, task->name,
           STR_DUE_LABEL, s_time_buffer,
           STR_STATUS_LABEL, task->completed ? STR_COMPLETED : STR_PENDING,
           STR_SELECT_TO_MARK_COMPLETE);

  // Push window to stack (this will trigger the load callback which sets the text and click config)
  window_stack_push(s_detail_window, true);
}

Window* task_detail_view_get_window(void) {
  return s_detail_window;
}
