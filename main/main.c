#include "waveshare_rgb_lcd_port.h"
#include "lvgl_port.h"
#include "utils.h"
#include "timesync.h"
#include "wifi.h"
#include "calendar.h"
#include "jwt.h"
#include "ui.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_system.h"

TaskHandle_t xHandle_Calendar = NULL;

void new_calendar_event_task(void *arg)
{
    while(1)
    {
        char *access_token = get_access_token();
        int event_count = fetch_calendar_events(access_token);
        ParsedEvent* parsed = return_calendar_events();
        if(event_count == -1)
        {
            vTaskDelete(xHandle_Calendar);
        }
        else 
        {
            calendar_screen(event_count, parsed);
        }
        ESP_LOGI("MEM", "Free heap: %d", esp_get_free_heap_size());
        ESP_LOGI("MEM", "Min heap: %d", esp_get_minimum_free_heap_size());
        ESP_LOGI("STACK", "Min free stack: %d", uxTaskGetStackHighWaterMark(xHandle_Calendar));
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI("MAIN", "Update events");
    }

}


void app_main(void)
{
    waveshare_esp32_s3_rgb_lcd_init();
    vTaskDelay(pdMS_TO_TICKS(500));
    init_wifi();
    sync_time();
    
    xTaskCreatePinnedToCore(new_calendar_event_task, "calendar", 24 * 1024, NULL, 5, &xHandle_Calendar, 0);
    
}