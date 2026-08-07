/**
 * @Author: Burgy Benjamin
 * @Date: 2016-02-16T19:20:15+01:00
 * @Email: benjamin@burgy.swiss
 * @Last modified by: benjamin
 * @Last modified time: 2026-06-03T20:57:11+02:00
 */

#include "layers.h"
#include <pebble.h>

static TextLayer *ptr_time_layer;
static TextLayer *ptr_date_layer;
static TextLayer *ptr_week_day_layer;

static Layer *ptr_line_layer;
static Layer *ptr_battery_layer;
static Layer *ptr_window_layer;
static Layer *ptr_bluetooth_layer;
static Layer *ptr_empty_battery_layer;

static GDrawCommandImage *ptr_bluetooth_icon;
static GDrawCommandImage *ptr_empty_battery_icon;

static GFont ptr_milford_font_30;

static GRect window_bounds;

static uint8_t charge_percent = 0;
static uint8_t text_padding_left = 15;
static uint8_t battery_line_width = 6;
static uint8_t top_padding = 35;

static uint32_t NO_BLUETOOTH = 1;
static uint32_t EMPTY_BATTERY = 2;

// Time (ms) before we show the "no Bluetooth" icon after the OS reports a
// disconnection.
//
// Why this exists (PebbleOS Changelog context for Emery / PT2):
//
//  * core35 (late 2025)           — Standby mode enabled by default.
//  * v4.9.175 (May 2026)          — "Standby mode threshold has been reduced,
//                                    it was causing spurious BT disconnections"
//                                    plus "Speculative fix for an annoying
//                                    Bluetooth issue causing disconnects and bad
//                                    battery life on PT2".
//  * v4.9.163 (April 2026)        — "Fixed an infinite disconnect/connect loop
//                                    on iOS".
//  * core31                       — Adjusted BLE advertising / connection
//                                    parameters as part of Apple accessory
//                                    guidelines compliance.
//  * v4.12.0 (June 2026)          — "Watch now advertises as BLE-capable only
//                                    (BR/EDR not supported)".
//  * v4.31.1 (July 2026)          — Updated to NimBLE 1.10.0.
//
// The root cause is standby mode too aggressively powering down the BLE radio.
// Despite firmware improvements, brief link drops still occur at the edge of
// range or during radio re-tuning (especially on Pebble Time 2 / Emery). The
// firmware itself reportedly debounces internally for ~25 s, so a further 60 s
// at the app layer prevents flashing the icon for transient drops while still
// alerting the user to genuinely sustained disconnections.
//
// See bt_debounce_callback() for the live re-check that fires when this timer
// expires — it peek()s the actual state rather than assuming the stale callback
// value is still current.
#define BT_DISCONNECT_DEBOUNCE_MS 60000

// NULL when no debounce timer is pending, otherwise the running AppTimer.
static AppTimer *s_bt_debounce_timer = NULL;
// Whether the "no Bluetooth" icon is currently displayed. The icon stays
// hidden while a debounce timer is pending, even if the OS reports a
// disconnection.
static bool s_bt_icon_shown = false;

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

  // Set the origin offset from the context for drawing the image
  GPoint origin = GPoint(0, 0);

  // Draw the GDrawCommandImage to the GContext
  gdraw_command_image_draw(context, ptr_bluetooth_icon, origin);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void draw_empty_battery_callback(Layer *layer, GContext *context) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing empty battery icon layer ...");
#endif

  // Set the origin offset from the context for drawing the image
  GPoint origin = GPoint(0, 0);

  // Draw the GDrawCommandImage to the GContext
  gdraw_command_image_draw(context, ptr_empty_battery_icon, origin);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void prepare_battery_line_layer() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing the battery line layer ...");
