idf_component_register(
    SRCS "sd_card.c"
    INCLUDE_DIRS "include"
    REQUIRES vfs lvgl__lvgl fatfs driver)