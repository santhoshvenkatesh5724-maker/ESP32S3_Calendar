idf_component_register(
    SRCS "calendar.c"
    INCLUDE_DIRS "include"
    REQUIRES mbedtls esp_wifi esp_http_client cjson net auth)