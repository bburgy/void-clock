/**
 * @Author: Burgy Benjamin
 * @Date: 2016-02-16T19:20:15+01:00
 * @Email: benjamin@burgy.swiss
 * @Last modified by: benjamin
 * @Last modified time: 2026-06-03T20:57:11+02:00
 */

#include "layers.h"
#include <pebble.h>

static TextLayer *time_layer;
static TextLayer *date_layer;
static TextLayer *week_day_layer;

static Layer *line_layer;
static Layer *battery_layer;
static Layer *window_layer;
static Layer *bluetooth_layer;
static Layer *empty_battery_layer;
static Layer *silent_mode_layer;

static GFont milford_font_30;

static GRect window_bounds;

static uint8_t charge_percent = 0;
static uint8_t text_padding_left = 15;
static uint8_t battery_line_width = 6;
static uint8_t top_padding = 35;

// Empty-battery warning threshold. When charge drops below this percentage,
// the red empty-battery icon appears in the status cluster.
//
// Rationale: 10 % gives users more time to find a charger before the watch
// shuts down. PebbleOS itself warns at ~10 %, so this aligns with system
// behavior.
#define EMPTY_BATTERY_THRESHOLD_PERCENT 10

// Bluetooth disconnect debounce (ms).
//
// The firmware internally debounces disconnections for ~25 s, but a further
// app-level delay prevents spurious icon flashing on transient BLE drops.
//
// Firmware evolution context:
//   * v4.9.175 (May 2026)  — "Speculative fix" for standby spurious disconnects.
//   * v4.31.2 (Jul 2026)  — "Several power consumption bug fixes."
//   * v4.33.0 (Aug 2026)  — "Fixed battery drain from fast advertising sticking
//                            on after airplane mode."
//
// The stack is now much more stable, so a SHORT debounce is sufficient to
// smooth remaining edge-of-range twitches without hiding real disconnections.
// Tunable as needed:
//
//   0     — no debounce; instant feedback (latest firmware only)
//   15000 — catches brief twitches, fast real-disconnect feedback (RECOMMENDED)
//   60000 — maximum smoothing for very old firmware (overkill post-v4.31)
//
// See bluetooth_debounce_callback() for the live re-check on expiry.
#define BLUETOOTH_DISCONNECT_DEBOUNCE_MS 15000

// NULL when no debounce timer is pending, otherwise the running AppTimer.
static AppTimer *bluetooth_debounce_timer = NULL;
// Whether the "no Bluetooth" icon is currently displayed. The icon stays
// hidden while a debounce timer is pending, even if the OS reports a
// disconnection.
static bool bluetooth_icon_shown = false;
static bool silent_mode_shown = false;

static void draw_battery_line_callback(Layer *layer, GContext *context) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing the battery line ...");
#endif

  uint8_t lineHeight = charge_percent * window_bounds.size.h / 100;
  GRect rect_bounds = GRect(0, window_bounds.size.h - lineHeight,
                            battery_line_width, lineHeight);

  // Draw a rectangle
  graphics_draw_rect(context, rect_bounds);

  // Fill rectangle
  graphics_fill_rect(context, rect_bounds, 0, GCornersAll);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void draw_line_callback(Layer *layer, GContext *context) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing the line ...");
#endif

  GPoint start = GPoint(0, 0);
  GPoint end = GPoint(window_bounds.size.w * 0.9, 0);

  graphics_draw_line(context, start, end);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void draw_bluetooth_callback(Layer *layer, GContext *context) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing bluetooth icon layer ...");
#endif

  // Thin phone outline (black, 2px stroke)
  graphics_context_set_stroke_color(context, GColorBlack);
  graphics_context_set_stroke_width(context, 2);

  // Tall rectangle 18×28 with slight corner rounding
  graphics_draw_line(context, GPoint(6, 3),  GPoint(24, 3));   // top
  graphics_draw_line(context, GPoint(24, 3), GPoint(24, 31));  // right
  graphics_draw_line(context, GPoint(24, 31), GPoint(6, 31));  // bottom
  graphics_draw_line(context, GPoint(6, 31), GPoint(6, 3));    // left

  // Subtle corner rounding (45° segments, 1px each)
  graphics_draw_line(context, GPoint(6, 4),  GPoint(7, 3));
  graphics_draw_line(context, GPoint(23, 3), GPoint(24, 4));
  graphics_draw_line(context, GPoint(24, 30), GPoint(23, 31));
  graphics_draw_line(context, GPoint(7, 31), GPoint(6, 30));

  // Red X inside (2px stroke)
  graphics_context_set_stroke_color(context, GColorRed);
  graphics_context_set_stroke_width(context, 2);
  graphics_draw_line(context, GPoint(11, 9), GPoint(19, 21));
  graphics_draw_line(context, GPoint(19, 9), GPoint(11, 21));

  // Home button (small filled circle at bottom center)
  graphics_context_set_fill_color(context, GColorBlack);
  graphics_fill_circle(context, GPoint(15, 27), 2);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void draw_empty_battery_callback(Layer *layer, GContext *context) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing empty battery icon layer ...");
