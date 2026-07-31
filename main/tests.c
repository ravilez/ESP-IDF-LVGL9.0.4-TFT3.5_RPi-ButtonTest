// ─── colour_demo.c ────────────────────────────────────────────────────────────
#include "tests.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "touch_calibration.h"

#define TAG "COLOUR_DEMO"

// Panel dimensions — adjust to match your display
#define PANEL_W 320
#define PANEL_H 480

// ─── Helpers ──────────────────────────────────────────────────────────────────

static lv_obj_t *get_clean_screen(lv_color_t bg)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, bg, 0);
    return scr;
}

static lv_obj_t *add_label(lv_obj_t *parent, const char *text,
                            lv_color_t color, const lv_font_t *font)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_font(lbl, font, 0);
    return lbl;
}

static void delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// ─── Test 1: Full-screen primary colour flashes ───────────────────────────────
// Fills the whole screen R → G → B → White.
// A byte-swap bug makes Red appear Blue and vice-versa.

void demo_test_primary_flashes(void)
{
    ESP_LOGI(TAG, "Test 1: primary colour flashes");

    static const struct { const char *name; lv_color_t col; } kPrimaries[] = {
        { "RED",   { .red = 255, .green =   0, .blue =   0 } },
        { "GREEN", { .red =   0, .green = 255, .blue =   0 } },
        { "BLUE",  { .red =   0, .green =   0, .blue = 255 } },
        { "WHITE", { .red = 255, .green = 255, .blue = 255 } },
    };

    for (int i = 0; i < 4; i++) {
        if (!lvgl_port_lock(0)) continue;

        lv_obj_t *scr = get_clean_screen(kPrimaries[i].col);
        bool is_white = (i == 3);
        lv_obj_t *lbl = add_label(scr, kPrimaries[i].name,
                                   is_white ? lv_color_black() : lv_color_white(),
                                   &lv_font_montserrat_14);
        lv_obj_center(lbl);

        lvgl_port_unlock();
        delay_ms(800);
    }
}

// ─── Test 2: Rainbow horizontal stripes ───────────────────────────────────────
// Seven ROYGBIV bands in one frame.
// Wrong hue on any band → RGB channel order is incorrect.

void demo_test_rainbow_stripes(void)
{
    ESP_LOGI(TAG, "Test 2: rainbow stripes");

    static const struct { const char *name; lv_color_t col; } kRainbow[] = {
        { "Red",    { .red = 255, .green =   0, .blue =   0 } },
        { "Orange", { .red = 255, .green = 127, .blue =   0 } },
        { "Yellow", { .red = 255, .green = 255, .blue =   0 } },
        { "Green",  { .red =   0, .green = 255, .blue =   0 } },
        { "Blue",   { .red =   0, .green =   0, .blue = 255 } },
        { "Indigo", { .red =  75, .green =   0, .blue = 130 } },
        { "Violet", { .red = 148, .green =   0, .blue = 211 } },
    };
    const int kBands      = sizeof(kRainbow) / sizeof(kRainbow[0]);
    const int kStripeH    = PANEL_H / kBands;

    if (!lvgl_port_lock(0)) return;

    lv_obj_t *scr = get_clean_screen(lv_color_black());

    for (int i = 0; i < kBands; i++) {
        lv_obj_t *band = lv_obj_create(scr);
        lv_obj_set_size(band, PANEL_W, kStripeH);
        lv_obj_set_pos(band, 0, i * kStripeH);
        lv_obj_set_style_bg_color(band, kRainbow[i].col, 0);
        lv_obj_set_style_border_width(band, 0, 0);
        lv_obj_set_style_radius(band, 0, 0);
        lv_obj_set_style_pad_all(band, 0, 0);

        lv_obj_t *lbl = add_label(band, kRainbow[i].name,
                                   lv_color_white(), &lv_font_montserrat_14);
        lv_obj_center(lbl);
    }

    lvgl_port_unlock();
    delay_ms(3000);
}

// ─── Test 3: Coloured labels at corners + centre ──────────────────────────────
// Verifies CASET/RASET window addressing across the full pixel grid.

void demo_test_corner_labels(void)
{
    ESP_LOGI(TAG, "Test 3: corner labels");

    static const struct {
        const char  *txt;
        lv_color_t   col;
        lv_align_t   align;
    } kCorners[] = {
        { "TOP-LEFT",  { .red = 255, .green =  80, .blue =  80 }, LV_ALIGN_TOP_LEFT     },
        { "TOP-RIGHT", { .red =  80, .green = 255, .blue =  80 }, LV_ALIGN_TOP_RIGHT    },
        { "BOT-LEFT",  { .red =  80, .green =  80, .blue = 255 }, LV_ALIGN_BOTTOM_LEFT  },
        { "BOT-RIGHT", { .red = 255, .green = 255, .blue =   0 }, LV_ALIGN_BOTTOM_RIGHT },
        { "CENTRE",    { .red =   0, .green = 255, .blue = 255 }, LV_ALIGN_CENTER       },
    };

    if (!lvgl_port_lock(0)) return;

    lv_obj_t *scr = get_clean_screen(lv_color_make(20, 20, 20));

    for (int i = 0; i < 5; i++) {
        lv_obj_t *lbl = add_label(scr, kCorners[i].txt,
                                   kCorners[i].col, &lv_font_montserrat_14);
        lv_obj_align(lbl, kCorners[i].align, 5, 0);
    }

    lvgl_port_unlock();
    delay_ms(2000);
}

