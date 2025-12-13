#include "waveshare_rgb_lcd_port.h"

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

#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

#include "cJSON.h"

#if __has_include("keys.c")
    #include "keys.c"
#else
    #include "keys_template.c"

#endif


/* Event Settings */
#define MAX_EVENTS 50
#define EVENT_TIME_WINDOW_DAYS 30  // Fetch events for next 30 days
#define FETCH_INTERVAL_MS 60000    // Refresh every 60 seconds

/* WiFi Settings */
#define WIFI_CONNECTED_BIT      BIT0
// ADD THESE TWO LINES --------------------------------
#define WIFI_FAIL_BIT           BIT1

/* Time Sync Settings */
#define NTP_SERVER "time.google.com"
#define TIME_SYNC_RETRY_COUNT 15
#define TIME_SYNC_RETRY_DELAY_MS 2000

/* HTTP Settings */
#define HTTP_TIMEOUT_MS 30000
#define HTTP_BUFFER_SIZE 4096
#define HTTP_TX_BUFFER_SIZE 2048
#define HTTP_RESPONSE_BUFFER_SIZE (32 * 1024)

#define WIFI_MAXIMUM_RETRY 5
#define WIFI_CONNECT_TIMEOUT_MS 10000

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;


/* Token Settings */
#define TOKEN_REFRESH_MARGIN_SEC 30  // Refresh token 30s before expiry

static const char *Calendar = "calendar";

/* ========== Data Structures ========== */

/* Event structure */
typedef struct {
    char name[256];
    char start_time[64];
    char end_time[64];
    char location[256];
    char description[512];
} calendar_event_t;

typedef struct {
    char name[64];        // event name only
    int start_day;
    int start_month;
    int start_year;
    char month_text[16];  // textual month name
    char start_hhmm[6];   // "HH:MM"
    char end_hhmm[6];     // "HH:MM"
} ParsedEvent;

#define HEARTBEAT_HOST           "8.8.8.8"        // Host used for connectivity check
#define HEARTBEAT_PORT           53               // Port used for connectivity check
#define HEARTBEAT_INTERVAL_MS    10000            // Interval between heartbeats (ms)
#define HEARTBEAT_TIMEOUT_MS     1500             // Connect timeout (ms) for each heartbeat
#define HEARTBEAT_MAX_FAILURES   3                // Failures before recovery action
#define HEARTBEAT_TASK_STACK     4096
#define HEARTBEAT_TASK_PRIORITY  5


static const char *MONTH_NAMES[13] = {
    "", "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

/* ========== Global Variables ========== */
static calendar_event_t g_events[MAX_EVENTS];
static int g_event_count = 0;
static char *g_access_token = NULL;
static time_t g_token_expiry = 0;
static EventGroupHandle_t s_wifi_event_group = NULL;

// Added for retry logic

ParsedEvent parsed[MAX_EVENTS];

/* ========== Utility Functions ========== */

static char* base64url_encode(const unsigned char *input, size_t ilen)
{
    size_t olen = 0;
    mbedtls_base64_encode(NULL, 0, &olen, input, ilen);
    unsigned char *b64 = malloc(olen + 1);
    if (!b64) return NULL;
    
    mbedtls_base64_encode(b64, olen, &olen, input, ilen);
    b64[olen] = 0;
    
    for (size_t i = 0; i < olen; ++i) {
        if (b64[i] == '+') b64[i] = '-';
        else if (b64[i] == '/') b64[i] = '_';
    }
    
    while (olen > 0 && b64[olen-1] == '=') olen--;
    b64[olen] = 0;
    
    char *ret = strdup((char*)b64);
    free(b64);
    return ret;
}

static char* url_encode(const char *src)
{
    if (!src) return NULL;
    size_t len = strlen(src);
    char *out = malloc(len * 3 + 1);
    if (!out) return NULL;
    
    char *p = out;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = src[i];
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || 
            (c >= 'a' && c <= 'z') || c == '-' || c == '.' || c == '_' || c == '~') {
            *p++ = c;
        } else {
            sprintf(p, "%%%02X", c);
            p += 3;
        }
    }
    *p = '\0';
    return out;
}