#endif

  ptr_battery_layer =
      layer_create(GRect(window_bounds.size.w - battery_line_width, 0,
                         battery_line_width, window_bounds.size.h));
  layer_set_update_proc(ptr_battery_layer, draw_battery_line_callback);
  layer_add_child(ptr_window_layer, ptr_battery_layer);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void prepare_date_layer() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing the date ...");
#endif

  ptr_date_layer =
      text_layer_create(GRect(text_padding_left, top_padding,
                              window_bounds.size.w - text_padding_left, 37));

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "Date layer pointer initialized: %p",
          ptr_date_layer);
#endif

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(ptr_date_layer, GColorClear);
  text_layer_set_text_color(ptr_date_layer, GColorBlack);
  text_layer_set_font(ptr_date_layer, ptr_milford_font_30);
  text_layer_set_text_alignment(ptr_date_layer, GTextAlignmentLeft);

  // Add it as a child layer to the Window's root layer
  layer_add_child(ptr_window_layer, text_layer_get_layer(ptr_date_layer));

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void prepare_weekday_layer() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing the week of day ...");
#endif

  ptr_week_day_layer =
      text_layer_create(GRect(text_padding_left, top_padding + 30,
                              window_bounds.size.w - text_padding_left, 37));

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "Week layer pointer initialized: %p",
          ptr_week_day_layer);
#endif

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(ptr_week_day_layer, GColorClear);
  text_layer_set_text_color(ptr_week_day_layer, GColorBlack);
  text_layer_set_font(ptr_week_day_layer, ptr_milford_font_30);
  text_layer_set_text_alignment(ptr_week_day_layer, GTextAlignmentLeft);

  // Add it as a child layer to the Window's root layer
  layer_add_child(ptr_window_layer, text_layer_get_layer(ptr_week_day_layer));

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void prepare_line_layer() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing the line layer ...");
#endif

  ptr_line_layer =
      layer_create(GRect(0, top_padding + 72, window_bounds.size.w * 0.9, 1));
  layer_set_update_proc(ptr_line_layer, draw_line_callback);
  layer_add_child(ptr_window_layer, ptr_line_layer);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void prepare_time_layer() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing the time ...");
#endif

  ptr_time_layer =
      text_layer_create(GRect(text_padding_left, top_padding + 67,
                              window_bounds.size.w - text_padding_left, 60));

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "Time layer pointer initialized: %p",
          ptr_time_layer);
#endif

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(ptr_time_layer, GColorClear);
  text_layer_set_text_color(ptr_time_layer, GColorBlack);
  text_layer_set_font(ptr_time_layer,
                      fonts_get_system_font(FONT_KEY_LECO_60_NUMBERS_AM_PM));
  text_layer_set_text_alignment(ptr_time_layer, GTextAlignmentLeft);

  // Add it as a child layer to the Window's root layer
  layer_add_child(ptr_window_layer, text_layer_get_layer(ptr_time_layer));

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void update_empty_battery_icon(status_t isEmpty) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Updating the battery icon ...");
#endif

  layer_set_hidden(ptr_empty_battery_layer, !isEmpty);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void update_battery_line(uint8_t percent) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Updating the battery line ...");
#endif

  charge_percent = percent;
  layer_mark_dirty(ptr_battery_layer);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void prepare_bluetooth_layer() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing bluetooth icon layer ...");
#endif

  int width = 15;
  int height = 21;
  int x = window_bounds.size.w - 50;
  int y = 3;

  // Create the canvas Layer
  ptr_bluetooth_layer = layer_create(GRect(x, y, width, height));

  // Set the LayerUpdateProc
  layer_set_update_proc(ptr_bluetooth_layer, draw_bluetooth_callback);

  // Add to parent Window
  layer_add_child(ptr_window_layer, ptr_bluetooth_layer);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

