#include "esp_log.h"
#include "esp_spiffs.h"

#include "lvgl.h"
#include "cJSON.h"
#include <stdio.h>

static const char* TAG = "Tabview";

char *load_json_file(const char *path);

typedef enum {
    TYPE_PUMP,
    TYPE_VALVE,
    TYPE_SENSOR,
    TYPE_METER,
    TYPE_TANK
} device_type_t;

typedef enum {
    STATE_ACTIVE,
    STATE_INACTIVE,
    STATE_ERROR,
    STATE_DISABLED
} device_state_t;

/* Forward declaration */
static void btn_state_event_cb(lv_event_t *e);

/* Helper to get state text and color */
static const char *get_state_text(device_state_t state)
{
    switch(state) {
        case STATE_ACTIVE:  return "Active";
        case STATE_INACTIVE: return "Inactive";
        case STATE_ERROR:   return "Error";
        case STATE_DISABLED:return "Disabled";
        default:            return "Unknown";
    }
}

static lv_color_t get_state_color(device_state_t state)
{
    switch(state) {
        case STATE_ACTIVE:  return lv_color_hex(0x00AEEF);
        case STATE_INACTIVE: return lv_color_hex(0xFFA500);
        case STATE_ERROR:   return lv_color_hex(0xFF0000);
        case STATE_DISABLED:return lv_color_hex(0xAAAAAA);
        default:            return lv_color_hex(0xFFFFFF);
    }
}

typedef struct {
    device_state_t state;
    const char *device_name;
    device_type_t device_type;
    lv_obj_t *label_state;
    lv_obj_t * img_obj;
} device_ctx_t;

/*
void create_image(lv_obj_t *cont, const void *src) {
	// Create an image object
	lv_obj_t * img = lv_img_create(cont);

	// Set the source of the image (e.g., a binary file or a declared image)
	LV_IMG_DECLARE(my_image); // Ensure 'my_image' is declared in your project
	lv_img_set_src(img, src);

	// Align the image to the center of the screen
	lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
}
*/

/* Create a device indicator with a button */
static lv_obj_t *create_device(lv_obj_t *parent, const char *name, device_state_t state, device_type_t device_type)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 240, 300);
    lv_obj_set_style_radius(cont, 10, 0);
    lv_obj_set_style_pad_all(cont, 10, 0);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x2E2E2E), 0);

    lv_obj_t *label_name = lv_label_create(cont);
    lv_label_set_text(label_name, name);
    lv_obj_align(label_name, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t * img_obj = lv_image_create(cont); // Create an image object
    switch (state) {
        case STATE_ACTIVE:
            switch (device_type) {
                case TYPE_PUMP:
                    lv_image_set_src(img_obj, "A:spiffs/Pump_Open_25pc.png"); // Set the image source
                break;
                default:
                    lv_image_set_src(img_obj, "A:spiffs/Valve_Open_25pc.png"); // Set the image source
                break;
            }
        break;
        case STATE_INACTIVE:
            switch (device_type) {
                case TYPE_PUMP:
                    lv_image_set_src(img_obj, "A:spiffs/Pump_Closed_25pc.png"); // Set the image source
                break;
                default:
                    lv_image_set_src(img_obj, "A:spiffs/Valve_Closed_25pc.png"); // Set the image source
                break;
            }
        break;
        case STATE_DISABLED:
            switch (device_type) {
                case TYPE_PUMP:
                    lv_image_set_src(img_obj, "A:spiffs/Pump_Disabled_25pc.png"); // Set the image source
                break;
                default:
                    lv_image_set_src(img_obj, "A:spiffs/Valve_Disabled_25pc.png"); // Set the image source
                break;
            }
        break;
        case STATE_ERROR:
        break;
    }

    lv_obj_t *label_state = lv_label_create(cont);
    lv_label_set_text(label_state, get_state_text(state));
    lv_obj_set_style_text_color(label_state, get_state_color(state), 0);
    lv_obj_align(label_state, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* Allocate context */
    device_ctx_t *ctx = malloc(sizeof(device_ctx_t));
    ctx->state = state;
    ctx->device_name = name;
    ctx->label_state = label_state;
    ctx->img_obj = img_obj;

    /* Create button */
    lv_obj_t *btn = lv_btn_create(cont);
    lv_obj_set_size(btn, 110, 40);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, get_state_text(state));
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(btn, btn_state_event_cb, LV_EVENT_CLICKED, ctx);

    return cont;
}