static void iso8601_now(char *buf, size_t len)
{
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

static void iso8601_future(char *buf, size_t len, int days)
{
    time_t future = time(NULL) + (days * 24 * 60 * 60);
    struct tm tm;
    gmtime_r(&future, &tm);
    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

/* ========== WiFi Functions ========== */
static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
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
            ESP_LOGI(Calendar, "retry to connect to the AP (%d/%d)", s_retry_num, WIFI_MAXIMUM_RETRY);
        } else {
            // Stop trying and tell the main loop we failed
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(Calendar, "Connection failed permanently.");
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        // CORRECT LOGIC: Wait for IP before signaling success
        s_retry_num = 0; // Reset retries on success
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(Calendar, "IP obtained, connection ready.");
    }
}

static void init_wifi(void)
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
    strncpy((char*)wifi_cfg.sta.password, WIFI_PASS, sizeof(wifi_cfg.sta.password) - 1);

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
        ESP_LOGI(Calendar, "WiFi connected successfully.");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(Calendar, "WiFi connection failed after %d retries.", WIFI_MAXIMUM_RETRY);
        // You may want to call esp_restart() or enter deep sleep here.
    } else {
        ESP_LOGE(Calendar, "WiFi connection timed out.");
    }
}

/* ========== Time Sync ========== */

static void sync_time(void)
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
    
    ESP_LOGI(Calendar, "Time synced");
}

/* ========== JWT Functions ========== */

static char* create_jwt(void)
{
    time_t now = time(NULL);
    
    // Build header
    cJSON *h = cJSON_CreateObject();
    cJSON_AddStringToObject(h, "alg", "RS256");
    cJSON_AddStringToObject(h, "typ", "JWT");
    char *h_str = cJSON_PrintUnformatted(h);
    cJSON_Delete(h);

    // Build payload
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "iss", SERVICE_ACCOUNT_EMAIL);
    cJSON_AddStringToObject(p, "scope", SCOPE);
    cJSON_AddStringToObject(p, "aud", TOKEN_URI);
    cJSON_AddNumberToObject(p, "iat", (double)now);
    cJSON_AddNumberToObject(p, "exp", (double)(now + 3600));
    char *p_str = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    // Encode header and payload
    char *h_b64 = base64url_encode((unsigned char*)h_str, strlen(h_str));
    char *p_b64 = base64url_encode((unsigned char*)p_str, strlen(p_str));
    free(h_str);
    free(p_str);

    // Create unsigned JWT
    size_t unsigned_len = strlen(h_b64) + strlen(p_b64) + 2;
    char *unsigned_jwt = malloc(unsigned_len);
    snprintf(unsigned_jwt, unsigned_len, "%s.%s", h_b64, p_b64);
    free(h_b64);
    free(p_b64);

    // Sign JWT
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr;
    unsigned char hash[32];
    unsigned char sig[512];
    size_t sig_len = 0;

    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr);

    mbedtls_pk_parse_key(&pk, (const unsigned char*)service_account_private_key_pem, 
                         strlen(service_account_private_key_pem) + 1, NULL, 0, NULL, NULL);
    mbedtls_ctr_drbg_seed(&ctr, mbedtls_entropy_func, &entropy, 
                          (const unsigned char*)"esp32", 5);

    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md(md, (const unsigned char*)unsigned_jwt, strlen(unsigned_jwt), hash);
    mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), sig, 
                    sizeof(sig), &sig_len, mbedtls_ctr_drbg_random, &ctr);

    char *sig_b64 = base64url_encode(sig, sig_len);

    // Complete JWT
    size_t jwt_len = strlen(unsigned_jwt) + strlen(sig_b64) + 2;
    char *jwt = malloc(jwt_len);
    snprintf(jwt, jwt_len, "%s.%s", unsigned_jwt, sig_b64);

    free(unsigned_jwt);
    free(sig_b64);
    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr);
    mbedtls_entropy_free(&entropy);

    return jwt;
}

/* ========== Token Functions ========== */

