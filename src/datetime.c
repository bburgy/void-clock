#include "datetime.h"

static TextLayer *time_layer;
static TextLayer *date_layer;
static TextLayer *week_day_layer;
static Layer *line_layer;
static GFont milford_font_30;

static const uint8_t text_padding_left = 15;
static const uint8_t top_padding = 35;

static void draw_line_callback(Layer *layer, GContext *context) {
  GRect bounds = layer_get_bounds(layer);
  graphics_draw_line(context, GPoint(0, 0), GPoint(bounds.size.w, 0));
}

void datetime_layers_create(Layer *window_layer, GRect bounds) {
  milford_font_30 = fonts_load_custom_font(
      resource_get_handle(RESOURCE_ID_MILFORD_FONT_30));

  date_layer = text_layer_create(
      GRect(text_padding_left, top_padding,
            bounds.size.w - text_padding_left, 37));
  text_layer_set_background_color(date_layer, GColorClear);
  text_layer_set_text_color(date_layer, GColorBlack);
  text_layer_set_font(date_layer, milford_font_30);
  text_layer_set_text_alignment(date_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(date_layer));

  week_day_layer = text_layer_create(
      GRect(text_padding_left, top_padding + 30,
            bounds.size.w - text_padding_left, 37));
  text_layer_set_background_color(week_day_layer, GColorClear);
  text_layer_set_text_color(week_day_layer, GColorBlack);
  text_layer_set_font(week_day_layer, milford_font_30);
  text_layer_set_text_alignment(week_day_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(week_day_layer));

  line_layer = layer_create(GRect(0, top_padding + 72,
                                   bounds.size.w * 0.9, 1));
  layer_set_update_proc(line_layer, draw_line_callback);
  layer_add_child(window_layer, line_layer);

  time_layer = text_layer_create(
      GRect(text_padding_left, top_padding + 67,
            bounds.size.w - text_padding_left, 60));
  text_layer_set_background_color(time_layer, GColorClear);
  text_layer_set_text_color(time_layer, GColorBlack);
  text_layer_set_font(time_layer,
                      fonts_get_system_font(FONT_KEY_LECO_60_NUMBERS_AM_PM));
  text_layer_set_text_alignment(time_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(time_layer));
}

void datetime_layers_destroy() {
  text_layer_destroy(time_layer);
  text_layer_destroy(date_layer);
  text_layer_destroy(week_day_layer);
  layer_destroy(line_layer);
  fonts_unload_custom_font(milford_font_30);
}

void datetime_update(struct tm *tick_time) {
  static char time_buffer[9];
  static char date_buffer[16];
  static char week_day_buffer[16];

  strftime(time_buffer, sizeof(time_buffer),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  strftime(date_buffer, sizeof(date_buffer), "%B %d", tick_time);
  strftime(week_day_buffer, sizeof(week_day_buffer), "%A", tick_time);

  text_layer_set_text(time_layer, time_buffer);
  text_layer_set_text(date_layer, date_buffer);
  text_layer_set_text(week_day_layer, week_day_buffer);
}