#endif

  // Battery body (black)
  graphics_context_set_stroke_color(context, GColorBlack);
  graphics_context_set_stroke_width(context, 3);
  graphics_draw_line(context, GPoint(2, 6),  GPoint(18, 6));   // top
  graphics_draw_line(context, GPoint(2, 6),  GPoint(2, 26));   // left
  graphics_draw_line(context, GPoint(2, 26), GPoint(18, 26));  // bottom
  graphics_draw_line(context, GPoint(18, 10), GPoint(18, 22)); // right (gap)

  // Battery nipple (black)
  graphics_draw_line(context, GPoint(18, 10), GPoint(22, 10));
  graphics_draw_line(context, GPoint(22, 10), GPoint(22, 22));
  graphics_draw_line(context, GPoint(22, 22), GPoint(18, 22));

  // Red X inside
  graphics_context_set_stroke_color(context, GColorRed);
  graphics_context_set_stroke_width(context, 3);
  graphics_draw_line(context, GPoint(5, 9),  GPoint(15, 23));
  graphics_draw_line(context, GPoint(15, 9), GPoint(5, 23));

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void prepare_battery_line_layer() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing the battery line layer ...");
#endif

  battery_layer =
      layer_create(GRect(window_bounds.size.w - battery_line_width, 0,
                         battery_line_width, window_bounds.size.h));
  layer_set_update_proc(battery_layer, draw_battery_line_callback);
  layer_add_child(window_layer, battery_layer);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void prepare_date_layer() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing the date ...");
#endif

  date_layer =
      text_layer_create(GRect(text_padding_left, top_padding,
                              window_bounds.size.w - text_padding_left, 37));

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "Date layer pointer initialized: %p",
          date_layer);
#endif

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(date_layer, GColorClear);
  text_layer_set_text_color(date_layer, GColorBlack);
  text_layer_set_font(date_layer, milford_font_30);
  text_layer_set_text_alignment(date_layer, GTextAlignmentLeft);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(date_layer));

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void prepare_weekday_layer() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing the week of day ...");
#endif

  week_day_layer =
      text_layer_create(GRect(text_padding_left, top_padding + 30,
                              window_bounds.size.w - text_padding_left, 37));

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "Week layer pointer initialized: %p",
          week_day_layer);
#endif

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(week_day_layer, GColorClear);
  text_layer_set_text_color(week_day_layer, GColorBlack);
  text_layer_set_font(week_day_layer, milford_font_30);
  text_layer_set_text_alignment(week_day_layer, GTextAlignmentLeft);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(week_day_layer));

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void prepare_line_layer() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing the line layer ...");
#endif

  line_layer =
      layer_create(GRect(0, top_padding + 72, window_bounds.size.w * 0.9, 1));
  layer_set_update_proc(line_layer, draw_line_callback);
  layer_add_child(window_layer, line_layer);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void prepare_time_layer() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing the time ...");
#endif

  time_layer =
      text_layer_create(GRect(text_padding_left, top_padding + 67,
                              window_bounds.size.w - text_padding_left, 60));

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "Time layer pointer initialized: %p",
          time_layer);
#endif

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(time_layer, GColorClear);
  text_layer_set_text_color(time_layer, GColorBlack);
  text_layer_set_font(time_layer,
                      fonts_get_system_font(FONT_KEY_LECO_60_NUMBERS_AM_PM));
  text_layer_set_text_alignment(time_layer, GTextAlignmentLeft);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(time_layer));

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void update_empty_battery_icon(status_t isEmpty) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Updating the battery icon ...");
#endif

  layer_set_hidden(empty_battery_layer, !isEmpty);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void update_battery_line(uint8_t percent) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Updating the battery line ...");
