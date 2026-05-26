/**
* @Author: Burgy Benjamin <minidfx>
* @Date:   2016-02-16T19:19:55+01:00
* @Email:  benjamin.burgy@gmail.com
* @Last modified by:   minidfx
* @Last modified time: 2016-10-11T21:54:22+02:00
*/

#include <pebble.h>
#include "layers.h"

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

static GRect window_bounds;

static uint8_t charge_percent = 0;
static uint8_t textPaddingLeft = 15;
static uint8_t battery_line_width = 4;

static uint32_t NO_BLUETOOTH = 1;
static uint32_t EMPTY_BATTERY = 2;

static status_t pebbleAppStatus = S_FALSE;
static status_t isInitialized = S_FALSE;

static void draw_battery_line_callback(Layer *layer, GContext *context)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing the battery line ...");

    uint8_t lineHeight = charge_percent * window_bounds.size.h / 100;
    GRect rect_bounds = GRect(0, window_bounds.size.h - lineHeight, battery_line_width, window_bounds.size.h);

    // Draw a rectangle
    graphics_draw_rect(context, rect_bounds);

    // Fill rectangle
    graphics_fill_rect(context, rect_bounds, 0, GCornersAll);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

static void draw_line_callback(Layer *layer, GContext *context)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing the line ...");

    GPoint start = GPoint(0, 0);
    GPoint end = GPoint(122, 0);

    graphics_draw_line(context, start, end);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

static void draw_bluetooth_callback(Layer *layer, GContext *context)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing bluetooth icon layer ...");

    // Set the origin offset from the context for drawing the image
    GPoint origin = GPoint(0, 0);

    // Draw the GDrawCommandImage to the GContext
    gdraw_command_image_draw(context, ptr_bluetooth_icon, origin);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

static void draw_empty_battery_callback(Layer *layer, GContext *context)
{
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing empty battery icon layer ...");

  // Set the origin offset from the context for drawing the image
  GPoint origin = GPoint(0, 0);

  // Draw the GDrawCommandImage to the GContext
  gdraw_command_image_draw(context, ptr_empty_battery_icon, origin);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

static void prepare_line_layer()
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing the line layer ...");

    ptr_line_layer = layer_create(GRect(0, 110, 122, 1));
    layer_set_update_proc(ptr_line_layer, draw_line_callback);
    layer_add_child(ptr_window_layer, ptr_line_layer);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

static void prepare_battery_line_layer()
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing the battery line layer ...");

    ptr_battery_layer = layer_create(GRect(window_bounds.size.w - battery_line_width, 0, battery_line_width, window_bounds.size.h));
    layer_set_update_proc(ptr_battery_layer, draw_battery_line_callback);
    layer_add_child(ptr_window_layer, ptr_battery_layer);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

static void prepare_time_layer()
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing the time ...");

    ptr_time_layer = text_layer_create(GRect(textPaddingLeft, PBL_IF_ROUND_ELSE(70, 105), window_bounds.size.w - textPaddingLeft, 42));

    APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "Time layer pointer initialized: %p", ptr_time_layer);

    // Improve the layout to be more like a watchface
    text_layer_set_background_color(ptr_time_layer, GColorClear);
    text_layer_set_text_color(ptr_time_layer, GColorBlack);
    text_layer_set_font(ptr_time_layer, fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS));
    text_layer_set_text_alignment(ptr_time_layer, GTextAlignmentLeft);

    // Add it as a child layer to the Window's root layer
    layer_add_child(ptr_window_layer, text_layer_get_layer(ptr_time_layer));

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

