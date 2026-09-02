#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_spiffs.h"

#include "lvgl.h"
#include "cJSON.h"
#include <stdio.h>

static const char* TAG = "Tabview";
static void ta_event_cb(lv_event_t * e);

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
} device_state_t;

typedef struct {
    device_state_t state;
    char *location;
    device_type_t type;
} device_ctx_t;

char *load_json_file(const char *path);
static void open_edit_dialog(lv_obj_t * parent, device_ctx_t * data);
static void dialog_ok_event(lv_event_t * e);
static void dialog_cancel_event(lv_event_t * e);


/* Forward declaration */
static void btn_state_event_cb(lv_event_t *e);

/* Helper to get state text and color */
static const char *get_state_text(device_state_t state)
{
    switch(state) {
        case STATE_ACTIVE:  return "Active";
        case STATE_INACTIVE: return "Inactive";
        case STATE_ERROR:   return "Error";
        default:            return "Unknown";
    }
}

static lv_color_t get_state_color(device_state_t state)
{
    switch(state) {
        case STATE_ACTIVE:  return lv_color_hex(0x00AEEF);
        case STATE_INACTIVE: return lv_color_hex(0xFFA500);
        case STATE_ERROR:   return lv_color_hex(0xFF0000);
        default:            return lv_color_hex(0xFFFFFF);
    }
}

/* Callback to cycle through states */
static void btn_state_event_cb(lv_event_t *e)
{
    device_ctx_t *ctx = lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);

    open_edit_dialog(lv_layer_top(), ctx);

    /* Cycle state */
    /*ctx->state = (ctx->state + 1) % 4;*/

    /* Update button label */
    /*lv_obj_t *btn_label = lv_obj_get_child(btn, 0);
    lv_label_set_text(btn_label, get_state_text(ctx->state));*/

}

static lv_obj_t *create_device_card(lv_obj_t *parent, const char *title, const char *info, device_ctx_t *ctx)
{
    if (!lvgl_port_lock(0)) return NULL;

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
    lv_label_set_text(btn_label, "Edit");
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(btn, btn_state_event_cb, LV_EVENT_CLICKED, ctx);

    lvgl_port_unlock();

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

        char *s_location = cJSON_GetObjectItem(dev, "location")->valuestring;
        const char *s_state = cJSON_GetObjectItem(dev, "state") ?
                            cJSON_GetObjectItem(dev, "state")->valuestring : "Error";

        char info[512];
        snprintf(info, sizeof(info), "\nState: %s\n", s_state);

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

        /* Allocate context */
        device_ctx_t *ctx = malloc(sizeof(device_ctx_t));
        if (s_state == NULL) {
            ctx->state = STATE_ERROR;
        } else if (strcmp(s_state, "on") == 0) {
            ctx->state = STATE_ACTIVE;
        } else if (strcmp(s_state, "off") == 0) {
            ctx->state = STATE_INACTIVE;
        } else {
            ctx->state = STATE_ERROR;
        }

        ctx->location = malloc(strlen(s_location) + 1);
        strcpy(ctx->location, s_location);

        create_device_card(cont, s_location, info, ctx);
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


static void open_edit_dialog(lv_obj_t * parent, device_ctx_t * data)
{
    /* Create a modal background */
    lv_obj_t * modal = lv_obj_create(parent);
    lv_obj_set_size(modal, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(modal, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(modal, LV_OPA_50, 0);

    /* Create the dialog window */
    lv_obj_t * win = lv_win_create(modal);
    lv_obj_set_flex_flow(win, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(win,
                        LV_FLEX_ALIGN_START,   /* children start at top */
                        LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER);
    lv_win_add_title(win, "Edit data");

    /* FIX: give the dialog a real size */
    lv_obj_set_size(win, LV_PCT(100), LV_PCT(70));
    lv_obj_center(win);

    /* Container for fields */
    lv_obj_t * cont = lv_obj_create(win);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont,
                        LV_FLEX_ALIGN_START,   /* children start at top */
                        LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);

    /* Location field */
    lv_obj_t * edit = lv_textarea_create(cont);
    lv_textarea_set_one_line(edit, true);
    lv_textarea_set_placeholder_text(edit, "Enter value");
    lv_textarea_set_text(edit, data->location);   // or any string
    lv_obj_set_width(edit, LV_PCT(100));

    /* Virtual keyboard */
    lv_obj_t * kb = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(kb, LV_PCT(100), LV_PCT(40));
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* Link keyboard to textarea */
    lv_keyboard_set_textarea(kb, edit);

    lv_obj_add_event_cb(edit, ta_event_cb, LV_EVENT_ALL, kb);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    
    /* State dropdown */
    lv_obj_t * dd_state = lv_dropdown_create(cont);
    lv_dropdown_set_options(dd_state,
        "Active\n"
        "Inactive\n"
        "Error");

    lv_dropdown_set_selected(dd_state, data->state);

    /* Buttons container */
    lv_obj_t * btn_cont = lv_obj_create(win);
    lv_obj_set_size(btn_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont,
                        LV_FLEX_ALIGN_SPACE_EVENLY,   /* space buttons evenly */
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

    /* OK button */
    lv_obj_t * btn_ok = lv_btn_create(btn_cont);
    lv_obj_set_width(btn_ok, LV_PCT(45));
    lv_obj_add_event_cb(btn_ok, dialog_ok_event, LV_EVENT_CLICKED, data);
    lv_label_set_text(lv_label_create(btn_ok), "OK");

    /* Cancel button */
    lv_obj_t * btn_cancel = lv_btn_create(btn_cont);
    lv_obj_set_width(btn_cancel, LV_PCT(45));
    lv_obj_add_event_cb(btn_cancel, dialog_cancel_event, LV_EVENT_CLICKED, modal);
    lv_label_set_text(lv_label_create(btn_cancel), "Cancel");

    /* Store pointers inside the window for later */
    lv_obj_set_user_data(win, edit);
}

static void dialog_ok_event(lv_event_t * e)
{
    device_ctx_t * data = lv_event_get_user_data(e);

    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * cont = lv_obj_get_parent(btn);
    lv_obj_t * win = lv_obj_get_parent(cont);
    lv_obj_t * edit = lv_obj_get_user_data(win);

    const char *new_text = lv_textarea_get_text(edit);
    ESP_LOGI(TAG, "Debug :%s", new_text);

    data->location = realloc(data->location, strlen(new_text) + 1);
    strcpy(data->location, new_text);

    lv_obj_del(lv_obj_get_parent(win));  // Delete the modal background, which also deletes the window
}

static void dialog_cancel_event(lv_event_t * e)
{
    lv_obj_t * modal = lv_event_get_user_data(e);
    lv_obj_del(modal);
}

static void ta_event_cb(lv_event_t * e)
{
    lv_obj_t * ta = lv_event_get_target(e);
    lv_obj_t * kb = lv_event_get_user_data(e);

    if(lv_event_get_code(e) == LV_EVENT_FOCUSED) {
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(kb, ta);
    }
    else if(lv_event_get_code(e) == LV_EVENT_DEFOCUSED) {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}
