idf_component_register(
    SRCS "lvgl_port.c" "waveshare_rgb_lcd_port.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio driver esp_lcd espressif__esp_lcd_touch espressif__esp_lcd_touch_gt911 lvgl__lvgl esp_timer)