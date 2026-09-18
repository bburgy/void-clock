#include "status.h"
#include "icons.h"

static AppTimer *bluetooth_debounce_timer = NULL;
static bool bluetooth_icon_shown = false;

#define EMPTY_BATTERY_THRESHOLD_PERCENT 10
#define BLUETOOTH_DISCONNECT_DEBOUNCE_MS 15000

static void bluetooth_set_icon_shown(bool shown) {
  bluetooth_icon_shown = shown;
  icons_set_bluetooth_shown(shown);
}

static void bluetooth_debounce_callback(void *data) {
  bluetooth_debounce_timer = NULL;
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Bluetooth debounce timer fired.");
#endif
  if (!connection_service_peek_pebble_app_connection()) {
    bluetooth_set_icon_shown(true);
  }
}

static void bluetooth_debounce_cancel() {
  if (bluetooth_debounce_timer) {
    app_timer_cancel(bluetooth_debounce_timer);
    bluetooth_debounce_timer = NULL;
  }
}

void status_deinit() {
  bluetooth_debounce_cancel();
}

void status_handle_battery(BatteryChargeState charge_state) {
  icons_update_battery_line(charge_state.charge_percent);
  icons_set_empty_battery_shown(
      charge_state.charge_percent < EMPTY_BATTERY_THRESHOLD_PERCENT);
}

void status_handle_bluetooth(bool connected) {
#ifdef PBL_DEBUG
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Pebble app %sconnected",
          connected ? "" : "dis");
#endif
  if (connected) {
    bluetooth_debounce_cancel();
    bluetooth_set_icon_shown(false);
  } else {
    if (!bluetooth_debounce_timer && !bluetooth_icon_shown) {
      bluetooth_debounce_timer = app_timer_register(
          BLUETOOTH_DISCONNECT_DEBOUNCE_MS,
          bluetooth_debounce_callback, NULL);
    }
  }
}

void status_update_icons() {
  bool quiet = quiet_time_is_active();
  icons_set_quiet_time_shown(quiet);
}
