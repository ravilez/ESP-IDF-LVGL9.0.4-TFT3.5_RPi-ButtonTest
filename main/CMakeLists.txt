idf_component_register(SRCS "ui_create_tabs.c" "main.c" "panel_lvgl_init.c" "tests.c" "touch_calibration.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES cjson esp-lcd-ili9486 lvgl esp_lvgl_port nvs_flash spiffs
                    REQUIRES esp_lcd driver)
spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)