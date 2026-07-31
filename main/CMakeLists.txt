idf_component_register(SRCS "main.c" "panel_lvgl_init.c" "tests.c" "touch_calibration.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp-lcd-ili9486 lvgl esp_lvgl_port nvs_flash
                    REQUIRES esp_lcd driver)