static void prepare_date_layer()
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing the date ...");

    ptr_date_layer = text_layer_create(GRect(textPaddingLeft, PBL_IF_ROUND_ELSE(70, 80), window_bounds.size.w - textPaddingLeft, 30));

    APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "Date layer pointer initialized: %p", ptr_date_layer);

    // Improve the layout to be more like a watchface
    text_layer_set_background_color(ptr_date_layer, GColorClear);
    text_layer_set_text_color(ptr_date_layer, GColorBlack);
    text_layer_set_font(ptr_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(ptr_date_layer, GTextAlignmentLeft);

    // Add it as a child layer to the Window's root layer
    layer_add_child(ptr_window_layer, text_layer_get_layer(ptr_date_layer));

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

static void prepare_weekday_layer()
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Drawing the week of day ...");

    ptr_week_day_layer = text_layer_create(GRect(textPaddingLeft, PBL_IF_ROUND_ELSE(50, 60), window_bounds.size.w - textPaddingLeft, 30));

    APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "Week layer pointer initialized: %p", ptr_week_day_layer);

    // Improve the layout to be more like a watchface
    text_layer_set_background_color(ptr_week_day_layer, GColorClear);
    text_layer_set_text_color(ptr_week_day_layer, GColorBlack);
    text_layer_set_font(ptr_week_day_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(ptr_week_day_layer, GTextAlignmentLeft);

    // Add it as a child layer to the Window's root layer
    layer_add_child(ptr_window_layer, text_layer_get_layer(ptr_week_day_layer));

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

static void update_empty_icon(status_t isEmpty)
{
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Updating the battery icon ...");

  layer_set_hidden(ptr_empty_battery_layer, !isEmpty);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

static void update_battery_line(uint8_t percent)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Updating the battery line ...");

    charge_percent = percent;
    layer_mark_dirty(ptr_battery_layer);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

static void prepare_bluetooth_layer()
{
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing bluetooth icon layer ...");

  // Create the canvas Layer
  ptr_bluetooth_layer = layer_create(GRect(textPaddingLeft, 40, 25, 21));

  // Set the LayerUpdateProc
  layer_set_update_proc(ptr_bluetooth_layer, draw_bluetooth_callback);

  // Add to parent Window
  layer_add_child(ptr_window_layer, ptr_bluetooth_layer);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

static void prepare_empty_battery_layer()
{
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing empty battery icon layer ...");

  int width = 24;
  int height = 15;
  int x = window_bounds.size.w - width;
  int y = window_bounds.size.h - height;

  // Create the canvas Layer
  ptr_empty_battery_layer = layer_create(GRect(x, y, width, height));

  // Set the LayerUpdateProc
  layer_set_update_proc(ptr_empty_battery_layer, draw_empty_battery_callback);

  // Add to parent Window
  layer_add_child(ptr_window_layer, ptr_empty_battery_layer);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

void prepare_layers()
{
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing layers ...");

  prepare_bluetooth_layer();
  prepare_empty_battery_layer();
  prepare_battery_line_layer();
  prepare_time_layer();
  prepare_date_layer();
  prepare_weekday_layer();
  prepare_line_layer();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

void setToReady(status_t state)
{
  isInitialized = state;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "The watchface is completely loaded.");
}

void load_resources()
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Loading resources ...");

    // Create the object from resource file
    ptr_bluetooth_icon = gdraw_command_image_create_with_resource(NO_BLUETOOTH);
    ptr_empty_battery_icon = gdraw_command_image_create_with_resource(EMPTY_BATTERY);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

void handle_app_connection_handler(bool connected)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Pebble app %sconnected", connected ? "" : "dis");

    pebbleAppStatus = (status_t)connected;

    if(!pebbleAppStatus && isInitialized)
    {
        vibes_double_pulse();
    }

    layer_set_hidden(ptr_bluetooth_layer, connected);
}

void handle_minute(struct tm *tick_time, TimeUnits units_changed)
{
    update_datetime(tick_time);
}

void update_datetime(struct tm *tick_time)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Updating date and time ...");

    static char s_time_buffer[8];
    static char s_date_buffer[15];
    static char s_week_day_buffer[15];

    strftime(s_time_buffer, sizeof(s_time_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
    strftime(s_date_buffer, sizeof(s_date_buffer), "%B %d", tick_time);
    strftime(s_week_day_buffer, sizeof(s_week_day_buffer), "%A", tick_time);

    text_layer_set_text(ptr_time_layer, s_time_buffer);
    text_layer_set_text(ptr_date_layer, s_date_buffer);
    text_layer_set_text(ptr_week_day_layer, s_week_day_buffer);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

void handle_battery(BatteryChargeState charge_state)
{
    unsigned int percent = charge_state.charge_percent;
    update_battery_line(percent);
    update_empty_icon(percent == 0);
}

void init_window_layer(Window *window)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Initializing the window ...");

    // Get information about the Window
    ptr_window_layer = window_get_root_layer(window);
    window_bounds = layer_get_bounds(ptr_window_layer);

    APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "Window layer pointer initialized: %p", ptr_window_layer);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done.");
}

void destroy_application_layers()
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Releasing resources ...");

    text_layer_destroy(ptr_time_layer);
    text_layer_destroy(ptr_date_layer);
    text_layer_destroy(ptr_week_day_layer);

    layer_destroy(ptr_line_layer);
    layer_destroy(ptr_battery_layer);
    layer_destroy(ptr_bluetooth_layer);
    layer_destroy(ptr_empty_battery_layer);

    gdraw_command_image_destroy(ptr_empty_battery_icon);
    gdraw_command_image_destroy(ptr_bluetooth_icon);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Resource released.");
}
