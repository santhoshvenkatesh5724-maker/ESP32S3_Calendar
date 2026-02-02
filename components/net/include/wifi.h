#pragma once

#include "esp_event.h"

#define WIFI_MAXIMUM_RETRY 5
#define WIFI_CONNECT_TIMEOUT_MS 10000 

#define HEARTBEAT_HOST           "8.8.8.8"        // Host used for connectivity check
#define HEARTBEAT_PORT           53               // Port used for connectivity check
#define HEARTBEAT_INTERVAL_MS    10000            // Interval between heartbeats (ms)
#define HEARTBEAT_TIMEOUT_MS     1500             // Connect timeout (ms) for each heartbeat
#define HEARTBEAT_MAX_FAILURES   3                // Failures before recovery action
#define HEARTBEAT_TASK_STACK     4096
#define HEARTBEAT_TASK_PRIORITY  5

#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1

extern EventGroupHandle_t s_wifi_event_group;
extern int s_retry_num;

void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);
void init_wifi(void);