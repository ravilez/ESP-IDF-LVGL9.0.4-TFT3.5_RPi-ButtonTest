#include "touch_calibration.h"
#include "panel_lvgl_init.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

#define TAG "CALIB"

static lv_obj_t *cross;
static lv_obj_t *msg = NULL;

static uint8_t l_counter = 1;

extern esp_lcd_touch_handle_t tp;
touch_calibration_t cal;

int xr1, yr1, xr2, yr2, xr3, yr3, xr4, yr4;


static void ui_show_message(const char *text)
{
    if (!lvgl_port_lock(0)) return;

    // Parent object (screen)
    lv_obj_t *parent = lv_scr_act();

    if (!msg) {
        msg = lv_label_create(parent);
        lv_obj_set_style_text_color(msg, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(msg, &lv_font_montserrat_16, 0);
    }
    lv_label_set_text(msg, text);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, -200);

    lvgl_port_unlock();
}

void touch_set_calibration(const touch_calibration_t *src)
{
    cal = *src;
}

// ---------------------------------------------------------
// NVS LOAD
// ---------------------------------------------------------
bool touch_load_calibration(touch_calibration_t *out)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("touch", NVS_READONLY, &h);
    if (err != ESP_OK) return false;

    size_t size = sizeof(touch_calibration_t);
    err = nvs_get_blob(h, "cal", out, &size);
    nvs_close(h);

    if (err == ESP_OK && out->valid) {
        ESP_LOGI(TAG, "Calibration loaded");
        return true;
    }

    return false;
}

// ---------------------------------------------------------
// NVS SAVE
// ---------------------------------------------------------
void touch_save_calibration(const touch_calibration_t *cal)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("touch", NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_blob(h, "cal", cal, sizeof(*cal)));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);
    ESP_LOGI(TAG, "Calibration saved");
}

// ---------------------------------------------------------
// APPLY CALIBRATION
// ---------------------------------------------------------
void touch_apply_calibration(int raw_x, int raw_y, int *x, int *y)
{
    if (!cal.valid) {
        *x = raw_x;
        *y = raw_y;
        return;
    }

    *x = (int)(cal.a * raw_x + cal.b * raw_y + cal.c);
    *y = (int)(cal.d * raw_x + cal.e * raw_y + cal.f);
}

// ---------------------------------------------------------
// COMPUTE CALIBRATION (4 points)
// ---------------------------------------------------------
static void compute_cal(
    int xr1, int yr1, int xs1, int ys1,
    int xr2, int yr2, int xs2, int ys2,
    int xr3, int yr3, int xs3, int ys3,
    int xr4, int yr4, int xs4, int ys4)
{
    float det = (xr1 - xr3) * (yr2 - yr3) - (xr2 - xr3) * (yr1 - yr3);

    cal.a = ((xs1 - xs3) * (yr2 - yr3) - (xs2 - xs3) * (yr1 - yr3)) / det;
    cal.b = ((xr1 - xr3) * (xs2 - xs3) - (xr2 - xr3) * (xs1 - xs3)) / det;
    cal.c = xs1 - cal.a * xr1 - cal.b * yr1;

    cal.d = ((ys1 - ys3) * (yr2 - yr3) - (ys2 - ys3) * (yr1 - yr3)) / det;
    cal.e = ((xr1 - xr3) * (ys2 - ys3) - (xr2 - xr3) * (ys1 - ys3)) / det;
    cal.f = ys1 - cal.d * xr1 - cal.e * yr1;

    cal.valid = true;

    ESP_LOGI("CAL", "a=%f b=%f c=%f d=%f e=%f f=%f", cal.a, cal.b, cal.c, cal.d, cal.e, cal.f);
}


