#include "icons.h"

static Layer *battery_layer;
static Layer *bluetooth_layer;
static Layer *empty_battery_layer;
static Layer *silent_mode_layer;
static Layer *alarm_layer;

static uint8_t charge_percent = 0;
static const uint8_t battery_line_width = 6;

static void draw_battery_line_callback(Layer *layer, GContext *context) {
  GRect bounds = layer_get_bounds(layer);
  uint8_t line_height = charge_percent * bounds.size.h / 100;
  GRect rect_bounds = GRect(0, bounds.size.h - line_height,
                             battery_line_width, line_height);
  graphics_draw_rect(context, rect_bounds);
  graphics_fill_rect(context, rect_bounds, 0, GCornersAll);
}

static void draw_bluetooth_callback(Layer *layer, GContext *context) {
  graphics_context_set_stroke_color(context, GColorBlack);
  graphics_context_set_stroke_width(context, 2);

  graphics_draw_line(context, GPoint(6, 3), GPoint(24, 3));
  graphics_draw_line(context, GPoint(24, 3), GPoint(24, 31));
  graphics_draw_line(context, GPoint(24, 31), GPoint(6, 31));
  graphics_draw_line(context, GPoint(6, 31), GPoint(6, 3));

  graphics_draw_line(context, GPoint(6, 4), GPoint(7, 3));
  graphics_draw_line(context, GPoint(23, 3), GPoint(24, 4));
  graphics_draw_line(context, GPoint(24, 30), GPoint(23, 31));
  graphics_draw_line(context, GPoint(7, 31), GPoint(6, 30));

  graphics_context_set_stroke_color(context, GColorRed);
  graphics_context_set_stroke_width(context, 2);
  graphics_draw_line(context, GPoint(11, 9), GPoint(19, 21));
  graphics_draw_line(context, GPoint(19, 9), GPoint(11, 21));

  graphics_context_set_fill_color(context, GColorBlack);
  graphics_fill_circle(context, GPoint(15, 27), 2);
}

static void draw_empty_battery_callback(Layer *layer, GContext *context) {
  graphics_context_set_stroke_color(context, GColorBlack);
  graphics_context_set_stroke_width(context, 3);
  graphics_draw_line(context, GPoint(2, 6), GPoint(18, 6));
  graphics_draw_line(context, GPoint(2, 6), GPoint(2, 26));
  graphics_draw_line(context, GPoint(2, 26), GPoint(18, 26));
  graphics_draw_line(context, GPoint(18, 10), GPoint(18, 22));

  graphics_draw_line(context, GPoint(18, 10), GPoint(22, 10));
  graphics_draw_line(context, GPoint(22, 10), GPoint(22, 22));
  graphics_draw_line(context, GPoint(22, 22), GPoint(18, 22));

  graphics_context_set_stroke_color(context, GColorRed);
  graphics_context_set_stroke_width(context, 3);
  graphics_draw_line(context, GPoint(5, 9), GPoint(15, 23));
  graphics_draw_line(context, GPoint(15, 9), GPoint(5, 23));
}

static void draw_silent_mode_callback(Layer *layer, GContext *context) {
  graphics_context_set_stroke_color(context, GColorBlack);
  graphics_context_set_stroke_width(context, 3);
  graphics_draw_line(context, GPoint(1, 4), GPoint(13, 4));
  graphics_draw_line(context, GPoint(13, 4), GPoint(1, 16));
  graphics_draw_line(context, GPoint(1, 16), GPoint(10, 16));

  graphics_context_set_stroke_color(context, GColorRed);
  graphics_context_set_stroke_width(context, 2);
  graphics_draw_line(context, GPoint(14, 10), GPoint(21, 10));
  graphics_draw_line(context, GPoint(21, 10), GPoint(14, 20));
  graphics_draw_line(context, GPoint(14, 20), GPoint(19, 20));

  graphics_draw_line(context, GPoint(21, 18), GPoint(26, 18));
  graphics_draw_line(context, GPoint(26, 18), GPoint(21, 26));
  graphics_draw_line(context, GPoint(21, 26), GPoint(24, 26));
}

static void draw_alarm_callback(Layer *layer, GContext *context) {
  graphics_context_set_fill_color(context, GColorBlack);
  graphics_fill_circle(context, GPoint(9, 7), 3);
  graphics_fill_circle(context, GPoint(23, 7), 3);

  graphics_context_set_stroke_color(context, GColorBlack);
  graphics_context_set_stroke_width(context, 2);
  graphics_draw_circle(context, GPoint(16, 20), 10);

  // Clock hands in red (warning/accent color)
  graphics_context_set_stroke_color(context, GColorRed);
  graphics_draw_line(context, GPoint(16, 20), GPoint(11, 16));
  graphics_draw_line(context, GPoint(16, 20), GPoint(22, 15));

  // Legs in black
  graphics_context_set_stroke_color(context, GColorBlack);
  graphics_draw_line(context, GPoint(10, 30), GPoint(8, 33));
  graphics_draw_line(context, GPoint(22, 30), GPoint(24, 33));
}

void icons_layers_create(Layer *window_layer, GRect bounds) {
  alarm_layer = layer_create(GRect(bounds.size.w - 186, 0, 32, 36));
  layer_set_update_proc(alarm_layer, draw_alarm_callback);
  layer_add_child(window_layer, alarm_layer);
  layer_set_hidden(alarm_layer, true);

  silent_mode_layer = layer_create(GRect(bounds.size.w - 138, 0, 36, 36));
  layer_set_update_proc(silent_mode_layer, draw_silent_mode_callback);
  layer_add_child(window_layer, silent_mode_layer);
  layer_set_hidden(silent_mode_layer, true);

  bluetooth_layer = layer_create(GRect(bounds.size.w - 86, 0, 32, 36));
  layer_set_update_proc(bluetooth_layer, draw_bluetooth_callback);
  layer_add_child(window_layer, bluetooth_layer);

  empty_battery_layer = layer_create(GRect(bounds.size.w - 38, 0, 32, 36));
  layer_set_update_proc(empty_battery_layer, draw_empty_battery_callback);
  layer_add_child(window_layer, empty_battery_layer);

  battery_layer = layer_create(GRect(bounds.size.w - battery_line_width, 0,
                                      battery_line_width, bounds.size.h));
  layer_set_update_proc(battery_layer, draw_battery_line_callback);
  layer_add_child(window_layer, battery_layer);
}

void icons_layers_destroy() {
  layer_destroy(battery_layer);
  layer_destroy(bluetooth_layer);
  layer_destroy(empty_battery_layer);
  layer_destroy(silent_mode_layer);
  layer_destroy(alarm_layer);
}

void icons_update_battery_line(uint8_t percent) {
  charge_percent = percent;
  layer_mark_dirty(battery_layer);
}

void icons_set_bluetooth_shown(bool shown) {
  layer_set_hidden(bluetooth_layer, !shown);
}

void icons_set_empty_battery_shown(bool shown) {
  layer_set_hidden(empty_battery_layer, !shown);
}

void icons_set_quiet_time_shown(bool shown) {
  layer_set_hidden(silent_mode_layer, !shown);
}

void icons_set_alarm_shown(bool shown) {
  layer_set_hidden(alarm_layer, !shown);
}