/* Callback to cycle through states */
static void btn_state_event_cb(lv_event_t *e)
{
    device_ctx_t *ctx = lv_event_get_user_data(e);

    /* Cycle state */
    ctx->state = (ctx->state + 1) % 4;

    /* Update button label */
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *btn_label = lv_obj_get_child(btn, 0);
    lv_label_set_text(btn_label, get_state_text(ctx->state));

    /* Update state label */
    lv_label_set_text(ctx->label_state, get_state_text(ctx->state));
    lv_obj_set_style_text_color(ctx->label_state, get_state_color(ctx->state), 0);

    /* Update image */
    switch (ctx->state) {
        case STATE_ACTIVE:
            switch (ctx->device_type) {
                case TYPE_PUMP:
                    lv_image_set_src(ctx->img_obj, "A:spiffs/Pump_Open_25pc.png"); // Set the image source
                break;
                default:
                    lv_image_set_src(ctx->img_obj, "A:spiffs/Valve_Open_25pc.png"); // Set the image source
                break;
            }
        break;
        case STATE_INACTIVE:
            switch (ctx->device_type) {
                case TYPE_PUMP:
                    lv_image_set_src(ctx->img_obj, "A:spiffs/Pump_Closed_25pc.png"); // Set the image source
                break;
                default:
                    lv_image_set_src(ctx->img_obj, "A:spiffs/Valve_Closed_25pc.png"); // Set the image source
                break;
            }
        break;
        case STATE_DISABLED:
            switch (ctx->device_type) {
                case TYPE_PUMP:
                    lv_image_set_src(ctx->img_obj, "A:spiffs/Pump_Disabled_25pc.png"); // Set the image source
                break;
                default:
                    lv_image_set_src(ctx->img_obj, "A:spiffs/Valve_Disabled_25pc.png"); // Set the image source
                break;
            }
        break;
        case STATE_ERROR:
        break;
    }


    /* Notify ESP32 */
    /*send_state_to_esp32(ctx->device_name, ctx->state);*/
}

