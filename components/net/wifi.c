#include "wifi.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "wifi_creds.h"

static const char *TAG = "WIFI";

EventGroupHandle_t s_wifi_event_group;
int s_retry_num;

void init_wifi(void)
{
    // Initialize NVS (Ensures that initialization failure is checked)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initial setup
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // STABILITY FIX: Disable Power Save Mode
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    wifi_config_t wifi_cfg = {0};
    strncpy((char*)wifi_cfg.sta.ssid, WIFI_SSID, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char*)wifi_cfg.sta.password, WIFI_PASSWORD, sizeof(wifi_cfg.sta.password) - 1);

    s_wifi_event_group = xEventGroupCreate();
    
    // Use instance registration for proper error handling
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    // SAFETY 3: Check Result Bits (Wait for connection OR failure)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, 
                        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, // Wait for success OR fail
                        pdFALSE, pdFALSE, 
                        pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected successfully.");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "WiFi connection failed after %d retries.", WIFI_MAXIMUM_RETRY);
        // You may want to call esp_restart() or enter deep sleep here.
    } else {
        ESP_LOGE(TAG, "WiFi connection timed out.");
    }
}

void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        // SAFETY 1: Clear the connected bit so the app knows we are offline
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        // SAFETY 2: Implement Retry Limit
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP (%d/%d)", s_retry_num, WIFI_MAXIMUM_RETRY);
        } else {
            // Stop trying and tell the main loop we failed
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Connection failed permanently.");
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        // CORRECT LOGIC: Wait for IP before signaling success
        s_retry_num = 0; // Reset retries on success
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "IP obtained, connection ready.");
    }
}
