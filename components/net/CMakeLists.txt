if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/wifi_creds.c")
    set(WIFI_CFG_SRC wifi_creds.c)
else()
    set(WIFI_CFG_SRC wifi_creds_template.c)
endif()

idf_component_register(
    SRCS "timesync.c" "utils.c" "wifi.c" ${WIFI_CFG_SRC}
    INCLUDE_DIRS "include"
    REQUIRES mbedtls esp_wifi nvs_flash)