#ifndef STATUS_H
#define STATUS_H

#include <pebble.h>

void status_deinit();
void status_handle_battery(BatteryChargeState charge_state);
void status_handle_bluetooth(bool connected);
void status_update_icons();
void status_handle_focus(bool in_focus);

#endif
