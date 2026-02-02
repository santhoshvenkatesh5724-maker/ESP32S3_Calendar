if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/jwt_config.c")
    set(KEYS_SRC jwt_config.c)
else()
    set(KEYS_SRC jwt_config_template.c)
endif()

idf_component_register(
    SRCS "jwt.c" ${KEYS_SRC}
    INCLUDE_DIRS "include"
    REQUIRES mbedtls esp_wifi esp_http_client cjson net)


