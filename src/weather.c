#include "pebble.h"

static Window *window;

static Layer *window_layer;
static TextLayer *time_layer;
static TextLayer *date_layer;
static TextLayer *temperature_layer;
static TextLayer *city_layer;

static BitmapLayer *icon_layer;
static BitmapLayer *notify_layer;
static GBitmap *icon_bitmap = NULL;

static const int MAX_RETRIES = 30;

static bool bluetooth_ok = false;
static uint8_t battery_level;
static bool battery_plugged;

static GBitmap *icon_battery;
static GBitmap *icon_battery_charge;
static GBitmap *icon_bluetooth;
static GBitmap *icon_refresh;
static GBitmap *icon_warning;
static GBitmap *icon_error;
static GBitmap *icon_waiting;
static GBitmap *icon_empty;

static Layer *battery_layer;
static Layer *bluetooth_layer;

static bool jsready = false;
static bool stale = false;
static char message[32];
static time_t message_time;
static char message_time_text[] = "00:00";

static int weather_icon;
static int weather_temperature;
static time_t weather_time;
static char weather_time_text[] = "00:00";
static char weather_city[128];

/*
 * Function prototypes
 */

enum dict_keys {
	WEATHER_MESSAGE_KEY = 0x1,         // TUPLE_CHAR
	WEATHER_ICON_KEY = 0x2,         // TUPLE_INT
	WEATHER_TEMPERATURE_KEY = 0x3,  // TUPLE_CSTRING
	WEATHER_CITY_KEY = 0x4,         // TUPLE_CSTRING
	WEATHER_TIME_KEY = 0x5,         // TUPLE_INT
	WEATHER_ERROR_KEY = 0x6         // TUPLE_CSTRING
};

static const uint32_t WEATHER_ICONS[] = {
	RESOURCE_ID_ICON_CLEAR_DAY,
	RESOURCE_ID_ICON_CLEAR_NIGHT,
	RESOURCE_ID_ICON_RAIN,
	RESOURCE_ID_ICON_SNOW,
	RESOURCE_ID_ICON_SLEET,
	RESOURCE_ID_ICON_WIND,
	RESOURCE_ID_ICON_FOG,
	RESOURCE_ID_ICON_CLOUDY,
	RESOURCE_ID_ICON_PARTLY_CLOUDY_DAY,
	RESOURCE_ID_ICON_PARTLY_CLOUDY_NIGHT,
	RESOURCE_ID_ICON_THUNDER,
	RESOURCE_ID_ICON_RAIN_SNOW,
	RESOURCE_ID_ICON_RAIN_SLEET,
	RESOURCE_ID_ICON_SNOW_SLEET,
	RESOURCE_ID_ICON_COLD,
	RESOURCE_ID_ICON_HOT,
	RESOURCE_ID_ICON_DRIZZLE,
	RESOURCE_ID_ICON_NOT_AVAILABLE,
	RESOURCE_ID_ICON_PHONE_ERROR,
	RESOURCE_ID_ICON_CLOUD_ERROR,
	RESOURCE_ID_ICON_LOADING1,
	RESOURCE_ID_ICON_LOADING2,
	RESOURCE_ID_ICON_LOADING3,
};

static void request_weather(void) {

	if (!jsready) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "JS NOT READY !!");
		return;
	}
	
	Tuplet value = TupletCString(WEATHER_MESSAGE_KEY, "update");
	DictionaryIterator *iter;

	app_message_outbox_begin(&iter);

	if (iter == NULL) {
		return;
	}
	
	dict_write_tuplet(iter, &value);
	dict_write_end(iter);

	app_message_outbox_send();

	bitmap_layer_set_bitmap(notify_layer, icon_refresh);
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent weather request...");
}

static void update_weather_display(void) {

	static char weather_temperature_text[128];
	static char city_text[128];

	if (icon_bitmap) {
		gbitmap_destroy(icon_bitmap);
	}
	
	icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[weather_icon]);
	bitmap_layer_set_bitmap(icon_layer, icon_bitmap);
	
	snprintf(weather_temperature_text, sizeof weather_temperature_text, "%d%s", weather_temperature, "°");
	text_layer_set_text(temperature_layer, weather_temperature_text);

	snprintf(city_text, sizeof city_text, "%s %s", message_time_text,weather_city);
	text_layer_set_text(city_layer, city_text);

}