static char* fetch_access_token(void)
{
    char *jwt = create_jwt();
    if (!jwt) return NULL;

    char *enc_jwt = url_encode(jwt);
    free(jwt);
    if (!enc_jwt) return NULL;

    const char *prefix = "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=";
    size_t body_len = strlen(prefix) + strlen(enc_jwt) + 1;
    char *body = malloc(body_len);
    snprintf(body, body_len, "%s%s", prefix, enc_jwt);
    free(enc_jwt);

    esp_http_client_config_t cfg = {
        .url = TOKEN_URI,
        .method = HTTP_METHOD_POST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .buffer_size = HTTP_BUFFER_SIZE,
        .buffer_size_tx = HTTP_TX_BUFFER_SIZE,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        free(body);
        return NULL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_http_client_open(client, strlen(body));
    esp_http_client_write(client, body, strlen(body));
    esp_http_client_fetch_headers(client);

    char buf[HTTP_BUFFER_SIZE];
    int len = esp_http_client_read(client, buf, sizeof(buf) - 1);
    buf[len > 0 ? len : 0] = '\0';

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(body);

    cJSON *root = cJSON_Parse(buf);
    if (!root) return NULL;

    cJSON *token = cJSON_GetObjectItem(root, "access_token");
    cJSON *expires = cJSON_GetObjectItem(root, "expires_in");
    
    char *result = token ? strdup(token->valuestring) : NULL;
    int exp_time = expires ? expires->valueint : 3600;
    
    if (result) {
        g_token_expiry = time(NULL) + exp_time - TOKEN_REFRESH_MARGIN_SEC;
        if (g_access_token) free(g_access_token);
        g_access_token = strdup(result);
    }

    cJSON_Delete(root);
    return result;
}

static const char* get_access_token(void)
{
    time_t now = time(NULL);
    if (g_access_token && now < g_token_expiry) {
        return g_access_token;
    }
    
    char *token = fetch_access_token();
    if (token) free(token);
    return g_access_token;
}

/* ========== Calendar Functions ========== */

static void clear_events(void)
{
    g_event_count = 0;
    memset(g_events, 0, sizeof(g_events));
}

static int fetch_calendar_events(void)
{
    const char *token = get_access_token();
    if (!token) return -1;

    clear_events();

    char time_min[64];
    char time_max[64];
    iso8601_now(time_min, sizeof(time_min));
    iso8601_future(time_max, sizeof(time_max), EVENT_TIME_WINDOW_DAYS);
    
    char *enc_time_min = url_encode(time_min);
    char *enc_time_max = url_encode(time_max);

    char url[1024];
    snprintf(url, sizeof(url),
             "https://www.googleapis.com/calendar/v3/calendars/%s/events?"
             "singleEvents=true&orderBy=startTime&timeMin=%s&timeMax=%s&maxResults=%d",
             CALENDAR_ID, enc_time_min, enc_time_max, MAX_EVENTS);
    free(enc_time_min);
    free(enc_time_max);

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .buffer_size = HTTP_BUFFER_SIZE,
        .buffer_size_tx = HTTP_TX_BUFFER_SIZE,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return -1;

    size_t auth_len = strlen(token) + 32;
    char *auth = malloc(auth_len);
    snprintf(auth, auth_len, "Bearer %s", token);

    esp_http_client_set_header(client, "Authorization", auth);
    esp_http_client_open(client, 0);
    esp_http_client_fetch_headers(client);

    int status = esp_http_client_get_status_code(client);

    char *buf = malloc(HTTP_RESPONSE_BUFFER_SIZE);
    int total = 0;
    int r;
    while ((r = esp_http_client_read(client, buf + total, HTTP_RESPONSE_BUFFER_SIZE - total)) > 0) {
        total += r;
    }
    buf[total] = '\0';

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(auth);

    if (status != 200 || total == 0) {
        free(buf);
        return -1;
    }

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return -1;

    cJSON *items = cJSON_GetObjectItem(root, "items");
    if (items && cJSON_IsArray(items)) {
        int count = cJSON_GetArraySize(items);
        g_event_count = count > MAX_EVENTS ? MAX_EVENTS : count;

        for (int i = 0; i < g_event_count; i++) {
            cJSON *item = cJSON_GetArrayItem(items, i);
            cJSON *summary = cJSON_GetObjectItem(item, "summary");
            cJSON *start = cJSON_GetObjectItem(item, "start");
            cJSON *end = cJSON_GetObjectItem(item, "end");
            cJSON *location = cJSON_GetObjectItem(item, "location");
            cJSON *description = cJSON_GetObjectItem(item, "description");

            // Store event name
            if (summary && summary->valuestring) {
                strncpy(g_events[i].name, summary->valuestring, sizeof(g_events[i].name) - 1);
            }

            // Store start time
            if (start) {
                cJSON *dt = cJSON_GetObjectItem(start, "dateTime");
                if (!dt) dt = cJSON_GetObjectItem(start, "date");
                if (dt && dt->valuestring) {
                    strncpy(g_events[i].start_time, dt->valuestring, sizeof(g_events[i].start_time) - 1);
                }
            }

            // Store end time
            if (end) {
                cJSON *dt = cJSON_GetObjectItem(end, "dateTime");
                if (!dt) dt = cJSON_GetObjectItem(end, "date");
                if (dt && dt->valuestring) {
                    strncpy(g_events[i].end_time, dt->valuestring, sizeof(g_events[i].end_time) - 1);
                }
            }

            // Store location
            if (location && location->valuestring) {
                strncpy(g_events[i].location, location->valuestring, sizeof(g_events[i].location) - 1);
            }

            // Store description
            if (description && description->valuestring) {
                strncpy(g_events[i].description, description->valuestring, sizeof(g_events[i].description) - 1);
            }
        }
    }

    cJSON_Delete(root);
    return g_event_count;
}

/* ========== Public API ========== */

int get_event_count(void)
{
    return g_event_count;
}

const calendar_event_t* get_event(int index)
{
    if (index < 0 || index >= g_event_count) return NULL;
    return &g_events[index];
}

void parse_events_to_new_struct(ParsedEvent out_events[], int *out_count)
{
    for (int i = 0; i < g_event_count; ++i) {
        ParsedEvent *dst = &out_events[i];

        // Copy only event name
        strcpy(dst->name, g_events[i].name);

        // Extract YYYY-MM-DDTHH:MM from ISO string
        int year, month, day, st_h, st_m, end_h, end_m;
        sscanf(g_events[i].start_time, "%d-%d-%dT%d:%d",
               &year, &month, &day, &st_h, &st_m);

        sscanf(g_events[i].end_time, "%*d-%*d-%*dT%d:%d",
               &end_h, &end_m);

        dst->start_year  = year;
        dst->start_month = month;
        dst->start_day   = day;

        strcpy(dst->month_text, MONTH_NAMES[month]);

        // Format times into HH:MM
        sprintf(dst->start_hhmm, "%02d:%02d", st_h, st_m);
        sprintf(dst->end_hhmm,   "%02d:%02d", end_h, end_m);
    }

    *out_count = g_event_count;
}

const char* format_day_month_text(int day, int month)
{
    static char buffer[32];

    static const char *MONTH_NAMES[13] = {
        "", "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    if (month < 1 || month > 12) {
        return "Invalid";
    }

    sprintf(buffer, "%02d %s", day, MONTH_NAMES[month]);
    return buffer;
}

void logger_task(void *arg)
{
    while(1)
    {
        ESP_LOGI("Logger", "Running Logger Background");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/* ========== Main Task ========== */

void calendar_task(void *arg)
{
    while(1)
    {
        int count = fetch_calendar_events();
        if(count == -1)
        {
            esp_restart();
        }
        ESP_LOGI(Calendar, "Fetched %d events", count);

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
            ESP_LOGI("Display Update", "Calendar display updated");
        }
        xTaskCreatePinnedToCore(logger_task, "logger", 24 * 1024, NULL, 5, NULL, 1);
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
    
    ESP_LOGI("Display Set", "Display LVGL custom blue-rect demo");

    xTaskCreatePinnedToCore(calendar_task, "calendar", 24 * 1024, NULL, 5, NULL, 0);
    
}
