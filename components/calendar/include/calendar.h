#pragma once

#include "esp_crt_bundle.h"
#include "esp_http_client.h"

#define MAX_EVENTS 50
#define EVENT_TIME_WINDOW_DAYS 30  // Fetch events for next 30 days
#define FETCH_INTERVAL_MS 60000    // Refresh every 60 seconds

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

static const char *MONTH_NAMES[13] = {
    "", "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

extern calendar_event_t g_events[MAX_EVENTS];
extern int g_event_count;
extern ParsedEvent parsed[MAX_EVENTS];

void clear_events(void);
int fetch_calendar_events(void);
int get_event_count(void);
const calendar_event_t* get_event(int index);
void parse_events_to_new_struct(ParsedEvent out_events[], int *out_count);
const char* format_day_month_text(int day, int month);