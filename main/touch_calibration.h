#pragma once

#include "lvgl.h"
#include "esp_lcd_touch.h"

typedef struct {
    float a, b, c;
    float d, e, f;
    bool valid;
} touch_calibration_t;

void touch_set_calibration(const touch_calibration_t *src);
bool touch_load_calibration(touch_calibration_t *out);
void touch_save_calibration(const touch_calibration_t *cal);
void touch_run_calibration(esp_lcd_touch_handle_t tp);
void touch_apply_calibration(int raw_x, int raw_y, int *x, int *y);
void btn_event(lv_event_t *e);
void ui_create_settings_menu(lv_disp_t *disp);
void draw_cross(int x, int y);
void clear_cross();
void btn_cal_event(lv_event_t *e);