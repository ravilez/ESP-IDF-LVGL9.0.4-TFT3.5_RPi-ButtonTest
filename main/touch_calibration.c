#include "touch_calibration.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#define TAG "CALIB"

static touch_calibration_t cal;
static lv_obj_t *cross;
static lv_obj_t *msg;

extern esp_lcd_touch_handle_t tp;

static void ui_show_message(const char *text)
{
    if (!msg) {
        msg = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_color(msg, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(msg, LV_FONT_MONTSERRAT_16, 0);
    }
    lv_label_set_text(msg, text);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, -40);
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
}

// ---------------------------------------------------------
// WAIT FOR TOUCH AND RETURN RAW
// ---------------------------------------------------------
static void wait_touch(esp_lcd_touch_handle_t tp, int *x, int *y)
{
    while (1) {
        esp_lcd_touch_read_data(tp);

        uint16_t tx, ty, strength;
        uint8_t point_num = 0;

        bool touched = esp_lcd_touch_get_coordinates(
            tp,
            &tx,
            &ty,
            &strength,
            &point_num,
            1
        );

        if (touched && point_num > 0) {
            *x = tx;
            *y = ty;
            return;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ---------------------------------------------------------
// DRAW CROSS
// ---------------------------------------------------------
static void draw_cross(int x, int y)
{
    lv_obj_clean(lv_scr_act());

    // Background dim
    lv_obj_t *bg = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(bg, LV_OPA_50, 0);

    // Cross
    cross = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cross, 30, 30);
    lv_obj_set_style_bg_color(cross, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_radius(cross, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_pos(cross, x - 15, y - 15);
}

// ---------------------------------------------------------
// MAIN CALIBRATION UI
// ---------------------------------------------------------
void touch_run_calibration(lv_disp_t *disp, esp_lcd_touch_handle_t tp)
{
    int w = lv_disp_get_hor_res(disp);
    int h = lv_disp_get_ver_res(disp);

    int xr1, yr1, xr2, yr2, xr3, yr3, xr4, yr4;

    // TOP LEFT
    ui_show_message("Toque no ponto 1");
    draw_cross(20, 20);
    wait_touch(tp,&xr1, &yr1);

    // TOP RIGHT
    ui_show_message("Toque no ponto 2");
    draw_cross(w - 20, 20);
    wait_touch(tp,&xr2, &yr2);

    // BOTTOM RIGHT
    ui_show_message("Toque no ponto 3");
    draw_cross(w - 20, h - 20);
    wait_touch(tp,&xr3, &yr3);

    // BOTTOM LEFT
    ui_show_message("Toque no ponto 4");
    draw_cross(20, h - 20);
    wait_touch(tp, &xr4, &yr4);

    compute_cal(
        xr1, yr1, 0, 0,
        xr2, yr2, w, 0,
        xr3, yr3, w, h,
        xr4, yr4, 0, h
    );

    touch_save_calibration(&cal);

    lv_obj_clean(lv_scr_act());
    ui_show_message("Calibração concluída!");
    vTaskDelay(pdMS_TO_TICKS(1000));
}



void btn_event(lv_event_t *e) {
    lv_disp_t *d = lv_event_get_user_data(e);
    touch_run_calibration(d,tp);
}

void ui_create_settings_menu(lv_disp_t *disp)
{
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 180, 60);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Recalibrar Touch");

    lv_obj_add_event_cb(btn, &btn_event, LV_EVENT_CLICKED, disp);
}
