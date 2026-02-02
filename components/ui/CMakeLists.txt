idf_component_register(
    SRCS "ui.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio driver calendar esp_lcd espressif__esp_lcd_touch espressif__esp_lcd_touch_gt911 lvgl__lvgl waveshare_lcd esp_timer net)