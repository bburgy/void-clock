#include "datetime.h"
#include "icons.h"
#include "status.h"
#include <pebble.h>
#include <locale.h>
#include <time.h>

static Window *window;

static void handle_minute(struct tm *tick_time, TimeUnits units_changed) {
  datetime_update(tick_time);
  status_update_icons();
}

static void handle_battery(BatteryChargeState charge_state) {
  status_handle_battery(charge_state);
}

static void handle_bluetooth(bool connected) {
  status_handle_bluetooth(connected);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  const char *language = i18n_get_system_locale();
  setlocale(LC_ALL, language);

  datetime_layers_create(window_layer, bounds);
  icons_layers_create(window_layer, bounds);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute);
  battery_state_service_subscribe(handle_battery);
  connection_service_subscribe((ConnectionHandlers){
      .pebble_app_connection_handler = handle_bluetooth});

  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  datetime_update(tick_time);
  status_handle_battery(battery_state_service_peek());
  status_handle_bluetooth(connection_service_peek_pebble_app_connection());
  status_update_icons();
}

static void window_unload(Window *window) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
  status_deinit();
  datetime_layers_destroy();
  icons_layers_destroy();
}

static void init() {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers){
                                         .load = window_load,
                                         .unload = window_unload,
                                     });
  window_stack_push(window, true);
}

static void deinit() { window_destroy(window); }

int main() {
  init();
  app_event_loop();
  deinit();
}
