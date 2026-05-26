/**
* @Author: Burgy Benjamin <minidfx>
* @Date:   2016-02-16T19:20:15+01:00
* @Email:  benjamin.burgy@gmail.com
* @Last modified by:   minidfx
* @Last modified time: 2016-10-09T16:53:21+02:00
*/

#include <pebble.h>
#include "layers.h"

static Window *window;

static void window_load(Window *window)
{
    init_window_layer(window);

    // Set the watch language
    const char *language = i18n_get_system_locale();
    setlocale(LC_ALL, language);

    prepare_layers();

    // Register services
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute);
    battery_state_service_subscribe(handle_battery);
    connection_service_subscribe((ConnectionHandlers)
    {
        .pebble_app_connection_handler = handle_app_connection_handler
    });

    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);

    update_datetime(tick_time);
    handle_battery(battery_state_service_peek());
    handle_app_connection_handler(connection_service_peek_pebble_app_connection());

    setToReady(S_TRUE);
}

static void window_unload(Window *window)
{
    tick_timer_service_unsubscribe();
    battery_state_service_unsubscribe();

    destroy_application_layers();
}

static void init()
{
    window = window_create();
    window_set_window_handlers(window, (WindowHandlers)
    {
        .load = window_load,
        .unload = window_unload,
    });

    load_resources();

    window_stack_push(window, true);
}

static void deinit()
{
    window_destroy(window);
}

int main()
{
    init();

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

    app_event_loop();

    deinit();
}
