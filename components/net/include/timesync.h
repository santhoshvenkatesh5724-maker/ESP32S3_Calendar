#pragma once

#include "esp_sntp.h"
#include "esp_log.h"
#include <stddef.h>

#define NTP_SERVER "time.google.com"
#define TIME_SYNC_RETRY_COUNT 15
#define TIME_SYNC_RETRY_DELAY_MS 2000

void sync_time(void);
void iso8601_now(char *buf, size_t len);
void iso8601_future(char *buf, size_t len, int days);

