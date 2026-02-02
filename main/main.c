#include "waveshare_rgb_lcd_port.h"
#include "lvgl_port.h"
#include "utils.h"
#include "timesync.h"
#include "wifi.h"
#include "calendar.h"
#include "jwt.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_sntp.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"

#include "cJSON.h"

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

        if (count > 0) {
            //print_all_events();
        }

        if (lvgl_port_lock(-1))
        {
            base_background();
            int k=0;
            int l=0;
            for(int i=0; i<count; i++)
            {
                if(i>0)
                {
                    if(parsed[i].start_day == parsed[i-1].start_day && parsed[i].start_month == parsed[i-1].start_month)
                    {
                        k=k+1;
                    }
                    else
                    {
                        k=0;
                        l = l + 1;
                    }
                }
                
                if(k == 0)
                {
                    date_month(80 + 160*l ,30, format_day_month_text(parsed[i].start_day, parsed[i].start_month));
                }
                waveshare_rect_event_box(5 + 160*(l) , 60 + 105*k, 150, 100, parsed[i].name, parsed[i].start_hhmm, parsed[i].end_hhmm);
            }
            lvgl_port_unlock();
            ESP_LOGI("MAIN", "Calendar display updated");
        }
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