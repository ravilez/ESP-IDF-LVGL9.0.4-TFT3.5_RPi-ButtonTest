

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "panel_lvgl_init.h"   // your existing display init header
#include "touch_calibration.h"   
#include "tests.h"   // your existing display init header
#include "esp_lvgl_port.h"
#include "lvgl.h"  // LVGL main header


static const char* TAG = "main";
// ── Pixel buffer — one row at a time to avoid large stack allocs ──────────────

extern esp_lcd_touch_handle_t tp;
extern touch_calibration_t cal;

static void delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void log_diagnostics(void)
{
    ESP_LOGI(TAG, "──────────────────────────────────────────");
    ESP_LOGI(TAG, "Colour demo complete — diagnostic hints:");
    ESP_LOGI(TAG, "  All primaries correct             -> driver OK");
    ESP_LOGI(TAG, "  Red/Blue swapped on solid screens -> byte-swap issue");
    ESP_LOGI(TAG, "  Stripe colours wrong hue          -> RGB order wrong");
    ESP_LOGI(TAG, "  Bar never changes colour          -> style refresh issue");
    ESP_LOGI(TAG, "  Text only in one strip            -> RASET not fixed");
    ESP_LOGI(TAG, "──────────────────────────────────────────");
}

static void msgbox_event_cb(lv_event_t * e)
{
    lv_obj_t * msgbox = lv_event_get_user_data(e);
    
    if (msgbox)
        lv_msgbox_close_async(msgbox);
}

static void msgbox_create(void)
{
    if (!lvgl_port_lock(0)) return;

    lv_obj_t * mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, "uSPX");
    lv_msgbox_add_text(mbox, "Welcome");

    lv_obj_set_style_bg_color(lv_msgbox_get_header(mbox), lv_color_black(), 0);
    

    lv_obj_t * btn = lv_msgbox_add_footer_button(mbox, "Ok");
    lv_obj_add_event_cb(btn, msgbox_event_cb, LV_EVENT_CLICKED, mbox);
    lv_obj_add_state(btn, LV_STATE_FOCUS_KEY);

    lv_obj_align(mbox, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t * bg = lv_obj_get_parent(mbox);
    lv_obj_set_style_bg_opa(bg, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(bg, lv_color_hex(0xaabb80), 0);

    lvgl_port_unlock();
}

void app_main(void)
{
    ESP_LOGI(TAG, "ILI9486 LVGL colour demo");

    lv_display_t *handle = NULL;
    esp_err_t ret = ili9486_display_init(&handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel init failed: %s", esp_err_to_name(ret));
        return;
    }

    // 2. Initialize touch driver (XPT2026)
    initialize_touch_driver();  // esp_lcd_touch_new_xpt2046()

    initialize_input();

    /*touch_calibration_t tmp;
    if (!touch_load_calibration(&tmp)) {
        touch_run_calibration(handle,tp);
    } else {
        touch_set_calibration(&tmp);
    }*/

    demo_test_styled_buttons();
    ui_create_settings_menu(disp);

    msgbox_create();

    while (1) {
        
        /*demo_test_primary_flashes();
        demo_test_rainbow_stripes();
        demo_test_corner_labels();
        demo_test_styled_buttons();
        demo_test_colour_bar();

        log_diagnostics();
        vTaskDelay(1000 / portTICK_PERIOD_MS);*/

        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}