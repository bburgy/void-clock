/**
* @Author: Burgy Benjamin <minidfx>
* @Date:   2016-03-22T20:22:31+01:00
* @Email:  benjamin.burgy@gmail.com
* @Last modified by:   minidfx
* @Last modified time: 2016-10-09T16:53:15+02:00
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
