idf_component_register(
    SRCS "waveshare_rgb_lcd_port.c" "main.c" "lvgl_port.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES spi_flash
    REQUIRES mbedtls nvs_flash esp_http_client cjson esp_wifi)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
