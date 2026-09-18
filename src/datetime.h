#ifndef DATETIME_H
#define DATETIME_H

#include <pebble.h>

void datetime_layers_create(Layer *window_layer, GRect bounds);
void datetime_layers_destroy();
void datetime_update(struct tm *tick_time);

#endif
