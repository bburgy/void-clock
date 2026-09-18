#ifndef ICONS_H
#define ICONS_H

#include <pebble.h>

void icons_layers_create(Layer *window_layer, GRect bounds);
void icons_layers_destroy();
void icons_update_battery_line(uint8_t percent);
void icons_set_bluetooth_shown(bool shown);
void icons_set_empty_battery_shown(bool shown);
void icons_set_quiet_time_shown(bool shown);
void icons_set_alarm_shown(bool shown);

#endif