// ---------------------------------------------------------
// DRAW CROSS
// ---------------------------------------------------------
void draw_cross(int cx, int cy)
{
    if (!lvgl_port_lock(0)) return;

    // Parent object (screen)
    lv_obj_t *parent = lv_scr_act();

    // Thickness of the lines
    int thickness = 2;
    int size = 30;

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, size, size);

    // Calculate top-left so that center is at (cx, cy)
    lv_coord_t x = cx - size / 2;
    lv_coord_t y = cy - size / 2;

    lv_obj_set_pos(btn, x, y);

    lv_obj_add_event_cb(btn, &btn_cal_event, LV_EVENT_CLICKED, NULL);

    // Vertical line
    lv_obj_t *vline = lv_obj_create(parent);
    lv_obj_set_size(vline, thickness, size);
    lv_obj_set_style_bg_color(vline, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(vline, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(vline, 0, 0);
    lv_obj_set_pos(vline, x + size/2 - thickness/2, y);

    // Horizontal line
    lv_obj_t *hline = lv_obj_create(parent);
    lv_obj_set_size(hline, size, thickness);
    lv_obj_set_style_bg_color(hline, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(hline, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(hline, 0, 0);
    lv_obj_set_pos(hline, x, y + size/2 - thickness/2);

    lvgl_port_unlock();

    vTaskDelay(pdMS_TO_TICKS(100));

}

// ---------------------------------------------------------
// MAIN CALIBRATION UI
// ---------------------------------------------------------
void touch_run_calibration(esp_lcd_touch_handle_t tp)
{
    int w = lv_disp_get_hor_res(disp);
    int h = lv_disp_get_ver_res(disp);

    ESP_LOGI("DISP", "w=%d h=%d", w, h);

    // TOP LEFT
    ui_show_message("Toque no ponto 1");
    if (l_counter == 1) {
        draw_cross(15, 15);
        return;
    }
 
    // TOP RIGHT
    ui_show_message("Toque no ponto 2");
    if (l_counter == 2) {
        draw_cross(w-15, 15);
        return;
    }

    // BOTTOM RIGHT
    ui_show_message("Toque no ponto 3");
    if (l_counter == 3) {
        draw_cross(w - 15, h - 15);
        return;
    }

    // BOTTOM LEFT
    ui_show_message("Toque no ponto 4");
    if (l_counter == 4) {
        draw_cross(15, h - 15);
        return;
    }

    if (l_counter == 5) {
        compute_cal(
            xr1, yr1, 0, 0,
            xr2, yr2, w, 0,
            xr3, yr3, w, h,
            xr4, yr4, 0, h
        );
        l_counter = 1;

        //touch_save_calibration(&cal);

        //lv_obj_clean(lv_scr_act());
        //ui_show_message("Calibração concluída!");*/
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void btn_cal_event(lv_event_t *e) {
    
    ESP_LOGI("CAL_BTN_EVENT", "CLICKED");
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI("CAL_BTN", "pressed");
        lv_indev_t * indev = lv_event_get_indev(e);
        if (indev) {
            lv_point_t point;
            lv_indev_get_point(indev, &point); // Get coordinates
            ESP_LOGI("CAL_TOUCH", "x=%d y=%d counter=%d", point.x, point.y, l_counter);
            if (l_counter==1) {
                xr1 = point.x; 
                yr1 = point.y;
            }
            if (l_counter==2) {
                xr2 = point.x; 
                yr2 = point.y;
            }
            if (l_counter==3) {
                xr3 = point.x; 
                yr3 = point.y;
            }
            if (l_counter==4) {
                xr4 = point.x; 
                yr4 = point.y;
            }
            l_counter++;
            lv_obj_t *widget = lv_event_get_target_obj(e);
            lv_obj_delete(widget);
            lv_display_refr_timer(NULL);

            touch_run_calibration(tp);

        } else {
            ESP_LOGI("CAL_BTN","No input device associated with this event.");
        }
    }
}

void btn_event(lv_event_t *e) {
    touch_run_calibration(tp);
}

void ui_create_settings_menu(lv_disp_t *disp)
{
    if (!lvgl_port_lock(0)) return;

    lv_obj_t *parent = lv_scr_act();

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 180, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -150);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "Recalibrate Touch");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF) , 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, &btn_event, LV_EVENT_CLICKED, NULL);

    lvgl_port_unlock();
}