#endif

  charge_percent = percent;
  layer_mark_dirty(battery_layer);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void prepare_bluetooth_layer() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing bluetooth icon layer ...");
#endif

  // Emery status icon cluster layout (200 px wide screen):
  //   Silent:  95–131 (36 px) | BT: 131–163 (32 px) | Empty: 163–195 (32 px)
  //   Battery bar: 194–200 (6 px) — 1 px gap to empty battery icon
  //
  // Icon canvas includes 2 px padding on all sides to prevent stroke-bleed
  // clipping from PebbleOS anti-aliasing (worst: 4 px diagonal + 2 px bleed).
  int width = 32;
  int height = 36;
  int x = window_bounds.size.w - 73;
  int y = 0;

  bluetooth_layer = layer_create(GRect(x, y, width, height));
  layer_set_update_proc(bluetooth_layer, draw_bluetooth_callback);
  layer_add_child(window_layer, bluetooth_layer);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void prepare_empty_battery_layer() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing empty battery icon layer ...");
#endif

  // See prepare_bluetooth_layer() for cluster layout comment.
  int width = 32;
  int height = 36;
  int x = window_bounds.size.w - 45;
  int y = 0;

  empty_battery_layer = layer_create(GRect(x, y, width, height));
  layer_set_update_proc(empty_battery_layer, draw_empty_battery_callback);
  layer_add_child(window_layer, empty_battery_layer);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void draw_silent_mode_callback(Layer *layer, GContext *context) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing silent mode icon layer ...");
#endif

  // Large black Z
  graphics_context_set_stroke_color(context, GColorBlack);
  graphics_context_set_stroke_width(context, 3);
  graphics_draw_line(context, GPoint(1, 4), GPoint(13, 4));
  graphics_draw_line(context, GPoint(13, 4), GPoint(1, 16));
  graphics_draw_line(context, GPoint(1, 16), GPoint(10, 16));

  // Medium red z
  graphics_context_set_stroke_color(context, GColorRed);
  graphics_context_set_stroke_width(context, 2);
  graphics_draw_line(context, GPoint(14, 10), GPoint(21, 10));
  graphics_draw_line(context, GPoint(21, 10), GPoint(14, 20));
  graphics_draw_line(context, GPoint(14, 20), GPoint(19, 20));

  // Small red z
  graphics_draw_line(context, GPoint(21, 18), GPoint(26, 18));
  graphics_draw_line(context, GPoint(26, 18), GPoint(21, 26));
  graphics_draw_line(context, GPoint(21, 26), GPoint(24, 26));

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void prepare_silent_mode_layer() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing silent mode icon layer ...");
#endif

  // Emery status icon cluster layout (200 px wide screen):
  //   Silent:  95–131 (36 px) | BT: 131–163 (32 px) | Empty: 163–195 (32 px)
  //   Battery bar: 194–200 (6 px) — 1 px gap to empty battery icon
  //
  // Icon canvas includes 2 px padding on all sides to prevent stroke-bleed
  // clipping from PebbleOS anti-aliasing (worst: 3 px diagonal + 1.5 px bleed).
  int width = 36;
  int height = 36;
  int x = window_bounds.size.w - 105;
  int y = 0;

  silent_mode_layer = layer_create(GRect(x, y, width, height));
  layer_set_update_proc(silent_mode_layer, draw_silent_mode_callback);
  layer_add_child(window_layer, silent_mode_layer);

  // Start hidden until quiet time becomes active
  layer_set_hidden(silent_mode_layer, true);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

void prepare_layers() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing layers ...");
#endif

  prepare_bluetooth_layer();
  prepare_empty_battery_layer();
  prepare_silent_mode_layer();
  prepare_battery_line_layer();
  prepare_time_layer();
  prepare_date_layer();
  prepare_weekday_layer();
  prepare_line_layer();

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

void load_resources() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Loading resources ...");
#endif

  // All status icons are drawn procedurally; no external image resources
  // to load here.

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

// Set the visibility of the "no Bluetooth" icon and track its state.
static void bluetooth_set_icon_shown(bool shown) {
  bluetooth_icon_shown = shown;
  layer_set_hidden(bluetooth_layer, !shown);
}