static void in_received_handler(DictionaryIterator *iter, void *context) {
	
	Tuple *message_tuple = dict_find(iter, WEATHER_MESSAGE_KEY);
	Tuple *icon_tuple = dict_find(iter, WEATHER_ICON_KEY);
	Tuple *temperature_tuple = dict_find(iter, WEATHER_TEMPERATURE_KEY);
	Tuple *city_tuple = dict_find(iter, WEATHER_CITY_KEY);
	Tuple *time_tuple = dict_find(iter, WEATHER_TIME_KEY);

	message_time = time(NULL);
	clock_copy_time_string(message_time_text,sizeof message_time_text);

	if (message_tuple) {
		strcpy(message,message_tuple->value->cstring);
	} else {
		return;
	}

	if (strcmp(message,"ready") == 0) {

		jsready = true;
		APP_LOG(APP_LOG_LEVEL_DEBUG, "RECEVIED ready message from PEBBLEJS");
		request_weather();

	} else if (strcmp(message,"weather") == 0) {

		stale = false;
		bitmap_layer_set_bitmap(notify_layer, icon_waiting);
		
		weather_icon = icon_tuple->value->uint8;
		weather_temperature = temperature_tuple->value->int8;
		strcpy(weather_city,city_tuple->value->cstring);
		weather_time = time_tuple->value->uint32;
		struct tm *tmpvar = localtime(&weather_time);
		if( clock_is_24h_style() ) {
			strftime(weather_time_text, sizeof(weather_time_text), "%H:%M", tmpvar);
		} else {
			strftime(weather_time_text, sizeof(weather_time_text), "%I:%M", tmpvar);
		}
		APP_LOG(APP_LOG_LEVEL_DEBUG, "%s ICON: %d, TEMPERATURE: %d, CITY: %s, TIME: %s",
			message_time_text,
			weather_icon,
			weather_temperature,
			weather_city,
			weather_time_text
		   );
		update_weather_display();

	} else if (strcmp(message,"error") == 0) {

		stale = true;
		bitmap_layer_set_bitmap(notify_layer, icon_error);
		strcpy(weather_city, dict_find(iter, WEATHER_ERROR_KEY)->value->cstring);
		update_weather_display();
		APP_LOG(APP_LOG_LEVEL_DEBUG, "WEATHER REQUEST FAILED!");
	}
}

static void in_dropped_handler(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "in_dropped_handler: App Error Message : %d",reason);
}

static void out_sent_handler(DictionaryIterator *sent, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "out_sent_handler: App Message sent OK!");
}

static void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "out_failed_handler: App Error Message : %d",reason);
}

void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
	
	static char date_text[] = "00 XXX";
	static char time_text[] = "00:00";

	clock_copy_time_string(time_text,sizeof time_text);

	APP_LOG(APP_LOG_LEVEL_DEBUG, "TICK @%s: (%d) %s%s%s%s",
			time_text,
			stale,
			(units_changed & DAY_UNIT) ? "D" : "-",
			(units_changed & HOUR_UNIT) ? "H" : "-",
			(units_changed & MINUTE_UNIT) ? "M" : "-",
			(units_changed & SECOND_UNIT) ? "S" : "-"
		   );
	
	if (units_changed & MINUTE_UNIT) {
		clock_copy_time_string(time_text,sizeof time_text);
		text_layer_set_text(time_layer,time_text);		
	}

	if (units_changed & DAY_UNIT) {
		strftime(date_text, sizeof(date_text), "%a %e", tick_time);
		text_layer_set_text(date_layer,date_text);
	}

	if (units_changed & HOUR_UNIT) {
		if (bluetooth_ok) {
			request_weather();
		}
	}	
}

/*
 * Battery icon callback handler
 */
void battery_layer_update_callback(Layer *layer, GContext *ctx) {
	graphics_context_set_compositing_mode(ctx, GCompOpAssign);
	if (!battery_plugged && ((battery_level<=20) || (battery_level>=90))) {
		graphics_draw_bitmap_in_rect(ctx, icon_battery, GRect(0, 0, 24, 12));
		graphics_context_set_stroke_color(ctx, GColorBlack);
		graphics_context_set_fill_color(ctx, GColorWhite);
		graphics_fill_rect(ctx, GRect(7, 4, (uint8_t)((battery_level / 100.0) * 11.0), 4), 0, GCornerNone);
	} else if (battery_plugged) {
		graphics_draw_bitmap_in_rect(ctx, icon_battery_charge, GRect(0, 0, 24, 12));
	}
}

void battery_state_handler(BatteryChargeState charge) {
	battery_level = charge.charge_percent;
	battery_plugged = charge.is_plugged;
	layer_mark_dirty(battery_layer);
}

/*
 * Bluetooth icon callback handler
 */
void bluetooth_layer_update_callback(Layer *layer, GContext *ctx) {
	if (bluetooth_ok) {
		graphics_context_set_compositing_mode(ctx, GCompOpAssign);
	} else {
		graphics_context_set_compositing_mode(ctx, GCompOpClear);
	}
	graphics_draw_bitmap_in_rect(ctx, icon_bluetooth, GRect(0, 0, 9, 12));
}

void bluetooth_connection_handler(bool connected) {
	bluetooth_ok = connected;
	layer_mark_dirty(bluetooth_layer);
}

