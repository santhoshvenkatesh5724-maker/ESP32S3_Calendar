#pragma once

#include "stdlib.h"
#include <time.h>

#define HTTP_TIMEOUT_MS 30000
#define HTTP_BUFFER_SIZE 4096
#define HTTP_TX_BUFFER_SIZE 2048
#define HTTP_RESPONSE_BUFFER_SIZE (32 * 1024)

#define WIFI_MAXIMUM_RETRY 5
#define WIFI_CONNECT_TIMEOUT_MS 10000

#define TOKEN_REFRESH_MARGIN_SEC 30

extern char *g_access_token;
extern time_t g_token_expiry;

char* create_jwt(void);
char* fetch_access_token(void);
const char* get_access_token(void);