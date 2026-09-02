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
    char *state_info; 
    const char *extra_info;
    char *schedule_info;
} device_info_t;

typedef struct {
    device_state_t state;
    char *location;
    device_type_t type;
    lv_obj_t *card; // Pointer to the card object for this device
    device_info_t *device_info;
} device_ctx_t;

typedef struct {
    device_ctx_t *device;     // your existing device context
    lv_obj_t *edit;        // textarea pointer
    lv_obj_t *dd_state;       // dropdown pointer
    lv_obj_t *kb;             // keyboard pointer
    lv_obj_t *modal;          // modal background
    lv_obj_t *win;            // dialog window
} dialog_ctx_t;


dialog_ctx_t *dctx = NULL;

cJSON *g_json_system = NULL;
cJSON *g_json_devices = NULL;
cJSON *g_json_valves = NULL;
cJSON *g_json_meters = NULL;
cJSON *g_json_pumps = NULL;
cJSON *g_json_tanks = NULL;
cJSON *g_json_sensors = NULL;

char *load_json_file(const char *path);
static void open_edit_dialog(lv_obj_t * parent, device_ctx_t * data);
static void dialog_ok_event(lv_event_t * e);
static void dialog_cancel_event(lv_event_t * e);
static void kb_event_cb(lv_event_t * e);
void rewrite_card_info(device_ctx_t *ctx);


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

static void btn_state_event_cb(lv_event_t *e)
{
    device_ctx_t *ctx = lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);

    open_edit_dialog(lv_layer_top(), ctx);
}

static lv_obj_t *create_device_card(lv_obj_t *parent, const char *title, device_ctx_t *ctx)
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

    //bind the card to the context so we can access it later
    ctx->card = card;

    lv_obj_t *label_title = lv_label_create(card);
    lv_label_set_text(label_title, title);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_24, 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *label_info = lv_label_create(card);

    size_t len = strlen(ctx->device_info->state_info) +
             strlen(ctx->device_info->extra_info) +
             strlen(ctx->device_info->schedule_info) + 1;   // +1 for '\0'

    char *result = malloc(len);
    if(result == NULL) {
        // handle allocation failure
    }

    result[0] = '\0';                
    strcat(result, ctx->device_info->state_info);
    strcat(result, ctx->device_info->extra_info);
    strcat(result, ctx->device_info->schedule_info);

    lv_label_set_text(label_info, result);
    free(result);

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

        device_info_t *device_info = malloc(sizeof(device_info_t));
        device_info->state_info = malloc(strlen(info) + 1);
        strcpy((char *)device_info->state_info, info);

        memset(info, 0, sizeof(info));
        // Extra fields depending on type
        if (strcmp(type, "valves") == 0) {
            double flow = cJSON_GetObjectItem(dev, "flow_rate_lpm")->valuedouble;
            snprintf(info, sizeof(info),"Flow: %.1f L/min\n", flow);

        }
        else if (strcmp(type, "meters") == 0) {
            double flow = cJSON_GetObjectItem(dev, "current_flow_lpm")->valuedouble;
            int total = cJSON_GetObjectItem(dev, "total_liters_today")->valueint;
            snprintf(info, sizeof(info),
                     "Flow: %.1f L/min\nTotal today: %d L\n", flow, total);
        }
        else if (strcmp(type, "pumps") == 0) {
            double pressure = cJSON_GetObjectItem(dev, "pressure_bar")->valuedouble;
            snprintf(info, sizeof(info) ,
                     "Pressure: %.1f bar\n", pressure);
        }

        if (strcmp(type, "tanks") == 0) {
            double level = cJSON_GetObjectItem(dev, "level_percent")->valuedouble;
            snprintf(info, sizeof(info) ,
                     "Level: %.1f%%\n", level);
        }

        if (info[0] != '\0') {         
            device_info->extra_info = malloc(strlen(info) + 1);
            strcpy((char *)device_info->extra_info, info);
        }

        // Schedules
        memset(info, 0, sizeof(info));
        cJSON *schedule = cJSON_GetObjectItem(dev, "schedule");
        int sched_count = cJSON_GetArraySize(schedule);

        snprintf(info, sizeof(info), "Schedules:\n");

        for (int s = 0; s < sched_count; s++) {
            cJSON *sch = cJSON_GetArrayItem(schedule, s);
            const char *start = cJSON_GetObjectItem(sch, "start_time")->valuestring;
            const char *end = cJSON_GetObjectItem(sch, "end_time")->valuestring;

            snprintf(info + strlen(info), sizeof(info) - strlen(info),
                     "  %s-%s\n", start, end);
        }
        if (sched_count > 0) {
            device_info->schedule_info = malloc(strlen(info) + 1);
            strcpy((char *)device_info->schedule_info, info);
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

        ctx->device_info = device_info;

        create_device_card(cont, s_location, ctx);
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

    g_json_system = cJSON_GetObjectItem(root, "system");
    g_json_devices = cJSON_GetObjectItem(g_json_system, "devices");

    g_json_valves = cJSON_GetObjectItem(g_json_devices, "valves");
    g_json_meters = cJSON_GetObjectItem(g_json_devices, "meters");
    g_json_pumps = cJSON_GetObjectItem(g_json_devices, "pumps");
    g_json_tanks = cJSON_GetObjectItem(g_json_devices, "tanks");
    g_json_sensors = cJSON_GetObjectItem(g_json_devices, "sensors");

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *tabview = lv_tabview_create(scr);
    lv_obj_set_size(tabview, LV_PCT(100), LV_PCT(100));

    lv_obj_t *tab_pumps   = lv_tabview_add_tab(tabview, "Pumps");
    lv_obj_t *tab_valves  = lv_tabview_add_tab(tabview, "Valves");
    lv_obj_t *tab_sensors = lv_tabview_add_tab(tabview, "Sensors");
    lv_obj_t *tab_flow    = lv_tabview_add_tab(tabview, "Meters");
    lv_obj_t *tab_tanks   = lv_tabview_add_tab(tabview, "Tanks");

    load_devices_into_tab(tab_valves, g_json_valves, "valves");
    load_devices_into_tab(tab_flow, g_json_meters, "meters");
    load_devices_into_tab(tab_pumps, g_json_pumps, "pumps");
    load_devices_into_tab(tab_tanks, g_json_tanks, "tanks");
    load_devices_into_tab(tab_sensors, g_json_sensors, "sensors");

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
    if (dctx != NULL) {
        ESP_LOGW(TAG, "Deleting dialog context");
        free(dctx);
        dctx = NULL;
    }
    dctx = malloc(sizeof(dialog_ctx_t));

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
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    
    /* State dropdown */
    lv_obj_t * dd_state = lv_dropdown_create(cont);
    lv_dropdown_set_options(dd_state,
        "Active\n"
        "Inactive\n"
        "Error");

    lv_dropdown_set_selected(dd_state, data->state);

    dctx->device = data;
    dctx->modal = modal;
    dctx->win = win;
    dctx->edit = edit;
    dctx->dd_state = dd_state;
    dctx->kb = kb;

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
    lv_obj_add_event_cb(btn_ok, dialog_ok_event, LV_EVENT_CLICKED, dctx);
    lv_label_set_text(lv_label_create(btn_ok), "OK");

    /* Cancel button */
    lv_obj_t * btn_cancel = lv_btn_create(btn_cont);
    lv_obj_set_width(btn_cancel, LV_PCT(45));
    lv_obj_add_event_cb(btn_cancel, dialog_cancel_event, LV_EVENT_CLICKED, modal);
    lv_label_set_text(lv_label_create(btn_cancel), "Cancel");

    /* Store pointers inside the window for later */
    lv_obj_set_user_data(win, dctx);
}

