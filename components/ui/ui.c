#include "waveshare_rgb_lcd_port.h"
#include "lvgl_port.h"
#include "utils.h"
#include "timesync.h"
#include "wifi.h"
#include "calendar.h"
#include "jwt.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_system.h"

void calendar_screen(int count, ParsedEvent parsed[])
{
    if (lvgl_port_lock(-1))
    {
        lv_obj_t *calendar_screen = base_background();
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
                date_month(80 + 160*l ,30, format_day_month_text(parsed[i].start_day, parsed[i].start_month), calendar_screen);
            }
            waveshare_rect_event_box(5 + 160*(l) , 60 + 105*k, 150, 100, parsed[i].name, parsed[i].start_hhmm, parsed[i].end_hhmm, calendar_screen);
        }
        lv_scr_load(calendar_screen);
        lvgl_port_unlock();
        ESP_LOGI("MAIN", "Calendar display updated");
    }
}