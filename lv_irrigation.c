#include "lvgl.h"
#include "cJSON.h"
#include <stdio.h>

static lv_obj_t *create_device_card(lv_obj_t *parent, const char *title, const char *info)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x2E2E2E), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x444444), 0);

    lv_obj_t *label_title = lv_label_create(card);
    lv_label_set_text(label_title, title);
    lv_obj_set_style_text_font(label_title, lv_theme_get_font_title(), 0);

    lv_obj_t *label_info = lv_label_create(card);
    lv_label_set_text(label_info, info);
    lv_obj_align(label_info, LV_ALIGN_BOTTOM_LEFT, 0, 0);

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
        snprintf(info, sizeof(info), "State: %s\n", state);

        // Extra fields depending on type
        if (strcmp(type, "valves") == 0) {
            double flow = cJSON_GetObjectItem(dev, "flow_rate_lpm")->valuedouble;
            snprintf(info + strlen(info), sizeof(info) - strlen(info),
                     "Flow: %.1f L/min\n", flow);
        }
        else if (strcmp(type, "flow_meters") == 0) {
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

        create_device_card(cont, location, info);
    }
}

void ui_irrigation_screen(const char *json_text)
{
    cJSON *root = cJSON_Parse(json_text);
    if (!root) {
        printf("JSON parse error\n");
        return;
    }

    cJSON *system = cJSON_GetObjectItem(root, "system");
    cJSON *devices = cJSON_GetObjectItem(system, "devices");

    cJSON *valves = cJSON_GetObjectItem(devices, "valves");
    cJSON *flow_meters = cJSON_GetObjectItem(devices, "flow_meters");
    cJSON *pumps = cJSON_GetObjectItem(devices, "pumps");

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *tabview = lv_tabview_create(scr, LV_DIR_TOP, 40);

    lv_obj_t *tab_valves = lv_tabview_add_tab(tabview, "Valves");
    lv_obj_t *tab_flow = lv_tabview_add_tab(tabview, "Flow Meters");
    lv_obj_t *tab_pumps = lv_tabview_add_tab(tabview, "Pumps");

    load_devices_into_tab(tab_valves, valves, "valves");
    load_devices_into_tab(tab_flow, flow_meters, "flow_meters");
    load_devices_into_tab(tab_pumps, pumps, "pumps");

    lv_screen_load(scr);

    cJSON_Delete(root);
}