static void prepare_empty_battery_layer() {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing empty battery icon layer ...");
#endif

  int width = 16;
  int height = 12;
  int x = window_bounds.size.w - 30;
  int y = 7;

  // Create the canvas Layer
  ptr_empty_battery_layer = layer_create(GRect(x, y, width, height));

  // Set the LayerUpdateProc
  layer_set_update_proc(ptr_empty_battery_layer, draw_empty_battery_callback);

  // Add to parent Window
  layer_add_child(ptr_window_layer, ptr_empty_battery_layer);

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

  // Create the object from resource file
  ptr_bluetooth_icon = gdraw_command_image_create_with_resource(NO_BLUETOOTH);
  ptr_empty_battery_icon =
      gdraw_command_image_create_with_resource(EMPTY_BATTERY);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

// Set the visibility of the "no Bluetooth" icon and track its state.
static void bt_set_icon_shown(bool shown) {
  s_bt_icon_shown = shown;
  layer_set_hidden(ptr_bluetooth_layer, !shown);
}

// Called when the debounce timer expires. The OS reported a disconnection more
// than BT_DISCONNECT_DEBOUNCE_MS ago; show the "no Bluetooth" icon only if the
// connection is still down at this moment. This deliberately does not call
// handle_app_connection_handler() so it can never re-arm a new timer.
static void bt_debounce_callback(void *data) {
  s_bt_debounce_timer = NULL;

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Bluetooth debounce timer fired.");
#endif

  if (!connection_service_peek_pebble_app_connection()) {
    bt_set_icon_shown(true);
  }
}

// Cancel a pending debounce timer, if any. Called when the window unloads to
// avoid leaving an AppTimer running on a dead window.
void bt_debounce_cancel(void) {
  if (s_bt_debounce_timer) {
    app_timer_cancel(s_bt_debounce_timer);
    s_bt_debounce_timer = NULL;
  }
}

void handle_app_connection_handler(bool connected) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Pebble app %sconnected",
          connected ? "" : "dis");
#endif

  if (connected) {
    // Connected (again): cancel any pending debounce timer and hide the icon
    // immediately.
    bt_debounce_cancel();
    bt_set_icon_shown(false);
  } else {
    // Disconnected: start the debounce timer (if not already running and the
    // icon is not already shown). If the connection comes back before the
    // timer expires, the icon never appears.
    if (!s_bt_debounce_timer && !s_bt_icon_shown) {
      s_bt_debounce_timer = app_timer_register(
          BT_DISCONNECT_DEBOUNCE_MS, bt_debounce_callback, NULL);
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

  text_layer_set_text(ptr_time_layer, s_time_buffer);
  text_layer_set_text(ptr_date_layer, s_date_buffer);
  text_layer_set_text(ptr_week_day_layer, s_week_day_buffer);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
#endif
}

void handle_battery(BatteryChargeState charge_state) {
  unsigned int percent = charge_state.charge_percent;
  update_battery_line(percent);
  update_empty_battery_icon(percent < 5);
}

void init_window_layer(Window *window) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Initializing the window ...");
#endif

  // Load custom fonts
  ptr_milford_font_30 =
      fonts_load_custom_font(resource_get_handle(RESOURCE_ID_MILFORD_FONT_30));

  // Get information about the Window
  ptr_window_layer = window_get_root_layer(window);
  window_bounds = layer_get_bounds(ptr_window_layer);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "Window layer pointer initialized: %p",
          ptr_window_layer);
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
  text_layer_destroy(ptr_time_layer);
  text_layer_destroy(ptr_date_layer);
  text_layer_destroy(ptr_week_day_layer);

  // Destroy custom layers
  layer_destroy(ptr_line_layer);
  layer_destroy(ptr_battery_layer);
  layer_destroy(ptr_bluetooth_layer);
  layer_destroy(ptr_empty_battery_layer);

  // Destroy image
  gdraw_command_image_destroy(ptr_empty_battery_icon);
  gdraw_command_image_destroy(ptr_bluetooth_icon);

  // Unload the fonts
  fonts_unload_custom_font(ptr_milford_font_30);

#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Resource released.");
#endif
}