// Called when the debounce timer expires. The OS reported a disconnection more
// than BLUETOOTH_DISCONNECT_DEBOUNCE_MS ago; show the "no Bluetooth" icon only if the
// connection is still down at this moment. This deliberately does not call
// handle_app_connection_handler() so it can never re-arm a new timer.
static void bluetooth_debounce_callback(void *data) {
  bluetooth_debounce_timer = NULL;

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Bluetooth debounce timer fired.");
#endif

  if (!connection_service_peek_pebble_app_connection()) {
    bluetooth_set_icon_shown(true);
  }
}

// Cancel a pending debounce timer, if any. Called when the window unloads to
// avoid leaving an AppTimer running on a dead window.
void bluetooth_debounce_cancel(void) {
  if (bluetooth_debounce_timer) {
    app_timer_cancel(bluetooth_debounce_timer);
    bluetooth_debounce_timer = NULL;
  }
}

void handle_quiet_time(bool active) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Quiet time %sactive", active ? "" : "in");
#endif

  silent_mode_shown = active;
  layer_set_hidden(silent_mode_layer, !active);
}

void handle_app_connection_handler(bool connected) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Pebble app %sconnected",
          connected ? "" : "dis");
#endif

  if (connected) {
    // Connected (again): cancel any pending debounce timer and hide the icon
    // immediately.
    bluetooth_debounce_cancel();
    bluetooth_set_icon_shown(false);
  } else {
    // Disconnected: start the debounce timer (if not already running and the
    // icon is not already shown). If the connection comes back before the
    // timer expires, the icon never appears.
    if (!bluetooth_debounce_timer && !bluetooth_icon_shown) {
      bluetooth_debounce_timer = app_timer_register(
          BLUETOOTH_DISCONNECT_DEBOUNCE_MS, bluetooth_debounce_callback, NULL);
    }
  }
}

void handle_minute(struct tm *tick_time, TimeUnits units_changed) {
  update_datetime(tick_time);
  // NOTE: Deliberately NO per-minute Bluetooth resync here.
  //
  // PebbleOS v4.30.0 explicitly optimizes for "fewer background wakeups" to
  // improve battery life ; polling the link every minute would work against
  // that system-level goal. The connection_service callbacks are reliable;
  // we trust them completely, and the debounce callback above re-checks live
  // state before showing the icon, which is cheaper than polling every 60 s.
  //
  // Quiet Time, however, has no OS subscription callback (SDK v4.33), so we
  // poll it here once per minute. The cost is a single boolean check.
  handle_quiet_time(quiet_time_is_active());
}

void update_datetime(struct tm *tick_time) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Updating date and time ...");
#endif

  static char s_time_buffer[9];
  static char s_date_buffer[16];
  static char s_week_day_buffer[16];

  strftime(s_time_buffer, sizeof(s_time_buffer),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  strftime(s_date_buffer, sizeof(s_date_buffer), "%B %d", tick_time);
  strftime(s_week_day_buffer, sizeof(s_week_day_buffer), "%A", tick_time);

  text_layer_set_text(time_layer, s_time_buffer);
  text_layer_set_text(date_layer, s_date_buffer);
  text_layer_set_text(week_day_layer, s_week_day_buffer);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

void handle_battery(BatteryChargeState charge_state) {
  unsigned int percent = charge_state.charge_percent;
  update_battery_line(percent);
  update_empty_battery_icon(percent < EMPTY_BATTERY_THRESHOLD_PERCENT);
}

void init_window_layer(Window *window) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Initializing the window ...");
#endif

  // Load custom fonts
  milford_font_30 =
      fonts_load_custom_font(resource_get_handle(RESOURCE_ID_MILFORD_FONT_30));

  // Get information about the Window
  window_layer = window_get_root_layer(window);
  window_bounds = layer_get_bounds(window_layer);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "Window layer pointer initialized: %p",
          window_layer);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Window sizes: %dx%d", window_bounds.size.w,
          window_bounds.size.h);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

void destroy_application_layers() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Releasing resources ...");
#endif

  // Destroy text layers
  text_layer_destroy(time_layer);
  text_layer_destroy(date_layer);
  text_layer_destroy(week_day_layer);

  // Destroy custom layers
  layer_destroy(line_layer);
  layer_destroy(battery_layer);
  layer_destroy(bluetooth_layer);
  layer_destroy(empty_battery_layer);
  layer_destroy(silent_mode_layer);

  // Unload the fonts
  fonts_unload_custom_font(milford_font_30);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Resource released.");
#endif
}