static void window_load(Window *window) {
	
	static time_t tm;
	
	window_layer = window_get_root_layer(window);
	
	time_layer = text_layer_create(GRect(0, 0, 144, 55));
	text_layer_set_text_color(time_layer, GColorWhite);
	text_layer_set_background_color(time_layer, GColorBlack);
	text_layer_set_font(time_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FUTURA_CONDENSED_53)));
	text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
	layer_add_child(window_layer, text_layer_get_layer(time_layer));

	date_layer = text_layer_create(GRect(0, 55, 144, 33));
	text_layer_set_text_color(date_layer, GColorWhite);
	text_layer_set_background_color(date_layer, GColorBlack);
	text_layer_set_font(date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
	layer_add_child(window_layer, text_layer_get_layer(date_layer));
	
	icon_layer = bitmap_layer_create(GRect(0, 88, 60, 60));
	layer_add_child(window_layer, bitmap_layer_get_layer(icon_layer));
	
	temperature_layer = text_layer_create(GRect(61, 88, 83, 60));
	text_layer_set_text_color(temperature_layer, GColorBlack);
	text_layer_set_background_color(temperature_layer, GColorWhite);
	//text_layer_set_font(temperature_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FUTURA_40)));
	text_layer_set_font(temperature_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
	text_layer_set_text_alignment(temperature_layer, GTextAlignmentCenter);
	layer_add_child(window_layer, text_layer_get_layer(temperature_layer));
	
	city_layer = text_layer_create(GRect(0, 142, 144, 28));
	text_layer_set_text_color(city_layer, GColorBlack);
	text_layer_set_background_color(city_layer, GColorWhite);
	text_layer_set_font(city_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
//	text_layer_set_font(city_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FUTURA_18)));
	text_layer_set_text_alignment(city_layer, GTextAlignmentLeft);
	layer_add_child(window_layer, text_layer_get_layer(city_layer));

	// Status setup
	icon_battery = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_ICON);
	icon_battery_charge = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_CHARGE);
	icon_bluetooth = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH);
	
	BatteryChargeState initial = battery_state_service_peek();
	battery_level = initial.charge_percent;
	battery_plugged = initial.is_plugged;
	battery_layer = layer_create(GRect(0,70,24,12)); //24*12
	layer_set_update_proc(battery_layer, &battery_layer_update_callback);
	layer_add_child(window_layer, battery_layer);
	
	bluetooth_ok = bluetooth_connection_service_peek();
	bluetooth_layer = layer_create(GRect(130,70,9,12)); //9*12
	layer_set_update_proc(bluetooth_layer, &bluetooth_layer_update_callback);
	layer_add_child(window_layer, bluetooth_layer);

	// Initial Weather Display
	//icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ICON_CLOUD_ERROR);
	//bitmap_layer_set_bitmap(icon_layer, icon_bitmap);

	// Init notification icons
	icon_refresh = gbitmap_create_with_resource(RESOURCE_ID_REFRESH);
	icon_warning = gbitmap_create_with_resource(RESOURCE_ID_WARNING);
	icon_error = gbitmap_create_with_resource(RESOURCE_ID_ERROR);
	icon_waiting = gbitmap_create_with_resource(RESOURCE_ID_WAITING);
	icon_empty = gbitmap_create_with_resource(RESOURCE_ID_EMPTY);

	// Init notification layer
	notify_layer = bitmap_layer_create(GRect(124, 148, 20, 20));
	bitmap_layer_set_background_color(notify_layer, GColorClear);
	bitmap_layer_set_alignment(notify_layer, GAlignTop);
	layer_add_child(window_layer, bitmap_layer_get_layer(notify_layer));
	
	// Force Refresh time
	handle_tick(localtime(&tm), SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT | DAY_UNIT);
}

static void window_unload(Window *window) {
	
	if (icon_bitmap) {
		gbitmap_destroy(icon_bitmap);
	}
	gbitmap_destroy(icon_battery);
	gbitmap_destroy(icon_battery_charge);
	gbitmap_destroy(icon_bluetooth);
	gbitmap_destroy(icon_refresh);
	gbitmap_destroy(icon_warning);
	gbitmap_destroy(icon_error);
	gbitmap_destroy(icon_waiting);
	gbitmap_destroy(icon_empty);
	
	text_layer_destroy(time_layer);
	text_layer_destroy(date_layer);
	text_layer_destroy(temperature_layer);
	text_layer_destroy(city_layer);

	layer_destroy(battery_layer);
	layer_destroy(bluetooth_layer);

	bitmap_layer_destroy(icon_layer);
	bitmap_layer_destroy(notify_layer);

	layer_destroy(window_layer);

}

static void app_message_init(void) {
	app_message_open(124 /* inbound_size */, 124 /* outbound_size */);
	app_message_register_inbox_received(in_received_handler);
	app_message_register_inbox_dropped(in_dropped_handler);
	app_message_register_outbox_sent(out_sent_handler);
	app_message_register_outbox_failed(out_failed_handler);
}

static void init(void) {

	app_message_init();
	
	window = window_create();
	window_set_background_color(window, GColorClear);
	window_set_fullscreen(window, true);
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload
	});

	const bool animated = true;
	window_stack_push(window, animated);

	tick_timer_service_subscribe(MINUTE_UNIT | HOUR_UNIT | DAY_UNIT, &handle_tick);
	bluetooth_connection_service_subscribe(&bluetooth_connection_handler);
	battery_state_service_subscribe(&battery_state_handler);
	
}

static void deinit(void) {
	tick_timer_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	battery_state_service_unsubscribe();
	
	window_destroy(window);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
