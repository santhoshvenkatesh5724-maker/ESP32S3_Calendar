idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES spi_flash waveshare_lcd
    REQUIRES mbedtls nvs_flash esp_http_client cjson esp_wifi net calendar auth waveshare_lcd ui sdcard)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