static void dialog_ok_event(lv_event_t * e)
{
    device_ctx_t * data = dctx->device;

    const char *new_text = lv_textarea_get_text(dctx->edit);
    ESP_LOGI(TAG, "Debug :%s", new_text);

    data->location = realloc(data->location, strlen(new_text) + 1);
    strcpy(data->location, new_text);

    lv_obj_t *label_title = lv_obj_get_child(data->card, 0); // Assuming the title label is the first child
    lv_label_set_text(label_title, data->location);


    /* Read dropdown */
    uint16_t sel = lv_dropdown_get_selected(dctx->dd_state);
    char txt[64];
    lv_dropdown_get_selected_str(dctx->dd_state, txt, sizeof(txt));
    if(strcmp(txt, "Active") == 0) 
        data->state = STATE_ACTIVE;
    else if(strcmp(txt, "Inactive") == 0) 
        data->state = STATE_INACTIVE;
    else if(strcmp(txt, "Error") == 0)
        data->state = STATE_ERROR;

    printf("Selected index: %d\n", sel);
    printf("Selected text: %s\n", txt);

    rewrite_card_info(data);

    lv_obj_del(lv_obj_get_parent(dctx->win));  // Delete the modal background, which also deletes the window

    free(dctx);  // Free the dialog context
    dctx = NULL;
}

static void dialog_cancel_event(lv_event_t * e)
{
    lv_obj_t * modal = lv_event_get_user_data(e);
    lv_obj_del(modal);

    free(dctx);  // Free the dialog context
    dctx = NULL;
}

static void kb_event_cb(lv_event_t * e)
{
    lv_obj_t * kb = lv_event_get_target(e);

    if(lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {

        // Get the ID of the last activated button
        uint16_t btn_id = lv_keyboard_get_selected_btn(kb);
        const char * txt = lv_keyboard_get_btn_text(kb,btn_id);

        fprintf(stdout, "Keyboard button pressed: %d, %s\n", btn_id, txt);


        if(btn_id == 39) { // Assuming 39 is the ID for the "Enter" button
            // Close keyboard
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

            // Optionally unfocus textarea
            lv_obj_t * ta = lv_keyboard_get_textarea(kb);
            if(ta) lv_obj_clear_state(ta, LV_STATE_FOCUSED);
        }
    }
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

void rewrite_card_info(device_ctx_t *ctx)
{
    char info[512];

    if (!ctx || !ctx->card) 
        return;

    switch(ctx->state) {
        case STATE_ACTIVE:
            snprintf(info, sizeof(info), "\nState: on\n");
            break;
        case STATE_INACTIVE:
            snprintf(info, sizeof(info), "\nState: off\n");
            break;
        case STATE_ERROR:
            snprintf(info, sizeof(info), "\nState: error\n");
            break;
        default:
            snprintf(info, sizeof(info), "\nState: n/a\n");
            break;
    }
 
    free(ctx->device_info->state_info);
    
    ctx->device_info->state_info = malloc(strlen(info) + 1);
    strcpy((char *)ctx->device_info->state_info, info);

    size_t len = strlen(ctx->device_info->state_info) +
             strlen(ctx->device_info->extra_info) +
             strlen(ctx->device_info->schedule_info) + 1;   // +1 for '\0'

    char *result = malloc(len);
    if(result == NULL) {
        // handle allocation failure
    }

    result[0] = '\0';                
    strcat(result, ctx->device_info->state_info);
    strcat(result, ctx->device_info->extra_info);
    strcat(result, ctx->device_info->schedule_info);

    lv_obj_t *label_info = lv_obj_get_child(ctx->card, 1); // Assuming the info label is the second child
    lv_label_set_text(label_info, result);

    free(result);
}