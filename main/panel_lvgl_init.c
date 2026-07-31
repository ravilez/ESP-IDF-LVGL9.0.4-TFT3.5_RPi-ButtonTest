
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_ili9486_panel.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "panel_lvgl_init.h"

#define TOUCH_IRQ    27

static const char *TAG = "ili9486_display";

/* ------------------------- */
/* ---- USER CONFIG AREA --- */
/* ------------------------- */

#define LCD_HOST           CONFIG_ILI9486_SPI_HOST
#define PIN_NUM_MOSI       CONFIG_ILI9486_PIN_MOSI
#define PIN_NUM_MISO       19  //used by touch
#define PIN_NUM_CLK        CONFIG_ILI9486_PIN_CLK
#define PIN_NUM_CS         CONFIG_ILI9486_PIN_CS
#define PIN_NUM_TCS        5 
#define PIN_NUM_DC         CONFIG_ILI9486_PIN_DC
#define PIN_NUM_RST        CONFIG_ILI9486_PIN_RST
#define PIN_NUM_BK_LIGHT   CONFIG_ILI9486_PIN_BL
#define LCD_PIXEL_CLOCK_HZ CONFIG_ILI9486_PIXEL_CLK_HZ
#define LCD_H_RES          CONFIG_ILI9486_H_RES
#define LCD_V_RES          CONFIG_ILI9486_V_RES/* ------------------------- */

// ─── ili9486_display.c ──────────────────────────────────────────────────────

static esp_lcd_panel_io_handle_t s_io_handle = NULL;
static esp_lcd_panel_handle_t   s_panel      = NULL;
static lv_display_t             *s_disp      = NULL;  // <-- ADD THIS

esp_lcd_touch_handle_t tp;

void initialize_touch_driver()
{
        gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << TOUCH_IRQ,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,      // XPT2046 PENIRQ is open-drain, needs pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,        // ESP_LCD will handle interrupts
    };
    gpio_config(&io_conf);

    esp_lcd_touch_config_t touch_config = {
        .x_max = 320,
        .y_max = 480,
        .int_gpio_num = TOUCH_IRQ,   // -1 means no IRQ, polling only
        .levels.interrupt = 0,
        .flags = {
                .swap_xy = false,      // IMPORTANTE
                .mirror_x = true,
                .mirror_y = true,
            },    
        };

    // 1. Create a separate SPI IO handle for the XPT2046
    esp_lcd_panel_io_handle_t touch_io = NULL;

    // You already initialized the SPI bus for LCD on LCD_HOST with MISO=PIN_NUM_MISO,
    // so we can reuse that bus here and just add a new device (touch CS).
    esp_lcd_panel_io_spi_config_t tp_io_config =
    ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(PIN_NUM_TCS /* or your TOUCH_CS pin */);
    // If your touch has a different CS than the LCD, define TOUCH_CS and use that instead.

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)LCD_HOST,
        &tp_io_config,
        &touch_io
    ));

    // 2. Create the touch driver with a valid IO handle
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(touch_io, &touch_config, &tp));
}

void touch_driver_read(lv_indev_t * indev, lv_indev_data_t * data)
{    
    uint16_t x, y;
    uint8_t count = 0;

    esp_lcd_touch_read_data(tp);

    data->state = LV_INDEV_STATE_RELEASED;

    if (esp_lcd_touch_get_coordinates(tp, &x, &y, NULL, &count, 1)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;

        ESP_LOGI("TOUCH", "x=%d y=%d count=%d", x, y, count);

    }

    data->continue_reading = false;
}

void initialize_input(void)
{
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_driver_read);
}

static bool ili9486_color_trans_done_cb(
    esp_lcd_panel_io_handle_t panel_io,
    esp_lcd_panel_io_event_data_t *edata,
    void *user_ctx)
{
    lvgl_port_flush_ready(user_ctx);
    return false;
}



esp_err_t ili9486_display_init(lv_display_t** handle)
{

    lv_display_t *s_disp;
    esp_err_t ret;

    /* ── SPI bus ───────────────────────────────────────────────────────────── */
    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .mosi_io_num     = PIN_NUM_MOSI,
        .miso_io_num     = PIN_NUM_MISO,
        .sclk_io_num     = PIN_NUM_CLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(
        spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO),
        TAG, "SPI bus init failed");

    /* ── Panel IO ──────────────────────────────────────────────────────────── */
    ESP_LOGI(TAG, "Install panel IO");
   esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num       = PIN_NUM_DC,
        .cs_gpio_num       = PIN_NUM_CS,
        .pclk_hz =  LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits      = 8,   // was 8
        .lcd_param_bits    = 8,   // was 8
        .spi_mode          = 0,
        .trans_queue_depth = 10,
        //.on_color_trans_done = ili9486_color_trans_done_cb
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                  &io_config, &s_io_handle),
        TAG, "Panel IO init failed");

    /* ── Backlight GPIO ────────────────────────────────────────────────────── */
    gpio_config_t bk_conf = {
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BK_LIGHT,
    };
    gpio_config(&bk_conf);
    gpio_set_level(PIN_NUM_BK_LIGHT, 0);   // keep off during init

    /* ── Create ILI9486 panel (reset pin handled inside) ──────────────────── */
    ESP_LOGI(TAG, "Install ILI9486 panel driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num  = PIN_NUM_RST,
        .bits_per_pixel  = 16,
        //.rgb_endian = LCD_RGB_ENDIAN_RGB,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_ili9486(s_io_handle, &panel_config, &s_panel),
        TAG, "Panel create failed");

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));

    // Force BGR bit in MADCTL (bit 3)
    //esp_lcd_panel_io_tx_param(s_io_handle, 0x36,
      //  (uint8_t[]){ 0x48 }, 1);   // MX=1, BGR=1 → portrait + correct colours


    //ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, true, false));  // mirror X only
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    /* Backlight ON */
    gpio_set_level(PIN_NUM_BK_LIGHT, 1);

    /* ── LVGL ──────────────────────────────────────────────────────────────── */
    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle     = s_io_handle,
        .panel_handle  = s_panel,           // ← valid handle, no crash
        .buffer_size   = LCD_H_RES * 80,
        .double_buffer = true,
        .hres          = LCD_H_RES,
        .vres          = LCD_V_RES,
        .monochrome    = false,
#if LVGL_VERSION_MAJOR >= 9
        .color_format  = LV_COLOR_FORMAT_RGB565,
#endif
        .rotation = {
            .swap_xy  = false,
            .mirror_x = true,
            .mirror_y = false,
        },
        .flags = {
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = true,   // ILI9486 is big-endian; LVGL is little-endian
#endif
            .sw_rotate  = false,
        }
    };
    
    // 2. Store the return value of lvgl_port_add_disp (was being discarded)
    s_disp = lvgl_port_add_disp(&disp_cfg);  // <-- was: lvgl_port_add_disp(&disp_cfg);
    *handle = s_disp;



//    vTaskDelay(pdMS_TO_TICKS(3000));   // wait for LVGL to flush the first frame before setting resolution


    ESP_LOGI(TAG, "ILI9486 initialization complete");
    return ESP_OK;
}




esp_lcd_panel_handle_t ili9486_display_get_panel(void)
{
    if(!s_panel) {
        ESP_LOGE(TAG, "Panel not initialized");
        return NULL;
    }
    return s_panel;
}

esp_lcd_panel_io_handle_t ili9486_display_get_panel_io(void)
{
    if(!s_io_handle) {
        ESP_LOGE(TAG, "Panel IO not initialized");
        return NULL;
    }
    return s_io_handle;
}

