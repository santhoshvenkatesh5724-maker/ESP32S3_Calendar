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

void calendar_task(void *arg)
{
    while(1)
    {
        int count = fetch_calendar_events();
        if(count == -1)
        {
            esp_restart();
        }
        ESP_LOGI("MAIN", "Fetched %d events", count);

        parse_events_to_new_struct(parsed, &count);

        calendar_screen(count);

        vTaskDelay(pdMS_TO_TICKS(1200000));
        esp_restart();
    }

}


void app_main(void)
{
    waveshare_esp32_s3_rgb_lcd_init();
    vTaskDelay(pdMS_TO_TICKS(500));
    if (lvgl_port_lock(-1))
        {
            base_background();
            lvgl_port_unlock();
        }
    init_wifi();
    sync_time();
    
    ESP_LOGI("MAIN", "Display LVGL custom blue-rect demo");

    xTaskCreatePinnedToCore(calendar_task, "calendar", 24 * 1024, NULL, 5, NULL, 0);
    
}