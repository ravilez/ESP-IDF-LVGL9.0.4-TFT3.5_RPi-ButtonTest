#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_ili9486_panel.h"
#include "lvgl.h"
#include <stdint.h>
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_xpt2046.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_disp_t *disp;

void initialize_touch_driver();
void initialize_input();


esp_err_t ili9486_display_init(lv_display_t** handle);

esp_lcd_panel_io_handle_t ili9486_display_get_panel_io(void);


esp_lcd_panel_handle_t ili9486_display_get_panel(void);
#ifdef __cplusplus
}
#endif