// ─── Test 4: Styled buttons ───────────────────────────────────────────────────
// Overrides the default theme colour so we can verify arbitrary RGB values
// render correctly on widget backgrounds and borders.

static lv_obj_t *create_coloured_button(lv_obj_t *parent,
                                         const char *label_text,
                                         lv_color_t  bg,
                                         lv_color_t  bg_pressed,
                                         lv_color_t  border,
                                         lv_color_t  text_col,
                                         lv_align_t  align,
                                         lv_coord_t  y_offset)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 180, 55);
    lv_obj_align(btn, align, 0, y_offset);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_color(btn, bg_pressed, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, border, 0);
    lv_obj_set_style_border_width(btn, 2, 0);

    lv_obj_t *lbl = add_label(btn, label_text, text_col, &lv_font_montserrat_14);
    lv_obj_center(lbl);

    return btn;
}

static void btn_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        ESP_LOGI("BTN", "pressed");
    }
}

void demo_test_styled_buttons(void)
{
    ESP_LOGI(TAG, "Test 4: styled buttons");

    if (!lvgl_port_lock(0)) return;

    lv_obj_t *scr = get_clean_screen(lv_color_make(30, 30, 30));

    lv_obj_t *btn1 = create_coloured_button(scr, "MAGENTA",
                           lv_color_make(200,   0, 200),
                           lv_color_make(255,  80, 255),
                           lv_color_make(255, 180, 255),
                           lv_color_white(),
                           LV_ALIGN_CENTER, -50);
    lv_obj_add_event_cb(btn1, btn_event_handler, LV_EVENT_PRESSED, NULL);

    lv_obj_t *btn2 = create_coloured_button(scr, "CYAN",
                           lv_color_make(  0, 180, 180),
                           lv_color_make(  0, 0, 0),
                           lv_color_make(255, 255, 255),
                           lv_color_make(255, 0, 0),
                           LV_ALIGN_CENTER, +50);
    lv_obj_add_event_cb(btn2, btn_event_handler, LV_EVENT_PRESSED, NULL);

    lvgl_port_unlock();
    delay_ms(2500);
}

// ─── Test 5: Colour-cycling animated progress bar ─────────────────────────────
// Sweeps 0 → 100 three times, changing the indicator colour each pass (R/G/B).
// Verifies continuous flushing AND per-channel colour accuracy over time.

static void run_bar_pass(lv_obj_t *bar, lv_obj_t *pass_lbl,
                          lv_color_t color, const char *pass_name)
{
    if (lvgl_port_lock(0)) {
        lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
        lv_label_set_text(pass_lbl, pass_name);
        lv_obj_set_style_text_color(pass_lbl, color, 0);
        lvgl_port_unlock();
    }

    for (int v = 0; v <= 100; v += 2) {
        if (lvgl_port_lock(0)) {
            lv_bar_set_value(bar, v, LV_ANIM_ON);
            lvgl_port_unlock();
        }
        delay_ms(40);
    }

    delay_ms(400);
}

void demo_test_colour_bar(void)
{
    ESP_LOGI(TAG, "Test 5: colour-cycling progress bar");

    lv_obj_t *scr     = NULL;
    lv_obj_t *bar     = NULL;
    lv_obj_t *pass_lbl = NULL;

    if (!lvgl_port_lock(0)) return;

    scr = get_clean_screen(lv_color_make(15, 15, 15));

    lv_obj_t *title = add_label(scr, "Colour Progress Bar",
                                 lv_color_white(), &lv_font_montserrat_14);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    bar = lv_bar_create(scr);
    lv_obj_set_size(bar, 280, 35);
    lv_obj_center(bar);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_make(50, 50, 50), 0);

    // Create label once — each pass updates it in-place
    pass_lbl = add_label(scr, "", lv_color_white(), &lv_font_montserrat_14);
    lv_obj_align(pass_lbl, LV_ALIGN_CENTER, 0, 50);

    lvgl_port_unlock();

    static const struct { lv_color_t col; const char *name; } kPasses[] = {
        { { .red = 255, .green =  50, .blue =  50 }, "RED PASS"   },
        { { .red =  50, .green = 255, .blue =  50 }, "GREEN PASS" },
        { { .red =  50, .green = 100, .blue = 255 }, "BLUE PASS"  },
    };

    for (int i = 0; i < 3; i++) {
        run_bar_pass(bar, pass_lbl, kPasses[i].col, kPasses[i].name);
    }

    delay_ms(1000);
}