/* Main screen with Tabview */
void ui_screen_devices(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *tabview = lv_tabview_create(scr);
    lv_obj_set_size(tabview, LV_PCT(100), LV_PCT(100));

    lv_obj_t *tab_pumps   = lv_tabview_add_tab(tabview, "Pumps");
    lv_obj_t *tab_valves  = lv_tabview_add_tab(tabview, "Valves");
    lv_obj_t *tab_sensors = lv_tabview_add_tab(tabview, "Sensors");
    lv_obj_t *tab_flow    = lv_tabview_add_tab(tabview, "Meters");
    lv_obj_t *tab_tanks   = lv_tabview_add_tab(tabview, "Tanks");

    /* Pumps */
    lv_obj_t *pump1 = create_device(tab_pumps, "Pump 1", STATE_ACTIVE, TYPE_PUMP);
    lv_obj_align(pump1, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t *pump2 = create_device(tab_pumps, "Pump 2", STATE_INACTIVE, TYPE_PUMP);
    lv_obj_align(pump2, LV_ALIGN_TOP_MID, 0, 340);

    /* Valves */
    lv_obj_t *valve1 = create_device(tab_valves, "Valve 1", STATE_ACTIVE, TYPE_VALVE);
    lv_obj_align(valve1, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t *valve2 = create_device(tab_valves, "Valve 2", STATE_ACTIVE, TYPE_VALVE);
    lv_obj_align(valve2, LV_ALIGN_TOP_MID, 0, 340);

    /* Sensors */
    lv_obj_t *sensor1 = create_device(tab_sensors, "Sensor 1", STATE_ACTIVE, TYPE_SENSOR);
    lv_obj_align(sensor1, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t *sensor2 = create_device(tab_sensors, "Sensor 2", STATE_DISABLED, TYPE_SENSOR);
    lv_obj_align(sensor2, LV_ALIGN_TOP_MID, 0, 340);

    /* Flow Meters */
    lv_obj_t *flow1 = create_device(tab_flow, "Flow Meter 1", STATE_INACTIVE, TYPE_METER);
    lv_obj_align(flow1, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t *flow2 = create_device(tab_flow, "Flow Meter 2", STATE_ERROR, TYPE_METER);
    lv_obj_align(flow2, LV_ALIGN_TOP_MID, 0, 340);

    /* Tanks */
    lv_obj_t *tank1 = create_device(tab_tanks, "Tank 1", STATE_ACTIVE, TYPE_TANK);
    lv_obj_align(tank1, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t *tank2 = create_device(tab_tanks, "Tank 2", STATE_INACTIVE, TYPE_TANK);
    lv_obj_align(tank2, LV_ALIGN_TOP_MID, 0, 340);

    lv_screen_load(scr);
}

static lv_obj_t *create_device_card(lv_obj_t *parent, const char *title, const char *info)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, 250);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xFF4444), 0);

    lv_obj_t *label_title = lv_label_create(card);
    lv_label_set_text(label_title, title);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_24, 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *label_info = lv_label_create(card);
    lv_label_set_text(label_info, info);
    lv_obj_set_style_text_color(label_info, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label_info, &lv_font_montserrat_18, 0);
    lv_obj_align(label_info, LV_ALIGN_CENTER, 0, 0);

    /* Create button */
    lv_obj_t *btn = lv_btn_create(card);
    lv_obj_set_size(btn, 110, 40);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Press me!");
    lv_obj_center(btn_label);

    return card;
}

static void load_devices_into_tab(lv_obj_t *tab, cJSON *array, const char *type)
{
    lv_obj_t *cont = lv_obj_create(tab);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(cont, 12, 0);

    int count = cJSON_GetArraySize(array);

    for (int i = 0; i < count; i++) {
        cJSON *dev = cJSON_GetArrayItem(array, i);

        const char *location = cJSON_GetObjectItem(dev, "location")->valuestring;
        const char *state = cJSON_GetObjectItem(dev, "state") ?
                            cJSON_GetObjectItem(dev, "state")->valuestring : "n/a";

        char info[512];
        snprintf(info, sizeof(info), "\nState: %s\n", state);

        // Extra fields depending on type
        if (strcmp(type, "valves") == 0) {
            double flow = cJSON_GetObjectItem(dev, "flow_rate_lpm")->valuedouble;
            snprintf(info + strlen(info), sizeof(info) - strlen(info),
                     "Flow: %.1f L/min\n", flow);
        }
        else if (strcmp(type, "meters") == 0) {
            double flow = cJSON_GetObjectItem(dev, "current_flow_lpm")->valuedouble;
            int total = cJSON_GetObjectItem(dev, "total_liters_today")->valueint;
            snprintf(info + strlen(info), sizeof(info) - strlen(info),
                     "Flow: %.1f L/min\nTotal today: %d L\n", flow, total);
        }
        else if (strcmp(type, "pumps") == 0) {
            double pressure = cJSON_GetObjectItem(dev, "pressure_bar")->valuedouble;
            snprintf(info + strlen(info), sizeof(info) - strlen(info),
                     "Pressure: %.1f bar\n", pressure);
        }

        // Schedules
        cJSON *schedule = cJSON_GetObjectItem(dev, "schedule");
        int sched_count = cJSON_GetArraySize(schedule);

        snprintf(info + strlen(info), sizeof(info) - strlen(info), "Schedules:\n");

        for (int s = 0; s < sched_count; s++) {
            cJSON *sch = cJSON_GetArrayItem(schedule, s);
            const char *start = cJSON_GetObjectItem(sch, "start_time")->valuestring;
            const char *end = cJSON_GetObjectItem(sch, "end_time")->valuestring;

            snprintf(info + strlen(info), sizeof(info) - strlen(info),
                     "  %s-%s\n", start, end);
        }

        //lv_obj_t *dev = create_device(tab_valves, "Valve 1", STATE_ACTIVE, TYPE_VALVE);
        create_device_card(cont, location, info);
    }
}

int read_irrigation_config()
{
    ESP_LOGI(TAG, "Opening file");
    char *json_text = load_json_file("/spiffs/config.txt");
    if (!json_text) {
        ESP_LOGE(TAG, "Failed to load JSON file");
        return -1;
    }
    ESP_LOGI(TAG, "Read from file: '%s'", json_text);

    cJSON *root = cJSON_Parse(json_text);
    if (!root) {
        printf("JSON parse error\n");
        return -2;
    }

    cJSON *system = cJSON_GetObjectItem(root, "system");
    cJSON *devices = cJSON_GetObjectItem(system, "devices");

    cJSON *valves = cJSON_GetObjectItem(devices, "valves");
    cJSON *meters = cJSON_GetObjectItem(devices, "meters");
    cJSON *pumps = cJSON_GetObjectItem(devices, "pumps");
    cJSON *tanks = cJSON_GetObjectItem(devices, "tanks");
    cJSON *sensors = cJSON_GetObjectItem(devices, "sensors");

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *tabview = lv_tabview_create(scr);
    lv_obj_set_size(tabview, LV_PCT(100), LV_PCT(100));

    lv_obj_t *tab_pumps   = lv_tabview_add_tab(tabview, "Pumps");
    lv_obj_t *tab_valves  = lv_tabview_add_tab(tabview, "Valves");
    lv_obj_t *tab_sensors = lv_tabview_add_tab(tabview, "Sensors");
    lv_obj_t *tab_flow    = lv_tabview_add_tab(tabview, "Meters");
    lv_obj_t *tab_tanks   = lv_tabview_add_tab(tabview, "Tanks");

    load_devices_into_tab(tab_valves, valves, "valves");
    load_devices_into_tab(tab_flow, meters, "meters");
    load_devices_into_tab(tab_pumps, pumps, "pumps");
    load_devices_into_tab(tab_tanks, tanks, "tanks");
    load_devices_into_tab(tab_sensors, sensors, "sensors");

    lv_screen_load(scr);

    cJSON_Delete(root);

    return 0;
}

char *load_json_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGE("JSON", "Failed to open file: %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        ESP_LOGE("JSON", "Memory allocation failed");
        return NULL;
    }

    fread(buffer, 1, size, f);
    buffer[size] = '\0';

    fclose(f);
    return buffer;
}
