/**
 * @Author: Burgy Benjamin
 * @Date: 2016-02-16T19:20:15+01:00
 * @Email: benjamin@burgy.swiss
 * @Last modified by: benjamin
 * @Last modified time: 2026-06-03T20:57:11+02:00
 */

#include <pebble.h>

void init_window_layer(Window *window);
void destroy_application_layers();
void update_datetime(struct tm *tick_time);

void setToReady(status_t state);
void prepare_layers();
void load_resources();

void handle_app_connection_handler(bool connected);
void handle_minute(struct tm *tick_time, TimeUnits units_changed);
void handle_battery(BatteryChargeState charge_state);
