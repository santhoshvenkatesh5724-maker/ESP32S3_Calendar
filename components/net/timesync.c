#include "timesync.h"

static const char *TAG = "timesync";

void sync_time(void)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, NTP_SERVER);
    esp_sntp_init();

    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;

    while (timeinfo.tm_year < (2020 - 1900) && ++retry < TIME_SYNC_RETRY_COUNT) {
        vTaskDelay(pdMS_TO_TICKS(TIME_SYNC_RETRY_DELAY_MS));
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    
    ESP_LOGI(TAG, "Time synced");
}

void iso8601_now(char *buf, size_t len)
{
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

void iso8601_future(char *buf, size_t len, int days)
{
    time_t future = time(NULL) + (days * 24 * 60 * 60);
    struct tm tm;
    gmtime_r(&future, &tm);
    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &tm);
}