#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_event.h"
#include "calendar.h"
#include "cJSON.h"
#include "timesync.h"
#include "jwt.h"
#include "jwt_config.h"
#include "utils.h"

const char *Calendar = "calendar";

static const char *MONTH_NAMES[13] = {
    "", "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};


calendar_event_t g_events[MAX_EVENTS];
ParsedEvent parsed[MAX_EVENTS];
int g_event_count;

void clear_events(void)
{
    g_event_count = 0;
    memset(g_events, 0, sizeof(g_events));
}

int fetch_calendar_events(const char *token)
{
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
             JWT_CALENDAR_ID, enc_time_min, enc_time_max, MAX_EVENTS);
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

int get_event_count(void)
{
    return g_event_count;
}

const calendar_event_t* get_event(int index)
{
    if (index < 0 || index >= g_event_count) return NULL;
    return &g_events[index];
}

void parse_events_to_new_struct(ParsedEvent out_events[])
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
    return;
}

const char* format_day_month_text(int day, int month)
{
    static char buffer[32];

    if (month < 1 || month > 12) {
        return "Invalid";
    }

    sprintf(buffer, "%02d %s", day, MONTH_NAMES[month]);
    return buffer;
}

ParsedEvent* return_calendar_events()
{
    parse_events_to_new_struct(parsed);
    return parsed;
}